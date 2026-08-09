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
    "GJBaseGameLayer::m_inShaderObjectLayer": r"cocos2d::CCLayer\* m_inShaderObjectLayer;",
    "GJBaseGameLayer::m_objectLayer": r"cocos2d::CCLayer\* m_objectLayer;",
    "GJBaseGameLayer::m_sections": r"gd::vector<gd::vector<gd::vector<GameObject\*>\*>\*> m_sections;",
    "GJBaseGameLayer::m_nonEffectObjects": r"gd::vector<gd::vector<gd::vector<GameObject\*>\*>\*> m_nonEffectObjects;",
    "GJBaseGameLayer::m_calcNonEffectObjects": r"gd::vector<GameObject\*> m_calcNonEffectObjects;",
    "GJBaseGameLayer::m_calcNonEffectObjectsSize": r"int m_calcNonEffectObjectsSize;",
    "GJBaseGameLayer::m_visibleObjectsCount": r"int m_visibleObjectsCount;",
    "GJBaseGameLayer::m_visibleObjects2Count": r"int m_visibleObjects2Count;",
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
    "GameObject::setVisible": r"class GameObject[\s\S]*?virtual void setVisible\(bool visible\)",
    "GameObject::setOpacity": r"class GameObject[\s\S]*?virtual void setOpacity\(unsigned char opacity\)",
    "GameObject::m_glowSprite": r"cocos2d::CCSprite\* m_glowSprite;",
    "GameObject::m_particle": r"cocos2d::CCParticleSystemQuad\* m_particle;",
    "GameObject::m_colorSprite": r"cocos2d::CCSprite\* m_colorSprite;",
    "GameObject::m_isObjectBlack": r"bool m_isObjectBlack;",
    "GameObject::m_isColorSpriteBlack": r"bool m_isColorSpriteBlack;",
    "GameObject::m_isGroupDisabled": r"bool m_isGroupDisabled;",
    "GameObject::m_isGroupDisabledTemp": r"bool m_isGroupDisabledTemp;",
    "GameObject::m_isDisabled2": r"bool m_isDisabled2;",
    "GameObject::m_isDisabled": r"bool m_isDisabled;",
    "ShaderLayer::m_gameLayer": r"class ShaderLayer[\s\S]*?GJBaseGameLayer\* m_gameLayer;",
}.items():
    require(gd, label, pattern)

for label, pattern in {
    "CCNode::visit": r"class cocos2d::CCNode[\s\S]*?virtual void visit\(\)",
    "CCSprite::getBatchNode": r"class cocos2d::CCSprite[\s\S]*?getBatchNode\(\)",
    "CCParticleSystem::getBatchNode": r"class cocos2d::CCParticleSystem[\s\S]*?getBatchNode\(\)",
    "CCEGLView::swapBuffers": r"virtual void swapBuffers\(\)",
    "CCDirector::setViewport": r"void setViewport\(\)",
    "CCDirector::setProjection": r"void setProjection\(cocos2d::ccDirectorProjection\)",
    "CCDirector::drawScene": r"void drawScene\(\)",
    "CCDirector::getWinSize": r"cocos2d::CCSize getWinSize\(\)",
    "CCDirector::getNotificationNode": r"cocos2d::CCNode\* getNotificationNode\(\)",
    "CCNode::convertToNodeSpace": r"cocos2d::CCPoint convertToNodeSpace\(cocos2d::CCPoint const&\)",
}.items():
    require(cocos, label, pattern)

print("Official Geode bindings main/2.2081 contain visit-mask and full-palette render declarations")
