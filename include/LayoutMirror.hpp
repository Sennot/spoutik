#pragma once

#include <Geode/Geode.hpp>
#include "CompanionProtocol.hpp"
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

class LayoutMirror final {
public:
    static LayoutMirror& get();

    void prepareFor(PlayLayer* real, GJGameLevel* level);
    void observeObject(PlayLayer* real, GameObject* object);
    void finishFor(PlayLayer* real);
    void destroyFor(PlayLayer* real);

    bool isStableGameplayScene(cocos2d::CCDirector* director, PlayLayer* real) const;
    bool writeCompanionFrame(
        layout_companion::SharedFrame& frame,
        cocos2d::CCDirector* director,
        PlayLayer* real
    );

private:
    LayoutMirror() = default;
    LayoutMirror(LayoutMirror const&) = delete;
    LayoutMirror& operator=(LayoutMirror const&) = delete;

    struct LayoutEntry {
        GameObject* object = nullptr;
        bool keep = false;
        bool forceHidden = false;
        std::uint64_t exportedSerial = 0;
    };

    struct SpatialEntry {
        float x = 0.f;
        float y = 0.f;
        std::size_t entryIndex = 0;
    };

    struct PendingRecord {
        bool keep = false;
        bool forceHidden = false;
    };

    void clear();
    void buildLayoutPlan(GJGameLevel* level);

    PlayLayer* m_real = nullptr;
    std::string m_modifiedString;
    std::vector<LayoutEntry> m_entries;
    std::vector<SpatialEntry> m_spatialEntries;
    std::vector<std::size_t> m_spatialOverflow;
    std::unordered_map<GameObject*, std::size_t> m_entryIndex;
    std::unordered_map<int, std::deque<PendingRecord>> m_pendingByObjectID;
    std::unordered_map<int, cocos2d::ccColor3B> m_layoutPalette;
    std::uint64_t m_exportSerial = 0;
    std::size_t m_originalRecordCount = 0;
    std::size_t m_transformedRecordCount = 0;
    std::size_t m_classifiedKeepCount = 0;
    std::size_t m_boundRecordCount = 0;
    std::size_t m_boundKeepCount = 0;
    std::size_t m_unclassifiedObjectCount = 0;
    bool m_spatialIndexReady = false;
};
