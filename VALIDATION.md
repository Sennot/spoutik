# Validation notes

Target: **Geometry Dash 2.2081, Geode v5.8.2, Windows x64**.

## Checks included in the source

### 1. Offline project invariants

```powershell
python tools/verify_bindings.py
```

Checks the source shape itself, including:

- `PlayLayer` / `GJBaseGameLayer` hook signatures used by this project;
- final `CCEGLView::swapBuffers` pre-hook with `Priority::Last`;
- Spout send occurring before the local Layout redraw and before the real swap;
- documented default-FBO `SendFbo(0, 0, 0, ...)`;
- rejection of actual Spout CPU fallback without misclassifying `GetGLDX()==false`;
- private `GJGameLevel` mirror cloning plus mirror `levelComplete()`, `commitJumps()`, `updateAttempts()` and `onQuit()` suppression;
- `Priority::VeryLate` mirror entry isolation plus `Priority::Last` side-effect guards;
- all hidden-layer scheduler selectors removed before release and after start/reset/checkpoint changes;
- the master enable flag stopping both mirror ticking and local redraw;
- StartPos mapping inside the mirror object graph rather than pointer cross-copy;
- Spout ABI prefix ordering;
- Geode 5.8.2 / GD 2.2081 / Win64 CI target and resource declarations.

### 2. XDBot transform audit

```powershell
python -m unittest discover -s tests -p "test_*.py" -v
python tools/bootstrap_deps.py --validate-only
```

After bootstrap, `--validate-only` rebuilds the expected adaptation from the pristine upstream copies and requires exact equality with the compiled `vendor/xdbot/layout_mode.hpp/.cpp`. It also compares the mirror-side `addObject` mutation sequence with the pinned XDBot hook and validates that `SpoutLibrary.dll` is a non-truncated PE x86-64 image.

### 3. Live official Geode binding declarations

CI runs:

```powershell
python tools/verify_upstream_bindings.py
```

This fetches the official `geode-sdk/bindings` files for GD 2.2081 and explicitly checks every important method/member used here, including `PlayLayer::startGame/startGameDelayed`, `commitJumps`, `updateAttempts`, checkpoint methods, `GJBaseGameLayer::update/handleButton`, `GJGameLevel::create/copyLevelInfo`, StartPos/practice/audio/timing fields, and `CCEGLView::swapBuffers`.

### 4. Authoritative compiler check

GitHub Actions then builds with **Clang/Ninja / Windows x64 / Geode v5.8.2** through `geode-sdk/build-geode-mod@main` in Release mode with LTO. This is the actual generated-binding and C++ ABI compile test.

### 5. Final `.geode` package check

After compilation:

```powershell
python tools/verify_geode_package.py <build-output>
```

The workflow opens the produced `.geode` and refuses the artifact unless the x64 `SpoutLibrary.dll`, Spout license, XDBot credits and `mod.json` are physically present.

## What this sandbox cannot claim

The local environment used to prepare this source does not provide a running Windows Geometry Dash + OBS + Spout stack, so runtime GPU interop, third-party mod ordering and actual level-by-level visual synchronization cannot be proven here. Those require launching the produced Win64 `.geode` in Geometry Dash. The source and public declarations can be checked here; the included GitHub Actions workflow performs the real Windows compile/package verification.

## Geode version metadata

`mod.json` uses `"geode": "5.8.2"` (without the Git tag prefix `v`); GitHub Actions still uses `sdk: v5.8.2`.


## v0.1.5 single-world regression gates

CI rejects any return of a second `PlayLayer::create` path or mirror calls to `update`, `resetLevel`, checkpoint APIs, or `handleButton`. It also verifies that the full SpoutLibrary 2.007.017 header is bootstrapped and that texture mode is requested with `SetShareMode(0)`, `SetCPUmode(false)`, `SetMemoryShareMode(false)`, `SetAutoShare(false)` and `SetCPUshare(false)`.
