# Сборка Spout Layout Dual View

Цель проекта: **Geometry Dash 2.2081 / Windows x64 / Geode v5.8.2**.

## Самый простой способ — GitHub Actions

1. Создай пустой GitHub-репозиторий.
2. Распакуй этот архив в корень репозитория, включая папку `.github`.
3. В `mod.json` замени `"developer": "YourName"` на свой ник/имя разработчика. При желании замени и `id` на свой уникальный namespace.
4. Сделай commit + push.
5. Открой **Actions → Build Windows Geode mod**.
6. После успешной сборки скачай artifact **SpoutLayoutDualView-Win64**. Внутри будет готовый `.geode`.

Workflow сам скачивает pinned XDBot Layout Mode и Spout2 2.007.017, проверяет XDBot-transform, официальные Geode bindings, x64-архитектуру DLL, собирает Release/LTO и затем открывает готовый `.geode` как ZIP, чтобы доказать наличие `SpoutLibrary.dll` и notices внутри пакета.

## Локальная сборка Windows

Нужны Python 3, CMake, Visual Studio 2022 C++ toolchain и Geode SDK.

```powershell
$env:GEODE_SDK = "C:\path\to\geode"
.\build.ps1
```

Или вручную:

```powershell
python tools/bootstrap_deps.py
python -m unittest discover -s tests -p "test_*.py" -v
python tools/verify_bindings.py
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

`bootstrap_deps.py` скачивает только exact pinned third-party dependencies. Если локальная сеть блокирует GitHub, просто используй GitHub Actions — workflow делает тот же bootstrap на runner-е.

## OBS

1. Установи OBS Spout2 Capture plugin / source, если его ещё нет.
2. Запусти Geometry Dash с модом.
3. В OBS добавь **Spout2 Capture**.
4. Выбери sender **Geometry Dash Full** (или имя из настройки мода).
5. Если изображение перевёрнуто, переключи **Invert Spout texture** в настройках мода.

OBS получает обычный уже отрисованный framebuffer игры до локальной подмены на Layout: декор, camera/shader effects, HUD, CPS/progress, pause/menu/editor и mod overlays, которые реально попали в game framebuffer.

## Geode version metadata

`mod.json` uses `"geode": "5.8.2"` (without the Git tag prefix `v`); GitHub Actions still uses `sdk: v5.8.2`.
