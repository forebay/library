package io.github.forebay.library.parity;

import io.github.intisy.ai.seam.jvm.library.LibraryPin;

import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/** The released libraries the parity host loads. */
final class ParityPins {

    /** The released apps library the parity suite holds to basekit, pinned by digest. */
    static final LibraryPin APPS_PIN = new LibraryPin("forebay/library", "v0.1.1", appsDigests());

    private ParityPins() {
    }

    static List<LibraryPin> pins() {
        return Collections.singletonList(APPS_PIN);
    }

    private static Map<String, String> appsDigests() {
        Map<String, String> sha256 = new LinkedHashMap<String, String>();
        sha256.put("library.json", "420e0f66f8b82b5ec4704c87089ec79caf9475e68262bd1b5878e079faefa2f6");
        sha256.put("linux-x64", "511a24739c7f05ff3c58d32171c42b3ee3c5bf840afad074aeaeba7d4d77f2cd");
        sha256.put("win32-x64", "ecc2616750b6f1168c127ddf3619b9d46e06c4e47078155681558e565e5c5f0d");
        return sha256;
    }
}
