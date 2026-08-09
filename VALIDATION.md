# Validation report

Validation target: **Baconsistent v0.4.2** (`strafe.baconsistent`), Geode **5.8.2**, Geometry Dash **2.2081**, **Windows x64**.

## Sandbox checks

The source package was checked with:

- GCC C++23 standalone core tests using `-Wall -Wextra -Werror -pedantic`.
- Clang C++23 standalone core tests under AddressSanitizer + UndefinedBehaviorSanitizer.
- JSON parsing of `mod.json`.
- YAML parsing of the Windows-only GitHub Actions workflow.
- PNG decoding/verification for `logo.png` and every custom file in `resources/`.
- Cross-check that every custom `*.png` resource referenced by the C++ UI exists in `resources/`.
- Scan ensuring PlayLayer still creates no persistent gameplay HUD nodes; the only gameplay UI path added in v0.4 is the transient success notification.
- Static checks that pause StartPos progression, notifications, Practice protection and Noclip protection settings are present and enabled by default.
- Clean-package extraction followed by a second standalone core-test compile/run.

The standalone tests cover:

- legacy 2.1 and modern 2.2 percentage math;
- paired StartPos normalization;
- irregular fixed-stage boundaries;
- strict stage starts;
- long runs counting only the selected small fixed stage once per attempt;
- backwards recommendation;
- repetition persistence and malformed input;
- per-stage repetition targets, including counts above the global default;
- statistics attempts/successes/success rates/streaks/training time;
- statistics serialization;
- completed-round history and current-round reset behavior;
- remaining-run calculations used by the pause StartPos progression bar;
- Practice Mode protection semantics;
- Death-Tracker-style noclip-hit validity semantics (clean noclip runs count; suppressed lethal collisions do not);
- invalid-attempt statistics rollback, including playtime.

## Native Geode build

The preparation sandbox does not contain a Geode SDK checkout and outbound `git` access is unavailable, so a native Windows Geode compile/link/package cannot be truthfully claimed here.

The repository includes `.github/workflows/build.yml`, configured only for `windows-latest` / `Win64` with `geode-sdk/build-geode-mod` and SDK version `5.8.2` from `mod.json`. That GitHub Actions job is the final platform-specific ABI/link/package check.

## Packaging identity

The source archive is repository-rooted. `mod.json`, `.github/`, `src/`, and `resources/` are top-level ZIP entries. CI explicitly requires `mod.json.version == v0.4.2`.

## v0.4.2 noclip regression

Standalone tests cover StartPos/reset pre-frame calls, null destroy events, conservative baseline detection, native ignore-damage detection, real deaths, and level-end exclusions.
