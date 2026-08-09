#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace baconsistent::core {

struct AggregateStats {
    int attempts = 0;
    int successes = 0;
    int currentStreak = 0;
    int bestStreak = 0;
    double playtimeSeconds = 0.0;

    [[nodiscard]] double successRate() const;
};

struct RoundSummary {
    int round = 0;
    AggregateStats stats;
};

class TrainingStats {
public:
    void configureStages(std::size_t count);
    void clearStageStats();
    void resetCurrentRound();

    // An attempt belongs to the selected fixed stage at the moment it starts.
    // Attempts are counted immediately; success is recorded at most once.
    void beginAttempt(std::size_t stageIndex);
    void tick(double dt);
    void markSuccess(std::size_t stageIndex);
    void finishAttempt();
    // Drop an invalid/cheat-protected attempt without turning it into a
    // failure or changing streaks. The attempt counters are rolled back.
    void cancelAttempt();

    // Stores a compact history entry and clears only round-local stats.
    // Lifetime and per-stage stats are preserved between rounds.
    RoundSummary completeRound(int roundNumber);

    [[nodiscard]] AggregateStats const& lifetime() const { return m_lifetime; }
    [[nodiscard]] AggregateStats const& round() const { return m_round; }
    [[nodiscard]] AggregateStats const& stage(std::size_t index) const;
    [[nodiscard]] std::vector<RoundSummary> const& history() const { return m_history; }

    [[nodiscard]] bool attemptActive() const { return m_attemptActive; }
    [[nodiscard]] bool attemptSucceeded() const { return m_attemptSucceeded; }
    [[nodiscard]] std::size_t activeStage() const { return m_activeStage; }

    // Persistence is intentionally plain text so core tests do not depend on
    // Geode or JSON. Invalid data is ignored conservatively.
    [[nodiscard]] std::string encode() const;
    void decode(std::string const& encoded);

private:
    static void recordSuccess(AggregateStats& stats);
    static void recordFailure(AggregateStats& stats);

    AggregateStats m_lifetime;
    AggregateStats m_round;
    std::vector<AggregateStats> m_stages;
    std::vector<RoundSummary> m_history;

    bool m_attemptActive = false;
    bool m_attemptSucceeded = false;
    std::size_t m_activeStage = 0;
    double m_activeAttemptPlaytime = 0.0;
};

std::string formatDuration(double seconds);

} // namespace baconsistent::core
