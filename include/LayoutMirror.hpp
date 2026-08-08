#pragma once
#include <Geode/Geode.hpp>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

class LayoutMirror final {
public:
    static LayoutMirror& get();

    void createFor(PlayLayer* real, GJGameLevel* level);
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
    };

    void clear();
    void buildLayoutMap(PlayLayer* real);
    void applyLayoutOverrides(PlayLayer* real);
    void restoreLayoutOverrides();
    void touchEntry(LayoutEntry& entry);

    PlayLayer* m_real = nullptr;
    std::string m_modifiedString;
    std::vector<LayoutEntry> m_entries;
    std::unordered_map<GameObject*, std::size_t> m_entryIndex;
    std::vector<SavedVisualState> m_savedStates;
    std::uint64_t m_frameSerial = 0;
    bool m_renderingLayout = false;

    bool m_backgroundTouched = false;
    cocos2d::ccColor3B m_savedBackgroundColor {255, 255, 255};
};
