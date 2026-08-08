#include "SpoutSender.hpp"
#include <Geode/loader/Mod.hpp>
#include <algorithm>
#include <filesystem>

#ifdef GEODE_IS_WINDOWS
#include <windows.h>
#include <GL/gl.h>
#endif

using namespace geode::prelude;

SpoutSender& SpoutSender::get() {
    static SpoutSender instance;
    return instance;
}

bool SpoutSender::ready() const {
#ifdef GEODE_IS_WINDOWS
    return m_spout != nullptr;
#else
    return false;
#endif
}

bool SpoutSender::ensureLoaded() {
#ifndef GEODE_IS_WINDOWS
    return false;
#else
    if (m_spout) return true;
    if (m_loadAttempted) return false;
    m_loadAttempted = true;

    auto resources = Mod::get()->getResourcesDir();
    std::filesystem::path candidates[] = {
        // Geode normally strips the source-side `resources/` prefix when it
        // extracts mod resources. Keep both layouts as a compatibility fallback
        // for CLI/package format changes and manual development installs.
        resources / "spout" / "SpoutLibrary.dll",
        resources / "resources" / "spout" / "SpoutLibrary.dll",
        resources / "SpoutLibrary.dll",
    };

    for (auto const& path : candidates) {
        if (!std::filesystem::exists(path)) continue;
        m_module = ::LoadLibraryW(path.wstring().c_str());
        if (m_module) break;
    }

    if (!m_module) {
        log::error("SpoutLibrary.dll was not found/loaded from mod resources");
        return false;
    }

    auto factory = reinterpret_cast<GetSpoutFn>(::GetProcAddress(m_module, "GetSpout"));
    if (!factory) {
        log::error("SpoutLibrary.dll has no GetSpout export");
        return false;
    }

    m_spout = factory();
    if (!m_spout) {
        log::error("GetSpout returned null");
        return false;
    }

    refreshName();
    log::info("Spout sender initialized: {}", m_name);
    return true;
#endif
}

void SpoutSender::refreshName() {
#ifdef GEODE_IS_WINDOWS
    if (!m_spout) return;
    auto wanted = Mod::get()->getSettingValue<std::string>("sender-name");
    if (wanted.empty()) wanted = "Geometry Dash Full";
    if (wanted == m_name) return;
    m_name = wanted;
    m_spout->SetSenderName(m_name.c_str());
#endif
}

bool SpoutSender::sendDefaultFramebuffer() {
#ifndef GEODE_IS_WINDOWS
    return false;
#else
    if (!Mod::get()->getSettingValue<bool>("enabled")) return false;
    if (m_cpuFallbackRejected) return false;
    if (!ensureLoaded()) return false;
    refreshName();

    // SpoutLibrary explicitly supports default framebuffer by FBO ID 0.
    // Prefer CCDirector's pixel size: another overlay may legitimately leave a
    // smaller GL viewport active even though the default framebuffer is full-size.
    auto size = cocos2d::CCDirector::get()->getWinSizeInPixels();
    auto width = static_cast<unsigned int>(std::max(size.width, 1.f));
    auto height = static_cast<unsigned int>(std::max(size.height, 1.f));
    auto invert = Mod::get()->getSettingValue<bool>("invert-spout");
    auto sent = m_spout->SendFbo(0, width, height, invert);

    // Optimization is a hard requirement for this mod: never silently fall
    // back to Spout's CPU sharing path. Spout exposes the active sender mode
    // through GetCPU()/GetGLDX() after initialization.
    if (m_spout->IsInitialized() && (m_spout->GetCPU() || !m_spout->GetGLDX())) {
        log::error(
            "Spout did not select GL/DX GPU texture sharing. "
            "Sender disabled for this session; put Geometry Dash and OBS on the same GPU."
        );
        m_spout->ReleaseSender(0);
        m_cpuFallbackRejected = true;
        return false;
    }
    return sent;
#endif
}

void SpoutSender::shutdown() {
#ifdef GEODE_IS_WINDOWS
    if (m_spout) {
        m_spout->ReleaseSender(0);
        // Keep the DLL loaded for process lifetime. The full upstream interface
        // owns the object and exposes Release() at the end of its vtable; avoiding
        // unloading here prevents dangling code pointers during Geode shutdown.
        m_spout = nullptr;
    }
#endif
}
