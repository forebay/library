import { existsSync, realpathSync } from "node:fs";
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

/**
 * The case's environment as basekit reads `process.env` on this platform.
 *
 * @implNote On Windows `process.env` matches a name ignoring case, so a plain object would hide a
 * native variable spelled in another case that every real basekit finds. Of two names differing
 * only in case, which Windows cannot hold, the later answers, as JSON.parse keeps the later key.
 */
function platformEnvironment(environment) {
  if (process.platform !== "win32") return environment;
  const matching = (name) =>
    typeof name === "string" ? Object.keys(environment).findLast((key) => key.toUpperCase() === name.toUpperCase()) : undefined;
  return new Proxy(environment, {
    get: (target, name) => Reflect.get(target, matching(name) ?? name),
    has: (target, name) => Reflect.has(target, matching(name) ?? name),
    getOwnPropertyDescriptor: (target, name) => Reflect.getOwnPropertyDescriptor(target, matching(name) ?? name),
  });
}

function basekitAnswer(basekit, call, staged) {
  const home = staged.home;
  const environment = platformEnvironment(staged.environment);
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

/** The basekit source as the loader names it in a stack, through the link and as it resolves. */
function basekitSourceUrls() {
  const source = join(siblingPath("basekit"), "src");
  return [pathToFileURL(source).href, pathToFileURL(realpathSync(source)).href];
}

/**
 * @implNote Only an error whose stack runs through basekit's own source is basekit's answer: the
 * oracle's own mistakes are thrown outside it and stop the run, rather than becoming an expected
 * failure that both drivers would then be held to.
 */
function isThrownByBasekit(error) {
  const stack = error instanceof Error && typeof error.stack === "string" ? error.stack : "";
  return BASEKIT_SOURCE_URLS.some((url) => stack.includes(url));
}

function answerOf(basekit, call, staged) {
  let answer;
  try {
    answer = basekitAnswer(basekit, call, staged);
  } catch (error) {
    if (isThrownByBasekit(error)) return failure();
    throw error;
  }
  return normalised(JSON.parse(JSON.stringify(answer)), staged);
}

const BASEKIT_SOURCE_URLS = basekitSourceUrls();
resolveEmittedNamesToSource();
const basekit = await import(pathToFileURL(join(siblingPath("basekit"), "src", "apps.ts")).href);
const names = caseNames();
if (names.length === 0) throw new Error("the oracle found no cases, so it would hold the library to nothing");
for (const name of names) {
  const staged = stageCase(name, workDirectory("oracle"));
  writeExpected(name, staged.calls.map((call) => answerOf(basekit, call, staged)));
}
console.log(`basekit answered ${names.length} cases`);
