# Source manifest

- `src/main.cpp` — минимальные PlayLayer/director hooks.
- `src/CompanionBridge.cpp` — Windows shared-memory writer; OpenGL здесь отсутствует.
- `src/LayoutMirror.cpp` — exact XDBot record classification и viewport geometry export.
- `include/CompanionProtocol.hpp` — общий POD protocol v1.
- `companion/` — отдельное SDL3/OpenGL Windows приложение.
- `vendor/xdbot/` — pinned/audited XDBot adaptation, создаваемая `tools/bootstrap_deps.py`.
- `tests/`, `tools/` — regression, binding и package checks.

`FrameCompositor`, `SpoutSender` и Spout binary/header больше не являются частью проекта.
