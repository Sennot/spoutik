# Source manifest

This archive is the complete first-party source for **Spout Layout Dual View** plus reproducible dependency bootstrap/validation scripts.

## Included directly

- `src/` — Geode hooks, dual-view render path, exact serialized Layout mapping and Spout sender.
- `include/` — mirror/sender declarations and the verified ABI prefix used to dynamically call `SpoutLibrary.dll`.
- `vendor/xdbot/xdbot_compat.hpp` — compatibility include for the pinned XDBot Layout Mode source.
- `tools/bootstrap_deps.py` — downloads exact pinned XDBot Layout Mode files and Spout2 binary, then validates them.
- `tools/deps.json` — dependency versions/commit.
- `tests/` and verifier scripts — transform, binding and final-package checks.
- `.github/workflows/build.yml` — authoritative Windows x64 GitHub Actions build.
- `BUILD-RU.md` / `build.ps1` — Russian quick-start and one-command local Windows build helper.

## Materialized during bootstrap / CI

The following files are intentionally fetched from their canonical upstream locations by `python tools/bootstrap_deps.py`:

- `vendor/xdbot/upstream/layout_mode.hpp`
- `vendor/xdbot/upstream/layout_mode.cpp`
- `vendor/xdbot/layout_mode.hpp`
- `vendor/xdbot/layout_mode.cpp`
- `vendor/xdbot/UPSTREAM.txt`
- `resources/spout/SpoutLibrary.dll`
- `resources/licenses/Spout2-LICENSE.txt`

The XDBot adaptation is audited: the actual `LayoutMode::getModifiedString` implementation suffix is kept verbatim, while only the project-specific umbrella include and XDBot's global hook wrapper are replaced. `src/main.cpp` observes the authoritative `addObject` path and `src/LayoutMirror.cpp` applies the pinned visual behavior only during the local render pass.

This design keeps the repository reproducible without pretending that third-party files were locally built or validated when the preparation sandbox could not download/run the Windows runtime stack. GitHub Actions fetches them, validates the exact source transform and x64 PE architecture, compiles with the official Windows Clang/Ninja Geode action against Geode v5.8.2, then verifies they are physically embedded in the produced `.geode`.


## v0.1.5 generated dependency

`vendor/spout/SpoutLibrary.h` is downloaded from the exact Spout2 tag in `tools/deps.json` by `tools/bootstrap_deps.py`. This replaces the old hand-written ABI prefix and is required so the application can explicitly control Spout sharing mode without static linking.
