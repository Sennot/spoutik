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
