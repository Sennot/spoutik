#include "FrameCompositor.hpp"
#include "LayoutMirror.hpp"
#include "SpoutSender.hpp"

#ifdef GEODE_IS_WINDOWS
#include <Geode/cocos/platform/CCGL.h>
#include <Geode/cocos/shaders/CCGLProgram.h>
#include <Geode/cocos/shaders/ccGLStateCache.h>
#endif

using namespace geode::prelude;

FrameCompositor& FrameCompositor::get() {
    static FrameCompositor instance;
    return instance;
}

void FrameCompositor::invalidate() {
#ifdef GEODE_IS_WINDOWS
    m_frameReady = false;
    m_readyGeneration = 0;
    m_frameScene = nullptr;
    m_frameLayer = nullptr;
#endif
}

#ifdef GEODE_IS_WINDOWS
namespace {
    struct VertexAttribState {
        GLint enabled = GL_FALSE;
        GLint size = 4;
        GLint type = GL_FLOAT;
        GLint normalized = GL_FALSE;
        GLint stride = 0;
        GLint buffer = 0;
        void* pointer = nullptr;
    };

    VertexAttribState captureAttrib(GLuint index) {
        VertexAttribState state;
        glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &state.enabled);
        glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_SIZE, &state.size);
        glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_TYPE, &state.type);
        glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_NORMALIZED, &state.normalized);
        glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &state.stride);
        glGetVertexAttribiv(index, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &state.buffer);
        glGetVertexAttribPointerv(index, GL_VERTEX_ATTRIB_ARRAY_POINTER, &state.pointer);
        return state;
    }

    void restoreAttrib(GLuint index, VertexAttribState const& state) {
        glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(state.buffer));
        glVertexAttribPointer(
            index,
            state.size,
            static_cast<GLenum>(state.type),
            static_cast<GLboolean>(state.normalized),
            state.stride,
            state.pointer
        );
        if (state.enabled) glEnableVertexAttribArray(index);
        else glDisableVertexAttribArray(index);
    }
}

void FrameCompositor::releaseTargets() {
    if (m_spoutFramebuffer) glDeleteFramebuffers(1, &m_spoutFramebuffer);
    GLuint const textures[] {
        m_decoratedTexture,
        m_layoutTexture,
        m_presentedTexture,
        m_spoutTexture,
    };
    glDeleteTextures(4, textures);

    m_decoratedTexture = 0;
    m_layoutTexture = 0;
    m_presentedTexture = 0;
    m_spoutTexture = 0;
    m_spoutFramebuffer = 0;
    m_width = 0;
    m_height = 0;
    invalidate();
}

bool FrameCompositor::ensureTargets(int width, int height) {
    if (width <= 0 || height <= 0) return false;
    if (m_decoratedTexture && m_layoutTexture && m_presentedTexture &&
        m_spoutTexture && m_spoutFramebuffer &&
        m_width == width && m_height == height) {
        return true;
    }

    GLint oldFramebuffer = 0;
    GLint oldActive = GL_TEXTURE0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFramebuffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActive);
    glActiveTexture(GL_TEXTURE0);
    GLint oldTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);

    auto const oldFramebufferWasOurs =
        static_cast<GLuint>(oldFramebuffer) == m_spoutFramebuffer;
    auto const oldTextureWasOurs =
        static_cast<GLuint>(oldTexture) == m_decoratedTexture ||
        static_cast<GLuint>(oldTexture) == m_layoutTexture ||
        static_cast<GLuint>(oldTexture) == m_presentedTexture ||
        static_cast<GLuint>(oldTexture) == m_spoutTexture;

    releaseTargets();

    GLuint textures[4] {};
    glGenTextures(4, textures);
    m_decoratedTexture = textures[0];
    m_layoutTexture = textures[1];
    m_presentedTexture = textures[2];
    m_spoutTexture = textures[3];
    for (auto texture : textures) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            width,
            height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            nullptr
        );
    }

    glGenFramebuffers(1, &m_spoutFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, m_spoutFramebuffer);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_spoutTexture, 0
    );
    auto const spoutComplete = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    glBindFramebuffer(
        GL_FRAMEBUFFER,
        oldFramebufferWasOurs ? 0 : static_cast<GLuint>(oldFramebuffer)
    );
    glBindTexture(
        GL_TEXTURE_2D,
        oldTextureWasOurs ? 0 : static_cast<GLuint>(oldTexture)
    );
    glActiveTexture(static_cast<GLenum>(oldActive));
    ccGLInvalidateStateCache();

    if (!spoutComplete) {
        if (!m_failureLogged) {
            m_failureLogged = true;
            log::error("Dual-frame Spout FBO creation failed");
        }
        releaseTargets();
        return false;
    }

    m_width = width;
    m_height = height;
    return true;
}

bool FrameCompositor::ensureProgram() {
    if (m_program) return true;

    static constexpr auto kVertexShader = R"glsl(
        attribute vec2 a_position;
        attribute vec2 a_texCoord;
        varying vec2 v_texCoord;
        void main() {
            gl_Position = vec4(a_position, 0.0, 1.0);
            v_texCoord = a_texCoord;
        }
    )glsl";
    static constexpr auto kFragmentShader = R"glsl(
        #ifdef GL_ES
        precision mediump float;
        #endif
        uniform int u_mode;
        uniform sampler2D u_texture0;
        uniform sampler2D u_texture1;
        uniform sampler2D u_texture2;
        varying vec2 v_texCoord;
        void main() {
            vec4 firstPixel = texture2D(u_texture0, v_texCoord);
            if (u_mode == 0) {
                gl_FragColor = firstPixel;
                return;
            }

            vec4 baselinePixel = texture2D(u_texture1, v_texCoord);
            vec4 presentedPixel = texture2D(u_texture2, v_texCoord);
            vec3 delta = abs(presentedPixel.rgb - baselinePixel.rgb);
            if (max(delta.r, max(delta.g, delta.b)) >= 0.0039) {
                gl_FragColor = vec4(presentedPixel.rgb, 1.0);
            }
            else {
                gl_FragColor = vec4(firstPixel.rgb, 1.0);
            }
        }
    )glsl";

    auto* program = new CCGLProgram();
    if (!program->initWithVertexShaderByteArray(kVertexShader, kFragmentShader)) {
        program->release();
        return false;
    }
    program->addAttribute(kCCAttributeNamePosition, kCCVertexAttrib_Position);
    program->addAttribute(kCCAttributeNameTexCoord, kCCVertexAttrib_TexCoords);
    if (!program->link()) {
        log::error("Dual-frame compositor shader link failed");
        program->release();
        return false;
    }
    program->updateUniforms();
    m_modeUniform = program->getUniformLocationForName("u_mode");
    m_texture0Uniform = program->getUniformLocationForName("u_texture0");
    m_texture1Uniform = program->getUniformLocationForName("u_texture1");
    m_texture2Uniform = program->getUniformLocationForName("u_texture2");
    if (m_modeUniform < 0 || m_texture0Uniform < 0 ||
        m_texture1Uniform < 0 || m_texture2Uniform < 0) {
        program->release();
        return false;
    }
    m_program = program;
    return true;
}

bool FrameCompositor::captureDefaultTo(unsigned int texture) {
    if (!texture || m_width <= 0 || m_height <= 0) return false;

    GLint framebuffer = 0;
    GLint viewport[4] {};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (framebuffer != 0 || viewport[0] != m_viewX || viewport[1] != m_viewY ||
        viewport[2] != m_width || viewport[3] != m_height) {
        return false;
    }

    GLint oldActive = GL_TEXTURE0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActive);
    glActiveTexture(GL_TEXTURE0);
    GLint oldTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glCopyTexSubImage2D(
        GL_TEXTURE_2D, 0, 0, 0, m_viewX, m_viewY, m_width, m_height
    );
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(oldTexture));
    glActiveTexture(static_cast<GLenum>(oldActive));
    return true;
}

bool FrameCompositor::drawFullscreen(
    unsigned int targetFramebuffer,
    int viewportX,
    int viewportY,
    int width,
    int height,
    int mode,
    unsigned int texture0,
    unsigned int texture1,
    unsigned int texture2
) {
    if (!ensureProgram() || !texture0 || width <= 0 || height <= 0) return false;

    GLint oldFramebuffer = 0;
    GLint oldProgram = 0;
    GLint oldActive = GL_TEXTURE0;
    GLint oldArrayBuffer = 0;
    GLint oldViewport[4] {};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFramebuffer);
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActive);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &oldArrayBuffer);
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    GLint oldTextures[3] {};
    for (int unit = 0; unit < 3; ++unit) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTextures[unit]);
    }

    auto const blend = glIsEnabled(GL_BLEND);
    auto const depth = glIsEnabled(GL_DEPTH_TEST);
    auto const stencil = glIsEnabled(GL_STENCIL_TEST);
    auto const scissor = glIsEnabled(GL_SCISSOR_TEST);
    auto const cull = glIsEnabled(GL_CULL_FACE);
    GLboolean colorMask[4] {};
    glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);
    auto const oldPosition = captureAttrib(kCCVertexAttrib_Position);
    auto const oldTexCoords = captureAttrib(kCCVertexAttrib_TexCoords);

    static constexpr GLfloat kPositions[] {
        -1.f, -1.f,
         1.f, -1.f,
        -1.f,  1.f,
         1.f,  1.f,
    };
    static constexpr GLfloat kTextureCoordinates[] {
        0.f, 0.f,
        1.f, 0.f,
        0.f, 1.f,
        1.f, 1.f,
    };

    glBindFramebuffer(GL_FRAMEBUFFER, targetFramebuffer);
    glViewport(viewportX, viewportY, width, height);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    m_program->use();
    glUniform1i(m_modeUniform, mode);
    GLuint const textures[] { texture0, texture1 ? texture1 : texture0, texture2 ? texture2 : texture0 };
    GLint const uniforms[] { m_texture0Uniform, m_texture1Uniform, m_texture2Uniform };
    for (int unit = 0; unit < 3; ++unit) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, textures[unit]);
        glUniform1i(uniforms[unit], unit);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableVertexAttribArray(kCCVertexAttrib_Position);
    glEnableVertexAttribArray(kCCVertexAttrib_TexCoords);
    glVertexAttribPointer(kCCVertexAttrib_Position, 2, GL_FLOAT, GL_FALSE, 0, kPositions);
    glVertexAttribPointer(kCCVertexAttrib_TexCoords, 2, GL_FLOAT, GL_FALSE, 0, kTextureCoordinates);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    restoreAttrib(kCCVertexAttrib_Position, oldPosition);
    restoreAttrib(kCCVertexAttrib_TexCoords, oldTexCoords);
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(oldArrayBuffer));
    for (int unit = 2; unit >= 0; --unit) {
        glActiveTexture(GL_TEXTURE0 + unit);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(oldTextures[unit]));
    }
    glActiveTexture(static_cast<GLenum>(oldActive));
    glUseProgram(static_cast<GLuint>(oldProgram));
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(oldFramebuffer));
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
    if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (stencil) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
    if (scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    if (cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    ccGLInvalidateStateCache();
    return true;
}
#endif

bool FrameCompositor::prepareLocalFrame(CCDirector* director, PlayLayer* real) {
#ifndef GEODE_IS_WINDOWS
    (void)director;
    (void)real;
    return false;
#else
    invalidate();
    if (!LayoutMirror::get().isStableGameplayScene(director, real)) return false;

    GLint framebuffer = 0;
    GLint viewport[4] {};
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    glGetIntegerv(GL_VIEWPORT, viewport);
    if (framebuffer != 0 || viewport[2] <= 0 || viewport[3] <= 0) return false;

    if (!ensureTargets(viewport[2], viewport[3])) return false;
    m_viewX = viewport[0];
    m_viewY = viewport[1];
    ++m_generation;
    auto const trace = m_traceFramesRemaining > 0;
    if (trace) {
        log::info(
            "Dual-frame trace {}: targets ready (viewport {},{} {}x{})",
            m_generation,
            m_viewX,
            m_viewY,
            m_width,
            m_height
        );
    }

    if (!captureDefaultTo(m_decoratedTexture)) return false;
    if (trace) log::info("Dual-frame trace {}: decorated capture complete", m_generation);
    if (!LayoutMirror::get().renderPlayerViewToDefaultFramebuffer(director, real)) {
        return false;
    }
    if (trace) log::info("Dual-frame trace {}: direct Layout visit complete", m_generation);
    if (!captureDefaultTo(m_layoutTexture)) return false;
    if (trace) log::info("Dual-frame trace {}: Layout baseline capture complete", m_generation);

    m_frameScene = director->getRunningScene();
    m_frameLayer = real;
    m_readyGeneration = m_generation;
    m_frameReady = true;
    if (!m_statusLogged) {
        m_statusLogged = true;
        log::info(
            "Stable dual-frame compositor active: {}x{}, direct Layout + Spout FBO {}",
            m_width,
            m_height,
            m_spoutFramebuffer
        );
    }
    if (trace) log::info("Dual-frame trace {}: prepared", m_generation);
    return true;
#endif
}

bool FrameCompositor::sendPreparedSpoutFrame(CCDirector* director, PlayLayer* real) {
#ifndef GEODE_IS_WINDOWS
    (void)director;
    (void)real;
    return false;
#else
    if (!m_frameReady || !m_readyGeneration || m_readyGeneration != m_generation) {
        return false;
    }
    if (!director || !real || m_frameScene != director->getRunningScene() ||
        m_frameLayer != real || !LayoutMirror::get().isStableGameplayScene(director, real)) {
        invalidate();
        return false;
    }

    m_frameReady = false;
    auto const trace = m_traceFramesRemaining > 0;
    if (trace) log::info("Dual-frame trace {}: swap capture begin", m_generation);
    if (!captureDefaultTo(m_presentedTexture)) {
        if (trace) log::warn("Dual-frame trace {}: swap capture unavailable", m_generation);
        SpoutSender::get().sendTexture(m_decoratedTexture, m_width, m_height);
        if (m_traceFramesRemaining) --m_traceFramesRemaining;
        invalidate();
        return true;
    }
    if (trace) log::info("Dual-frame trace {}: swap capture complete", m_generation);

    auto const composed = drawFullscreen(
        m_spoutFramebuffer,
        0,
        0,
        m_width,
        m_height,
        1,
        m_decoratedTexture,
        m_layoutTexture,
        m_presentedTexture
    );
    if (composed) {
        if (trace) log::info("Dual-frame trace {}: Spout composition complete", m_generation);
        SpoutSender::get().sendFramebuffer(m_spoutFramebuffer, m_width, m_height);
    }
    else {
        SpoutSender::get().sendTexture(m_decoratedTexture, m_width, m_height);
    }
    if (trace) log::info("Dual-frame trace {}: send returned", m_generation);
    if (m_traceFramesRemaining) --m_traceFramesRemaining;
    invalidate();
    return true;
#endif
}
