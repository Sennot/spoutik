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
2. XDBot's global Geode hook wrapper is removed because applying it to the real `PlayLayer` would destroy the decorated world OBS needs. The actual implementation beginning at `LayoutMode::getModifiedString` is preserved verbatim; its output is converted into render metadata for the one authoritative `PlayLayer`.

CI verifies the transform against the pristine downloaded files and separately compares the local render mutation sequence against the pinned upstream `addObject` hook. The complete XDBot object sets (`excludedTriggerIDs`, `importantTriggerIDs`, `decoObjectIDs`, `solidObjectIDs`, colors, etc.) are therefore not retyped or shortened in this project.

## Exact single-PlayLayer mapping

There is no hidden or secondary `PlayLayer`. Before the real `PlayLayer::init`, the mod runs the pinned XDBot transformation and compares its stable output subsequence with the decompressed original serialized records. Property `135` is normalized because it is the only per-object property XDBot adds or removes.

While the real decorated layer performs its ordinary `addObject` calls, each live `GameObject*` is bound to that precomputed record classification by object-ID occurrence order. This avoids the invalid runtime-position comparison used before v0.1.7 and preserves XDBot's exact decisions for removed deco, important groups, solids and hidden objects.

During the local pass:

- the complete exact map is compiled once into direct main/detail/glow node decisions; pooled particles are resolved only from their current camera owner;
- ordinary nodes are masked inside the actual `CCNode::visit` traversal; batched sprites are handled from GD's current-frame vectors and camera-local spatial candidates because Cocos does not individually visit them;
- removed nodes skip drawing, retained sprites receive scoped white/black color and XDBot's logical visible/opaque baseline, and the full classified level is never scanned per frame;
- live gameplay toggle/disable flags and opacity updates are separated from decorated-world camera culling, so originally hidden/invisible structures can appear without changing the authoritative scene;
- BG, G1, G2, LINE, MG1 and MG2 receive every special color from pinned XDBot `newColors`;
- objects inside GD's shader z-range are visited directly from raw `m_inShaderObjectLayer`, so the decorated/black shader render-texture cannot cover the local Layout pass;
- all touched fields, colors, opacity and visibility are restored in the same frame after the local redraw.

## StartPos / practice design

The decorated real `PlayLayer` is the only owner of physics, player state, inputs, StartPos, practice checkpoints, attempts, triggers, camera, FMOD and timing. The local Layout pass only changes render-visible fields around a second visit of that same scene. It never calls gameplay update/reset/checkpoint/audio functions.

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

The unavoidable additional cost is one extra local scene visit plus indexed overrides for objects in the active camera sections. There is no second simulation and no per-frame scan of the full serialized level. The original decorated frame must still be rendered once because OBS needs it, so the local Layout output cannot honestly cost literally zero frame time on every level or mod stack.

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

Starting with v0.1.5 there is **no hidden gameplay PlayLayer at all**. The real decorated PlayLayer is the only physics, practice, checkpoint, StartPos, camera and music authority. Since v0.1.7, the full pinned XDBot output is aligned to original serialized records before init and bound directly during the real layer's `addObject` calls. v0.2.1 applies those decisions at the actual Cocos node visit boundary plus an adaptive camera-local path for batched and originally invisible sprites, without scanning the full level. The shader z-range is drawn from its raw object layer instead of replaying the decorated shader texture. The Spout swap hook is explicitly ordered after `absolllute.hackmega`, so its overlay is present in the untouched framebuffer sent to OBS before the local Layout redraw.

## Credits / licensing

- **XDBotFork / xdBot Layout Mode authors and contributors** — Layout preprocessing, object classification and trigger exclusion logic.
- **Lynn Jarvis / Spout2 contributors** — Spout2 / SpoutLibrary GPU sharing.

The inspected XDBotFork root at the pinned revision does not expose a license file in its repository listing, so this project does **not** invent or claim an MIT grant. Redistribution of the XDBot-derived material should follow the direct permission/license terms you received from its authors. Spout's license notice is bundled by the bootstrap.

## Validation boundary

This sandbox can verify the project structure, transform logic and the current public binding declarations, but it does not run Geometry Dash/OBS and does not contain a Windows + installed Geode runtime capable of launching the mod here. The authoritative binary check is therefore the included Windows GitHub Actions build. See `VALIDATION.md`.

## Geode version metadata

`mod.json` uses `"geode": "5.8.2"` (without the Git tag prefix `v`); GitHub Actions still uses `sdk: v5.8.2`.
