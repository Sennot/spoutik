# Changelog

## v0.1.8

- Replaced the per-frame scan of every classified level object with O(visible camera candidates) lookups through the complete exact XDBot object map.
- Fixed shader-heavy levels by drawing GD's live `m_inShaderParent` directly during the local Layout pass instead of replaying ShaderLayer's decorated render-texture.
- Preserved the untouched shader output in the Spout frame; the shader bypass still applies only after capture and only to the game window redraw.

## v0.1.7
- Replaced the failed `(objectID, m_startPosition)` layout mapping with an exact serialized-record plan bound during the authoritative `PlayLayer::addObject` path.
- Removed decoration is now masked from the complete classified object list instead of relying on the two transient visible-object caches.
- Added immediate white/black XDBot main/detail sprite coloring with same-frame restoration of the original decorated colors.
- Applied all six special colors from pinned XDBot `newColors`: background, both ground channels, line, and both middleground channels.
- Expanded runtime and official-binding regression gates for exact mapping, full palette coverage and visual-state restoration.

## v0.1.6
- Fixed the Win64 Clang build: renamed a local `near` identifier that Windows headers expand as a legacy compatibility macro.
- Added a regression test preventing macro-sensitive `near` identifiers from returning to the Layout object-matching path.

## v0.1.5
- Replaced the simulated hidden PlayLayer with a single-authoritative-world render-mask architecture.
- Practice/reset/checkpoint/input physics are no longer duplicated; StartPos/music/physics stay entirely in the real PlayLayer.
- Uses full pinned XDBot `getModifiedString` output to decide which live objects are shown/hidden locally, with the exact XDBot post-add styling block for retained objects.
- Local Layout rerender bypasses `ShaderLayer` shader pass while OBS receives the untouched decorated frame first.
- Vendors the exact pinned SpoutLibrary 2.007.017 interface header and forces application-local texture share mode (`SetShareMode(0)`, CPU/memory/auto share off).
- CPU mode no longer destroys the sender; it stays visible for diagnosis while logging that GPU-only requirements were not met.

- Fixed Spout sender self-disabling on valid GPU systems: `GetGLDX()` is legacy NV_DX_interop2 hardware compatibility, not the sender sharing-method flag. GPU-only enforcement now rejects only `GetCPU() == true`.
- Switched default framebuffer capture to Spout's documented `SendFbo(0, 0, 0, ...)` path and made transient send failures retry instead of disabling the session.
- Added sender diagnostics with actual Spout width/height, CPU mode rejection and legacy GL/DX compatibility status.
- Isolated the hidden Layout `PlayLayer` from ordinary third-party gameplay hooks using Geode's preserved hook-priority chain: real-layer control hooks are `Priority::VeryLate`, mirror-only guards remain `Priority::Last`.
- Replaced update-only unscheduling with `CCScheduler::unscheduleAllForTarget` and quiesce the hidden target before release and after start/reset/checkpoint operations, preventing delayed callbacks from hitting a destroyed mirror.
- Added runtime regression tests for the Spout `GetGLDX` mistake, retry behavior, hook isolation and scheduler lifetime.

# v0.1.3
- Fixed Geode metadata compatibility: `mod.json` now uses `"geode": "5.8.2"` (no leading `v`).
- Kept `sdk: v5.8.2` in GitHub Actions, because the action input is a Git tag while `mod.json` is an SDK semantic version.
- Added a regression test that mirrors Geode v5.8.2's major/minor version comparison and catches this exact mismatch.

# v0.1.2
- Fixed CMake configure failure with Geode 5.8.2: `setup_geode_mod()` uses the plain `target_link_libraries` signature, so the explicit Win32 `opengl32` link now uses the same signature.
- Added an offline invariant that rejects keyword-form `target_link_libraries(... PRIVATE/PUBLIC/INTERFACE ...)` on the mod target.

# v0.1.1
- Fixed GitHub Actions bootstrap false-positive on the pinned XDBot `layout_mode.cpp`: removed brittle minimum-byte-size checks.
- XDBot completeness is now validated structurally (all required ID tables, preprocessing path, and complete `mergeVector` tail).
- Added regression coverage proving compact complete XDBot logic is accepted while a truncated cpp is rejected.
- Documentation corrected: the official `geode-sdk/build-geode-mod` Windows action currently configures Clang/Clang++ with Ninja, not MSVC.

# v0.1.0
- Full-game Spout2 sender at the final `CCEGLView::swapBuffers` pre-presentation boundary.
- XDBot Layout Mode mirror generated before its own `PlayLayer::init` from the complete pinned Layout Mode logic/tables.
- Real decorated PlayLayer remains authoritative for music, timing and gameplay.
- StartPos mapped into the mirror's own object graph; no cross-world StartPos pointer copy.
- Mirror uses a private cloned `GJGameLevel`; `levelComplete()`, `commitJumps()`, `updateAttempts()` and mirror-side `onQuit()` are blocked to isolate known persistent stats/rewards/scene-exit side effects.
- Hidden mirror scheduler updates and delayed auto-start are suppressed; it advances only from the real gameplay tick, including normal death-animation ticks until reset.
- Practice checkpoints, input and runtime mode flags mirrored.
- Spout CPU-sharing fallback rejected; GL/DX GPU path required.
- `SpoutLibrary.dll` bundled as a Geode resource and verified inside the final `.geode` by CI.
- GitHub Actions validates current official GD 2.2081 bindings before the Windows x64 Clang/Ninja build.
