#pragma once
#include <Geode/Geode.hpp>
#include <string>

#ifdef GEODE_IS_WINDOWS
#include <windows.h>
#include "SpoutLibrary.h"
#endif

class SpoutSender final {
public:
    static SpoutSender& get();
    bool sendDefaultFramebuffer();
    void shutdown();
    bool ready() const;

private:
    SpoutSender() = default;
    ~SpoutSender() = default;
    SpoutSender(SpoutSender const&) = delete;
    SpoutSender& operator=(SpoutSender const&) = delete;

    bool ensureLoaded();
    void refreshName();
    void forceGpuTextureSharing();

#ifdef GEODE_IS_WINDOWS
    HMODULE m_module = nullptr;
    SPOUTLIBRARY* m_spout = nullptr;
#endif
    std::string m_name;
    bool m_loadAttempted = false;
    bool m_shareModeConfigured = false;
    bool m_statusLogged = false;
    bool m_cpuWarningLogged = false;
    unsigned int m_sendFailures = 0;
};
