#pragma once

#include <Geode/Geode.hpp>

namespace cocos2d {
    class CCGLProgram;
}

// HackMega and several other mods draw their global interface after the
// ordinary Cocos scene, immediately before swapBuffers. The local Layout pass
// replaces that framebuffer, so preserve only pixels changed by presentation
// overlays and replay them over Layout on the GPU.
class PresentationOverlay final {
public:
    static PresentationOverlay& get();

    void setGameplayActive(bool active);
    void discardFrame();
    void captureSceneBaseline();
    bool capturePresentedFrame();
    void replayDifference();

private:
    PresentationOverlay() = default;
    PresentationOverlay(PresentationOverlay const&) = delete;
    PresentationOverlay& operator=(PresentationOverlay const&) = delete;

#ifdef GEODE_IS_WINDOWS
    bool captureTexture(bool presented);
    bool ensureTextures(int width, int height);
    bool ensureProgram();

    cocos2d::CCGLProgram* m_program = nullptr;
    unsigned int m_sceneTexture = 0;
    unsigned int m_presentedTexture = 0;
    int m_sceneUniform = -1;
    int m_presentedUniform = -1;
    int m_width = 0;
    int m_height = 0;
    int m_viewX = 0;
    int m_viewY = 0;
    bool m_baselineReady = false;
    bool m_presentedReady = false;
    bool m_statusLogged = false;
    bool m_failureLogged = false;
    bool m_gameplayActive = false;
#endif
};
