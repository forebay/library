export interface ParityCall {
  function: string;
  argument: Record<string, unknown>;
}

export interface StagedCase {
  name: string;
  root: string;
  home: string;
  environment: Record<string, string>;
  arguments: string[];
  calls: ParityCall[];
}

export interface ExpectedAnswers {
  platform: string;
  answers: unknown[];
}

export interface Divergence {
  case: string;
  call: number;
  implementations: string[];
  answer: unknown;
  because: string;
}

export const PARITY_DIRECTORY: string;
export const CASES_DIRECTORY: string;
export const EXPECTED_DIRECTORY: string;
export const DIVERGENCES_FILE: string;
export const PIN_FILE: string;
export function declaredCallCount(names: readonly string[]): number;
export function readPin(): { repository: string; tag: string; sha256: Record<string, string> };
export function siblingPath(name: string): string;
export function workDirectory(driver: string): string;
export function currentPlatform(): string;
export function caseNames(): string[];
export function failure(): { fails: true };
export function stageCase(name: string, workRoot: string): StagedCase;
export function normalised(value: unknown, staged: StagedCase): unknown;
export function readExpected(name: string): ExpectedAnswers;
export function writeExpected(name: string, answers: unknown[]): void;
export function readDivergences(): Divergence[];
