#include "SpoutSender.hpp"
#include <Geode/loader/Mod.hpp>
#include <filesystem>

#ifdef GEODE_IS_WINDOWS
#include <windows.h>
#endif

using namespace geode::prelude;

#ifdef GEODE_IS_WINDOWS
using GetSpoutFn = SPOUTLIBRARY* (WINAPI*)();
#endif

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
        resources / "spout" / "SpoutLibrary.dll",
        resources / "resources" / "spout" / "SpoutLibrary.dll",
        resources / "SpoutLibrary.dll",
    };

    for (auto const& path : candidates) {
        if (!std::filesystem::exists(path)) continue;
        m_module = ::LoadLibraryW(path.wstring().c_str());
        if (m_module) {
            log::debug("Loaded bundled SpoutLibrary.dll from {}", path.string());
            break;
        }
    }

    if (!m_module) {
        log::error("SpoutLibrary.dll was not found/loaded from mod resources (Win32 error {})", ::GetLastError());
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

    // The user's global SpoutSettings registry may be configured for CPU mode.
    // This mod must be application-local GPU texture sharing, so override those
    // defaults before the first sender is created. These methods are from the
    // exact pinned SpoutLibrary 2.007.017 interface vendored by bootstrap.
    forceGpuTextureSharing();
    refreshName();
    log::info("Spout sender interface initialized: {}", m_name);
    return true;
#endif
}

void SpoutSender::forceGpuTextureSharing() {
#ifdef GEODE_IS_WINDOWS
    if (!m_spout || m_shareModeConfigured) return;

    // Spout 2.007.017 share modes: 0 texture, 1 memory, 2 CPU.
    // Keep these application-only overrides together and disable automatic CPU
    // fallback. SetCPUshare(false) explicitly re-tests GL/DX compatibility.
    m_spout->SetMemoryShareMode(false);
    m_spout->SetCPUmode(false);
    m_spout->SetShareMode(0);
    m_spout->SetAutoShare(false);
    m_spout->SetCPUshare(false);
    m_shareModeConfigured = true;

    log::info(
        "Spout requested GPU texture mode: shareMode={}, CPUmode={}, memoryMode={}, autoShare={}, GLDXready={}",
        m_spout->GetShareMode(),
        m_spout->GetCPUmode(),
        m_spout->GetMemoryShareMode(),
        m_spout->GetAutoShare(),
        m_spout->IsGLDXready()
    );
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
    if (!ensureLoaded()) return false;
    forceGpuTextureSharing();
    refreshName();

    // Official SpoutLibrary default-framebuffer path: zero FBO, width and height.
    auto invert = Mod::get()->getSettingValue<bool>("invert-spout");
    auto sent = m_spout->SendFbo(0, 0, 0, invert);

    if (!sent) {
        ++m_sendFailures;
        if (m_sendFailures == 1 || (m_sendFailures % 300) == 0) {
            log::warn(
                "Spout SendFbo failed (attempt {}, shareMode={}, CPU={}, GLDXready={}); will retry",
                m_sendFailures,
                m_spout->GetShareMode(),
                m_spout->GetCPU(),
                m_spout->IsGLDXready()
            );
        }
        return false;
    }

    m_sendFailures = 0;
    if (!m_spout->IsInitialized()) return true;

    // Do not destroy the sender merely because a machine/registry setting forced
    // CPU mode. Keeping it alive makes diagnostics visible in OBS; however we
    // loudly report that the user's GPU-only requirement was not met. Normally
    // the explicit application overrides above keep this false on an RTX 3090.
    if (m_spout->GetCPU()) {
        if (!m_cpuWarningLogged) {
            m_cpuWarningLogged = true;
            log::error(
                "Spout sender is still in CPU sharing after forcing texture mode "
                "(shareMode={}, GLDXready={}). Sender remains live for diagnosis.",
                m_spout->GetShareMode(),
                m_spout->IsGLDXready()
            );
        }
    }
    else if (!m_statusLogged) {
        m_statusLogged = true;
        log::info(
            "Spout GPU sender active: {} ({}x{}, shareMode={}, GLDXready={})",
            m_spout->GetName() ? m_spout->GetName() : m_name.c_str(),
            m_spout->GetWidth(),
            m_spout->GetHeight(),
            m_spout->GetShareMode(),
            m_spout->IsGLDXready()
        );
    }
    return true;
#endif
}

void SpoutSender::shutdown() {
#ifdef GEODE_IS_WINDOWS
    if (m_spout) {
        m_spout->ReleaseSender(0);
        // Release the COM-like Spout instance while its DLL is still loaded.
        m_spout->Release();
        m_spout = nullptr;
    }
    if (m_module) {
        ::FreeLibrary(m_module);
        m_module = nullptr;
    }
#endif
}
