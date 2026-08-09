#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/ShaderLayer.hpp>
#include <Geode/modify/CCEGLView.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/modify/CCNode.hpp>
#include <Geode/modify/GameObject.hpp>
#include "FrameCompositor.hpp"
#include "LayoutMirror.hpp"
#include "SpoutSender.hpp"

using namespace geode::prelude;

namespace {
    constexpr unsigned kStableDrawsBeforeLayout = 3;

    struct PresentationGateState {
        cocos2d::CCScene* scene = nullptr;
        PlayLayer* layer = nullptr;
        unsigned stableDraws = 0;
    };

    PresentationGateState s_presentationGate;

    void resetPresentationGate() {
        s_presentationGate = {};
    }

    bool advancePresentationGate(cocos2d::CCDirector* director, PlayLayer* real) {
        if (!LayoutMirror::get().isStableGameplayScene(director, real)) {
            resetPresentationGate();
            return false;
        }

        auto* scene = director->getRunningScene();
        if (s_presentationGate.scene != scene || s_presentationGate.layer != real) {
            s_presentationGate = { scene, real, 1 };
            return false;
        }

        if (s_presentationGate.stableDraws < kStableDrawsBeforeLayout) {
            ++s_presentationGate.stableDraws;
        }
        return s_presentationGate.stableDraws >= kStableDrawsBeforeLayout;
    }

}

// This mod deliberately has ONE gameplay world only.
// The decorated authoritative PlayLayer is rendered normally, copied on-GPU,
// and then rendered with the XDBot-derived visual mask into a private FBO. The
// local backbuffer receives that finished Layout texture before late overlays;
// swapBuffers only composes/sends offscreen and never visits the scene.

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
        resetPresentationGate();
        FrameCompositor::get().invalidate();
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
        static thread_local bool drawingRawLayer = false;
        if (LayoutMirror::get().isRenderingLayout()) {
            if (drawingRawLayer) return;
            // m_inShaderParent owns ShaderLayer's cached output. Re-visiting it
            // can recurse into or redraw the black/decorated render texture.
            // Draw GD's raw in-shader object layer instead; below/above shader
            // ranges are already visited normally by the authoritative scene.
            if (m_gameLayer && m_gameLayer->m_inShaderObjectLayer) {
                auto* rawLayer = m_gameLayer->m_inShaderObjectLayer;
                auto const wasVisible = rawLayer->isVisible();
                if (!wasVisible) rawLayer->cocos2d::CCNode::setVisible(true);
                drawingRawLayer = true;
                rawLayer->visit();
                drawingRawLayer = false;
                if (!wasVisible) rawLayer->cocos2d::CCNode::setVisible(false);
            }
            return;
        }
        ShaderLayer::visit();
    }
};

class $modify(SpoutLayoutGameObject, GameObject) {
    static void onModify(auto& self) {
        // Visibility is deliberately not hooked: GD batch/camera culling can
        // bypass that virtual function, and several gameplay mods also own it.
        // Opacity updates are semantic trigger state and remain mirrored.
        if (!self.setHookPriorityPre("GameObject::setOpacity", Priority::First)) {
            log::warn("Could not set GameObject::setOpacity tracking priority to First");
        }
    }

    void setOpacity(unsigned char opacity) {
        GameObject::setOpacity(opacity);
        LayoutMirror::get().observeOpacity(this, opacity);
    }
};

class $modify(SpoutLayoutNode, cocos2d::CCNode) {
    static void onModify(auto& self) {
        if (!self.setHookPriorityPre("cocos2d::CCNode::visit", Priority::Last)) {
            log::warn("Could not set CCNode::visit layout mask priority to Last");
        }
    }

    void visit() {
        auto& layout = LayoutMirror::get();
        auto const action = layout.beginNodeVisit(this);
        if (action == LayoutMirror::NodeVisitAction::Skip) return;

        cocos2d::CCNode::visit();
        if (action == LayoutMirror::NodeVisitAction::Styled) {
            layout.endNodeVisit(this);
        }
    }
};

class $modify(SpoutLayoutDirector, cocos2d::CCDirector) {
    static void onModify(auto& self) {
        // HackMega draws its global CoreDirector UI after the ordinary scene.
        // Running inside its drawScene hook prepares and installs Layout before
        // that post-draw UI; the final presentation is captured at swapBuffers.
        if (!self.setHookPriorityAfterPre(
            "cocos2d::CCDirector::drawScene", "absolllute.hackmega"
        )) {
            log::warn("Could not order the presentation baseline inside absolllute.hackmega");
            if (!self.setHookPriorityPre("cocos2d::CCDirector::drawScene", Priority::Last)) {
                log::warn("Could not set CCDirector::drawScene baseline fallback priority");
            }
        }
        if (!self.setHookPriorityPre(
            "cocos2d::CCDirector::willSwitchToScene", Priority::First
        )) {
            log::warn("Could not set CCDirector::willSwitchToScene transition guard priority");
        }
    }

    void willSwitchToScene(cocos2d::CCScene* scene) {
        resetPresentationGate();
        FrameCompositor::get().invalidate();
        CCDirector::willSwitchToScene(scene);
    }

    void drawScene() {
        CCDirector::drawScene();
        auto* real = PlayLayer::get();
        auto const stable = advancePresentationGate(this, real);
        auto& compositor = FrameCompositor::get();
        if (stable) compositor.prepareLocalFrame(this, real);
        else compositor.invalidate();
    }
};

class $modify(SpoutLayoutEGLView, cocos2d::CCEGLView) {
    static void onModify(auto& self) {
        // A named relative priority makes the final capture run after HackMega's
        // presentation hook even if HackMega changes its raw priority number.
        if (!self.setHookPriorityAfterPre(
            "cocos2d::CCEGLView::swapBuffers", "absolllute.hackmega"
        )) {
            log::warn("Could not order Spout capture after absolllute.hackmega; using Priority::Last");
            if (!self.setHookPriorityPre("cocos2d::CCEGLView::swapBuffers", Priority::Last)) {
                log::warn("Could not set CCEGLView::swapBuffers fallback priority to Last");
            }
        }
    }

    void swapBuffers() {
        // No clear, scene visit or default-framebuffer write is permitted here.
        // The prepared frame is matched to the current scene/layer generation;
        // transitions and stale frames fail closed to the ordinary GD image.
        auto* director = cocos2d::CCDirector::get();
        auto* real = PlayLayer::get();
        if (!FrameCompositor::get().sendPreparedSpoutFrame(director, real)) {
            SpoutSender::get().sendDefaultFramebuffer();
        }

        CCEGLView::swapBuffers();
    }
};
