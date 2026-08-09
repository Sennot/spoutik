# Layout Companion for Geometry Dash

Безопасная замена старой Spout/FBO-архитектуры для **Geometry Dash 2.2081 / Geode 5.8.2 / Windows x64**.

## Как теперь устроен вывод

```text
GeometryDash.exe
  ├─ обычный декорированный кадр → монитор / OBS Game Capture
  └─ read-only данные камеры и объектов → shared memory
                                             ↓
                                  LayoutCompanion.exe
                                  SDL3 + OpenGL Layout
```

Geode-мод больше не перерисовывает сцену, не меняет объекты и не вмешивается в OpenGL Geometry Dash. Поэтому shader/FBO-код других модов, переходы между сценами и интерфейс MegaHack остаются в обычном игровом окне.

Companion получает ограниченный список экранных четырёхугольников через Windows shared memory и рисует Layout в собственном OpenGL-контексте. Передача ограничена 125 Гц и выбирает объекты только из текущего viewport, компактных списков GD и резервных camera sections — полного прохода тяжёлого уровня каждый кадр нет.

## Первый запуск

Из GitHub Actions скачай два artifact:

- `LayoutCompanionBridge-Win64` — установи `.geode` обычным способом;
- `LayoutCompanion-SDL3-Win64` — держи `LayoutCompanion.exe` и `SDL3.dll` рядом.

Обычное отдельное окно:

```powershell
.\LayoutCompanion.exe
```

Layout поверх окна Geometry Dash:

```powershell
.\LayoutCompanion.exe --overlay
```

В overlay-режиме companion автоматически находит окно Geometry Dash по PID, повторяет размер его client area, остаётся поверх него и пропускает мышь/фокус в игру. Вне уровня overlay полностью скрывается. Закрыть его можно через консоль или завершив `LayoutCompanion.exe`.

Дополнительный режим отдельного окна поверх остальных:

```powershell
.\LayoutCompanion.exe --always-on-top
```

## OBS и MegaHack

В OBS используй **Game Capture / Захват игры** и выбери `GeometryDash.exe`. OBS получит настоящий декорированный кадр Geometry Dash с shaders, HUD и модовыми overlay. Отдельный companion в этот source не входит.

MegaHack продолжает открываться и принимать ввод в настоящем окне Geometry Dash. Click-through overlay только визуально закрывает его Layout-изображением; он не перехватывает управление.

## Что реализовано в protocol v1

- точная классификация retained/deco из pinned XDBotFork `getModifiedString`;
- белый/чёрный main/detail стиль retained-объектов;
- XDBot background/ground/line palette;
- screen-space transform с camera rotation/zoom через реальные Cocos node transforms;
- игроки отдельными заметными primitives;
- структуры экспортируются независимо от `visible`, opacity и toggle-состояния декорированного мира;
- lock-free bounded seqlock, максимум 16 384 quads, явный счётчик truncation;
- никаких texture readback, скриншотов или CPU-копий framebuffer.

Protocol v1 — геометрический structural renderer: он уже даёт стабильный играбельный Layout, но пока не переносит оригинальные sprite-atlas UV/текстуры. Это намеренная следующая стадия, не повод снова внедрять рендер в Geometry Dash.

## Сборка

Geode bridge:

```powershell
$env:GEODE_SDK = 'C:\path\to\geode'
python tools/bootstrap_deps.py
cmake -S . -B build -A x64
cmake --build build --config Release --parallel
```

Companion (SDL 3.4.10 скачивается FetchContent):

```powershell
cmake -S companion -B companion/build -A x64
cmake --build companion/build --config Release --parallel
```

Готовые `LayoutCompanion.exe` и `SDL3.dll` будут в `companion/build/bin`.

## XDBot credit

Layout preprocessing и полные classification tables взяты из `NakoMellia/XDBotFork`, commit `3737fb2e98b8a10f0c40fa4a982f03c9991ce3f4`. Преобразование исходников проверяется тестами; credits включены в `.geode`.
