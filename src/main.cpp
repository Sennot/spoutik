#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/ShaderLayer.hpp>
#include <Geode/modify/CCEGLView.hpp>
#include <Geode/modify/CCDirector.hpp>
#include <Geode/modify/CCNode.hpp>
#include <Geode/modify/GameObject.hpp>
#include "LayoutMirror.hpp"
#include "PresentationOverlay.hpp"
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

    bool isPresentationGateOpen(cocos2d::CCDirector* director, PlayLayer* real) {
        return director && real &&
            s_presentationGate.scene == director->getRunningScene() &&
            s_presentationGate.layer == real &&
            s_presentationGate.stableDraws >= kStableDrawsBeforeLayout &&
            LayoutMirror::get().isStableGameplayScene(director, real);
    }
}

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
        resetPresentationGate();
        PresentationOverlay::get().setGameplayActive(false);
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
        // Running inside its drawScene hook captures the pristine decorated
        // scene first; the final presentation is captured at swapBuffers.
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
        PresentationOverlay::get().setGameplayActive(false);
        CCDirector::willSwitchToScene(scene);
    }

    void drawScene() {
        CCDirector::drawScene();
        auto* real = PlayLayer::get();
        auto const stable = advancePresentationGate(this, real);
        auto& presentation = PresentationOverlay::get();
        presentation.setGameplayActive(stable);
        if (stable) presentation.captureSceneBaseline();
    }
};

class $modify(SpoutLayoutEGLView, cocos2d::CCEGLView) {
    static void onModify(auto& self) {
        // HackMega renders its overlay in the same presentation hook. A named
        // relative priority is stable even if HackMega changes its raw number.
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
        // The first render is untouched Geometry Dash, including all menus,
        // pause UI, HUD/mod overlays, decoration, camera state and shaders.
        SpoutSender::get().sendDefaultFramebuffer();

        // Replace only the local backbuffer while gameplay is active. The real
        // scene remains authoritative and is restored before the actual swap.
        auto* director = cocos2d::CCDirector::get();
        auto* real = PlayLayer::get();
        auto& layout = LayoutMirror::get();
        auto& presentation = PresentationOverlay::get();
        auto const stable = isPresentationGateOpen(director, real);
        presentation.setGameplayActive(stable);
        if (stable) {
            // Save the fully presented frame too. After the Layout redraw, a
            // GPU difference pass restores only HackMega/late-overlay pixels;
            // OBS keeps the untouched full frame captured above.
            auto const replayPresentation = presentation.capturePresentedFrame();
            auto const rendered = layout.renderPlayerView(director, real);
            if (rendered && replayPresentation) presentation.replayDifference();
            else presentation.discardFrame();
        }

        CCEGLView::swapBuffers();
    }
};
