import { existsSync, mkdirSync, readdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

export const PARITY_DIRECTORY = dirname(fileURLToPath(import.meta.url));
/** `PARITY_CASES` points a driver at another case set, which is how the suite is shown to refuse an empty one. */
export const CASES_DIRECTORY = process.env.PARITY_CASES ? resolve(process.env.PARITY_CASES) : join(PARITY_DIRECTORY, "cases");
export const PIN_FILE = join(PARITY_DIRECTORY, "pin.json");
export const EXPECTED_DIRECTORY = join(PARITY_DIRECTORY, "expected");
export const DIVERGENCES_FILE = join(PARITY_DIRECTORY, "divergences.json");
const DEFAULT_REGISTRY = "{home}/.config/forebay/apps.json";
const FAILURE = Object.freeze({ fails: true });

export function siblingPath(name) {
  return join(PARITY_DIRECTORY, "..", "siblings", name);
}

export function workDirectory(driver) {
  return join(PARITY_DIRECTORY, "work", driver);
}

export function currentPlatform() {
  return `${process.platform}-${process.arch}`;
}

export function caseNames() {
  return readdirSync(CASES_DIRECTORY, { withFileTypes: true })
    .filter((entry) => entry.isDirectory())
    .map((entry) => entry.name)
    .sort();
}

export function failure() {
  return FAILURE;
}

function definitionOf(name) {
  return JSON.parse(readFileSync(join(CASES_DIRECTORY, name, "case.json"), "utf8"));
}

/** How many calls the case files declare, counted apart from any run so a run that skips one is caught. */
export function declaredCallCount(names) {
  return names.reduce((total, name) => total + definitionOf(name).calls.length, 0);
}

export function readPin() {
  return JSON.parse(readFileSync(PIN_FILE, "utf8"));
}

function substitute(text, staged) {
  return text.split("{home}").join(staged.home).split("{root}").join(staged.root);
}

/** @implNote A registry is bytes, not text, so a BOM or an invalid sequence reaches the reader untouched; only the ASCII tokens are replaced, JSON-escaped. */
function substituteInRegistry(bytes, staged) {
  const escaped = {
    home: JSON.stringify(staged.home).slice(1, -1),
    root: JSON.stringify(staged.root).slice(1, -1),
  };
  let result = Buffer.from(bytes);
  for (const [token, value] of [["{home}", escaped.home], ["{root}", escaped.root]]) {
    const parts = [];
    let start = 0;
    let found = result.indexOf(token, start, "latin1");
    while (found !== -1) {
      parts.push(result.subarray(start, found), Buffer.from(value, "utf8"));
      start = found + token.length;
      found = result.indexOf(token, start, "latin1");
    }
    parts.push(result.subarray(start));
    result = Buffer.concat(parts);
  }
  return result;
}

function registryFilesOf(caseDirectory, definition) {
  if (definition.files) return definition.files;
  return existsSync(join(caseDirectory, "apps.json")) ? { [DEFAULT_REGISTRY]: "apps.json" } : {};
}

function substitutedValues(record, staged) {
  return Object.fromEntries(Object.entries(record).map(([name, value]) => [name, substitute(value, staged)]));
}

/**
 * Lays one case out afresh under the driver's work directory and answers the facts every
 * implementation is handed for it.
 */
export function stageCase(name, workRoot) {
  const caseDirectory = join(CASES_DIRECTORY, name);
  const definition = definitionOf(name);
  const root = resolve(workRoot, name);
  const staged = { name, root, home: join(root, "home") };
  rmSync(root, { recursive: true, force: true });
  mkdirSync(staged.home, { recursive: true });
  for (const directory of definition.homeLayout) mkdirSync(substitute(directory, staged), { recursive: true });
  for (const [target, source] of Object.entries(registryFilesOf(caseDirectory, definition))) {
    const path = substitute(target, staged);
    mkdirSync(dirname(path), { recursive: true });
    writeFileSync(path, substituteInRegistry(readFileSync(join(caseDirectory, source)), staged));
  }
  return {
    ...staged,
    environment: substitutedValues(definition.environment, staged),
    arguments: definition.arguments.map((argument) => substitute(argument, staged)),
    calls: definition.calls,
  };
}

/** An answer with the case's own directories written back as their tokens, so it reads the same wherever the case was laid out. */
export function normalised(value, staged) {
  if (typeof value === "string") return value.split(staged.home).join("{home}").split(staged.root).join("{root}");
  if (Array.isArray(value)) return value.map((element) => normalised(element, staged));
  if (value !== null && typeof value === "object") {
    return Object.fromEntries(Object.entries(value).map(([key, element]) => [key, normalised(element, staged)]));
  }
  return value;
}

export function readExpected(name) {
  return JSON.parse(readFileSync(join(EXPECTED_DIRECTORY, `${name}.json`), "utf8"));
}

export function writeExpected(name, answers) {
  mkdirSync(EXPECTED_DIRECTORY, { recursive: true });
  writeFileSync(join(EXPECTED_DIRECTORY, `${name}.json`), JSON.stringify({ platform: currentPlatform(), answers }, null, 2) + "\n");
}

export function readDivergences() {
  return JSON.parse(readFileSync(DIVERGENCES_FILE, "utf8")).divergences;
}
