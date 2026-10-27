package io.github.forebay.library.parity;

import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import io.github.intisy.ai.seam.jvm.library.LibraryPin;

import java.io.IOException;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** The released libraries the parity host loads, pinned by digest in {@code parity/pin.json}. */
final class ParityPins {

    private ParityPins() {
    }

    static List<LibraryPin> pins(ParityCases cases) throws IOException {
        return Collections.singletonList(appsPin(cases.pin()));
    }

    private static LibraryPin appsPin(JsonObject pin) {
        Map<String, String> sha256 = new LinkedHashMap<String, String>();
        for (Map.Entry<String, JsonElement> digest : pin.getAsJsonObject("sha256").entrySet()) {
            sha256.put(digest.getKey(), digest.getValue().getAsString());
        }
        return new LibraryPin(pin.get("repository").getAsString(), pin.get("tag").getAsString(), sha256);
    }
}
