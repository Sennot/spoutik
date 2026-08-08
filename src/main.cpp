#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/ShaderLayer.hpp>
#include <Geode/modify/CCEGLView.hpp>
#include "LayoutMirror.hpp"
#include "SpoutSender.hpp"

using namespace geode::prelude;

// This mod deliberately has ONE gameplay world only.
// The decorated authoritative PlayLayer is rendered normally and sent to Spout.
// At swap time we temporarily apply the XDBot-derived visual mask, render that
// same scene into the local backbuffer, then restore every touched visual field.
// No second update/reset/checkpoint/input/music path exists.

class $modify(SpoutLayoutPlayLayer, PlayLayer) {
    static void onModify(auto& self) {
        // Prepare the XDBot record plan immediately before the authoritative
        // init, then bind records as that same PlayLayer adds its real objects.
        if (!self.setHookPriorityPre("PlayLayer::init", Priority::VeryLate)) {
            log::warn("Could not set PlayLayer::init layout-map priority to VeryLate");
        }
        if (!self.setHookPriorityPre("PlayLayer::addObject", Priority::VeryLate)) {
            log::warn("Could not set PlayLayer::addObject layout-map priority to VeryLate");
        }
        if (!self.setHookPriorityPre("PlayLayer::onQuit", Priority::Last)) {
            log::warn("Could not set PlayLayer::onQuit cleanup priority to Last");
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
        LayoutMirror::get().destroyFor(this);
        PlayLayer::onQuit();
    }
};

class $modify(SpoutLayoutShaderLayer, ShaderLayer) {
    static void onModify(auto& self) {
        if (!self.setHookPriorityPre("ShaderLayer::visit", Priority::Last)) {
            log::warn("Could not set ShaderLayer::visit layout bypass priority to Last");
        }
    }

    void visit() {
        if (LayoutMirror::get().isRenderingLayout()) {
            // Keep the layer's children/order/camera transform, but bypass the
            // render-texture shader pass for the local Layout view only.
            return cocos2d::CCLayer::visit();
        }
        ShaderLayer::visit();
    }
};

class $modify(SpoutLayoutEGLView, cocos2d::CCEGLView) {
    static void onModify(auto& self) {
        if (!self.setHookPriorityPre("cocos2d::CCEGLView::swapBuffers", Priority::Last)) {
            log::warn("Could not set CCEGLView::swapBuffers hook to Priority::Last");
        }
    }

    void swapBuffers() {
        // The first render is untouched Geometry Dash, including all menus,
        // pause UI, HUD/mod overlays, decoration, camera state and shaders.
        SpoutSender::get().sendDefaultFramebuffer();

        // Replace only the local backbuffer while gameplay is active. The real
        // scene remains authoritative and is restored before the actual swap.
        auto* director = cocos2d::CCDirector::get();
        auto* real = PlayLayer::get();
        if (director && real) LayoutMirror::get().renderPlayerView(director, real);

        CCEGLView::swapBuffers();
    }
};
