# Сборка Layout Companion

Цель: Geometry Dash 2.2081, Geode 5.8.2, Windows x64.

Самый простой вариант — открыть GitHub Actions workflow `Build Windows Geode mod`. Он публикует два artifact:

1. `LayoutCompanionBridge-Win64` с `.geode`;
2. `LayoutCompanion-SDL3-Win64` с `LayoutCompanion.exe` и `SDL3.dll`.

Локально нужны Visual Studio 2022 C++ workload, CMake, Git, Python 3 и Geode SDK.

```powershell
$env:GEODE_SDK = 'C:\path\to\geode'
.\build.ps1
```

Скрипт проверяет XDBot transform и bridge, собирает `.geode`, затем отдельно собирает SDL3/OpenGL companion. SDL 3.4.10 загружается из официального репозитория SDL через CMake FetchContent.

Запуск поверх GD:

```powershell
.\companion\build\bin\LayoutCompanion.exe --overlay
```

В OBS захватывай `GeometryDash.exe` через Game Capture, а не companion.
