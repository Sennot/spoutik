#include "LayoutMirror.hpp"
#include "layout_mode.hpp"
#include <Geode/loader/Mod.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

#ifdef GEODE_IS_WINDOWS
#include <windows.h>
#include <GL/gl.h>
#endif

using namespace geode::prelude;

LayoutMirror& LayoutMirror::get() {
    static LayoutMirror instance;
    return instance;
}

LayoutMirror::~LayoutMirror() {
    releaseMirror();
}

LayoutMirror::GameManagerScope::GameManagerScope(PlayLayer* layer) {
    gm = GameManager::get();
    if (!gm || !layer) return;
    oldPlay = gm->m_playLayer;
    oldGame = gm->m_gameLayer;
    gm->m_playLayer = layer;
    gm->m_gameLayer = layer;
}

LayoutMirror::GameManagerScope::~GameManagerScope() {
    if (!gm) return;
    gm->m_playLayer = oldPlay;
    gm->m_gameLayer = oldGame;
}

void LayoutMirror::quiesceMirrorScheduler() {
    if (!m_mirror) return;
    // A hidden PlayLayer must never receive autonomous scheduler callbacks.
    // unscheduleAllForTarget includes update and custom selectors such as a
    // delayed start. Manual direct calls from the authoritative layer still work.
    if (auto* scheduler = cocos2d::CCScheduler::get()) {
        scheduler->unscheduleAllForTarget(m_mirror);
    }
}

void LayoutMirror::releaseMirror() {
    // Release PlayLayer first because it can retain/use its GJGameLevel during
    // destruction. We keep one explicit retain on the private level copy too.
    auto hadMirror = m_mirror != nullptr;
    if (m_mirror) {
        // Remove every scheduler callback before the retained hidden layer can
        // be destroyed. This prevents delayed callbacks from targeting a freed
        // PlayLayer after leaving/changing levels.
        quiesceMirrorScheduler();
        m_mirror->release();
        m_mirror = nullptr;
    }
    if (m_mirrorLevel) {
        m_mirrorLevel->release();
        m_mirrorLevel = nullptr;
    }
    m_real = nullptr;
    m_lastRealStartPos = nullptr;
    m_lastMirrorStartPos = nullptr;
    m_startPosSynced = false;
    m_modifiedString.clear();
    if (hadMirror) log::debug("Layout mirror released cleanly");
}

void LayoutMirror::createFor(PlayLayer* real, GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
    if (!real || !level) return;
    if (!Mod::get()->getSettingValue<bool>("enabled")) return;

    releaseMirror();
    m_real = real;

    try {
        m_modifiedString = LayoutMode::getModifiedString(std::string(level->m_levelString));
    }
    catch (std::exception const& e) {
        log::error("XDBot LayoutMode preprocessing failed: {}", e.what());
        releaseMirror();
        return;
    }

    // Never let the hidden visual simulation share the user's live GJGameLevel
    // object. resetLevel / levelComplete / attempts may touch level statistics;
    // a private copy makes those writes harmless while preserving all metadata.
    auto mirrorLevel = GJGameLevel::create();
    if (!mirrorLevel) {
        log::error("Could not allocate private GJGameLevel for Layout Mode mirror");
        releaseMirror();
        return;
    }
    mirrorLevel->retain();
    mirrorLevel->copyLevelInfo(level);
    mirrorLevel->m_levelString = m_modifiedString;
    mirrorLevel->m_recordString = level->m_recordString;
    m_mirrorLevel = mirrorLevel;

    auto gm = GameManager::get();
    auto oldPlay = gm ? gm->m_playLayer : nullptr;
    auto oldGame = gm ? gm->m_gameLayer : nullptr;

    m_creating = true;
    auto mirror = PlayLayer::create(mirrorLevel, useReplay, dontCreateObjects);
    m_creating = false;

    if (gm) {
        gm->m_playLayer = oldPlay ? oldPlay : real;
        gm->m_gameLayer = oldGame ? oldGame : real;
    }

    if (!mirror) {
        log::error("Could not create Layout Mode mirror PlayLayer");
        releaseMirror();
        return;
    }

    mirror->retain();
    m_mirror = mirror;
    // PlayLayer::init can schedule update plus custom/delayed selectors. The
    // mirror is manually lockstepped, so remove all callbacks for this target.
    quiesceMirrorScheduler();
    m_mirror->m_audioPaused = true;
    m_mirror->m_isSilent = true;
    m_mirror->m_musicPrepared = true;
    syncRuntimeFlags(real);
    m_mirror->m_gameState.m_levelTime = real->m_gameState.m_levelTime;
    m_mirror->m_timePlayed = real->m_timePlayed;

    // Normally startGame is called later, after the transition. This also
    // handles unusual loaders that return an already-started real PlayLayer.
    if (real->m_started) startFromReal(real);

    log::info("Layout mirror created: {} source objects -> {} mirror objects",
        real->m_objects ? real->m_objects->count() : 0,
        mirror->m_objects ? mirror->m_objects->count() : 0);
}

void LayoutMirror::destroyFor(PlayLayer* real) {
    if (real && real == m_real) releaseMirror();
}

void LayoutMirror::syncStartPos(PlayLayer* real) {
    if (!m_mirror || !real || real != m_real) return;

    auto* wanted = real->m_startPosObject;
    if (m_startPosSynced && wanted == m_lastRealStartPos) return;
    m_startPosSynced = true;
    m_lastRealStartPos = wanted;
    m_lastMirrorStartPos = nullptr;

    if (!wanted) {
        m_mirror->m_startPosObject = nullptr;
        return;
    }

    auto* objects = m_mirror->m_objects;
    if (!objects) return;

    auto wantedPos = wanted->getPosition();
    auto bestDistance = std::numeric_limits<float>::max();
    StartPosObject* best = nullptr;

    for (unsigned i = 0; i < objects->count(); ++i) {
        auto* candidate = typeinfo_cast<StartPosObject*>(objects->objectAtIndex(i));
        if (!candidate) continue;

        // m_uniqueID can differ after XDBot removes earlier objects, so position
        // is the stable identity shared by both independently parsed worlds.
        auto pos = candidate->getPosition();
        auto dx = pos.x - wantedPos.x;
        auto dy = pos.y - wantedPos.y;
        auto distance = dx * dx + dy * dy;
        if (distance < bestDistance) {
            bestDistance = distance;
            best = candidate;
        }
    }

    // Object coordinates are serialized with finite precision. A quarter-unit
    // radius is generous enough for parse rounding but rejects a different SP.
    if (best && bestDistance <= 0.0625f) {
        m_mirror->m_startPosObject = best;
        m_lastMirrorStartPos = best;
    }
    else {
        log::warn("Could not map changed StartPos into the Layout mirror; using level start for mirror visuals");
        m_mirror->m_startPosObject = nullptr;
    }
}

void LayoutMirror::syncRuntimeFlags(PlayLayer* real) {
    if (!m_mirror || !real || real != m_real) return;

    // Keep gameplay-mode switches made after PlayLayer::init in lockstep.
    // The StartPos pointer itself is NEVER copied across object graphs;
    // syncStartPos maps it to the corresponding StartPosObject in the mirror.
    m_mirror->m_isPracticeMode = real->m_isPracticeMode;
    m_mirror->m_practiceMusicSync = real->m_practiceMusicSync;
    m_mirror->m_isTestMode = real->m_isTestMode;
    m_mirror->m_ignoreDamage = real->m_ignoreDamage;
    syncStartPos(real);
}

void LayoutMirror::startFromReal(PlayLayer* real) {
    if (!m_mirror || real != m_real || m_starting || m_creating) return;
    if (!real->m_started || m_mirror->m_started) return;

    m_starting = true;
    syncRuntimeFlags(real);
    {
        GameManagerScope scope(m_mirror);
        // startMusic() is intercepted for the mirror, so this initializes the
        // gameplay state without creating a second FMOD timeline.
        m_mirror->startGame();
        // startGame may schedule update/delayed selectors again.
        quiesceMirrorScheduler();
        m_mirror->m_audioPaused = true;
        m_mirror->m_isSilent = true;
        m_mirror->m_gameState.m_levelTime = real->m_gameState.m_levelTime;
        m_mirror->m_timePlayed = real->m_timePlayed;
    }
    log::debug("Layout mirror start synchronized to authoritative PlayLayer");
    m_starting = false;
}

void LayoutMirror::stepFromReal(PlayLayer* real, float dt) {
    if (!m_mirror || real != m_real || m_stepping || m_creating) return;
    if (!Mod::get()->getSettingValue<bool>("enabled")) return;
    if (!Mod::get()->getSettingValue<bool>("mirror-simulation")) return;
    if (real->m_isPaused || !real->m_started) return;

    if (!m_mirror->m_started) startFromReal(real);
    if (!m_mirror->m_started) return;

    m_stepping = true;
    syncRuntimeFlags(real);
    {
        GameManagerScope scope(m_mirror);
        // Tick exactly once with the same dt. Afterwards snap public timing
        // counters to the authoritative layer; doing this before update would
        // advance the mirror one frame ahead.
        m_mirror->update(dt);
        m_mirror->m_gameState.m_levelTime = real->m_gameState.m_levelTime;
        m_mirror->m_timePlayed = real->m_timePlayed;
    }
    m_stepping = false;
}

void LayoutMirror::forwardButton(PlayLayer* real, bool down, int button, bool player1) {
    if (!m_mirror || real != m_real || m_forwardingInput || m_creating) return;
    if (!Mod::get()->getSettingValue<bool>("enabled")) return;
    if (!m_mirror->m_started && real->m_started) startFromReal(real);
    m_forwardingInput = true;
    syncRuntimeFlags(real);
    {
        GameManagerScope scope(m_mirror);
        m_mirror->handleButton(down, button, player1);
    }
    m_forwardingInput = false;
}

void LayoutMirror::resetFromReal(PlayLayer* real) {
    if (!m_mirror || real != m_real || m_resetting || m_creating) return;
    m_resetting = true;
    syncRuntimeFlags(real);
    {
        GameManagerScope scope(m_mirror);
        m_mirror->resetLevel();
        quiesceMirrorScheduler();
        m_mirror->m_audioPaused = true;
        m_mirror->m_isSilent = true;
        m_mirror->m_gameState.m_levelTime = real->m_gameState.m_levelTime;
        m_mirror->m_timePlayed = real->m_timePlayed;
    }
    m_resetting = false;
}

void LayoutMirror::markCheckpointFromReal(PlayLayer* real) {
    if (!m_mirror || real != m_real || m_checkpointing || m_creating) return;
    m_checkpointing = true;
    syncRuntimeFlags(real);
    {
        GameManagerScope scope(m_mirror);
        m_mirror->markCheckpoint();
        quiesceMirrorScheduler();
    }
    m_checkpointing = false;
}

void LayoutMirror::removeCheckpointFromReal(PlayLayer* real, bool first) {
    if (!m_mirror || real != m_real || m_removingCheckpoint || m_creating) return;
    m_removingCheckpoint = true;
    syncRuntimeFlags(real);
    {
        GameManagerScope scope(m_mirror);
        m_mirror->removeCheckpoint(first);
        quiesceMirrorScheduler();
    }
    m_removingCheckpoint = false;
}

void LayoutMirror::removeAllCheckpointsFromReal(PlayLayer* real) {
    if (!m_mirror || real != m_real || m_removingCheckpoint || m_creating) return;
    m_removingCheckpoint = true;
    syncRuntimeFlags(real);
    {
        GameManagerScope scope(m_mirror);
        m_mirror->removeAllCheckpoints();
        quiesceMirrorScheduler();
    }
    m_removingCheckpoint = false;
}

void LayoutMirror::renderSceneSiblings(CCScene* scene, PlayLayer* real, bool before) {
    if (!scene || !scene->getChildren()) return;

    // Match CCNode visit order, including equal-z children. This matters for
    // pause layers and mod overlays that deliberately share PlayLayer's z-order.
    scene->sortAllChildren();
    auto children = scene->getChildren();
    bool passedReal = false;
    for (unsigned i = 0; i < children->count(); ++i) {
        auto node = typeinfo_cast<CCNode*>(children->objectAtIndex(i));
        if (node == real) {
            passedReal = true;
            continue;
        }
        if (!node || node == m_mirror || !node->isVisible()) continue;
        if (before ? !passedReal : passedReal) node->visit();
    }
}

void LayoutMirror::renderPlayerView(CCDirector* director, PlayLayer* real) {
#ifndef GEODE_IS_WINDOWS
    (void)director; (void)real;
#else
    if (!director || !m_mirror || real != m_real) return;
    if (!Mod::get()->getSettingValue<bool>("enabled")) return;
    if (!Mod::get()->getSettingValue<bool>("layout-player-view")) return;

    auto scene = director->getRunningScene();
    if (!scene) return;

    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    director->setViewport();
    director->setProjection(director->getProjection());

    renderSceneSiblings(scene, real, true);

    // The mirror's own UI is intentionally hidden. The real UILayer is rendered
    // afterwards so progress, CPS widgets attached to UILayer, pause UI etc. are
    // never duplicated and remain synchronized with authoritative gameplay.
    auto mirrorUI = m_mirror->m_uiLayer;
    bool oldMirrorUIVisible = mirrorUI ? mirrorUI->isVisible() : false;
    if (mirrorUI) mirrorUI->setVisible(false);
    {
        GameManagerScope scope(m_mirror);
        m_mirror->visit();
    }
    if (mirrorUI) mirrorUI->setVisible(oldMirrorUIVisible);

    if (real->m_uiLayer && real->m_uiLayer->isVisible()) real->m_uiLayer->visit();
    renderSceneSiblings(scene, real, false);

    if (auto notification = director->getNotificationNode(); notification && notification->isVisible()) {
        notification->visit();
    }
#endif
}
