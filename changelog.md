# v0.1.4
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
