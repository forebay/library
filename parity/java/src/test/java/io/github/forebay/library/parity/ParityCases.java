package io.github.forebay.library.parity;

import com.google.gson.Gson;
import com.google.gson.GsonBuilder;
import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import com.google.gson.JsonPrimitive;

import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Reads and lays out the parity cases exactly as {@code parity/stage.mjs} does for the oracle and
 * the TypeScript driver.
 */
final class ParityCases {

    private static final String DEFAULT_REGISTRY = "{home}/.config/forebay/apps.json";
    private static final Gson JSON_STRINGS = new GsonBuilder().disableHtmlEscaping().create();

    private final Path parityDirectory;

    ParityCases(Path parityDirectory) {
        this.parityDirectory = parityDirectory;
    }

    static ParityCases fromSystemProperty() {
        return new ParityCases(Paths.get(System.getProperty("parity.directory")).toAbsolutePath().normalize());
    }

    Path workDirectory(String driver) {
        return parityDirectory.resolve("work").resolve(driver);
    }

    List<String> names() {
        List<String> names = new ArrayList<String>();
        File[] entries = casesDirectory().toFile().listFiles();
        if (entries != null) {
            for (File entry : entries) {
                if (entry.isDirectory()) {
                    names.add(entry.getName());
                }
            }
        }
        Collections.sort(names);
        return names;
    }

    JsonObject expected(String name) throws IOException {
        return parse(parityDirectory.resolve("expected").resolve(name + ".json")).getAsJsonObject();
    }

    JsonArray divergences() throws IOException {
        return parse(parityDirectory.resolve("divergences.json")).getAsJsonObject().getAsJsonArray("divergences");
    }

    StagedCase stage(String name, Path workRoot) throws IOException {
        Path caseDirectory = casesDirectory().resolve(name);
        JsonObject definition = parse(caseDirectory.resolve("case.json")).getAsJsonObject();
        Path root = workRoot.resolve(name).toAbsolutePath().normalize();
        deleteTree(root);
        String home = root.resolve("home").toString();
        Files.createDirectories(Paths.get(home));
        Tokens tokens = new Tokens(root.toString(), home);
        for (JsonElement directory : definition.getAsJsonArray("homeLayout")) {
            Files.createDirectories(Paths.get(tokens.substitute(directory.getAsString())));
        }
        for (Map.Entry<String, String> file : registryFiles(caseDirectory, definition).entrySet()) {
            Path target = Paths.get(tokens.substitute(file.getKey()));
            Files.createDirectories(target.getParent());
            Files.write(target, tokens.substituteInRegistry(Files.readAllBytes(caseDirectory.resolve(file.getValue()))));
        }
        Map<String, String> environment = new LinkedHashMap<String, String>();
        for (Map.Entry<String, JsonElement> variable : definition.getAsJsonObject("environment").entrySet()) {
            environment.put(variable.getKey(), tokens.substitute(variable.getValue().getAsString()));
        }
        List<String> arguments = new ArrayList<String>();
        for (JsonElement argument : definition.getAsJsonArray("arguments")) {
            arguments.add(tokens.substitute(argument.getAsString()));
        }
        List<JsonObject> calls = new ArrayList<JsonObject>();
        for (JsonElement call : definition.getAsJsonArray("calls")) {
            calls.add(call.getAsJsonObject());
        }
        return new StagedCase(name, root.toString(), home, environment, arguments, calls);
    }

    /** An answer with the case's own directories written back as their tokens, so it reads the same wherever the case was laid out. */
    static JsonElement normalised(JsonElement value, StagedCase staged) {
        if (value.isJsonPrimitive() && value.getAsJsonPrimitive().isString()) {
            return new JsonPrimitive(value.getAsString().replace(staged.home(), "{home}").replace(staged.root(), "{root}"));
        }
        if (value.isJsonArray()) {
            JsonArray array = new JsonArray();
            for (JsonElement element : value.getAsJsonArray()) {
                array.add(normalised(element, staged));
            }
            return array;
        }
        if (value.isJsonObject()) {
            JsonObject object = new JsonObject();
            for (Map.Entry<String, JsonElement> member : value.getAsJsonObject().entrySet()) {
                object.add(member.getKey(), normalised(member.getValue(), staged));
            }
            return object;
        }
        return value;
    }

    private Path casesDirectory() {
        return parityDirectory.resolve("cases");
    }

    private static Map<String, String> registryFiles(Path caseDirectory, JsonObject definition) {
        Map<String, String> files = new LinkedHashMap<String, String>();
        if (definition.has("files")) {
            for (Map.Entry<String, JsonElement> file : definition.getAsJsonObject("files").entrySet()) {
                files.put(file.getKey(), file.getValue().getAsString());
            }
        } else if (Files.isRegularFile(caseDirectory.resolve("apps.json"))) {
            files.put(DEFAULT_REGISTRY, "apps.json");
        }
        return files;
    }

    private static JsonElement parse(Path file) throws IOException {
        return JsonParser.parseString(new String(Files.readAllBytes(file), StandardCharsets.UTF_8));
    }

    private static void deleteTree(Path path) throws IOException {
        File file = path.toFile();
        if (!file.exists()) {
            return;
        }
        File[] children = Files.isSymbolicLink(path) ? null : file.listFiles();
        if (children != null) {
            for (File child : children) {
                deleteTree(child.toPath());
            }
        }
        Files.delete(path);
    }

    /** The two tokens a case writes where its directories go. */
    private static final class Tokens {
        private final String root;
        private final String home;

        Tokens(String root, String home) {
            this.root = root;
            this.home = home;
        }

        String substitute(String text) {
            return text.replace("{home}", home).replace("{root}", root);
        }

        /** @implNote A registry is bytes, not text, so a BOM or an invalid sequence reaches the reader untouched; only the ASCII tokens are replaced, JSON-escaped. */
        byte[] substituteInRegistry(byte[] bytes) {
            byte[] once = replaced(bytes, "{home}", jsonEscaped(home));
            return replaced(once, "{root}", jsonEscaped(root));
        }

        private static String jsonEscaped(String value) {
            String quoted = JSON_STRINGS.toJson(value);
            return quoted.substring(1, quoted.length() - 1);
        }

        private static byte[] replaced(byte[] bytes, String token, String value) {
            byte[] pattern = token.getBytes(StandardCharsets.US_ASCII);
            byte[] replacement = value.getBytes(StandardCharsets.UTF_8);
            ByteArrayOutputStream out = new ByteArrayOutputStream();
            int index = 0;
            while (index < bytes.length) {
                if (startsWith(bytes, index, pattern)) {
                    out.write(replacement, 0, replacement.length);
                    index += pattern.length;
                } else {
                    out.write(bytes[index]);
                    index += 1;
                }
            }
            return out.toByteArray();
        }

        private static boolean startsWith(byte[] bytes, int offset, byte[] pattern) {
            if (offset + pattern.length > bytes.length) {
                return false;
            }
            for (int index = 0; index < pattern.length; index++) {
                if (bytes[offset + index] != pattern[index]) {
                    return false;
                }
            }
            return true;
        }
    }
}
