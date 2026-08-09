#pragma once

#include <Geode/Geode.hpp>
#include <cstdint>

namespace cocos2d {
    class CCGLProgram;
}

// Owns every auxiliary GPU target used by dual-view presentation. The default
// framebuffer is touched only from CCDirector::drawScene, before late overlays
// render. swapBuffers only captures/composes offscreen and never redraws GD.
class FrameCompositor final {
public:
    static FrameCompositor& get();

    void invalidate();
    bool prepareLocalFrame(cocos2d::CCDirector* director, PlayLayer* real);
    bool sendPreparedSpoutFrame(cocos2d::CCDirector* director, PlayLayer* real);

private:
    FrameCompositor() = default;
    FrameCompositor(FrameCompositor const&) = delete;
    FrameCompositor& operator=(FrameCompositor const&) = delete;

#ifdef GEODE_IS_WINDOWS
    bool ensureTargets(int width, int height);
    bool ensureProgram();
    bool captureDefaultTo(unsigned int texture);
    bool blitLayoutToDefault();
    bool drawFullscreen(
        unsigned int targetFramebuffer,
        int viewportX,
        int viewportY,
        int width,
        int height,
        int mode,
        unsigned int texture0,
        unsigned int texture1 = 0,
        unsigned int texture2 = 0
    );
    void releaseTargets();

    cocos2d::CCGLProgram* m_program = nullptr;
    unsigned int m_decoratedTexture = 0;
    unsigned int m_layoutTexture = 0;
    unsigned int m_presentedTexture = 0;
    unsigned int m_spoutTexture = 0;
    unsigned int m_layoutFramebuffer = 0;
    unsigned int m_spoutFramebuffer = 0;
    unsigned int m_depthStencil = 0;
    int m_modeUniform = -1;
    int m_texture0Uniform = -1;
    int m_texture1Uniform = -1;
    int m_texture2Uniform = -1;
    int m_width = 0;
    int m_height = 0;
    int m_viewX = 0;
    int m_viewY = 0;
    cocos2d::CCScene* m_frameScene = nullptr;
    PlayLayer* m_frameLayer = nullptr;
    std::uint64_t m_generation = 0;
    std::uint64_t m_readyGeneration = 0;
    bool m_frameReady = false;
    bool m_statusLogged = false;
    bool m_failureLogged = false;
    unsigned int m_traceFramesRemaining = 3;
#endif
};
