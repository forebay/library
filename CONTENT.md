## What this is

The forebay ecosystem's C libraries. Today it holds one, **`apps`**: basekit's app detection
(`getApps`, `getApp`, `resolveHome`, `currentAppId`) as a shared library a plugin reaches through the
`apps` service, so every weivel implementation answers exactly what basekit answers.

A function reads no state of the process it runs in. The registry location, the user's home, the
environment and the arguments arrive as the call's `context`, which the host fills from the facts
each function declares in `library.json`.

| function | symbol | context | argument | answer |
| --- | --- | --- | --- | --- |
| `list` | `apps_list` | environment, home | `{}` | `{ "apps": [appDescriptor, ...] }` |
| `get` | `apps_get` | environment, home | `{ "id": string }` | `{ "app": appDescriptor }`, or `{}` for an unknown id |
| `home` | `apps_home` | environment, home | `{ "id": string }` | `{ "home": string }`, or `{}` for an unknown id |
| `current` | `apps_current` | environment, home, arguments | `{}` | `{ "id": string }`, `""` when no app owns the process |

`appDescriptor` is the resolved descriptor the model publishes at
`https://weivel.github.io/model/plugin.schema.json#/definitions/appDescriptor`.

**What it is not.** It writes nothing: basekit's `registerApp` and `setAppPaths` are not here. It
keeps no cache either, so it answers what basekit answers on a cold cache; basekit keys its cache by
the registry path and mtime alone, so a second call with a different environment, or a write within
the same mtime, can answer stale there and never here.

## Where basekit answers empty and where it throws

An absent, unreadable or malformed registry, or one that is not an object, answers `{"apps": []}` with
status 0, as basekit's `readJson` fallback does. A failure object (`{message, fix}`, status 1) is
answered only for a malformed call or where basekit **throws**:

- a registry entry that is JSON `null` (basekit reads `entry.id` on it), for every function that reads
  the registry;
- `home` for an app whose candidates hold a non-string, or whose `xdgSubdir` is a non-string while
  `XDG_CONFIG_HOME` is set;
- `current` when any app's `detect.binary` is a non-empty non-string, or when the `HUB_CONFIG_DIR`
  match reaches an app with a non-string candidate or `xdgSubdir` before an earlier app matched;
- reading an unset environment variable named after an `Object.prototype` member (`toString`,
  `__proto__` and the rest), which basekit's plain-object lookup answers with a function and then
  fails to trim.

## How the registry text is read

basekit reads the file as UTF-8, strips whole-line comments with `/^\s*\/\/[^\n]*/gm`, and hands the
rest to `JSON.parse`. Each step is mirrored, in that order. The Node and JavaScript semantics come from
`spisor/library` (`spisor/javascript.h` and `spisor_parse_json`); only the comment regex, which is
basekit's own, lives here.

- **Invalid UTF-8 becomes U+FFFD** per maximal invalid subpart, as Node decodes it
  (`spisor_decode_utf8_lossy`), rather than failing; so does an unpaired surrogate escape, through
  `spisor_parse_json`.
- **The comment regex is JavaScript's**: its `\s` also eats a BOM, a no-break space, every other
  Unicode space, and whole earlier lines of nothing but whitespace (a vertical tab or form feed
  included) before the `//`; its `^` also holds after a CR, U+2028 or U+2029, while the comment runs
  to the next LF. Anything inside a stripped comment, a NUL or invalid bytes included, is gone before
  parsing.
- **The grammar is `JSON.parse`'s**: `spisor_parse_json` checks it before cJSON reads anything,
  because cJSON is laxer (a BOM, any byte up to 0x20 as whitespace, raw control characters in a
  string, `01` and `1.`). So a registry starting with a BOM answers `[]`, exactly as basekit's does.
- **Entries come out in `Object.entries` order**: array-index keys (`0` to `4294967294`, no leading
  zero, no sign) ascending, then every other key as written. A duplicated key keeps its first
  position and its last value.

## How a descriptor is resolved

As basekit's `build`: the id is `entry.id ?? key`; an entry is kept only with a non-empty string id, a
string label, and a home whose `candidates` is a non-empty array (its elements unchecked). Every `??`
default (`detect.binary` to the id, `detect.pkg` to `""`, `commandsSubdir` to `commands`, `proxyPort`
to 0, `integration` to `env-baseurl`, `wireFormat` to `anthropic`) treats JSON `null` as absent. A
declared value of another type is copied as it is. The optional fields (`icon`, `loader`, `usage`,
`accent`, `wrapperCommand`, `npmPlugins`, `startupHook`, `discovery`, `projects`, `modelCatalog`) are
copied raw when present, `null` included, and omitted when absent. Each storage name in `paths` is the
declared one, then the `HUB_<KIND>_SUBDIR` variable, then the default, taking the first that is a
string and, trimmed, one path segment (not empty, `.` or `..`, and holding no `/` or `\`).

A home field that names a variable (`envOverride`, `nativeEnv`) is keyed as JavaScript keys any
value (`spisor_javascript_string`): a number by its shortest decimal form, `true` as `true`, an
array by its join, an object as `[object Object]`. Paths are joined with the rules of the OS the binary runs on, as Node's
`path.join` there does; trimming is `String.prototype.trim` (`spisor_javascript_trim`).

## Recorded divergences

- **Lowercasing is ASCII only**, where basekit's `toLowerCase` is Unicode. `current` compares argument
  tokens and `HUB_CONFIG_DIR` paths case-insensitively, so a non-ASCII letter whose case differs, or
  the Kelvin sign and dotted capital I that lowercase to ASCII letters, can match in basekit and not
  here.
- **An escaped NUL (`\u0000`) anywhere in the registry answers `[]`**, where `JSON.parse` reads it: no
  C string can carry one.
- **Nesting deeper than cJSON's limit of 1000 answers `[]`**, where `JSON.parse` has no limit.
- **Out of memory is status 2**, never an answer, except for an allocation failing inside cJSON while
  it parses the registry, which `spisor_parse_json` cannot tell from a refusal and so reads as `[]`.
- **A relative candidate or override is checked against the working directory** of the process, as
  `existsSync` does; that is the one ambient fact a call depends on.

## Building and testing

```
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build --output-on-failure -C Debug
```

The shared C conventions come from `spisor/library` through `FetchContent`, pinned to commit
`e048e09e743ae7d924ad5b33af48c4d3bc436644`. The tests call the exported functions through the built
shared library, one CTest executable per concern, on Windows and Linux in CI.

## Releasing

A `v*` tag runs `intisy/workflows`' `library.yml`, which builds `apps-linux-x64.so` and
`apps-win32-x64.dll`, and publishes them with `library.json` and `SHA256SUMS` as a GitHub release.

### Released

| version | commit | asset | SHA-256 |
| --- | --- | --- | --- |
| `v0.1.0` | `f870fd9934e4664b1ab61151d6a91b1f9afc23d1` | `apps-linux-x64.so` | `c9fadf1be92669abff11b81a0ac3cf1d1a620b51a785be383a4836d62f9cba16` |
| | | `apps-win32-x64.dll` | `ab82e9fcce22f5c1e032c924f5deafdbd14421ab2c60026fc7f20165f8f7c59a` |
| | | `library.json` | `8bc33a51b755b8cb1dd167279fd2a53605c5de909d0150af1982f90177dd8b86` |

A consumer pins these digests: <https://github.com/forebay/library/releases/tag/v0.1.0>.
