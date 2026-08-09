#pragma once
#include <Geode/Geode.hpp>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <unordered_map>
#include <vector>

class LayoutMirror final {
public:
    enum class NodeVisitAction {
        PassThrough,
        Skip,
        Styled,
    };

    static LayoutMirror& get();

    void prepareFor(PlayLayer* real, GJGameLevel* level);
    void observeObject(PlayLayer* real, GameObject* object);
    void observeOpacity(GameObject* object, unsigned char opacity);
    void finishFor(PlayLayer* real);
    void destroyFor(PlayLayer* real);
    bool isStableGameplayScene(cocos2d::CCDirector* director, PlayLayer* real) const;
    bool renderPlayerView(cocos2d::CCDirector* director, PlayLayer* real);
    NodeVisitAction beginNodeVisit(cocos2d::CCNode* node);
    void endNodeVisit(cocos2d::CCNode* node);

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
        unsigned char layoutOpacity = 255;
        std::uint64_t touchedSerial = 0;
    };

    struct SpatialEntry {
        float x = 0.f;
        float y = 0.f;
        std::size_t entryIndex = 0;
    };

    struct SavedVisualState {
        cocos2d::CCNode* node = nullptr;
        cocos2d::CCSprite* sprite = nullptr;
        cocos2d::CCParticleSystemQuad* particle = nullptr;
        cocos2d::ccColor3B color {255, 255, 255};
        unsigned char opacity = 255;
        bool restoreColor = false;
        bool restoreOpacity = false;
        bool restoreVisible = false;
        bool visible = false;
    };

    enum class RenderNodeKind {
        Main,
        Detail,
        Suppress,
    };

    struct RenderNodeEntry {
        GameObject* owner = nullptr;
        RenderNodeKind kind = RenderNodeKind::Suppress;
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
    void beginLayoutPass(cocos2d::CCDirector* director, PlayLayer* real);
    void endLayoutPass();
    void restoreLayoutOverrides();
    void registerRenderNodes(LayoutEntry const& entry);
    void applyCameraOverrides(cocos2d::CCDirector* director, PlayLayer* real);
    void touchCameraEntry(LayoutEntry& entry);
    void applyScenePalette(PlayLayer* real);
    void saveAndColorSceneSprite(cocos2d::CCSprite* sprite, cocos2d::ccColor3B color);

    PlayLayer* m_real = nullptr;
    std::string m_modifiedString;
    std::vector<LayoutEntry> m_entries;
    std::vector<SpatialEntry> m_spatialEntries;
    std::vector<std::size_t> m_spatialOverflow;
    std::unordered_map<GameObject*, std::size_t> m_entryIndex;
    std::unordered_map<cocos2d::CCNode*, RenderNodeEntry> m_renderNodes;
    std::unordered_map<int, std::deque<PendingRecord>> m_pendingByObjectID;
    std::unordered_map<int, cocos2d::ccColor3B> m_layoutPalette;
    std::vector<SavedVisualState> m_savedStates;
    std::vector<SavedSceneSprite> m_savedSceneSprites;
    std::uint64_t m_frameSerial = 0;
    std::size_t m_originalRecordCount = 0;
    std::size_t m_transformedRecordCount = 0;
    std::size_t m_classifiedKeepCount = 0;
    std::size_t m_boundRecordCount = 0;
    std::size_t m_boundKeepCount = 0;
    std::size_t m_unclassifiedObjectCount = 0;
    std::size_t m_frameNodeProbeCount = 0;
    std::size_t m_frameMappedNodeCount = 0;
    std::size_t m_frameStyledNodeCount = 0;
    std::size_t m_frameSuppressedNodeCount = 0;
    std::size_t m_frameCandidateObjectCount = 0;
    std::size_t m_frameRetainedCandidateCount = 0;
    std::size_t m_frameCompactCandidateCount = 0;
    std::size_t m_frameSpatialCandidateCount = 0;
    std::size_t m_frameFallbackCandidateCount = 0;
    std::size_t m_frameForcedVisibleCount = 0;
    std::size_t m_frameBatchedMutationCount = 0;
    float m_frameViewportLeft = 0.f;
    float m_frameViewportRight = 0.f;
    float m_frameViewportBottom = 0.f;
    float m_frameViewportTop = 0.f;
    bool m_frameViewportValid = false;
    bool m_reportedRenderCoverage = false;
    bool m_spatialIndexReady = false;
    bool m_renderMapReady = false;
    bool m_renderingLayout = false;
    bool m_reportedFramebufferSkip = false;
    bool m_reportedFramebufferLeak = false;
};
