#pragma once

#include "../core/AttemptGuard.hpp"
#include "../core/TrainingPlan.hpp"
#include "../core/TrainingStats.hpp"
#include "StartPosAnalyzer.hpp"

#include <Geode/Geode.hpp>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace baconsistent {

struct ProgressEvent {
    int segmentIndex = 0;
    int repetitions = 0;
    int target = 20;
    bool segmentCompleted = false;
    bool planCompleted = false;
    bool roundCompleted = false;
    int completedRound = 0;
    int newRound = 1;
};

class TrainingManager {
public:
    static TrainingManager& get();

    void loadLevel(GJGameLevel* level, StartPosAnalysis const& analysis = {});
    void unloadLevel();
    void refreshSettings();

    [[nodiscard]] bool loaded() const { return m_loaded; }
    [[nodiscard]] bool enabled() const;
    [[nodiscard]] std::string const& levelKey() const { return m_levelKey; }

    [[nodiscard]] PercentageMode percentageMode() const;
    [[nodiscard]] bool usingStartPosBoundaries() const { return m_usingStartPosBoundaries; }
    [[nodiscard]] int detectedStartPosCount() const;
    [[nodiscard]] std::string sourceLabel() const;

    void beginAttempt(double percent, bool practiceMode);
    void observePracticeMode(bool practiceMode);
    void markSuppressedDeath();
    void tick(double dt);
    void finishAttempt();
    std::optional<ProgressEvent> update(double currentPercent);

    [[nodiscard]] core::TrainingPlan const& plan() const { return m_plan; }
    [[nodiscard]] core::TrainingPlan& plan() { return m_plan; }
    [[nodiscard]] core::TrainingStats const& stats() const { return m_stats; }
    [[nodiscard]] int selected() const { return m_session.selected(); }
    [[nodiscard]] int liveSegmentIndex() const;
    [[nodiscard]] core::Segment selectedSegment() const;
    [[nodiscard]] bool attemptCountable() const { return m_attemptGuard.countable(); }
    [[nodiscard]] bool attemptBlockedByPractice() const { return m_attemptGuard.blockedByPractice(); }
    [[nodiscard]] bool attemptBlockedByNoclip() const { return m_attemptGuard.blockedByNoclip(); }

    [[nodiscard]] int roundNumber() const { return m_roundNumber; }
    [[nodiscard]] int completedRounds() const { return std::max(0, m_roundNumber - 1); }
    [[nodiscard]] int recentCompletedRound() const { return m_recentCompletedRound; }
    void clearRecentRoundBanner() { m_recentCompletedRound = 0; }

    void select(int index);
    void selectRelative(int delta);
    void selectRecommended();
    void resetSelected();
    void resetRoundProgress();
    void adjustSelectedTarget(int delta);
    void resetSelectedTarget();
    [[nodiscard]] int selectedTarget() const;

    // Select the physical StartPos matching the current fixed stage and reset
    // the level. Stage 0 means beginning from zero. Returns false when the
    // current level has only a cached profile and no live StartPos objects.
    [[nodiscard]] bool canActivateSelectedStartPos() const;
    bool activateSelectedStartPos();

    [[nodiscard]] std::string compactSegmentText(int index) const;
    [[nodiscard]] std::string segmentRangeText(int index) const;

private:
    TrainingManager();

    std::string makeLevelKey(GJGameLevel* level) const;
    std::string saveKey(std::string_view suffix) const;
    void saveProgress();
    void saveSelected();
    void saveStats();
    void saveRound();
    void flushCrashSafe();
    void maybeAutoMatchStart(double percent);
    void advanceRound();

    void loadCachedStartPosProfile();
    void updateStartPosProfile(StartPosAnalysis const& analysis);
    void applyConfiguredPlan(bool initialLoad);

    [[nodiscard]] std::vector<double> activeBoundaries() const;
    [[nodiscard]] std::string configuredPlanSignature() const;
    [[nodiscard]] StartPosRuntimeMarker const* markerForSegmentStart(int segmentIndex) const;

    core::TrainingPlan m_plan;
    core::TrainingSession m_session;
    core::TrainingStats m_stats;
    core::AttemptGuard m_attemptGuard;
    std::string m_levelKey;
    std::string m_activePlanSignature;
    std::vector<double> m_startPos21Boundaries;
    std::vector<double> m_startPos22Boundaries;
    std::vector<StartPosRuntimeMarker> m_runtimeStartPositions;
    bool m_hasStartPosProfile = false;
    bool m_usingStartPosBoundaries = false;
    bool m_loaded = false;
    bool m_attemptProgressCounted = false;
    int m_roundNumber = 1;
    int m_recentCompletedRound = 0;
};

} // namespace baconsistent
