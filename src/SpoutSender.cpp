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

    // SpoutLibrary's documented default-framebuffer path is exactly
    // SendFbo(0, 0, 0, ...). Passing zero dimensions lets Spout derive the
    // actual WGL framebuffer size and avoids DPI / viewport-size mismatches.
    auto invert = Mod::get()->getSettingValue<bool>("invert-spout");
    auto sent = m_spout->SendFbo(0, 0, 0, invert);

    if (!sent) {
        ++m_sendFailures;
        // A transient first-frame failure is recoverable. Do not permanently
        // disable Spout merely because the sender was not ready on one swap.
        if (m_sendFailures == 1 || (m_sendFailures % 300) == 0) {
            log::warn("Spout SendFbo failed (attempt {}); will retry", m_sendFailures);
        }
        return false;
    }

    m_sendFailures = 0;
    if (!m_spout->IsInitialized()) return true;

    // GetCPU() is the actual sender sharing-method flag. GetGLDX() is only
    // Spout's legacy NVIDIA NV_DX_interop2 hardware-compatibility query; a
    // false GetGLDX() does NOT mean that the sender fell back to CPU sharing.
    // Optimization remains a hard requirement: reject only a real CPU sender.
    if (m_spout->GetCPU()) {
        log::error(
            "Spout selected CPU sharing. Sender disabled for this session; "
            "GPU-only sharing is required by this mod."
        );
        m_spout->ReleaseSender(0);
        m_cpuFallbackRejected = true;
        return false;
    }

    if (!m_statusLogged) {
        m_statusLogged = true;
        log::info(
            "Spout GPU sender active: {} ({}x{}, legacy GL/DX compatibility={})",
            m_spout->GetName() ? m_spout->GetName() : m_name.c_str(),
            m_spout->GetWidth(),
            m_spout->GetHeight(),
            m_spout->GetGLDX()
        );
    }
    return true;
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
