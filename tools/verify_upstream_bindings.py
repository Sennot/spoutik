#!/usr/bin/env python3
"""Verify the exact official Geode binding declarations used by this mod.

Runs in GitHub Actions where network access is available. The subsequent Windows Clang/Ninja
build remains authoritative; this produces a much clearer failure if bindings
change before compilation starts.
"""
from __future__ import annotations
import pathlib
import re
import urllib.request

ROOT = pathlib.Path(__file__).resolve().parents[1]
UA = {"User-Agent": "spout-layout-dualview-binding-check/1"}
BASE = "https://raw.githubusercontent.com/geode-sdk/bindings/main/bindings/2.2081/"


def fetch(name: str) -> str:
    req = urllib.request.Request(BASE + name, headers=UA)
    with urllib.request.urlopen(req, timeout=60) as r:
        return r.read().decode("utf-8")


def require(text: str, label: str, pattern: str) -> None:
    if not re.search(pattern, text, re.M):
        raise SystemExit(f"Official Geode 2.2081 binding mismatch: {label}")


gd = fetch("GeometryDash.bro")
cocos = fetch("Cocos2d.bro")

for label, pattern in {
    "PlayLayer::create": r"static PlayLayer\* create\(GJGameLevel\* level, bool useReplay, bool dontCreateObjects\)",
    "PlayLayer::init": r"bool init\(GJGameLevel\* level, bool useReplay, bool dontCreateObjects\)",
    "PlayLayer::addObject": r"void addObject\(GameObject\* object\)",
    "PlayLayer::resetLevel": r"void resetLevel\(\)",
    "PlayLayer::levelComplete": r"void levelComplete\(\)",
    "PlayLayer::commitJumps": r"void commitJumps\(\) = win 0x[0-9a-f]+",
    "PlayLayer::updateAttempts": r"void updateAttempts\(\) = win 0x[0-9a-f]+",
    "PlayLayer::markCheckpoint": r"CheckpointObject\* markCheckpoint\(\)",
    "PlayLayer::removeCheckpoint": r"void removeCheckpoint\(bool first\)",
    "PlayLayer::removeAllCheckpoints": r"virtual void removeAllCheckpoints\(\)",
    "PlayLayer::prepareMusic": r"void prepareMusic\(bool dontWait\)",
    "PlayLayer::startGame": r"void startGame\(\)",
    "PlayLayer::startGameDelayed": r"void startGameDelayed\(\)",
    "PlayLayer::startMusic": r"void startMusic\(\)",
    "PlayLayer::onQuit": r"void onQuit\(\)",
    "GJBaseGameLayer::handleButton": r"void handleButton\(bool down, int button, bool isPlayer1\)",
    "GJBaseGameLayer::update": r"virtual void update\(float dt\)",
    "LevelTools::verifyLevelIntegrity": r"static bool verifyLevelIntegrity\(gd::string str, int id\)",
    "GJGameLevel::create": r"static GJGameLevel\* create\(\)",
    "GJGameLevel::copyLevelInfo": r"void copyLevelInfo\(GJGameLevel\* level\)",
    "m_startPosObject": r"StartPosObject\* m_startPosObject;",
    "m_isPracticeMode": r"bool m_isPracticeMode;",
    "m_practiceMusicSync": r"bool m_practiceMusicSync;",
    "m_isTestMode": r"bool m_isTestMode;",
    "m_started": r"bool m_started;",
    "m_audioPaused": r"bool m_audioPaused;",
    "m_isSilent": r"bool m_isSilent;",
    "m_musicPrepared": r"bool m_musicPrepared;",
    "m_timePlayed": r"double m_timePlayed;",
    "m_gameState": r"GJGameState m_gameState;",
    "GJGameState::m_levelTime": r"double m_levelTime;",
    "PlayLayer::m_isPaused": r"bool m_isPaused;",
    "GJBaseGameLayer::m_objects": r"cocos2d::CCArray\* m_objects;",
    "GJBaseGameLayer::m_uiLayer": r"UILayer\* m_uiLayer;",
    "GJBaseGameLayer::m_ignoreDamage": r"bool m_ignoreDamage;",
    "StartPosObject class": r"class StartPosObject\s*:\s*EffectGameObject",
}.items():
    require(gd, label, pattern)

for label, pattern in {
    "CCEGLView::swapBuffers": r"virtual void swapBuffers\(\)",
    "CCDirector::setViewport": r"void setViewport\(\)",
    "CCDirector::setProjection": r"void setProjection\(cocos2d::ccDirectorProjection\)",
    "CCDirector::getWinSizeInPixels": r"cocos2d::CCSize getWinSizeInPixels\(\)",
    "CCDirector::getNotificationNode": r"cocos2d::CCNode\* getNotificationNode\(\)",
    "CCNode::unscheduleUpdate": r"void unscheduleUpdate\(\)",
}.items():
    require(cocos, label, pattern)

print("Official Geode bindings main/2.2081 contain the key declarations used by the mod")
