#pragma once
#include <Geode/Geode.hpp>
#include <string>

#ifdef GEODE_IS_WINDOWS
#include "SpoutMini.hpp"
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

#ifdef GEODE_IS_WINDOWS
    HMODULE m_module = nullptr;
    SpoutMini* m_spout = nullptr;
#endif
    std::string m_name;
    bool m_loadAttempted = false;
    bool m_cpuFallbackRejected = false;
};
