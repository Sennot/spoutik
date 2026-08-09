#pragma once

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace baconsistent::core {

struct Segment {
    double start = 0.0;
    double end = 0.0;
    int repetitions = 0;
};

// Converts arbitrary percentage markers into safe ordered segment boundaries.
// 0 and 100 are always present, duplicate / near-duplicate markers are removed.
std::vector<double> normalizeBoundaries(std::vector<double> boundaries);
std::vector<double> boundariesFromStartPosPercentages(std::vector<double> percentages);

class TrainingPlan {
public:
    TrainingPlan(int parts = 10, int target = 20);

    // Equal fallback split (for levels where no StartPos profile is known).
    void reconfigure(int parts, int target);

    // StartPos-driven split. Boundaries may be irregular and are normalized.
    // Existing counters and per-stage targets are preserved by index when possible.
    void reconfigureBoundaries(std::vector<double> boundaries, int target);

    [[nodiscard]] int parts() const { return static_cast<int>(m_counts.size()); }
    [[nodiscard]] int target() const { return m_defaultTarget; }
    [[nodiscard]] int target(std::size_t index) const;
    [[nodiscard]] std::size_t size() const { return m_counts.size(); }
    [[nodiscard]] std::vector<double> const& boundaries() const { return m_boundaries; }

    [[nodiscard]] Segment segment(std::size_t index) const;
    [[nodiscard]] int count(std::size_t index) const;
    [[nodiscard]] int remaining(std::size_t index) const;
    [[nodiscard]] bool completed(std::size_t index) const;
    [[nodiscard]] bool allCompleted() const;
    [[nodiscard]] int totalCompletions() const;
    [[nodiscard]] int totalGoal() const;

    bool increment(std::size_t index);
    void reset(std::size_t index);
    void resetAll();

    // A per-stage target lets hard parts receive more repetitions without
    // changing the rest of the fixed StartPos plan. Passing the default target
    // is equivalent to clearing an override.
    void setTarget(std::size_t index, int target);
    void resetTargetsToDefault();

    [[nodiscard]] int recommendedBackwards() const;
    [[nodiscard]] int previousIncomplete(int fromIndex) const;

    [[nodiscard]] std::string encodeCounts() const;
    void decodeCounts(std::string const& encoded);
    [[nodiscard]] std::string encodeTargets() const;
    void decodeTargets(std::string const& encoded);

private:
    int m_defaultTarget = 20;
    std::vector<double> m_boundaries;
    std::vector<int> m_counts;
    std::vector<int> m_targets;
};

class TrainingSession {
public:
    explicit TrainingSession(TrainingPlan* plan = nullptr);

    void attach(TrainingPlan* plan);
    void setSelected(int index);
    [[nodiscard]] int selected() const { return m_selected; }

    void beginAttempt(double percent);
    [[nodiscard]] double attemptStart() const { return m_attemptStart; }
    [[nodiscard]] bool countedThisAttempt() const { return m_countedThisAttempt; }

    // Returns the new repetition count when the selected segment has just been
    // completed on this attempt. Returns std::nullopt otherwise.
    std::optional<int> update(
        double currentPercent,
        bool strictStart,
        double startTolerance
    );

private:
    TrainingPlan* m_plan = nullptr;
    int m_selected = 0;
    double m_attemptStart = 0.0;
    bool m_countedThisAttempt = false;
};

} // namespace baconsistent::core
