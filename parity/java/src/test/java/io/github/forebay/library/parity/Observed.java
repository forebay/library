package io.github.forebay.library.parity;

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonNull;
import com.google.gson.JsonObject;
import com.google.gson.JsonPrimitive;
import io.github.intisy.ai.engine.PluginException;
import io.github.intisy.ai.tsemit.TsLiteral;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.lang.reflect.Proxy;
import java.util.Collection;
import java.util.Map;

/**
 * The JSON a Java plugin observes in what the generated service returned, read through the
 * getters it would call.
 *
 * @implNote Deliberately not Gson's own serialisation: a data interface writes back the JSON it was
 * read from, which would hide a value the getter reads differently (a number through a String
 * getter). A getter answering null is written as absent, since a plugin cannot tell the two apart.
 */
final class Observed {

    private static final JsonObject REFUSED = refused();

    private Observed() {
    }

    private static JsonObject refused() {
        JsonObject refused = new JsonObject();
        refused.addProperty("refused", true);
        return refused;
    }

    static JsonElement of(Object value) {
        if (value == null) {
            return JsonNull.INSTANCE;
        }
        if (value instanceof JsonElement) {
            return ((JsonElement) value).deepCopy();
        }
        if (value instanceof String) {
            return new JsonPrimitive((String) value);
        }
        if (value instanceof Number) {
            return new JsonPrimitive((Number) value);
        }
        if (value instanceof Boolean) {
            return new JsonPrimitive((Boolean) value);
        }
        if (value instanceof Enum) {
            return new JsonPrimitive(literalOf((Enum<?>) value));
        }
        if (value instanceof Collection) {
            JsonArray array = new JsonArray();
            for (Object element : (Collection<?>) value) {
                array.add(of(element));
            }
            return array;
        }
        if (value instanceof Map) {
            JsonObject object = new JsonObject();
            for (Map.Entry<?, ?> entry : ((Map<?, ?>) value).entrySet()) {
                object.add(String.valueOf(entry.getKey()), of(entry.getValue()));
            }
            return object;
        }
        return Proxy.isProxyClass(value.getClass()) ? dataInterface(value) : generatedClass(value);
    }

    /**
     * @implNote A getter the seam refuses, which it does for a value its declared type cannot hold,
     * is written as {@code {"refused": true}} so the other properties are still observed.
     */
    private static JsonObject dataInterface(Object proxy) {
        JsonObject object = new JsonObject();
        for (Method property : proxy.getClass().getInterfaces()[0].getMethods()) {
            if (!Modifier.isStatic(property.getModifiers()) && property.getParameterTypes().length == 0) {
                try {
                    addProperty(object, property.getName(), invoke(property, proxy));
                } catch (GetterRefusal refused) {
                    object.add(property.getName(), REFUSED.deepCopy());
                }
            }
        }
        return object;
    }

    private static JsonObject generatedClass(Object value) {
        JsonObject object = new JsonObject();
        for (Method getter : value.getClass().getDeclaredMethods()) {
            String name = getter.getName();
            if (Modifier.isPublic(getter.getModifiers()) && getter.getParameterTypes().length == 0 && name.startsWith("get") && name.length() > 3) {
                addProperty(object, Character.toLowerCase(name.charAt(3)) + name.substring(4), invoke(getter, value));
            }
        }
        return object;
    }

    private static void addProperty(JsonObject object, String name, Object value) {
        if (value != null) {
            object.add(name, of(value));
        }
    }

    private static Object invoke(Method getter, Object target) {
        try {
            return getter.invoke(target);
        } catch (InvocationTargetException thrown) {
            if (thrown.getCause() instanceof PluginException) {
                throw new GetterRefusal();
            }
            throw new IllegalStateException("cannot read " + getter, thrown);
        } catch (IllegalAccessException unreadable) {
            throw new IllegalStateException("cannot read " + getter, unreadable);
        }
    }

    private static final class GetterRefusal extends RuntimeException {
        GetterRefusal() {
            super(null, null, false, false);
        }
    }

    private static String literalOf(Enum<?> constant) {
        try {
            TsLiteral literal = constant.getDeclaringClass().getField(constant.name()).getAnnotation(TsLiteral.class);
            return literal == null ? constant.name() : literal.value();
        } catch (NoSuchFieldException impossible) {
            throw new IllegalStateException("every enum constant is a public field of its enum", impossible);
        }
    }
}
