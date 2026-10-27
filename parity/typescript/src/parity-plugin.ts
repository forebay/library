import type { Plugin, PluginContext, PluginManifest } from "@weivel/typescript";

export type AppsService = Record<string, (argument: unknown) => unknown>;

export interface ParityPlugin {
  manifest: PluginManifest;
  module: { default: Plugin };
  apps(): AppsService;
}

const MANIFEST: PluginManifest = {
  id: "apps-parity",
  api: 1,
  entry: "dist/index.js",
  capabilities: [],
  services: { consumes: [{ id: "apps", optional: false }] },
};

/** A plugin that requires the apps service and holds what it was handed during activation. */
export function parityPlugin(): ParityPlugin {
  let apps: AppsService | undefined;
  const plugin: Plugin = {
    activate: (context: PluginContext) => {
      apps = context.services.get(context.service("apps")) as AppsService;
    },
    deactivate: () => {},
  };
  return {
    manifest: MANIFEST,
    module: { default: plugin },
    apps: () => {
      if (!apps) throw new Error("the parity plugin was never activated, so it holds no apps service");
      return apps;
    },
  };
}
