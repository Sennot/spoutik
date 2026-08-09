#include <Geode/Geode.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include "CompanionBridge.hpp"
#include "LayoutMirror.hpp"

using namespace geode::prelude;

// Geometry Dash remains a completely ordinary decorated game. This module
// only classifies its authoritative objects and publishes screen-space quads
// to a separate process; it performs no OpenGL calls and no second scene visit.
class $modify(LayoutCompanionPlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        if (!self.setHookPriorityPre("PlayLayer::init", Priority::VeryLate)) {
            log::warn("Could not set PlayLayer::init companion-map priority");
        }
        if (!self.setHookPriorityPre("PlayLayer::addObject", Priority::VeryLate)) {
            log::warn("Could not set PlayLayer::addObject companion-map priority");
        }
        if (!self.setHookPriorityPre("PlayLayer::onQuit", Priority::Last)) {
            log::warn("Could not set PlayLayer::onQuit companion cleanup priority");
        }
    }

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        auto& layout = LayoutMirror::get();
        layout.prepareFor(this, level);
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            layout.destroyFor(this);
            return false;
        }
        layout.finishFor(this);
        return true;
    }

    void addObject(GameObject* object) {
        PlayLayer::addObject(object);
        LayoutMirror::get().observeObject(this, object);
    }

    void onQuit() {
        CompanionBridge::get().suspend();
        LayoutMirror::get().destroyFor(this);
        PlayLayer::onQuit();
    }
};

class $modify(LayoutCompanionDirector, cocos2d::CCDirector) {
    static void onModify(auto& self) {
        if (!self.setHookPriorityPre("cocos2d::CCDirector::drawScene", Priority::Last)) {
            log::warn("Could not set CCDirector::drawScene companion publish priority");
        }
        if (!self.setHookPriorityPre(
            "cocos2d::CCDirector::willSwitchToScene", Priority::First
        )) {
            log::warn("Could not set CCDirector::willSwitchToScene companion guard priority");
        }
    }

    void willSwitchToScene(cocos2d::CCScene* scene) {
        CompanionBridge::get().suspend();
        CCDirector::willSwitchToScene(scene);
    }

    void drawScene() {
        CCDirector::drawScene();
        CompanionBridge::get().publish(this, PlayLayer::get());
    }
};
