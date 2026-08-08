#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/LevelTools.hpp>
#include <Geode/modify/CCEGLView.hpp>
#include "LayoutMirror.hpp"
#include "SpoutSender.hpp"
#include "layout_mode.hpp"

using namespace geode::prelude;

// XDBot's integrity bypass is active only while the stripped mirror is being
// constructed. The authoritative decorated PlayLayer keeps normal integrity.
class $modify(SpoutLayoutLevelTools, LevelTools) {
    static bool verifyLevelIntegrity(gd::string levelString, int objectCount) {
        if (LayoutMirror::get().integrityBypass()) return true;
        return LevelTools::verifyLevelIntegrity(levelString, objectCount);
    }
};

class $modify(SpoutLayoutPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        auto& mirrors = LayoutMirror::get();
        if (mirrors.isCreating()) {
            // The caller already swapped m_levelString to XDBot's fully
            // preprocessed level. Do not recursively create another mirror.
            return PlayLayer::init(level, useReplay, dontCreateObjects);
        }

        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;
        mirrors.createFor(this, level, useReplay, dontCreateObjects);
        return true;
    }

    void prepareMusic(bool dontWait) {
        auto& mirrors = LayoutMirror::get();
        if (mirrors.isCreating() || mirrors.isMirror(this)) {
            // Never let construction/simulation of the visual-only layer touch
            // FMOD. The real PlayLayer remains the only music timeline, which
            // is critical for StartPos song offsets.
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
        if (mirrors.isCreating() || mirrors.isMirror(this)) {
            // PlayLayer::init may schedule this callback even though the mirror
            // is not attached to the running scene. The real PlayLayer alone
            // decides when gameplay starts; startFromReal() starts the mirror.
            return;
        }
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
        if (mirrors.isCreating() || mirrors.isMirror(this)) {
            // The mirror is visual-only. Never let an independently simulated
            // layer submit completion, rewards, stats or achievements.
            return;
        }
        PlayLayer::levelComplete();
    }

    void commitJumps() {
        auto& mirrors = LayoutMirror::get();
        if (mirrors.isCreating() || mirrors.isMirror(this)) {
            // incrementJumps() is inline on Win64, but persistent jump totals
            // are committed through this hookable PlayLayer function. Keep the
            // mirror's temporary counters private and never publish them.
            return;
        }
        PlayLayer::commitJumps();
    }

    void updateAttempts() {
        auto& mirrors = LayoutMirror::get();
        if (mirrors.isCreating() || mirrors.isMirror(this)) {
            // resetLevel() invokes attempt bookkeeping. The hidden visual world
            // must not double-count attempts or touch global gameplay stats.
            return;
        }
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
        // The mirror is not a real scene. Never allow a mirror-side quit path
        // (including one triggered by another mod) to replace/leave the scene.
        if (mirrors.isCreating() || mirrors.isMirror(this)) return;
        mirrors.destroyFor(this);
        PlayLayer::onQuit();
    }
};

class $modify(SpoutLayoutBaseGameLayer, GJBaseGameLayer) {
    void update(float dt) {
        auto* play = typeinfo_cast<PlayLayer*>(this);
        auto& mirrors = LayoutMirror::get();

        // The hidden PlayLayer can register itself with Cocos' scheduler during
        // init even though it is not in the scene. Ignore those autonomous
        // callbacks: the mirror is advanced exactly once, explicitly, from the
        // real PlayLayer below. During that explicit call m_stepping is true.
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
        // swapBuffers is the final presentation boundary. Priority::Last on the
        // PRE side puts our capture after ordinary pre-swap hooks but before the
        // actual buffer swap, so post-draw overlays are included too.
        if (!self.setHookPriorityPre("cocos2d::CCEGLView::swapBuffers", Priority::Last)) {
            log::warn("Could not set CCEGLView::swapBuffers hook to Priority::Last");
        }
    }

    void swapBuffers() {
        // Immediately before presentation, the default framebuffer contains the
        // complete untouched game output: level decoration/shaders/camera, HUD,
        // menus, pause/editor UI and mod overlays drawn during the frame. Spout
        // receives this frame first; only the local backbuffer is then replaced.
        SpoutSender::get().sendDefaultFramebuffer();

        auto* director = cocos2d::CCDirector::get();
        auto* real = PlayLayer::get();
        if (director && real) LayoutMirror::get().renderPlayerView(director, real);

        CCEGLView::swapBuffers();
    }
};
