#!/usr/bin/env python3
"""Verify official Geode 2.2081 declarations used by the bridge exporter."""
from __future__ import annotations
import re
import urllib.request

UA = {"User-Agent": "layout-companion-binding-check/1"}
BASE = "https://raw.githubusercontent.com/geode-sdk/bindings/main/bindings/2.2081/"


def fetch(name: str) -> str:
    request = urllib.request.Request(BASE + name, headers=UA)
    with urllib.request.urlopen(request, timeout=60) as response:
        return response.read().decode("utf-8")


def require(text: str, label: str, pattern: str) -> None:
    if not re.search(pattern, text, re.M):
        raise SystemExit(f"Official Geode 2.2081 binding mismatch: {label}")


gd = fetch("GeometryDash.bro")
cocos = fetch("Cocos2d.bro")

for label, pattern in {
    "PlayLayer::init": r"bool init\(GJGameLevel\* level, bool useReplay, bool dontCreateObjects\)",
    "PlayLayer::addObject": r"void addObject\(GameObject\* object\)",
    "PlayLayer::onQuit": r"void onQuit\(\)",
    "object layer": r"cocos2d::CCLayer\* m_objectLayer;",
    "camera compact objects": r"gd::vector<GameObject\*> m_calcNonEffectObjects;",
    "camera compact size": r"int m_calcNonEffectObjectsSize;",
    "visible object count": r"int m_visibleObjectsCount;",
    "visible object 2 count": r"int m_visibleObjects2Count;",
    "section grid": r"gd::vector<gd::vector<gd::vector<GameObject\*>\*>\*> m_sections;",
    "non-effect grid": r"gd::vector<gd::vector<gd::vector<GameObject\*>\*>\*> m_nonEffectObjects;",
    "player one": r"PlayerObject\* m_player1;",
    "player two": r"PlayerObject\* m_player2;",
    "object id": r"int m_objectID;",
    "object detail sprite": r"cocos2d::CCSprite\* m_colorSprite;",
    "object black flag": r"bool m_isObjectBlack;",
    "detail black flag": r"bool m_isColorSpriteBlack;",
}.items():
    require(gd, label, pattern)

for label, pattern in {
    "CCDirector::drawScene": r"void drawScene\(\)",
    "CCDirector::getWinSize": r"cocos2d::CCSize getWinSize\(\)",
    "CCNode::convertToNodeSpace": r"cocos2d::CCPoint convertToNodeSpace\(cocos2d::CCPoint const&\)",
    "CCNode::convertToWorldSpace": r"cocos2d::CCPoint convertToWorldSpace\(cocos2d::CCPoint const&\)",
    "CCNode::getContentSize": r"cocos2d::CCSize getContentSize\(\)",
    "CCNode::getZOrder": r"int getZOrder\(\)",
}.items():
    require(cocos, label, pattern)

print("Official Geode bindings contain all read-only companion bridge declarations")
