#include "TrainingPlan.hpp"

#include <cmath>
#include <numeric>
#include <sstream>
#include <utility>

namespace baconsistent::core {

namespace {
int clampParts(int parts) {
    return std::clamp(parts, 2, 20);
}

int clampTarget(int target) {
    return std::clamp(target, 1, 100);
}

constexpr double kBoundaryEpsilon = 0.01;
constexpr std::size_t kMaxBoundaries = 101; // 100 trainable segments is plenty.
} // namespace

std::vector<double> normalizeBoundaries(std::vector<double> boundaries) {
    std::vector<double> normalized;
    normalized.reserve(boundaries.size() + 2);
    normalized.push_back(0.0);

    for (auto value : boundaries) {
        if (!std::isfinite(value)) {
            continue;
        }
        normalized.push_back(std::clamp(value, 0.0, 100.0));
    }
    normalized.push_back(100.0);

    std::sort(normalized.begin(), normalized.end());
    normalized.erase(
        std::unique(normalized.begin(), normalized.end(), [](double a, double b) {
            return std::abs(a - b) <= kBoundaryEpsilon;
        }),
        normalized.end()
    );

    if (normalized.empty() || normalized.front() > kBoundaryEpsilon) {
        normalized.insert(normalized.begin(), 0.0);
    }
    else {
        normalized.front() = 0.0;
    }

    if (normalized.back() < 100.0 - kBoundaryEpsilon) {
        normalized.push_back(100.0);
    }
    else {
        normalized.back() = 100.0;
    }

    // Protect the UI / persistence layer against deliberately pathological
    // copies containing thousands of StartPos objects. Preserve the final 100%.
    if (normalized.size() > kMaxBoundaries) {
        normalized.resize(kMaxBoundaries);
        normalized.back() = 100.0;
    }

    if (normalized.size() < 2) {
        return {0.0, 100.0};
    }
    return normalized;
}

std::vector<double> boundariesFromStartPosPercentages(std::vector<double> percentages) {
    return normalizeBoundaries(std::move(percentages));
}

TrainingPlan::TrainingPlan(int parts, int target) {
    reconfigure(parts, target);
}

void TrainingPlan::reconfigure(int parts, int target) {
    parts = clampParts(parts);

    std::vector<double> boundaries;
    boundaries.reserve(static_cast<std::size_t>(parts) + 1);
    for (int i = 0; i <= parts; ++i) {
        boundaries.push_back(
            i == parts ? 100.0 : (100.0 * static_cast<double>(i) / static_cast<double>(parts))
        );
    }
    reconfigureBoundaries(std::move(boundaries), target);
}

void TrainingPlan::reconfigureBoundaries(std::vector<double> boundaries, int targetValue) {
    auto oldCounts = m_counts;
    auto oldTargets = m_targets;
    auto const oldDefaultTarget = m_defaultTarget;

    m_defaultTarget = clampTarget(targetValue);
    m_boundaries = normalizeBoundaries(std::move(boundaries));
    m_counts.assign(m_boundaries.size() - 1, 0);
    m_targets.assign(m_counts.size(), m_defaultTarget);

    auto const preserved = std::min(oldCounts.size(), m_counts.size());
    for (std::size_t i = 0; i < preserved; ++i) {
        // Preserve custom targets across ordinary refreshes. Entries that were
        // just following the old global default now follow the new default.
        if (i < oldTargets.size() && oldTargets[i] != oldDefaultTarget) {
            m_targets[i] = clampTarget(oldTargets[i]);
        }
        m_counts[i] = std::clamp(oldCounts[i], 0, m_targets[i]);
    }
}

int TrainingPlan::target(std::size_t index) const {
    if (index >= m_targets.size()) {
        return m_defaultTarget;
    }
    return m_targets[index];
}

Segment TrainingPlan::segment(std::size_t index) const {
    if (m_counts.empty() || m_boundaries.size() < 2) {
        return {};
    }

    index = std::min(index, m_counts.size() - 1);
    return Segment{m_boundaries[index], m_boundaries[index + 1], m_counts[index]};
}

int TrainingPlan::count(std::size_t index) const {
    if (index >= m_counts.size()) {
        return 0;
    }
    return m_counts[index];
}

int TrainingPlan::remaining(std::size_t index) const {
    return std::max(0, target(index) - count(index));
}

bool TrainingPlan::completed(std::size_t index) const {
    return count(index) >= target(index);
}

bool TrainingPlan::allCompleted() const {
    if (m_counts.empty()) {
        return false;
    }
    for (std::size_t i = 0; i < m_counts.size(); ++i) {
        if (!completed(i)) {
            return false;
        }
    }
    return true;
}

int TrainingPlan::totalCompletions() const {
    return std::accumulate(m_counts.begin(), m_counts.end(), 0);
}

int TrainingPlan::totalGoal() const {
    return std::accumulate(m_targets.begin(), m_targets.end(), 0);
}

bool TrainingPlan::increment(std::size_t index) {
    if (index >= m_counts.size()) {
        return false;
    }
    if (m_counts[index] >= target(index)) {
        return false;
    }
    ++m_counts[index];
    return true;
}

void TrainingPlan::reset(std::size_t index) {
    if (index < m_counts.size()) {
        m_counts[index] = 0;
    }
}

void TrainingPlan::resetAll() {
    std::fill(m_counts.begin(), m_counts.end(), 0);
}

void TrainingPlan::setTarget(std::size_t index, int targetValue) {
    if (index >= m_targets.size()) {
        return;
    }
    m_targets[index] = clampTarget(targetValue);
    m_counts[index] = std::min(m_counts[index], m_targets[index]);
}

void TrainingPlan::resetTargetsToDefault() {
    std::fill(m_targets.begin(), m_targets.end(), m_defaultTarget);
    for (std::size_t i = 0; i < m_counts.size(); ++i) {
        m_counts[i] = std::min(m_counts[i], m_targets[i]);
    }
}

int TrainingPlan::recommendedBackwards() const {
    for (int i = static_cast<int>(m_counts.size()) - 1; i >= 0; --i) {
        if (!completed(static_cast<std::size_t>(i))) {
            return i;
        }
    }
    return m_counts.empty() ? 0 : static_cast<int>(m_counts.size()) - 1;
}

int TrainingPlan::previousIncomplete(int fromIndex) const {
    if (m_counts.empty()) {
        return 0;
    }

    fromIndex = std::clamp(fromIndex, 0, static_cast<int>(m_counts.size()) - 1);
    for (int i = fromIndex - 1; i >= 0; --i) {
        if (!completed(static_cast<std::size_t>(i))) {
            return i;
        }
    }

    return recommendedBackwards();
}

std::string TrainingPlan::encodeCounts() const {
    std::ostringstream out;
    for (std::size_t i = 0; i < m_counts.size(); ++i) {
        if (i != 0) {
            out << ',';
        }
        out << m_counts[i];
    }
    return out.str();
}

void TrainingPlan::decodeCounts(std::string const& encoded) {
    if (encoded.empty()) {
        return;
    }

    std::istringstream in(encoded);
    std::string token;
    std::size_t index = 0;

    while (std::getline(in, token, ',') && index < m_counts.size()) {
        try {
            auto value = std::stoi(token);
            m_counts[index] = std::clamp(value, 0, target(index));
        }
        catch (...) {
            m_counts[index] = 0;
        }
        ++index;
    }
}

std::string TrainingPlan::encodeTargets() const {
    std::ostringstream out;
    for (std::size_t i = 0; i < m_targets.size(); ++i) {
        if (i != 0) {
            out << ',';
        }
        // 0 means "use the global default" in persistence. This keeps saves
        // compact and makes later changes to the default repetitions setting
        // automatically propagate to stages without explicit overrides.
        out << (m_targets[i] == m_defaultTarget ? 0 : m_targets[i]);
    }
    return out.str();
}

void TrainingPlan::decodeTargets(std::string const& encoded) {
    if (encoded.empty()) {
        return;
    }

    std::istringstream in(encoded);
    std::string token;
    std::size_t index = 0;
    while (std::getline(in, token, ',') && index < m_targets.size()) {
        try {
            auto value = std::stoi(token);
            m_targets[index] = value <= 0 ? m_defaultTarget : clampTarget(value);
            m_counts[index] = std::min(m_counts[index], m_targets[index]);
        }
        catch (...) {
            m_targets[index] = m_defaultTarget;
        }
        ++index;
    }
}

TrainingSession::TrainingSession(TrainingPlan* plan) : m_plan(plan) {}

void TrainingSession::attach(TrainingPlan* plan) {
    m_plan = plan;
    if (m_plan && m_plan->size() > 0) {
        m_selected = std::clamp(m_selected, 0, static_cast<int>(m_plan->size()) - 1);
    }
    else {
        m_selected = 0;
    }
}

void TrainingSession::setSelected(int index) {
    if (!m_plan || m_plan->size() == 0) {
        m_selected = 0;
        return;
    }
    m_selected = std::clamp(index, 0, static_cast<int>(m_plan->size()) - 1);
}

void TrainingSession::beginAttempt(double percent) {
    m_attemptStart = std::clamp(percent, 0.0, 100.0);
    m_countedThisAttempt = false;
}

std::optional<int> TrainingSession::update(
    double currentPercent,
    bool strictStart,
    double startTolerance
) {
    if (!m_plan || m_plan->size() == 0 || m_countedThisAttempt) {
        return std::nullopt;
    }

    auto const index = static_cast<std::size_t>(m_selected);
    auto const seg = m_plan->segment(index);
    currentPercent = std::clamp(currentPercent, 0.0, 100.0);
    startTolerance = std::max(0.0, startTolerance);

    // The Baconsistent method is about many exact small runs. In strict mode
    // the attempt must begin near this fixed segment's StartPos boundary.
    // Reaching the boundary counts once; the player is free to keep going.
    if (strictStart && std::abs(m_attemptStart - seg.start) > startTolerance) {
        return std::nullopt;
    }

    // Tiny tolerance protects exact boundaries from float rounding.
    if (currentPercent + 0.05 < seg.end) {
        return std::nullopt;
    }

    if (!m_plan->increment(index)) {
        return std::nullopt;
    }

    m_countedThisAttempt = true;
    return m_plan->count(index);
}

} // namespace baconsistent::core
