import { isPluginError } from "@weivel/typescript/engine";
import { failure, normalised } from "../../stage.mjs";
import type { ParityCall, StagedCase } from "../../stage.mjs";
import type { AppsService } from "./parity-plugin.js";

/** A refusal the library itself answered, which the bridge labels with the service and function before the library's own message. */
function isLibraryFailure(error: unknown, call: ParityCall): boolean {
  return isPluginError(error) && error.detail.startsWith(`apps.${call.function}: `);
}

/** What the plugin observes for one call, as JSON, with a failure the library answered read as basekit's throw. */
export function observedAnswer(apps: AppsService, call: ParityCall, staged: StagedCase): unknown {
  const method = apps[call.function];
  if (typeof method !== "function") throw new Error(`the apps service offers no ${call.function}`);
  try {
    return normalised(JSON.parse(JSON.stringify(method(call.argument))), staged);
  } catch (error) {
    if (isLibraryFailure(error, call)) return failure();
    throw error;
  }
}
