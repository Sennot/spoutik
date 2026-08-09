# Changelog

## v0.4.0

- Полностью удалён нестабильный in-process dual-render: Spout sender, FBO compositor, hooks `CCEGLView::swapBuffers`, `ShaderLayer::visit`, глобальный `CCNode::visit`, повторный `scene->visit` и все временные изменения объектов.
- Добавлен read-only Geode bridge с versioned Windows shared-memory protocol и bounded seqlock.
- Добавлен отдельный companion на SDL 3.4.10 + OpenGL compatibility context.
- Overlay автоматически следует за client area Geometry Dash, остаётся click-through и скрывается вне стабильного PlayLayer.
- OBS теперь захватывает неизменённый `GeometryDash.exe` через Game Capture; MegaHack и shader mods больше не зависят от порядка render hooks этого мода.
- XDBot-классификация, special palette и viewport-spatial index сохранены; opacity/visibility/toggle декорированного мира намеренно не скрывают Layout-структуры.
- IPC ограничен 125 Гц и 16 384 видимыми primitives, поэтому тяжёлые уровни не вызывают полный per-frame scan.

## v0.3.2

- Removed the confirmed crashing private-Layout-FBO to default-framebuffer blit (`0xc0000374` immediately inside that boundary on the target NVIDIA context).
- The stable second Layout visit now draws directly to the game backbuffer during `CCDirector::drawScene`, then captures that result as the overlay baseline. It still never runs in `swapBuffers` or during transitions.
- Removed the Layout framebuffer and depth/stencil allocation entirely; only the offscreen composed Spout framebuffer remains.

## v0.3.1

- Fixed a first-gameplay-frame heap-corruption crash detected immediately after the isolated Layout visit.
- Replaced the post-visit shader/vertex-attribute copy into the game window with a native framebuffer blit and removed the redundant post-visit projection reset.
- Added bounded stage diagnostics for the first three prepared frames so any remaining failure identifies its exact capture, visit, blit, composition or send boundary.

## v0.3.0

- Rebuilt dual-view presentation around two private FBOs and four reusable GPU textures. The Layout scene visit now happens during `CCDirector::drawScene`, never inside `CCEGLView::swapBuffers`.
- Made `swapBuffers` read/compose/send-only: it never clears or writes the window backbuffer and never revisits a scene, eliminating retained menu-card glitches, transition hallucinations and blue exit frames by construction.
- Added strict scene, `PlayLayer`, viewport and frame-generation matching. Transitions, teardown and stale prepared frames fail closed to ordinary default-framebuffer Spout capture.
- Preserved `absolllute.hackmega` on the local Layout output and the decorated Spout output using a GPU RGB-only late-overlay difference composed into a dedicated Spout FBO.
- Added direct Spout nonzero-FBO and texture sending paths while retaining default-FBO capture for menus and defensive fallback; no CPU pixel readback was introduced.

## v0.2.5

- Fixed the v0.2.4 level-entry hang: removed the aggressive depth/stencil mask and framebuffer recovery changes that ran immediately before/after the second Cocos scene visit.
- Restored Cocos' proven viewport/projection preparation used by v0.2.3 while retaining the explicit transition-scene rejection, three-draw gate and RGB-only MegaHack overlay extraction from v0.2.4.

## v0.2.4

- Replaced the insufficient PlayLayer-root transition heuristic with an explicit `CCTransitionScene` type check, a pending-next-scene check, a `willSwitchToScene` invalidation hook, and a three-draw stable-scene gate.
- Ignored framebuffer alpha-only differences when extracting MegaHack's late overlay. Presentation hooks that rewrite alpha can no longer replay the entire decorated frame over Layout as a false overlay.
- Restored the clear color, viewport, scissor rectangle/state, color mask, depth mask and stencil mask after the extra local visit, and recovers the default FBO if a visited render node leaks its framebuffer.
- Removed the redundant projection reset from the swap-time pass so transition rendering cannot inherit a modified Cocos matrix state.

## v0.2.3

- Prevented the local Layout pass from revisiting `CCTransitionScene`: entering a level no longer flashes the retained level card/menu, and leaving gameplay no longer clears the transition into a full-screen blue frame.
- Invalidated presentation-overlay textures whenever gameplay enters a transition or tears down, so no baseline/difference pixels can leak across scene lifetimes.
- Preserved and restored the OpenGL position/texture-coordinate attribute bindings around the MegaHack overlay replay, preventing corrupted geometry on following frames.
- Refused to clear or redraw any non-default framebuffer, protecting render textures owned by Geometry Dash and other mods.

## v0.2.2

- Fixed HackMega's global interface being erased inside levels. A GPU-only baseline/difference replay preserves presentation pixels drawn by HackMega and other late overlays over the local Layout pass while Spout still receives the complete original framebuffer.
- Replaced late-populating Geometry Dash camera grids as the primary batch-mask source with a stable X-sorted spatial index queried through the authoritative object-layer camera transform. Removed decoration is now suppressed from the first local frame instead of leaking in as GD's transient visibility arrays fill.
- Kept the compact engine vectors for moving/effect objects and the section grids only as a defensive fallback, preserving performance independently of total level object count.
- Added first-frame and settled-frame diagnostics containing compact/spatial/fallback candidate counts and exact world viewport bounds.

## v0.2.1

- Restored structures on fully invisible levels by keeping XDBot's own visible/opaque post-add state separately from the decorated object's camera-culling state, then applying it only to current camera candidates.
- Replaced the incomplete batch `setVisible` active set with an adaptive candidate path: GD's compact current-frame vectors, the ordinary non-effect camera grid, and the general section grid only as a low-candidate fallback. The pass remains independent of total level object count.
- Prevented permanently pooled particle pointers from inheriting a removed owner's node decision, and made shared render-node collisions prefer retained main/detail visuals over suppression.
- Fixed black local frames during shaders by visiting `m_inShaderObjectLayer` directly with a recursion guard instead of revisiting the shader parent/cache.
- Added scoped opacity restoration and expanded runtime/official-binding regression checks for invisible structures, adaptive batching and the raw shader layer.

## v0.2.0

- Replaced the camera-section pre-pass with a hybrid render mask: direct `CCNode::visit` decisions for ordinary nodes plus a visibility-tracked active set for sprites drawn through Cocos batch nodes. Reparented detail sprites, glow and particles no longer depend on object origins or section margins.
- Removed per-frame visibility, opacity, active-channel, HSV and glow setter churn. Trigger-controlled visibility/opacity remain authoritative; retained sprites only receive a scoped direct color change around their own draw, while removed nodes skip drawing entirely.
- Added an explicit Geode relative priority after `absolllute.hackmega` so the Spout framebuffer includes HackMega's presentation overlay before the local Layout redraw.
- Added render hot-path diagnostics reporting visited, mapped, styled and skipped nodes.

## v0.1.9

- Fixed the v0.1.8 regression where only the Layout background/ground changed: GD's transient visible-object vectors omit ordinary rendered decoration on some levels.
- The local pass now walks only the active rectangle of GD's full `m_sections` and `m_nonEffectObjects` grids, retaining complete decoration colors/masking without returning to a 100k+ per-frame scan.
- Map diagnostics now distinguish missing retained records from removed decoration that GD intentionally did not instantiate.

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
