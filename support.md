При проблеме приложи Geode log и напиши:

- запускается ли `LayoutCompanion.exe` и что показывает его title;
- использован обычный режим или `--overlay`;
- Geometry Dash / Geode version;
- название уровня и пример пропавшего объекта;
- значения `source`, `retained`, `quads` и `dropped` из будущего debug title/log.

Crash Geometry Dash при установленном v0.4.0 особенно важен: bridge не вызывает OpenGL и не меняет scene nodes, поэтому такой crash должен расследоваться отдельно от старого FBO пути.
