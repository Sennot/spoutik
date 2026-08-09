# Spout Layout Dual View — Project Context

## 1. Что это за проект

Нужно разработать Geode-мод для **Geometry Dash 2.2081 на Windows x64**, который одновременно показывает два разных изображения одной игровой сессии:

### На экране игрока Geometry Dash

Игрок должен видеть **Layout Mode в стиле XDBot**:

* без декораций;
* с сохранением gameplay objects;
* с layout-цветами / layout-визуалом XDBot;
* физика должна полностью соответствовать настоящему уровню.

### В OBS через Spout2

OBS должен видеть **полный оригинальный framebuffer Geometry Dash**:

* оригинальные декорации;
* shaders;
* particles;
* camera effects;
* HUD;
* progress;
* CPS;
* Geode/MegaHack overlays;
* pause menu;
* editor/menu UI;
* другие UI/mod overlays.

То есть желаемая схема:

```text
Geometry Dash window:
LAYOUT MODE

OBS / Spout2 Capture:
FULL ORIGINAL DECORATED GAME
```

Передача должна быть максимально GPU-only.

Компьютер пользователя имеет **RTX 3090**.

---

# 2. Основные технические требования

Цели:

* Geometry Dash: `2.2081`
* Geode SDK: `v5.8.2`
* Windows x64
* compiler в официальном GitHub Action Geode: Clang + Ninja
* Spout2 SDK: `2.007.017`
* XDBot Layout Mode должен основываться на оригинальной upstream-логике.

Нельзя использовать:

```text
glReadPixels
CPU screenshot capture
software encoding
постоянный framebuffer → CPU → GPU roundtrip
```

Главный принцип:

```text
original game framebuffer
        ↓
Spout2 Capture
        ↓
OBS

после capture:
локальное окно GD превращается в Layout
```

---

# 3. XDBot upstream

Используется pinned XDBotFork:

```text
Repository:
https://github.com/NakoMellia/XDBotFork

Commit:
3737fb2e98b8a10f0c40fa4a982f03c9991ce3f4
```

Основные файлы:

```text
src/hacks/layout_mode.hpp
src/hacks/layout_mode.cpp
```

В частности обязательно сохраняется upstream:

```cpp
LayoutMode::getModifiedString(...)
LayoutMode::mergeVector(...)
```

и таблицы:

```text
newColors
robtopLevelIDs
excludedTriggerIDs
importantTriggerIDs
decoObjectIDs
solidObjectIDs
```

Логика `getModifiedString` не должна переписываться вручную.

Также сохранено XDBot-поведение styling gameplay objects из `PlayLayer::addObject`.

Credits XDBot должны оставаться в проекте.

---

# 4. Spout2

Pinned Spout:

```text
Version:
2.007.017

Release asset:
Spout-SDK-binaries_2-007-017_1.zip
```

Spout DLL должен автоматически скачиваться bootstrap script и встраиваться в `.geode`.

Sender name:

```text
Geometry Dash Full
```

---

# 5. История архитектуры — ВАЖНО

## v0.1.0 — v0.1.4

Первоначально использовался **второй hidden PlayLayer**.

Архитектура была:

```text
REAL PlayLayer
    ↓
полный decorated render
    ↓
Spout
    ↓
clear framebuffer
    ↓
hidden Layout PlayLayer render
    ↓
swapBuffers
```

Hidden PlayLayer создавался из:

```cpp
LayoutMode::getModifiedString(realLevelString)
```

То есть XDBot level preprocessing выполнялся до:

```cpp
PlayLayer::init
```

Идея была логичной, но на настоящей heavily-modded установке Geometry Dash оказалась нестабильной.

---

# 6. Почему второй PlayLayer пришлось удалить

Пользователь имеет большое количество gameplay-модов.

В логах присутствуют, среди прочих:

```text
MegaHack
HackMega
Click Between Frames
Eclipse
ZCB Live
xdBot
Death Tracker
Globed
Misc Bugfixes
и другие
```

Многие из них hook'ают:

```text
PlayLayer::init
PlayLayer::resetLevel
PlayLayer::destroyPlayer
PlayLayer::levelComplete
PlayLayer::postUpdate

GJBaseGameLayer::update
GJBaseGameLayer::handleButton
GJBaseGameLayer::processCommands
GJBaseGameLayer::getModifiedDelta

PlayerObject::update
collision functions

checkpoint functions
```

Поэтому hidden PlayLayer воспринимался другими модами как **вторая настоящая gameplay session**.

---

# 7. Подтверждение проблемы из runtime logs

После создания hidden Layout PlayLayer появлялись сообщения:

```text
Layout mirror created: 2399 source objects -> 1406 mirror objects
```

После этого Death Tracker писал:

```text
Metadata Corrupted
Failed to get session
```

Также регулярно:

```text
Speedhack Detected!
Noclip Detected!
```

Это происходило непосредственно после создания / запуска hidden mirror.

Practice Mode был особенно проблемным.

Пользователь сообщил:

```text
игра крашит при restart attempt,
например в Practice Mode
```

В v0.1.4 были добавлены:

```cpp
CCScheduler::unscheduleAllForTarget(mirror)
```

и попытка изоляции hidden PlayLayer через Geode hook priorities.

Это не решило проблему полностью.

### Важный вывод

**Не возвращаться к архитектуре с двумя PlayLayer.**

Это принципиальное решение.

---

# 8. Текущая архитектура v0.1.5

Начиная с **v0.1.5 второго PlayLayer больше нет вообще**.

Должна существовать только одна реальная gameplay simulation:

```text
REAL PlayLayer
```

Она единственная отвечает за:

```text
physics
player
inputs
practice mode
checkpoints
StartPos
music
attempts
camera
triggers
completion
statistics
```

Никакой отдельной физики Layout больше нет.

---

# 9. Новый Layout render

Текущая идея:

```text
REAL PlayLayer
       │
       │ normal render
       ▼
FULL DECORATED FRAMEBUFFER
       │
       ├── Spout2 → OBS
       │
       ▼
temporarily apply Layout representation
       │
       ▼
render same REAL PlayLayer locally
       │
       ▼
restore original object state
```

То есть Layout является теперь **render-time representation того же PlayLayer**, а не второй gameplay-world.

---

# 10. Принцип Layout mapping

При входе в уровень:

1. Берётся настоящий:

```cpp
level->m_levelString
```

2. Выполняется:

```cpp
LayoutMode::getModifiedString(originalLevelString)
```

3. Получившийся XDBot Layout level string используется, чтобы определить:

```text
какие objects должны остаться в Layout
какие являются deco
какой styling должен применяться
```

4. Строится Layout map / mask для объектов настоящего PlayLayer.

Во время локального Layout-render:

```text
deco objects скрываются
gameplay objects получают XDBot layout styling
```

После render состояние настоящих объектов должно немедленно восстанавливаться.

---

# 11. Чего НЕТ в v0.1.5

В коде больше НЕ должно существовать:

```cpp
PlayLayer::create(...) // для hidden mirror
```

Не должно быть:

```cpp
mirror->resetLevel()
mirror->markCheckpoint()
mirror->removeCheckpoint()
mirror->removeAllCheckpoints()
mirror->handleButton()
mirror->update()
mirror->startGame()
```

Не должно быть второй:

```text
physics simulation
practice session
checkpoint list
audio timeline
StartPos pointer
attempt counter
```

Это проверяется regression tests.

---

# 12. Почему новая схема должна исправить Practice crash

Раньше при practice restart происходило примерно:

```text
REAL PlayLayer checkpoint/reset
+
MIRROR PlayLayer checkpoint/reset
+
MegaHack hooks
+
CBF hooks
+
Eclipse hooks
+
ZCB hooks
+
другие gameplay hooks
```

Из-за этого возникало несколько независимых состояний.

Теперь:

```text
Practice reset
    ↓
только REAL PlayLayer
    ↓
Geometry Dash + обычные пользовательские mods
```

Наш Layout render вообще не должен участвовать в:

```text
resetLevel
checkpoint state
physics
input handling
```

---

# 13. Spout — история проблемы

## v0.1.3

Spout sender создавался, но затем мод проверял:

```cpp
GetGLDX()
```

и ошибочно считал:

```text
GetGLDX() == false
```

признаком отсутствия GPU sharing.

Из-за этого sender отключался.

Это было неверно.

---

# 14. v0.1.4 Spout problem

В v0.1.4 проверка `GetGLDX` была исправлена.

Но runtime log показал:

```text
Spout sender initialized: Geometry Dash Full

Spout selected CPU sharing.
Sender disabled for this session;
GPU-only sharing is required by this mod.
```

То есть Spout действительно выбрал CPU sharing.

Из-за нашего запрета CPU fallback sender уничтожался.

Поэтому OBS вообще не показывал:

```text
Geometry Dash Full
```

---

# 15. Spout architecture в v0.1.5

Начиная с v0.1.5 используется более полный официальный SpoutLibrary API.

Перед sender creation код пытается принудительно выбрать texture/GPU sharing:

```cpp
SetMemoryShareMode(false);
SetCPUmode(false);
SetShareMode(0);
SetAutoShare(false);
SetCPUshare(false);
```

Где:

```text
ShareMode 0 = texture sharing
```

После этого создаётся sender.

Критически важное изменение:

### Sender больше нельзя уничтожать только потому, что Spout остался в CPU mode.

То есть даже если:

```cpp
GetCPU() == true
```

мод должен:

```text
LOG WARNING
KEEP SENDER ALIVE
```

чтобы пользователь хотя бы увидел:

```text
Geometry Dash Full
```

в OBS и можно было дальше диагностировать sharing mode.

---

# 16. Ожидаемый Spout log v0.1.5

Желательно получить что-то похожее:

```text
Spout requested GPU texture mode:
shareMode=0
CPUmode=0
memoryMode=0
autoShare=0
```

После sender creation:

```text
Spout GPU sender active: Geometry Dash Full
```

Если всё равно CPU:

```text
WARNING:
Spout remained in CPU sharing mode
```

Но sender должен продолжить существовать.

---

# 17. Default framebuffer capture

Spout получает original frame **до Layout local redraw**.

Используется:

```cpp
SendFbo(...)
```

для default OpenGL framebuffer.

Не использовать:

```cpp
glReadPixels
```

Capture hook находится около:

```cpp
cocos2d::CCEGLView::swapBuffers()
```

Порядок должен быть:

```text
1. Geometry Dash / mods полностью отрендерили кадр

2. Spout отправляет original framebuffer

3. локально применяется Layout mask

4. Layout render

5. object state восстанавливается

6. настоящий swapBuffers
```

---

# 18. Важная проблема hook ordering

У пользователя есть другие моды, hook'ающие:

```cpp
CCEGLView::swapBuffers
```

Например:

```text
Eclipse
ZCB Live
```

Поэтому hook ordering важен.

Spout capture должен происходить как можно позже после normal render, но ДО destructive Layout redraw.

Local Layout redraw должен быть непосредственно перед actual window presentation.

Если появятся проблемы:

```text
OBS получает Layout вместо original
или
GD window получает original вместо Layout
```

сначала проверять priority/order swapBuffers hooks.

---

# 19. XDBot самого пользователя

У пользователя установлен настоящий:

```text
zilko.xdbot v2.7.6
```

Это нормально.

Но при тесте нашего мода рекомендуется:

```text
XDBot Layout Mode = OFF
```

Иначе сам XDBot изменит настоящий PlayLayer, который нужен нашему моду как decorated source для OBS.

Сам xdBot можно оставить включённым.

---

# 20. Другой Spout mod

В системе также установлен:

```text
local.spout-capture v1.0.0
```

Но в runtime log он был:

```text
not enabled
```

Поэтому не считать его причиной текущего конфликта без новых доказательств.

---

# 21. Current project version

Текущая версия проекта:

```text
v0.1.5
```

Полный project archive:

```text
Spout-Layout-Dual-View-v0.1.5-FULL-PROJECT.zip
```

SHA-256:

```text
f77a40a2d947b2340449850b42f85278574e2f6b77cbbcb52cdc6736f3c218a1
```

---

# 22. Структура проекта

Ожидаемая структура:

```text
gd-spout-layout/
├── .github/
│   └── workflows/
│       └── build.yml
│
├── include/
│
├── src/
│
├── tests/
│
├── tools/
│   ├── bootstrap_deps.py
│   ├── verify_bindings.py
│   ├── verify_upstream_bindings.py
│   ├── verify_geode_package.py
│   └── deps.json
│
├── vendor/
│   └── xdbot/
│
├── resources/
│   ├── licenses/
│   └── spout/
│
├── CMakeLists.txt
├── mod.json
├── README.md
├── BUILD-RU.md
├── VALIDATION.md
├── SOURCE-MANIFEST.md
├── changelog.md
├── about.md
└── support.md
```

---

# 23. Dependency bootstrap

`tools/bootstrap_deps.py` скачивает pinned:

```text
XDBot Layout Mode
Spout2 DLL/header
```

Ранее CI падал с:

```text
XDBot layout_mode.cpp unexpectedly small
```

Причиной был неверный minimum byte-size guard.

Это уже исправлено.

Теперь completeness XDBot проверяется структурно.

Проверяется наличие:

```text
newColors
robtopLevelIDs
excludedTriggerIDs
importantTriggerIDs
decoObjectIDs
solidObjectIDs

LayoutMode::getModifiedString
LayoutMode::mergeVector
```

и целостность хвоста implementation.

Нельзя возвращать arbitrary minimum-size checks.

---

# 24. Старые CI ошибки, которые уже исправлены

## CMake target_link_libraries

Было:

```cmake
target_link_libraries(${PROJECT_NAME} PRIVATE opengl32)
```

Geode `setup_geode_mod()` использовал plain signature.

CMake падал:

```text
The keyword signature for target_link_libraries
has already been used...
```

Исправлено на:

```cmake
target_link_libraries(${PROJECT_NAME} opengl32)
```

Не возвращать `PRIVATE`.

---

## mod.json Geode version

Было:

```json
"geode": "v5.8.2"
```

Но Geode SDK ожидает:

```json
"geode": "5.8.2"
```

При этом GitHub Actions должен оставаться:

```yaml
sdk: v5.8.2
```

То есть:

```text
mod.json:    5.8.2
GitHub tag:  v5.8.2
```

---

# 25. GitHub Actions

Используется официальный action:

```text
geode-sdk/build-geode-mod@main
```

Параметры:

```yaml
sdk: v5.8.2
target: Win64
build-config: Release
use-lto: true
```

Официальный Windows build currently использует:

```text
Clang
Clang++
Ninja
```

а не MSVC.

---

# 26. Build workflow

В workflow желательно сохранять такой порядок:

```text
1. bootstrap regression tests

2. fetch pinned dependencies

3. offline invariant verification

4. official Geode bindings verification

5. CMake/build via build-geode-mod

6. verify Spout DLL embedded in .geode

7. upload Win64 artifact
```

---

# 27. Bindings

Target bindings:

```text
Geometry Dash 2.2081
```

Основные bindings раньше были проверены через:

```text
geode-sdk/bindings/main
bindings/2.2081/GeometryDash.bro
bindings/2.2081/Cocos2d.bro
```

Если компиляция падает из-за field/method name — не угадывать.

Сначала сверять binding.

---

# 28. StartPos requirement

StartPos должен полностью соответствовать настоящему PlayLayer.

Но начиная с v0.1.5 это значительно проще:

```text
StartPos существует только в REAL PlayLayer
```

Layout render не должен иметь отдельного StartPos state.

То же относится к:

```text
song offset
speed portal state
gamemode
gravity
camera state
```

Не копировать gameplay state в отдельный мир.

---

# 29. Music requirement

Только настоящий PlayLayer управляет:

```text
FMOD
music position
song offset
practice music
StartPos music offset
```

Layout layer не должен:

```text
startMusic
prepareMusic
seek FMOD
```

---

# 30. Physics requirement

Начиная с v0.1.5 Layout Mode должен быть **полностью passive относительно физики**.

Он не должен менять:

```text
player velocity
player position
gravity
gamemode
collision
delta time
TPS
input
checkpoint state
```

Если layout выглядит на один кадр иначе, лучше исправлять render transform, а не создавать дополнительную physics simulation.

---

# 31. Practice requirement

Главный regression test после v0.1.5:

```text
1. открыть уровень
2. Practice Mode
3. поставить checkpoint
4. умереть
5. restart checkpoint
6. повторить 10–20 раз
```

Ожидается:

```text
NO CRASH
NO SECOND SESSION
NO METADATA CORRUPTION
NO DOUBLE ATTEMPTS
```

---

# 32. Следующие тесты пользователя

После сборки v0.1.5 первым делом проверить только:

## Test A — Spout

OBS:

```text
Add Source
→ Spout2 Capture
```

Sender должен называться:

```text
Geometry Dash Full
```

Проверить лог на:

```text
Spout requested GPU texture mode
Spout GPU sender active
```

или CPU warning.

Главное: sender не должен исчезать.

---

## Test B — Practice

```text
Practice
checkpoint
restart
checkpoint
restart
...
```

Сделать минимум:

```text
10 restarts
```

Если crash — нужен crashlog.

---

# 33. Где брать crash stack trace

Обычный:

```text
Geode YYYY-MM-DD HH.MM.SS.log
```

не всегда содержит exception.

Пользователь уже дважды присылал runtime logs, которые просто обрывались около gameplay events.

Если Geometry Dash крашится, запросить:

```text
Geometry Dash/geode/crashlogs/
```

Нужен самый свежий crash file со:

```text
Exception
fault address
stack trace
loaded modules
```

Не пытаться точно назвать функцию crash только по обычному runtime log без stack trace.

---

# 34. Последний известный runtime result v0.1.4

До перехода на v0.1.5:

Spout:

```text
Spout sender initialized: Geometry Dash Full

Spout selected CPU sharing.
Sender disabled for this session;
GPU-only sharing is required by this mod.
```

Practice / Layout:

```text
Layout mirror created
Layout mirror start synchronized
Noclip Detected!
Noclip Detected!
```

После этого log обрывался.

Это стало причиной полной отмены second PlayLayer architecture.

---

# 35. Что другой AI НЕ должен делать

Не предлагать снова:

```text
hidden PlayLayer
second PlayLayer
secondary gameplay simulation
duplicate practice checkpoints
duplicate inputs
duplicate resetLevel calls
duplicate StartPos
duplicate audio timeline
```

Не возвращать:

```cpp
glReadPixels
```

Не делать CPU frame copy как основную архитектуру.

Не удалять sender только потому, что Spout выбрал CPU fallback.

Не переписывать XDBot Layout algorithm вручную.

Не менять pinned XDBot logic без необходимости.

---

# 36. Что можно менять

Можно менять:

```text
render masking
object visibility state
object render styling
swapBuffers hook ordering
Spout initialization
Spout sharing configuration
framebuffer handling
state save/restore implementation
Layout object matching
diagnostic logging
```

При условии:

```text
REAL gameplay state остаётся одним
```

---

# 37. Самая важная архитектурная инварианта

В любой момент gameplay должно существовать:

```text
ONE authoritative PlayLayer
ONE Player state
ONE Practice state
ONE StartPos state
ONE Music timeline
ONE Trigger simulation
ONE Physics simulation
```

Layout — только альтернативный render этой же сессии.

---

# 38. Desired final behavior

Идеальный результат:

```text
                     ┌──────────────────┐
                     │ REAL PLAYLAYER   │
                     │ full decorated   │
                     │ real physics     │
                     └────────┬─────────┘
                              │
                       normal render
                              │
                              ▼
                     ORIGINAL FRAMEBUFFER
                              │
                    ┌─────────┴─────────┐
                    │                   │
                    ▼                   ▼
              Spout SendFbo      temporary Layout
                    │              render mask
                    ▼                   │
                   OBS                  ▼
              full decorated       GD window
                                  XDBot Layout
```

No duplicated gameplay.

---

# 39. Current status

Build/CI infrastructure уже проходила после исправлений v0.1.1–v0.1.4.

Основные unresolved runtime вопросы после v0.1.5:

```text
1. Появится ли Geometry Dash Full в OBS Spout2 Capture.

2. Получится ли Spout texture/GPU sharing на RTX 3090.

3. Устранён ли Practice restart crash после удаления второго PlayLayer.

4. Корректно ли Layout mask совпадает с XDBot.

5. Не попадает ли Layout redraw случайно в Spout из-за swapBuffers hook ordering.

6. Корректно ли state restore работает со shaders / mod overlays.
```

v0.1.5 ещё должен быть протестирован пользователем.

---

# 40. Communication style

Пользователь говорит по-русски.

Он хочет:

```text
конкретные исправления
полный готовый source archive
минимум лишних уточняющих вопросов
```

Если найден bug:

```text
исправить исходник
поднять версию
прогнать regression tests
дать полный ZIP проекта
```

а не только присылать отдельный code snippet.

---

# 41. Current artifact

Последний полный проект:

```text
Spout-Layout-Dual-View-v0.1.5-FULL-PROJECT.zip
```

Version:

```text
v0.1.5
```

SHA-256:

```text
f77a40a2d947b2340449850b42f85278574e2f6b77cbbcb52cdc6736f3c218a1
```

Работу продолжать именно от этой версии, а не от старой v0.1.3/v0.1.4.

---

# 42. Immediate next action

Если пользователь присылает результат v0.1.5:

### Если Spout sender отсутствует

Искать в log:

```text
Spout
GetSpout
SetShareMode
SetCPUmode
SetCPUshare
CreateSender
SendFbo
```

Определить:

```text
DLL load?
GetSpout succeeded?
CreateSender succeeded?
sender destroyed?
SendFbo returning false?
CPU/GPU mode?
```

Не угадывать.

---

### Если Practice всё ещё crash

Получить:

```text
geode/crashlogs/<latest crash>
```

и по stack trace определить точный hook/function.

Поскольку v0.1.5 не имеет второго PlayLayer, crash уже будет означать другую проблему, вероятнее всего:

```text
temporary render-state mutation
invalid object pointer
visible object cache
render traversal
shader/render-state restore
hook ordering
```

---

# END OF CONTEXT

Core rule:

```text
OBS = full original framebuffer
GD window = XDBot-style Layout
ONE PlayLayer only
GPU path preferred
NO duplicated physics
```
