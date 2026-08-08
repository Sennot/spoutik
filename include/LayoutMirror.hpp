#pragma once
#include <Geode/Geode.hpp>
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
    void renderPlayerView(cocos2d::CCDirector* director, PlayLayer* real);

    bool isRenderingLayout() const { return m_renderingLayout; }
    std::string const& modifiedLevelString() const { return m_modifiedString; }

private:
    LayoutMirror() = default;
    ~LayoutMirror() = default;
    LayoutMirror(LayoutMirror const&) = delete;
    LayoutMirror& operator=(LayoutMirror const&) = delete;

    struct LayoutEntry {
        GameObject* object = nullptr;
        bool keep = false;
        bool forceHidden = false;
        std::uint64_t touchedSerial = 0;
    };

    struct SavedVisualState {
        GameObject* object = nullptr;
        bool visible = false;
        unsigned char opacity = 255;
        int activeMainColorID = -1;
        int activeDetailColorID = -1;
        bool detailUsesHSV = false;
        bool baseUsesHSV = false;
        bool hasNoGlow = false;
        bool isHide = false;
        bool glowVisible = false;
        bool particleVisible = false;
        bool hadGlow = false;
        bool hadParticle = false;
        bool hadColorSprite = false;
        cocos2d::ccColor3B mainColor {255, 255, 255};
        cocos2d::ccColor3B detailColor {255, 255, 255};
        cocos2d::ccColor3B glowColor {255, 255, 255};
    };

    struct PendingRecord {
        bool keep = false;
        bool forceHidden = false;
    };

    struct SavedSceneSprite {
        cocos2d::CCSprite* sprite = nullptr;
        cocos2d::ccColor3B color {255, 255, 255};
        unsigned char opacity = 255;
    };

    void clear();
    void buildLayoutPlan(GJGameLevel* level);
    void applyLayoutOverrides(PlayLayer* real);
    void restoreLayoutOverrides();
    void touchEntry(LayoutEntry& entry);
    void applyScenePalette(PlayLayer* real);
    void saveAndColorSceneSprite(cocos2d::CCSprite* sprite, cocos2d::ccColor3B color);

    PlayLayer* m_real = nullptr;
    std::string m_modifiedString;
    std::vector<LayoutEntry> m_entries;
    std::unordered_map<int, std::deque<PendingRecord>> m_pendingByObjectID;
    std::unordered_map<int, cocos2d::ccColor3B> m_layoutPalette;
    std::vector<SavedVisualState> m_savedStates;
    std::vector<SavedSceneSprite> m_savedSceneSprites;
    std::uint64_t m_frameSerial = 0;
    std::size_t m_originalRecordCount = 0;
    std::size_t m_transformedRecordCount = 0;
    std::size_t m_classifiedKeepCount = 0;
    std::size_t m_boundRecordCount = 0;
    std::size_t m_unclassifiedObjectCount = 0;
    bool m_renderingLayout = false;
};
