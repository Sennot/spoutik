#include "../core/NoclipHitDetector.hpp"
#include "../runtime/StartPosAnalyzer.hpp"
#include "../runtime/TrainingManager.hpp"
#include "../ui/ProgressNotification.hpp"

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>

#include <cstdint>

using namespace geode::prelude;

class $modify(BaconsistentPlayLayer, PlayLayer) {
    struct Fields {
        baconsistent::PercentageMode lastMode = baconsistent::PercentageMode::Modern22;
        baconsistent::core::NoclipHitDetector noclipDetector;
    };

    static void onModify(auto& self) {
        // Run our wrapper before most destroyPlayer hooks. We call the next
        // hook/original first and inspect the final result afterwards. If GD
        // tried to kill an alive player but the hook chain left them alive,
        // that is a noclip-suppressed death for this attempt.
        (void)self.setHookPriorityPre("PlayLayer::destroyPlayer", Priority::First);
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        auto& manager = baconsistent::TrainingManager::get();
        if (level && !level->isPlatformer()) {
            // PlayLayer has already created m_objects here, so StartPos copies
            // can be analysed without editor metadata or manual setup.
            auto const analysis = baconsistent::analyzeStartPositions(this);
            manager.loadLevel(level, analysis);
            m_fields->lastMode = manager.percentageMode();
            manager.beginAttempt(
                baconsistent::currentPercentForMode(this, m_fields->lastMode),
                m_isPracticeMode
            );
            m_fields->noclipDetector.reset();
        }
        else {
            manager.unloadLevel();
        }

        // Intentionally no gameplay HUD. Baconsistent lives in PauseLayer only
        // so the level view remains pixel-for-pixel unobstructed.
        return true;
    }

    void resetLevel() {
        auto& manager = baconsistent::TrainingManager::get();
        manager.finishAttempt();

        // Disarm noclip detection before GD performs any StartPos/reset
        // bookkeeping. destroyPlayer may be called during this window even
        // though no gameplay collision happened.
        m_fields->noclipDetector.reset();
        PlayLayer::resetLevel();

        if (manager.loaded()) {
            m_fields->lastMode = manager.percentageMode();
            manager.beginAttempt(
                baconsistent::currentPercentForMode(this, m_fields->lastMode),
                m_isPracticeMode
            );
        }
    }

    void destroyPlayer(PlayerObject* player, GameObject* object) {
        auto const wasAlive = player && !player->m_isDead;

        // Let GD and every inner hook decide what actually happens first.
        PlayLayer::destroyPlayer(player, object);

        auto& manager = baconsistent::TrainingManager::get();
        if (!manager.loaded() || !wasAlive || !player || m_levelEndAnimationStarted) {
            return;
        }

        // v0.4.2: do not treat every live-returning destroyPlayer call as
        // noclip. GD emits bookkeeping calls around StartPos/reset. Feed the
        // event through a conservative Death-Tracker-style detector instead.
        auto const nativeIgnoreDamage = m_isIgnoreDamageEnabled || m_ignoreDamage;
        auto const suppressed = m_fields->noclipDetector.observe(
            reinterpret_cast<std::uintptr_t>(object),
            wasAlive,
            !player->m_isDead,
            m_levelEndAnimationStarted,
            nativeIgnoreDamage
        );
        if (suppressed) {
            manager.markSuppressedDeath();
        }
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);

        auto& manager = baconsistent::TrainingManager::get();
        if (!manager.loaded()) {
            return;
        }

        // Practice protection is sticky for the current attempt. If Practice
        // Mode is enabled after the attempt began, toggling it back off cannot
        // make that checkpoint-assisted run valid retroactively.
        manager.observePracticeMode(m_isPracticeMode);

        auto const mode = manager.percentageMode();
        if (mode != m_fields->lastMode) {
            // Percentage mode is normally changed while paused. Treat the
            // resumed state as a fresh statistical attempt so 2.1 and 2.2
            // progress are never mixed in one run.
            manager.finishAttempt();
            m_fields->lastMode = mode;
            manager.beginAttempt(
                baconsistent::currentPercentForMode(this, mode),
                m_isPracticeMode
            );
            m_fields->noclipDetector.reset();
        }

        manager.tick(dt);
        if (auto event = manager.update(baconsistent::currentPercentForMode(this, mode))) {
            baconsistent::ui::showProgressNotification(*event);
        }

        // Arm only after at least one complete gameplay update has finished.
        // This is what prevents entering/loading a StartPos from looking like
        // a noclip collision.
        m_fields->noclipDetector.advanceFrame();
    }

    void levelComplete() {
        auto& manager = baconsistent::TrainingManager::get();
        if (manager.loaded()) {
            manager.observePracticeMode(m_isPracticeMode);
            manager.update(100.0);
            manager.finishAttempt();
        }
        PlayLayer::levelComplete();
    }

    void onQuit() {
        auto& manager = baconsistent::TrainingManager::get();
        manager.finishAttempt();
        manager.unloadLevel();
        PlayLayer::onQuit();
    }
};
