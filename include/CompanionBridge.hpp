#pragma once

#include <Geode/Geode.hpp>

class CompanionBridge final {
public:
    static CompanionBridge& get();

    void publish(cocos2d::CCDirector* director, PlayLayer* real);
    void suspend();
    void shutdown();

private:
    CompanionBridge() = default;
    ~CompanionBridge();
    CompanionBridge(CompanionBridge const&) = delete;
    CompanionBridge& operator=(CompanionBridge const&) = delete;

    bool ensureMapping();

    void* m_mappingHandle = nullptr;
    void* m_view = nullptr;
    std::uint64_t m_frameNumber = 0;
    std::uint64_t m_lastPublishAtMilliseconds = 0;
    bool m_active = false;
    bool m_reportedReady = false;
};
