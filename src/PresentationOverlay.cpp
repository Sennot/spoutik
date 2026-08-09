#include "PresentationOverlay.hpp"

#ifdef GEODE_IS_WINDOWS
#include <Geode/cocos/platform/CCGL.h>
#include <Geode/cocos/shaders/CCGLProgram.h>
#include <Geode/cocos/shaders/ccGLStateCache.h>
#endif

using namespace geode::prelude;

PresentationOverlay& PresentationOverlay::get() {
    static PresentationOverlay instance;
    return instance;
}

void PresentationOverlay::captureSceneBaseline() {
#ifdef GEODE_IS_WINDOWS
    m_presentedReady = false;
    m_baselineReady = captureTexture(false);
#endif
}

bool PresentationOverlay::capturePresentedFrame() {
#ifndef GEODE_IS_WINDOWS
    return false;
#else
    if (!m_baselineReady) return false;
    m_presentedReady = captureTexture(true);
    return m_presentedReady;
#endif
}

#ifdef GEODE_IS_WINDOWS
bool PresentationOverlay::ensureTextures(int width, int height) {
    if (width <= 0 || height <= 0) return false;

    if (!m_sceneTexture || !m_presentedTexture) {
        GLuint textures[2] {};
        glGenTextures(2, textures);
        m_sceneTexture = textures[0];
        m_presentedTexture = textures[1];
    }
    if (!m_sceneTexture || !m_presentedTexture) return false;
    if (m_width == width && m_height == height) return true;

    GLint oldActive = GL_TEXTURE0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActive);
    glActiveTexture(GL_TEXTURE0);
    GLint oldTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);

    GLuint const textures[] { m_sceneTexture, m_presentedTexture };
    for (auto texture : textures) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0,
            GL_RGBA, GL_UNSIGNED_BYTE, nullptr
        );
    }

    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(oldTexture));
    glActiveTexture(static_cast<GLenum>(oldActive));
    m_width = width;
    m_height = height;
    return true;
}

bool PresentationOverlay::captureTexture(bool presented) {
    GLint framebuffer = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    GLint viewport[4] {};
    glGetIntegerv(GL_VIEWPORT, viewport);

    if (framebuffer != 0 || viewport[2] <= 0 || viewport[3] <= 0) {
        if (!m_failureLogged) {
            m_failureLogged = true;
            log::warn(
                "Presentation overlay capture unavailable: framebuffer {}, viewport {}x{}",
                framebuffer, viewport[2], viewport[3]
            );
        }
        return false;
    }

    if (presented) {
        if (!m_baselineReady || viewport[0] != m_viewX || viewport[1] != m_viewY ||
            viewport[2] != m_width || viewport[3] != m_height) {
            return false;
        }
    }
    else {
        if (!ensureTextures(viewport[2], viewport[3])) return false;
        m_viewX = viewport[0];
        m_viewY = viewport[1];
    }

    GLint oldActive = GL_TEXTURE0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActive);
    glActiveTexture(GL_TEXTURE0);
    GLint oldTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture);

    auto const texture = presented ? m_presentedTexture : m_sceneTexture;
    glBindTexture(GL_TEXTURE_2D, texture);
    glCopyTexSubImage2D(
        GL_TEXTURE_2D, 0, 0, 0,
        viewport[0], viewport[1], viewport[2], viewport[3]
    );

    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(oldTexture));
    glActiveTexture(static_cast<GLenum>(oldActive));
    return true;
}

bool PresentationOverlay::ensureProgram() {
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
        uniform sampler2D u_scene;
        uniform sampler2D u_presented;
        varying vec2 v_texCoord;
        void main() {
            vec4 scenePixel = texture2D(u_scene, v_texCoord);
            vec4 presentedPixel = texture2D(u_presented, v_texCoord);
            vec4 delta = abs(presentedPixel - scenePixel);
            if (max(max(delta.r, delta.g), max(delta.b, delta.a)) < 0.0019) discard;
            gl_FragColor = presentedPixel;
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
        log::error("Presentation overlay shader link failed");
        program->release();
        return false;
    }
    program->updateUniforms();
    m_sceneUniform = program->getUniformLocationForName("u_scene");
    m_presentedUniform = program->getUniformLocationForName("u_presented");
    if (m_sceneUniform < 0 || m_presentedUniform < 0) {
        program->release();
        return false;
    }
    m_program = program;
    return true;
}
#endif

void PresentationOverlay::replayDifference() {
#ifdef GEODE_IS_WINDOWS
    if (!m_baselineReady || !m_presentedReady) return;
    m_baselineReady = false;
    m_presentedReady = false;
    if (!ensureProgram()) {
        if (!m_failureLogged) {
            m_failureLogged = true;
            log::error("Presentation overlay GPU shader could not be initialized");
        }
        return;
    }

    GLint oldProgram = 0;
    GLint oldActive = GL_TEXTURE0;
    GLint oldArrayBuffer = 0;
    GLint oldViewport[4] {};
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActive);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &oldArrayBuffer);
    glGetIntegerv(GL_VIEWPORT, oldViewport);

    glActiveTexture(GL_TEXTURE0);
    GLint oldTexture0 = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture0);
    glActiveTexture(GL_TEXTURE1);
    GLint oldTexture1 = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture1);

    auto const blend = glIsEnabled(GL_BLEND);
    auto const depth = glIsEnabled(GL_DEPTH_TEST);
    auto const stencil = glIsEnabled(GL_STENCIL_TEST);
    auto const scissor = glIsEnabled(GL_SCISSOR_TEST);
    auto const cull = glIsEnabled(GL_CULL_FACE);
    GLboolean colorMask[4] {};
    glGetBooleanv(GL_COLOR_WRITEMASK, colorMask);

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

    glViewport(m_viewX, m_viewY, m_width, m_height);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    m_program->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_sceneTexture);
    glUniform1i(m_sceneUniform, 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_presentedTexture);
    glUniform1i(m_presentedUniform, 1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    ccGLEnableVertexAttribs(kCCVertexAttribFlag_Position | kCCVertexAttribFlag_TexCoords);
    glVertexAttribPointer(
        kCCVertexAttrib_Position, 2, GL_FLOAT, GL_FALSE, 0, kPositions
    );
    glVertexAttribPointer(
        kCCVertexAttrib_TexCoords, 2, GL_FLOAT, GL_FALSE, 0, kTextureCoordinates
    );
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(oldArrayBuffer));
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(oldTexture1));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(oldTexture0));
    glActiveTexture(static_cast<GLenum>(oldActive));
    glUseProgram(static_cast<GLuint>(oldProgram));
    glViewport(oldViewport[0], oldViewport[1], oldViewport[2], oldViewport[3]);
    if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (depth) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (stencil) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
    if (scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    if (cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    glColorMask(colorMask[0], colorMask[1], colorMask[2], colorMask[3]);
    ccGLInvalidateStateCache();

    if (!m_statusLogged) {
        m_statusLogged = true;
        log::info(
            "GPU presentation-overlay replay active: {}x{} baseline/difference textures",
            m_width, m_height
        );
    }
#endif
}
