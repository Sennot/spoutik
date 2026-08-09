# Validation

```powershell
python -m unittest discover -s tests -p 'test_*.py' -v
python tools/verify_bindings.py
python tools/verify_upstream_bindings.py
python tools/bootstrap_deps.py --validate-only
```

Проверки запрещают возвращение второго PlayLayer, `scene->visit`, framebuffer clear/blit/readback, Spout sender/compositor и глобальных render hooks. Отдельно проверяются exact XDBot transform, bounded protocol/seqlock, viewport query, read-only mapping companion и SDL3/OpenGL API.

GitHub Actions выполняет две настоящие Windows x64 сборки:

- `.geode` через Geode SDK v5.8.2 / Release + LTO;
- `LayoutCompanion.exe` через Visual Studio 2022 и pinned SDL 3.4.10.

Финальная package-проверка требует XDBot credits и отклоняет старые `SpoutLibrary.dll`/Spout notices внутри `.geode`.

Остающаяся runtime-граница: визуальную точность конкретных уровней и поведение overlay поверх fullscreen/windowed GD нужно проверять в запущенной игре. Protocol v1 передаёт геометрию, а не sprite-atlas textures.
