# Spout Layout Dual View

Windows-only Geode mod targeting **Geometry Dash 2.2081 / Geode v5.8.2**.

## Result

- **OBS / Spout2 Capture:** the normal Geometry Dash output — decorated level, camera effects, shaders, HUD, progress/CPS overlays, pause screens, editor, main menu and other mod UI that was actually drawn into the game framebuffer.
- **Player's game window:** XDBot-style Layout Mode while a `PlayLayer` is active.
- Outside gameplay, the game window is untouched and Spout behaves like full-game capture.

## Render path

The capture is placed at the presentation boundary rather than on individual HUD nodes:

1. Geometry Dash performs its ordinary frame render once.
2. A `cocos2d::CCEGLView::swapBuffers()` **pre-hook** at `Priority::Last` runs immediately before the real backbuffer swap.
3. The current default OpenGL framebuffer is sent to Spout with `SendFbo(0, 0, 0, ...)`.
4. Only after the send, the local backbuffer is cleared and redrawn with the stripped Layout mirror plus the authoritative real HUD / scene overlays.
5. The original `swapBuffers()` presents that Layout frame to the player.

This is deliberately later than a `CCDirector::drawScene` hook, so normal post-draw work has already happened before Spout sees the frame. Geode hook ordering is cooperative: another mod can still explicitly request an even later dependency order or use raw/native hooks outside Geode, so no mod can guarantee absolute last position against hostile/custom ordering.

## Full XDBot Layout Mode integration

Pinned source:

- repository: `NakoMellia/XDBotFork`
- commit: `3737fb2e98b8a10f0c40fa4a982f03c9991ce3f4`
- files: `src/hacks/layout_mode.hpp`, `src/hacks/layout_mode.cpp`

Run:

```powershell
python tools/bootstrap_deps.py
```

The bootstrap keeps pristine upstream copies under `vendor/xdbot/upstream/` and records SHA-256 hashes in `vendor/xdbot/UPSTREAM.txt`.

The compiled adaptation changes only two integration details:

1. XDBot's umbrella include is replaced by this project's small `xdbot_compat.hpp`.
2. XDBot's global Geode hook wrapper is removed because applying it to the real `PlayLayer` would destroy the decorated world OBS needs. The actual implementation beginning at `LayoutMode::getModifiedString` is preserved verbatim, and the same `addObject` behavior is recreated only for the hidden mirror.

CI verifies the transform against the pristine downloaded files and separately compares the mirror `addObject` mutation sequence against the pinned upstream hook. The complete XDBot object sets (`excludedTriggerIDs`, `importantTriggerIDs`, `decoObjectIDs`, `solidObjectIDs`, colors, etc.) are therefore not retyped or shortened in this project.

## Why there is a second lightweight PlayLayer

XDBot's Layout Mode is a **pre-load transformation**. It changes `m_levelString` before `PlayLayer::init`, so a single PlayLayer cannot simultaneously contain both the original decorated object graph and the XDBot-stripped one.

This mod therefore keeps:

- the real decorated `PlayLayer` as the only authoritative gameplay/audio run;
- a private visual-only `PlayLayer` built from XDBot's transformed level string.

The mirror also receives its **own cloned `GJGameLevel`**, so per-level state writes from mirror resets cannot mutate the user's real level object. Its `levelComplete()`, `commitJumps()` and `updateAttempts()` paths are blocked (including during mirror construction), and mirror-side `onQuit()` is suppressed. This prevents the known hookable completion/jump-commit/attempt/scene-exit side effects from being published by the visual simulation. `incrementJumps()` itself is inline on Win64, so the protection is intentionally placed at its persistent `commitJumps()` boundary rather than pretending an inline function can be hooked.

## StartPos / practice design

The decorated real `PlayLayer` remains the authority for FMOD, timing and gameplay:

- mirror `prepareMusic` / `startMusic` are suppressed;
- mirror is kept `m_isSilent = true` and `m_audioPaused = true`;
- mirror `startGame()` is synchronized to the real layer, but it never creates a second music timeline;
- autonomous mirror `startGameDelayed()` / scheduler updates are suppressed, so the hidden layer advances exactly once per authoritative real gameplay tick;
- real `m_gameState.m_levelTime` / `m_timePlayed` are used as timing authority;
- death-animation ticks are not artificially frozen; the mirror continues through the same engine update path until the authoritative real reset arrives;
- resets and practice checkpoint creation/removal are mirrored;
- practice/test/ignore-damage runtime flags are synchronized;
- the real `m_startPosObject` pointer is **never copied** to the mirror because it belongs to another object graph. The corresponding mirror `StartPosObject` is found by level position.

That means StartPos song offset remains controlled by the real Geometry Dash audio path. The Layout mirror only follows the visual start state.

## GPU / performance rules

The sender path intentionally has no CPU frame extraction:

- no `glReadPixels`;
- no screenshots;
- no software encoder;
- no per-frame CPU pixel buffer;
- Spout uses its documented default-FBO path `SendFbo(0, 0, 0, ...)`;
- after sender initialization, the mod rejects only `GetCPU() == true`, which is Spout's actual sender sharing-method flag;
- `GetGLDX()` is logged only as legacy NVIDIA `NV_DX_interop2` compatibility information; `false` is **not** treated as CPU fallback;
- Release + LTO is enabled in GitHub Actions.

Turning **Enable dual view** off also stops the mirror tick and local redraw for the current level.

The unavoidable additional cost is the stripped Layout mirror render/simulation. The original decorated frame must still be rendered once because OBS needs it. A second output can therefore be optimized heavily, but it cannot honestly be guaranteed to cost literally zero frame time on every level or mod stack.

For an RTX 3090 setup, put **GeometryDash.exe and OBS on the same RTX 3090** in Windows Graphics Settings / NVIDIA Control Panel; cross-adapter sharing is exactly what this design avoids.

## Spout2 DLL inside `.geode`

The project pins **Spout2 2.007.017** and fetches x64 `SpoutLibrary.dll`. `mod.json` declares it under `resources.files`, so Geode packages it into the `.geode` resource archive.

Runtime loading is from `Mod::get()->getResourcesDir()` using `LoadLibraryW`; no Spout DLL needs to be placed beside `GeometryDash.exe` manually.

CI additionally opens the final `.geode` as an archive and fails the build unless it physically contains:

- `SpoutLibrary.dll`;
- `Spout2-LICENSE.txt`;
- `XDBotFork-CREDITS.txt`;
- `mod.json`.

It also re-checks that the embedded DLL is PE x86-64.

See `SOURCE-MANIFEST.md` for exactly which files are first-party source and which pinned third-party files are materialized by bootstrap/CI.

## GitHub Actions

`.github/workflows/build.yml` performs:

1. pinned XDBot + Spout bootstrap;
2. audited XDBot transform unit test;
3. offline project invariant checks;
4. live declaration checks against official Geode `bindings/main/bindings/2.2081`;
5. x64 Spout DLL validation;
6. Windows x64 Clang/Ninja build with `geode-sdk/build-geode-mod@main`, Geode `v5.8.2`, Release + LTO;
7. final `.geode` package inspection;
8. artifact upload as `SpoutLayoutDualView-Win64`.

Before publishing, replace `"developer": "YourName"` in `mod.json`. You can also change the mod ID to your own namespace.

## OBS

Add an OBS **Spout2 Capture** source and select sender **Geometry Dash Full** (or your configured sender name). If the source is upside down, toggle `Invert Spout texture` in the mod settings.

## Third-party mod compatibility

The frame sent to Spout is intentionally captured from the already-rendered default framebuffer, so UI/HUD mods generally do not need explicit support.

The Layout side is more subtle because the mirror is a real hidden `PlayLayer`. Starting with v0.1.4, mirror creation/ticking is initiated from `Priority::VeryLate` hooks. Geode preserves the current hook priority across same-thread nested calls, so ordinary third-party gameplay hooks still run normally for the real authoritative layer but are skipped for nested mirror calls. Mirror-only audio/stat/exit/XDBot guards remain at `Priority::Last`. The hidden target also has every Cocos scheduler selector removed before release and after start/reset/checkpoint changes. This greatly reduces duplicate sessions, physics patches, delayed callbacks and global side effects from mods that hook gameplay.

## Credits / licensing

- **XDBotFork / xdBot Layout Mode authors and contributors** — Layout preprocessing, object classification and trigger exclusion logic.
- **Lynn Jarvis / Spout2 contributors** — Spout2 / SpoutLibrary GPU sharing.

The inspected XDBotFork root at the pinned revision does not expose a license file in its repository listing, so this project does **not** invent or claim an MIT grant. Redistribution of the XDBot-derived material should follow the direct permission/license terms you received from its authors. Spout's license notice is bundled by the bootstrap.

## Validation boundary

This sandbox can verify the project structure, transform logic and the current public binding declarations, but it does not run Geometry Dash/OBS and does not contain a Windows + installed Geode runtime capable of launching the mod here. The authoritative binary check is therefore the included Windows GitHub Actions build. See `VALIDATION.md`.

## Geode version metadata

`mod.json` uses `"geode": "5.8.2"` (without the Git tag prefix `v`); GitHub Actions still uses `sdk: v5.8.2`.
