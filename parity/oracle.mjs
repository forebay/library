import { existsSync } from "node:fs";
import { registerHooks } from "node:module";
import { join } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { caseNames, failure, normalised, siblingPath, stageCase, workDirectory, writeExpected } from "./stage.mjs";

/**
 * @implNote basekit's full build needs its TeaVM and Gradle toolchain, while `apps.ts` imports only
 * Node built-ins and two leaf modules, so the oracle runs that source at the pinned commit through
 * Node's type stripping. Its imports name the `.js` its build would emit, which this hook maps
 * back to the `.ts` beside it.
 */
function resolveEmittedNamesToSource() {
  registerHooks({
    resolve(specifier, context, nextResolve) {
      if (specifier.startsWith(".") && specifier.endsWith(".js") && context.parentURL?.endsWith(".ts")) {
        const source = new URL(`${specifier.slice(0, -3)}.ts`, context.parentURL);
        if (existsSync(fileURLToPath(source))) return nextResolve(source.href, context);
      }
      return nextResolve(specifier, context);
    },
  });
}

/** basekit reads `process.argv` and `homedir()` itself for `currentAppId`, so the case's facts are put there for the call. */
function withProcessFacts(staged, work) {
  const argv = process.argv;
  const saved = { HOME: process.env.HOME, USERPROFILE: process.env.USERPROFILE };
  process.argv = [...staged.arguments];
  process.env.HOME = staged.home;
  process.env.USERPROFILE = staged.home;
  try {
    return work();
  } finally {
    process.argv = argv;
    for (const [name, value] of Object.entries(saved)) {
      if (value === undefined) delete process.env[name];
      else process.env[name] = value;
    }
  }
}

function basekitAnswer(basekit, call, staged) {
  const { environment, home } = staged;
  switch (call.function) {
    case "list":
      return { apps: basekit.getApps(environment, home) };
    case "get":
      return { app: basekit.getApp(call.argument.id, environment, home) };
    case "home": {
      const app = basekit.getApp(call.argument.id, environment, home);
      return app ? { home: basekit.resolveHome(app, environment, home) } : {};
    }
    case "current":
      return withProcessFacts(staged, () => ({ id: basekit.currentAppId(environment) }));
    default:
      throw new Error(`case ${staged.name} calls ${call.function}, which the apps service does not declare`);
  }
}

function answerOf(basekit, call, staged) {
  let answer;
  try {
    answer = basekitAnswer(basekit, call, staged);
  } catch (error) {
    if (error instanceof Error && error.message.includes("which the apps service does not declare")) throw error;
    return failure();
  }
  return normalised(JSON.parse(JSON.stringify(answer)), staged);
}

resolveEmittedNamesToSource();
const basekit = await import(pathToFileURL(join(siblingPath("basekit"), "src", "apps.ts")).href);
const names = caseNames();
for (const name of names) {
  const staged = stageCase(name, workDirectory("oracle"));
  writeExpected(name, staged.calls.map((call) => answerOf(basekit, call, staged)));
}
console.log(`basekit answered ${names.length} cases`);
