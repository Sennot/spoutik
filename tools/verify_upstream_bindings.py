#!/usr/bin/env python3
"""Verify exact official Geode 2.2081 declarations used by the current render path."""
from __future__ import annotations
import re
import urllib.request

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
    "PlayLayer::init": r"bool init\(GJGameLevel\* level, bool useReplay, bool dontCreateObjects\)",
    "PlayLayer::addObject": r"void addObject\(GameObject\* object\)",
    "PlayLayer::onQuit": r"void onQuit\(\)",
    "ShaderLayer::visit": r"class ShaderLayer[\s\S]*?virtual void visit\(\)",
    "GJBaseGameLayer::m_objects": r"cocos2d::CCArray\* m_objects;",
    "GJBaseGameLayer::m_visibleObjects": r"gd::vector<GameObject\*> m_visibleObjects;",
    "GJBaseGameLayer::m_visibleObjects2": r"gd::vector<GameObject\*> m_visibleObjects2;",
    "GJBaseGameLayer::m_inShaderParent": r"cocos2d::CCNode\* m_inShaderParent;",
    "GJBaseGameLayer::m_background": r"cocos2d::CCSprite\* m_background;",
    "GJBaseGameLayer::m_groundLayer": r"GJGroundLayer\* m_groundLayer;",
    "GJBaseGameLayer::m_groundLayer2": r"GJGroundLayer\* m_groundLayer2;",
    "GJBaseGameLayer::m_middleground": r"GJMGLayer\* m_middleground;",
    "GJGroundLayer::m_ground1Sprite": r"class GJGroundLayer[\s\S]*?cocos2d::CCSprite\* m_ground1Sprite;",
    "GJGroundLayer::m_ground2Sprite": r"class GJGroundLayer[\s\S]*?cocos2d::CCSprite\* m_ground2Sprite;",
    "GJGroundLayer::m_lineSprite": r"class GJGroundLayer[\s\S]*?cocos2d::CCSprite\* m_lineSprite;",
    "GJMGLayer::m_ground1Sprite": r"class GJMGLayer[\s\S]*?cocos2d::CCSprite\* m_ground1Sprite;",
    "GJMGLayer::m_ground2Sprite": r"class GJMGLayer[\s\S]*?cocos2d::CCSprite\* m_ground2Sprite;",
    "GameObject::m_objectID": r"int m_objectID;",
    "GameObject::m_activeMainColorID": r"int m_activeMainColorID;",
    "GameObject::m_activeDetailColorID": r"int m_activeDetailColorID;",
    "GameObject::m_baseUsesHSV": r"bool m_baseUsesHSV;",
    "GameObject::m_detailUsesHSV": r"bool m_detailUsesHSV;",
    "GameObject::m_glowSprite": r"cocos2d::CCSprite\* m_glowSprite;",
    "GameObject::m_particle": r"cocos2d::CCParticleSystemQuad\* m_particle;",
    "GameObject::m_hasNoGlow": r"bool m_hasNoGlow;",
    "GameObject::m_isHide": r"bool m_isHide;",
    "GameObject::m_colorSprite": r"cocos2d::CCSprite\* m_colorSprite;",
    "GameObject::m_isObjectBlack": r"bool m_isObjectBlack;",
    "GameObject::m_isColorSpriteBlack": r"bool m_isColorSpriteBlack;",
    "GameObject::setObjectColor": r"virtual void setObjectColor\(cocos2d::ccColor3B const& color\)",
    "GameObject::setChildColor": r"virtual void setChildColor\(cocos2d::ccColor3B const& color\)",
    "ShaderLayer::m_gameLayer": r"class ShaderLayer[\s\S]*?GJBaseGameLayer\* m_gameLayer;",
}.items():
    require(gd, label, pattern)

for label, pattern in {
    "CCEGLView::swapBuffers": r"virtual void swapBuffers\(\)",
    "CCDirector::setViewport": r"void setViewport\(\)",
    "CCDirector::setProjection": r"void setProjection\(cocos2d::ccDirectorProjection\)",
    "CCDirector::getNotificationNode": r"cocos2d::CCNode\* getNotificationNode\(\)",
}.items():
    require(cocos, label, pattern)

print("Official Geode bindings main/2.2081 contain exact-map and full-palette render declarations")
