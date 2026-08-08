# Spout Layout Dual View

OBS receives the complete normal Geometry Dash framebuffer through **Spout2** at the final `CCEGLView::swapBuffers` presentation boundary. During gameplay, the player's window is then redrawn with a lightweight **XDBot Layout Mode mirror** while the real decorated PlayLayer remains authoritative for gameplay, audio and StartPos timing.

## Credits

- **XDBotFork / xdBot Layout Mode authors and contributors** — complete Layout Mode preprocessing, object classification and trigger-exclusion logic. Upstream is pinned in `tools/deps.json` and audited by `tools/bootstrap_deps.py`.
- **Lynn Jarvis / Spout2 contributors** — Spout2 / SpoutLibrary GPU texture sharing.

The XDBot-derived material is included on the basis of the permission stated by the project integrator. Preserve the upstream license/permission notices supplied by the XDBot authors when redistributing.
