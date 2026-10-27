import { isDeepStrictEqual } from "node:util";
import { join } from "node:path";
import { describe, expect, it } from "vitest";
import { caseNames, currentPlatform, readDivergences, readExpected, stageCase, workDirectory } from "../../stage.mjs";
import type { Divergence } from "../../stage.mjs";
import { observedAnswer } from "./observe.js";
import { startParityHost } from "./parity-host.js";
import { PARITY_PINS } from "./pin.js";

const IMPLEMENTATION = "typescript";
const WORK = workDirectory(IMPLEMENTATION);
const LIBRARY_CACHE = join(WORK, ".library-cache");

interface Mismatch {
  case: string;
  call: number;
  observed: unknown;
  wanted: unknown;
  reason: string;
}

function divergenceFor(divergences: readonly Divergence[], name: string, call: number): Divergence | undefined {
  return divergences.find((entry) => entry.case === name && entry.call === call);
}

/**
 * @implNote A divergence is held in both directions: the library must still answer what the table
 * records, and basekit must still answer something else, so neither side drifts into or out of it
 * unnoticed.
 */
function mismatchOf(name: string, call: number, observed: unknown, basekit: unknown, divergence: Divergence | undefined): Mismatch | undefined {
  if (!divergence) {
    return isDeepStrictEqual(observed, basekit) ? undefined : { case: name, call, observed, wanted: basekit, reason: "differs from basekit" };
  }
  if (!isDeepStrictEqual(observed, divergence.answer)) {
    return { case: name, call, observed, wanted: divergence.answer, reason: "differs from the recorded divergence" };
  }
  if (isDeepStrictEqual(basekit, divergence.answer)) {
    return { case: name, call, observed, wanted: basekit, reason: "basekit agrees, so the recorded divergence is stale" };
  }
  return undefined;
}

describe("the apps library through the TypeScript host", () => {
  it("answers every case as basekit does", async () => {
    const divergences = readDivergences().filter((entry) => entry.implementations.includes(IMPLEMENTATION));
    const names = caseNames();
    const mismatches: Mismatch[] = [];
    const held = new Set<Divergence>();
    let calls = 0;

    for (const name of names) {
      const expected = readExpected(name);
      expect(expected.platform, `${name} was answered by basekit on another platform`).toBe(currentPlatform());
      const staged = stageCase(name, WORK);
      expect(expected.answers.length, `${name} has a stale expected file`).toBe(staged.calls.length);
      const host = await startParityHost(staged, PARITY_PINS, LIBRARY_CACHE);
      try {
        staged.calls.forEach((call, index) => {
          calls += 1;
          const divergence = divergenceFor(divergences, name, index);
          if (divergence) held.add(divergence);
          const mismatch = mismatchOf(name, index, observedAnswer(host.apps, call, staged), expected.answers[index], divergence);
          if (mismatch) mismatches.push(mismatch);
        });
      } finally {
        await host.stop();
      }
    }

    console.log(`typescript: ${names.length} cases, ${calls} calls, ${held.size} recorded divergences held, ${mismatches.length} mismatches`);
    expect(divergences.filter((entry) => !held.has(entry)), "a divergence names no case and call").toEqual([]);
    expect(mismatches).toEqual([]);
  });
});
