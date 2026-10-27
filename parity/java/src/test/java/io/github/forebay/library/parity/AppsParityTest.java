package io.github.forebay.library.parity;

import com.google.gson.Gson;
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import io.github.intisy.ai.engine.PluginException;
import org.junit.jupiter.api.Test;

import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Locale;
import java.util.Set;

import static org.junit.jupiter.api.Assertions.assertEquals;

class AppsParityTest {

    private static final String IMPLEMENTATION = "java";
    private static final Gson GSON = new Gson();

    private final ParityCases cases = ParityCases.fromSystemProperty();

    @Test
    void answersEveryCaseAsBasekitDoes() throws Exception {
        List<JsonObject> divergences = divergencesOfThisImplementation();
        Path work = cases.workDirectory(IMPLEMENTATION);
        Path libraryCache = work.resolve(".library-cache");
        List<String> names = cases.names();
        List<String> mismatches = new ArrayList<String>();
        Set<JsonObject> held = new HashSet<JsonObject>();
        int calls = 0;

        for (String name : names) {
            JsonObject expected = cases.expected(name);
            assertEquals(currentPlatform(), expected.get("platform").getAsString(), name + " was answered by basekit on another platform");
            StagedCase staged = cases.stage(name, work);
            JsonArray answers = expected.getAsJsonArray("answers");
            assertEquals(staged.calls().size(), answers.size(), name + " has a stale expected file");
            try (ParityHost host = ParityHost.start(staged, ParityPins.pins(), libraryCache)) {
                for (int index = 0; index < staged.calls().size(); index++) {
                    calls += 1;
                    JsonObject divergence = divergenceFor(divergences, name, index);
                    if (divergence != null) {
                        held.add(divergence);
                    }
                    JsonElement observed = observedAnswer(host.apps(), staged.calls().get(index), staged);
                    String mismatch = mismatchOf(name, index, observed, answers.get(index), divergence);
                    if (mismatch != null) {
                        mismatches.add(mismatch);
                    }
                }
            }
        }

        List<JsonObject> unheld = new ArrayList<JsonObject>(divergences);
        unheld.removeAll(held);
        System.out.println("java: " + names.size() + " cases, " + calls + " calls, " + held.size()
                + " recorded divergences held, " + mismatches.size() + " mismatches");
        assertEquals(new ArrayList<JsonObject>(), unheld, "a divergence names no case and call");
        assertEquals(new ArrayList<String>(), mismatches);
    }

    private List<JsonObject> divergencesOfThisImplementation() throws Exception {
        List<JsonObject> divergences = new ArrayList<JsonObject>();
        for (JsonElement entry : cases.divergences()) {
            JsonObject divergence = entry.getAsJsonObject();
            for (JsonElement implementation : divergence.getAsJsonArray("implementations")) {
                if (IMPLEMENTATION.equals(implementation.getAsString())) {
                    divergences.add(divergence);
                }
            }
        }
        return divergences;
    }

    private static JsonObject divergenceFor(List<JsonObject> divergences, String name, int call) {
        for (JsonObject divergence : divergences) {
            if (divergence.get("case").getAsString().equals(name) && divergence.get("call").getAsInt() == call) {
                return divergence;
            }
        }
        return null;
    }

    /**
     * @implNote A divergence is held in both directions: the library must still answer what the
     * table records, and basekit must still answer something else, so neither side drifts into or
     * out of it unnoticed.
     */
    private static String mismatchOf(String name, int call, JsonElement observed, JsonElement basekit, JsonObject divergence) {
        if (divergence == null) {
            return observed.equals(basekit) ? null : describe(name, call, "differs from basekit", observed, basekit);
        }
        JsonElement recorded = divergence.get("answer");
        if (!observed.equals(recorded)) {
            return describe(name, call, "differs from the recorded divergence", observed, recorded);
        }
        return basekit.equals(recorded) ? describe(name, call, "basekit agrees, so the recorded divergence is stale", observed, basekit) : null;
    }

    private static String describe(String name, int call, String reason, JsonElement observed, JsonElement wanted) {
        return name + " call " + call + " " + reason + ": observed " + observed + ", wanted " + wanted;
    }

    /** What the plugin observes for one call, with a failure the library answered read as basekit's throw. */
    private static JsonElement observedAnswer(AppsService apps, JsonObject call, StagedCase staged) throws Exception {
        String function = call.get("function").getAsString();
        Method method = methodNamed(function);
        Class<?> parameter = method.getParameterTypes()[0];
        JsonObject argument = call.getAsJsonObject("argument");
        Object typedArgument = parameter == JsonObject.class ? argument : GSON.fromJson(argument, parameter);
        try {
            return ParityCases.normalised(Observed.of(method.invoke(apps, typedArgument)), staged);
        } catch (InvocationTargetException thrown) {
            if (isLibraryFailure(thrown.getCause(), function)) {
                JsonObject failure = new JsonObject();
                failure.addProperty("fails", true);
                return failure;
            }
            throw thrown;
        }
    }

    /** A refusal the library itself answered, which the bridge labels with the service and function before the library's own message. */
    private static boolean isLibraryFailure(Throwable thrown, String function) {
        return thrown instanceof PluginException && ((PluginException) thrown).getDetail().startsWith("apps." + function + ": ");
    }

    private static Method methodNamed(String function) {
        for (Method method : AppsService.class.getMethods()) {
            if (method.getName().equals(function) && method.getParameterTypes().length == 1) {
                return method;
            }
        }
        throw new IllegalArgumentException("the apps service offers no " + function);
    }

    private static String currentPlatform() {
        String os = System.getProperty("os.name").toLowerCase(Locale.ROOT);
        String arch = System.getProperty("os.arch").toLowerCase(Locale.ROOT);
        String platform = os.startsWith("windows") ? "win32" : os.startsWith("mac") ? "darwin" : os;
        return platform + "-" + ("amd64".equals(arch) || "x86_64".equals(arch) ? "x64" : arch);
    }
}
