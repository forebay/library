package io.github.forebay.library.parity;

import com.google.gson.JsonObject;

import java.util.List;
import java.util.Map;

/** One case laid out on disk, with the facts every implementation is handed for it. */
final class StagedCase {

    private final String name;
    private final String root;
    private final String home;
    private final Map<String, String> environment;
    private final List<String> arguments;
    private final List<JsonObject> calls;

    StagedCase(String name, String root, String home, Map<String, String> environment, List<String> arguments, List<JsonObject> calls) {
        this.name = name;
        this.root = root;
        this.home = home;
        this.environment = environment;
        this.arguments = arguments;
        this.calls = calls;
    }

    String name() {
        return name;
    }

    String root() {
        return root;
    }

    String home() {
        return home;
    }

    Map<String, String> environment() {
        return environment;
    }

    List<String> arguments() {
        return arguments;
    }

    List<JsonObject> calls() {
        return calls;
    }
}
