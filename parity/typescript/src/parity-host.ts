import { join } from "node:path";
import { startPlugins } from "@weivel/typescript/host";
import type { LibraryPin, LoadedHost } from "@weivel/typescript/host";
import type { PluginRuntime } from "@weivel/typescript/engine";
import type { StagedCase } from "../../stage.mjs";
import { parityPlugin } from "./parity-plugin.js";
import type { AppsService } from "./parity-plugin.js";

export interface ParityHost {
  apps: AppsService;
  stop(): Promise<void>;
}

function runtimeFor(staged: StagedCase): PluginRuntime {
  const { home } = staged;
  return {
    config: { all: () => ({}), get: () => undefined, set: async () => {} },
    log: { info: () => {}, warn: () => {}, error: () => {}, debug: () => {} },
    paths: { home, repos: join(home, "repos"), plugin: join(home, "plugin"), cache: join(home, "cache"), config: join(home, "config") },
    events: { publish: () => {}, subscribe: () => () => {} },
  };
}

function refusalsOf(loaded: LoadedHost): string {
  return loaded.quarantined.map((error) => `${error.pluginId}: ${error.detail}`).join("; ");
}

/** Starts a host whose only plugin requires the apps service, handing the library this case's facts. */
export async function startParityHost(staged: StagedCase, pins: readonly LibraryPin[], libraryCache: string): Promise<ParityHost> {
  const plugin = parityPlugin();
  const pluginDir = join(staged.home, "plugin");
  const loaded = await startPlugins({
    app: "parity",
    pluginDir,
    runtimeFor: () => runtimeFor(staged),
    scan: {
      loaded: [{ manifest: plugin.manifest, manifestPath: join(pluginDir, "apps-parity.json"), entryPath: join(pluginDir, "apps-parity.js") }],
      failed: [],
    },
    importEntry: async () => plugin.module,
    libraries: pins,
    libraryCache,
    libraryContext: () => ({ environment: staged.environment, home: staged.home, arguments: staged.arguments }),
  });
  if (!loaded.started.includes(plugin.manifest.id)) {
    await loaded.stop();
    throw new Error(`the parity plugin did not activate for case ${staged.name}: ${refusalsOf(loaded)}`);
  }
  return { apps: plugin.apps(), stop: () => loaded.stop() };
}
