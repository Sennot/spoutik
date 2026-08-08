#pragma once
#include <Geode/Geode.hpp>
#include <string>

class LayoutMirror final {
public:
    static LayoutMirror& get();

    bool isCreating() const { return m_creating; }
    bool isStepping() const { return m_stepping; }
    bool isStarting() const { return m_starting; }
    bool integrityBypass() const {
        return m_creating || m_starting || m_stepping || m_forwardingInput ||
               m_resetting || m_checkpointing || m_removingCheckpoint;
    }
    bool isMirror(PlayLayer* layer) const { return layer && layer == m_mirror; }
    bool layoutContext(PlayLayer* layer) const { return m_creating || isMirror(layer); }

    void createFor(PlayLayer* real, GJGameLevel* level, bool useReplay, bool dontCreateObjects);
    void destroyFor(PlayLayer* real);
    void startFromReal(PlayLayer* real);
    void stepFromReal(PlayLayer* real, float dt);
    void forwardButton(PlayLayer* real, bool down, int button, bool player1);
    void resetFromReal(PlayLayer* real);
    void markCheckpointFromReal(PlayLayer* real);
    void removeCheckpointFromReal(PlayLayer* real, bool first);
    void removeAllCheckpointsFromReal(PlayLayer* real);
    void renderPlayerView(cocos2d::CCDirector* director, PlayLayer* real);

    std::string const& modifiedLevelString() const { return m_modifiedString; }

private:
    LayoutMirror() = default;
    ~LayoutMirror();
    LayoutMirror(LayoutMirror const&) = delete;
    LayoutMirror& operator=(LayoutMirror const&) = delete;

    struct GameManagerScope {
        explicit GameManagerScope(PlayLayer* layer);
        ~GameManagerScope();
        GameManager* gm = nullptr;
        PlayLayer* oldPlay = nullptr;
        GJBaseGameLayer* oldGame = nullptr;
    };

    void releaseMirror();
    void syncRuntimeFlags(PlayLayer* real);
    void syncStartPos(PlayLayer* real);
    void renderSceneSiblings(cocos2d::CCScene* scene, PlayLayer* real, bool before);

    PlayLayer* m_real = nullptr;
    PlayLayer* m_mirror = nullptr;
    GJGameLevel* m_mirrorLevel = nullptr;
    StartPosObject* m_lastRealStartPos = nullptr;
    StartPosObject* m_lastMirrorStartPos = nullptr;
    bool m_startPosSynced = false;
    std::string m_modifiedString;
    bool m_creating = false;
    bool m_starting = false;
    bool m_stepping = false;
    bool m_forwardingInput = false;
    bool m_resetting = false;
    bool m_checkpointing = false;
    bool m_removingCheckpoint = false;
};
