# FULL SOURCE BUILD — v0.4.2

This repository root is the complete Baconsistent source tree. The in-game Geode version must display **v0.4.2**.

# Baconsistent

**Baconsistent** is a Geometry Dash consistency-training mod for **Geode**, made by **Strafe**.

The mod is built around a fixed-part method: every StartPos-defined stage is repeated a chosen number of times (20 by default), preferably from the end of the level backwards. Unlike expanding-run systems, a Baconsistent stage never grows after a lucky completion.

## Training flow

1. Open a training copy that contains StartPos objects.
2. Baconsistent scans the copy automatically and turns the physical StartPos markers into fixed stages.
3. Pause the level and open the circular Baconsistent button in Geode's `left-button-menu`.
4. Pick a stage from the Stage Browser and press **Load Stage**.
5. Reach that stage's fixed end to add one repetition. The attempt is allowed to continue after the boundary.
6. Finish every stage's target to complete the round. Baconsistent then starts the next round while preserving lifetime stats and completed-round history.

Example detected profile:

```text
0 -> 11.3 -> 26.8 -> 43.5 -> 67.1 -> 84.9 -> 100
```

With a default target of 20 this becomes six fixed stages, each trained to `20/20`.

## v0.4 Pause progression / notifications

The standard Practice Mode progress area in PauseLayer is now repurposed for the **physically active StartPos**. It shows the current stage range, completed repetitions and how many are still left, for example `RUNS 14/20   LEFT 6`. The value refreshes while paused, so changing the active StartPos through another compatible StartPos switcher immediately changes the Baconsistent bar as well.

Successful fixed-stage runs can also show a short branded Geode notification. Regular repetitions report the updated `x/target` and remaining count; reaching the target uses a mastered-stage badge, while completing the whole plan uses a round-complete badge. These are transient event notifications, not a persistent gameplay HUD. Both pause progression and success notifications can be disabled in settings.

## v0.3 UI / UX

Baconsistent intentionally has **no persistent gameplay HUD**. The main interface lives in the pause flow; v0.4 optionally adds short success notifications when a fixed stage is counted.

The pause popup now has three branded pages:

- **Stages** — fixed StartPos browser, selected-stage progress, per-stage target controls, Load Stage, backwards recommendation and reset.
- **Stats** — Selected Stage / Current Round / Lifetime attempts, successes, success rate, streaks and training time.
- **Rounds** — current round progress plus the most recent completed-round summaries.

The pause button is a fixed-size layout item inserted into Geode's existing `left-button-menu` followed by `updateLayout()`. There is no absolute-position fallback, so Baconsistent does not intentionally stack a large icon on top of other mod buttons.

The release also includes an original Baconsistent sprite pack for the pause icon, header, tabs, cards, stage rows, progress bars, completion badges and action icons.

## StartPos analysis

Baconsistent stores the same physical StartPos profile in two percentage systems:

- **2.1 / legacy:** `x / levelLength * 100`
- **2.2 / modern:** `timeForPos(x) / timeForPos(levelLength) * 100`

The 2.2 mode accounts for speed changes. Switching modes only changes how the same physical stages are labelled/measured; their repetition counters stay attached to the same stage indexes.

If Geometry Dash preserves `m_originalLevel` on a StartPos copy, the detected profile and training progress are stored under the original online level ID. This lets the copy teach Baconsistent the stage layout and the original level reuse it later.

When no StartPos profile is available, Baconsistent can fall back to equal percentage splits configured in settings.

## Round system

The default target is 20 repetitions per stage, but each stage can override its own target from the pause menu.

When all fixed stages reach their target:

- the current round is recorded in history;
- the round's attempts / success rate / best streak / time are preserved;
- repetition counters reset for the next round;
- the recommended stage moves back to the last unfinished stage of the new round;
- the next time the player pauses, the UI can acknowledge that the previous round was completed.

No forced restart or gameplay popup is injected when the final repetition happens.

## Settings

- Enable / disable tracking
- Auto-detect StartPos stages
- 2.1 or 2.2 percentage mode
- Equal-split fallback part count
- Global default repetition target
- Automatically move to the previous unfinished stage
- Automatically match the selected stage to the attempt's StartPos
- Strict small-run validation
- Start boundary tolerance
- Practice Mode protection (independent toggle)
- Noclip suppressed-death protection (independent toggle)
- Pause StartPos progression bar
- Successful-run notifications

## Build

Current target:

- Geometry Dash **2.2081**
- Geode **5.8.2**
- **Windows x64 only**

With a Geode SDK checkout available through `GEODE_SDK`:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The included GitHub Actions workflow builds only Win64 via `geode-sdk/build-geode-mod`.

Pure training/statistics tests can be compiled independently of Geode:

```sh
g++ -std=c++23 -Wall -Wextra -Werror -pedantic \
    tests/test_training.cpp \
    src/core/AttemptGuard.cpp \
    src/core/TrainingPlan.cpp \
    src/core/TrainingStats.cpp \
    -o baconsistent-tests
./baconsistent-tests
```

## Project structure

```text
resources/    original Baconsistent UI sprite pack
src/core/     fixed-stage plan, round-independent logic and statistics
src/runtime/  StartPos profiles, persistence, settings and round progression
src/hooks/    PlayLayer tracking + layout-safe PauseLayer entry button
src/ui/       branded popup, pause StartPos bar and transient success notifications
tests/        standalone core regression tests
```

## Credits

The training method implemented by Baconsistent is the fixed-stage repetition strategy provided to the developer: train many small unchanged parts, preferably backwards, then focus on play-from-zero after consistency has been built.

Blitzkrieg was used as an open-source reference for Geometry Dash StartPos workflow, percentage compatibility and training-product UX. See `THIRD_PARTY_NOTICES.md`.

## v0.4.2 run safety

Baconsistent writes completed training data to disk early so a Geometry Dash crash does not roll back the last successful repetitions. `Practice Mode protection` and `Noclip protection` are independent settings. With noclip protection enabled, noclip may remain turned on: a fixed A-to-B run still counts if no lethal collision was suppressed. If GD tries to kill the player and another hook keeps the player alive, that attempt is ignored.
