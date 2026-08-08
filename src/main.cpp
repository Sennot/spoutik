#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/LevelTools.hpp>
#include <Geode/modify/CCEGLView.hpp>
#include "LayoutMirror.hpp"
#include "SpoutSender.hpp"
#include "layout_mode.hpp"

using namespace geode::prelude;

// The mirror is created/ticked from VeryLate hooks. Geode preserves the current
// hook priority across same-thread nested calls, so calls made for the hidden
// PlayLayer skip ordinary third-party gameplay hooks. Last-priority guard hooks
// below still intercept mirror-only side effects (audio/stats/exit/addObject).
// This keeps every normal mod hook active for the real authoritative PlayLayer
// while making the second visual world much less visible to other gameplay mods.

class $modify(SpoutLayoutLevelTools, LevelTools) {
    static void onModify(auto& self) {
        if (!self.setHookPriorityPre("LevelTools::verifyLevelIntegrity", Priority::Last)) {
            log::warn("Could not isolate LevelTools::verifyLevelIntegrity for layout mirror");
        }
    }

    static bool verifyLevelIntegrity(gd::string levelString, int objectCount) {
        if (LayoutMirror::get().integrityBypass()) return true;
        return LevelTools::verifyLevelIntegrity(levelString, objectCount);
    }
};

class $modify(SpoutLayoutPlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        // Entry/control hooks run after ordinary gameplay mods have handled the
        // REAL layer. Nested mirror calls inherit this VeryLate priority and
        // therefore bypass those ordinary hooks.
        for (auto name : {
            "PlayLayer::init",
            "PlayLayer::startGame",
            "PlayLayer::resetLevel",
            "PlayLayer::markCheckpoint",
            "PlayLayer::removeCheckpoint",
            "PlayLayer::removeAllCheckpoints",
        }) {
            if (!self.setHookPriorityPre(name, Priority::VeryLate)) {
                log::warn("Could not set mirror-isolation priority for {}", name);
            }
        }

        // These hooks must remain reachable from a VeryLate nested mirror call.
        // Priority::Last gives us a final guard before the original function.
        for (auto name : {
            "PlayLayer::prepareMusic",
            "PlayLayer::startMusic",
            "PlayLayer::startGameDelayed",
            "PlayLayer::addObject",
            "PlayLayer::levelComplete",
            "PlayLayer::commitJumps",
            "PlayLayer::updateAttempts",
            "PlayLayer::onQuit",
        }) {
            if (!self.setHookPriorityPre(name, Priority::Last)) {
                log::warn("Could not set mirror guard priority for {}", name);
            }
        }
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        auto& mirrors = LayoutMirror::get();
        if (mirrors.isCreating()) {
            return PlayLayer::init(level, useReplay, dontCreateObjects);
        }

        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        mirrors.createFor(this, level, useReplay, dontCreateObjects);
        return true;
    }

    void prepareMusic(bool dontWait) {
        auto& mirrors = LayoutMirror::get();
        if (mirrors.isCreating() || mirrors.isMirror(this)) {
            m_audioPaused = true;
            m_isSilent = true;
            m_musicPrepared = true;
            return;
        }
        PlayLayer::prepareMusic(dontWait);
    }

    void startMusic() {
        auto& mirrors = LayoutMirror::get();
        if (mirrors.isCreating() || mirrors.isMirror(this)) {
            m_audioPaused = true;
            m_isSilent = true;
            return;
        }
        PlayLayer::startMusic();
    }

    void startGame() {
        auto& mirrors = LayoutMirror::get();
        if (mirrors.isMirror(this) || mirrors.isStarting()) {
            return PlayLayer::startGame();
        }
        PlayLayer::startGame();
        mirrors.startFromReal(this);
    }

    void startGameDelayed() {
        auto& mirrors = LayoutMirror::get();
        if (mirrors.isCreating() || mirrors.isMirror(this)) return;
        PlayLayer::startGameDelayed();
    }

    void addObject(GameObject* object) {
        auto& mirrors = LayoutMirror::get();
        if (!mirrors.layoutContext(this)) {
            return PlayLayer::addObject(object);
        }

        // Exact XDBot addObject behavior for the layout PlayLayer.
        if (excludedTriggerIDs.contains(object->m_objectID)) return;
        PlayLayer::addObject(object);
        object->m_activeMainColorID = -1;
        object->m_activeDetailColorID = -1;
        object->m_detailUsesHSV = false;
        object->m_baseUsesHSV = false;
        object->m_hasNoGlow = true;
        object->m_isHide = object->m_objectID == 2065;
        object->setOpacity(object->m_objectID == 2065 ? 0 : 255);
        object->setVisible(object->m_objectID != 2065);
    }

    void resetLevel() {
        auto& mirrors = LayoutMirror::get();
        if (mirrors.isMirror(this)) return PlayLayer::resetLevel();
        PlayLayer::resetLevel();
        mirrors.resetFromReal(this);
    }

    void levelComplete() {
        auto& mirrors = LayoutMirror::get();
        if (mirrors.isCreating() || mirrors.isMirror(this)) return;
        PlayLayer::levelComplete();
    }

    void commitJumps() {
        auto& mirrors = LayoutMirror::get();
        if (mirrors.isCreating() || mirrors.isMirror(this)) return;
        PlayLayer::commitJumps();
    }

    void updateAttempts() {
        auto& mirrors = LayoutMirror::get();
        if (mirrors.isCreating() || mirrors.isMirror(this)) return;
        PlayLayer::updateAttempts();
    }

    CheckpointObject* markCheckpoint() {
        auto& mirrors = LayoutMirror::get();
        if (mirrors.isMirror(this)) return PlayLayer::markCheckpoint();
        auto result = PlayLayer::markCheckpoint();
        mirrors.markCheckpointFromReal(this);
        return result;
    }

    void removeCheckpoint(bool first) {
        auto& mirrors = LayoutMirror::get();
        if (mirrors.isMirror(this)) return PlayLayer::removeCheckpoint(first);
        PlayLayer::removeCheckpoint(first);
        mirrors.removeCheckpointFromReal(this, first);
    }

    void removeAllCheckpoints() {
        auto& mirrors = LayoutMirror::get();
        if (mirrors.isMirror(this)) return PlayLayer::removeAllCheckpoints();
        PlayLayer::removeAllCheckpoints();
        mirrors.removeAllCheckpointsFromReal(this);
    }

    void onQuit() {
        auto& mirrors = LayoutMirror::get();
        if (mirrors.isCreating() || mirrors.isMirror(this)) return;
        mirrors.destroyFor(this);
        PlayLayer::onQuit();
    }
};

class $modify(SpoutLayoutBaseGameLayer, GJBaseGameLayer) {
    static void onModify(auto& self) {
        for (auto name : {
            "GJBaseGameLayer::update",
            "GJBaseGameLayer::handleButton",
        }) {
            if (!self.setHookPriorityPre(name, Priority::VeryLate)) {
                log::warn("Could not set mirror-isolation priority for {}", name);
            }
        }
    }

    void update(float dt) {
        auto* play = typeinfo_cast<PlayLayer*>(this);
        auto& mirrors = LayoutMirror::get();

        // A scheduler callback for the mirror should normally be impossible
        // after quiesceMirrorScheduler(). Keep this guard as a final fallback.
        if (play && mirrors.isMirror(play)) {
            if (mirrors.isStepping()) return GJBaseGameLayer::update(dt);
            return;
        }

        GJBaseGameLayer::update(dt);
        if (play) mirrors.stepFromReal(play, dt);
    }

    void handleButton(bool down, int button, bool player1) {
        auto* play = typeinfo_cast<PlayLayer*>(this);
        auto& mirrors = LayoutMirror::get();
        GJBaseGameLayer::handleButton(down, button, player1);
        if (play && !mirrors.isMirror(play)) {
            mirrors.forwardButton(play, down, button, player1);
        }
    }
};

class $modify(SpoutLayoutEGLView, cocos2d::CCEGLView) {
    static void onModify(auto& self) {
        if (!self.setHookPriorityPre("cocos2d::CCEGLView::swapBuffers", Priority::Last)) {
            log::warn("Could not set CCEGLView::swapBuffers hook to Priority::Last");
        }
    }

    void swapBuffers() {
        // Send the fully decorated frame first. Only afterwards replace the
        // local backbuffer with the stripped player view.
        SpoutSender::get().sendDefaultFramebuffer();

        auto* director = cocos2d::CCDirector::get();
        auto* real = PlayLayer::get();
        if (director && real) LayoutMirror::get().renderPlayerView(director, real);

        CCEGLView::swapBuffers();
    }
};
