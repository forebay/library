import type { LibraryPin } from "@weivel/typescript/host";
import { readPin } from "../../stage.mjs";

/** The released apps library the parity suite holds to basekit, pinned by digest in `parity/pin.json`. */
export const APPS_PIN: LibraryPin = readPin();

/** The released libraries the parity host loads. */
export const PARITY_PINS: readonly LibraryPin[] = [APPS_PIN];
