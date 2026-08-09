#include "TrainingStats.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace baconsistent::core {

namespace {
constexpr std::size_t kMaxRoundHistory = 5;

std::vector<std::string> split(std::string const& text, char delimiter) {
    std::vector<std::string> out;
    std::istringstream in(text);
    std::string token;
    while (std::getline(in, token, delimiter)) {
        out.push_back(token);
    }
    return out;
}

std::string encodeAggregate(AggregateStats const& stats) {
    std::ostringstream out;
    out << stats.attempts << ','
        << stats.successes << ','
        << stats.currentStreak << ','
        << stats.bestStreak << ','
        << std::fixed << std::setprecision(3) << stats.playtimeSeconds;
    return out.str();
}

bool decodeAggregate(std::string const& text, AggregateStats& out) {
    auto parts = split(text, ',');
    if (parts.size() != 5) {
        return false;
    }

    try {
        AggregateStats parsed;
        parsed.attempts = std::max(0, std::stoi(parts[0]));
        parsed.successes = std::clamp(std::stoi(parts[1]), 0, parsed.attempts);
        parsed.currentStreak = std::max(0, std::stoi(parts[2]));
        parsed.bestStreak = std::max(parsed.currentStreak, std::stoi(parts[3]));
        parsed.playtimeSeconds = std::max(0.0, std::stod(parts[4]));
        if (!std::isfinite(parsed.playtimeSeconds)) {
            return false;
        }
        out = parsed;
        return true;
    }
    catch (...) {
        return false;
    }
}
} // namespace

double AggregateStats::successRate() const {
    if (attempts <= 0) {
        return 0.0;
    }
    return 100.0 * static_cast<double>(successes) / static_cast<double>(attempts);
}

void TrainingStats::configureStages(std::size_t count) {
    m_stages.resize(count);
    if (m_stages.empty()) {
        m_attemptActive = false;
        m_attemptSucceeded = false;
        m_activeStage = 0;
    }
    else if (m_activeStage >= m_stages.size()) {
        m_activeStage = m_stages.size() - 1;
    }
}

void TrainingStats::clearStageStats() {
    for (auto& stats : m_stages) {
        stats = {};
    }
    m_attemptActive = false;
    m_attemptSucceeded = false;
}

void TrainingStats::resetCurrentRound() {
    m_round = {};
    m_attemptActive = false;
    m_attemptSucceeded = false;
    for (auto& stageStats : m_stages) {
        stageStats.currentStreak = 0;
    }
}

void TrainingStats::beginAttempt(std::size_t stageIndex) {
    if (m_stages.empty()) {
        return;
    }

    // Never let two attempts overlap in the accounting model.
    finishAttempt();

    m_activeStage = std::min(stageIndex, m_stages.size() - 1);
    m_attemptActive = true;
    m_attemptSucceeded = false;
    m_activeAttemptPlaytime = 0.0;

    ++m_lifetime.attempts;
    ++m_round.attempts;
    ++m_stages[m_activeStage].attempts;
}

void TrainingStats::tick(double dt) {
    if (!m_attemptActive || !std::isfinite(dt) || dt <= 0.0) {
        return;
    }

    // A giant frame hitch should not inflate training time by multiple seconds.
    dt = std::min(dt, 0.25);
    m_activeAttemptPlaytime += dt;
    m_lifetime.playtimeSeconds += dt;
    m_round.playtimeSeconds += dt;
    if (m_activeStage < m_stages.size()) {
        m_stages[m_activeStage].playtimeSeconds += dt;
    }
}

void TrainingStats::recordSuccess(AggregateStats& stats) {
    ++stats.successes;
    ++stats.currentStreak;
    stats.bestStreak = std::max(stats.bestStreak, stats.currentStreak);
}

void TrainingStats::recordFailure(AggregateStats& stats) {
    stats.currentStreak = 0;
}

void TrainingStats::markSuccess(std::size_t stageIndex) {
    if (!m_attemptActive || m_attemptSucceeded || m_stages.empty()) {
        return;
    }

    stageIndex = std::min(stageIndex, m_stages.size() - 1);
    if (stageIndex != m_activeStage) {
        // A stage switch while paused does not retroactively reclassify the
        // already-running attempt. The next restart will start a fresh attempt.
        return;
    }

    recordSuccess(m_lifetime);
    recordSuccess(m_round);
    recordSuccess(m_stages[stageIndex]);
    m_attemptSucceeded = true;
}

void TrainingStats::finishAttempt() {
    if (!m_attemptActive) {
        return;
    }

    if (!m_attemptSucceeded) {
        recordFailure(m_lifetime);
        recordFailure(m_round);
        if (m_activeStage < m_stages.size()) {
            recordFailure(m_stages[m_activeStage]);
        }
    }

    m_attemptActive = false;
    m_attemptSucceeded = false;
    m_activeAttemptPlaytime = 0.0;
}

void TrainingStats::cancelAttempt() {
    if (!m_attemptActive) {
        return;
    }

    // Invalid attempts (protected Practice Mode / a noclip-suppressed death)
    // should be invisible to training stats, not counted as failures.
    m_lifetime.attempts = std::max(0, m_lifetime.attempts - 1);
    m_round.attempts = std::max(0, m_round.attempts - 1);
    m_lifetime.playtimeSeconds = std::max(0.0, m_lifetime.playtimeSeconds - m_activeAttemptPlaytime);
    m_round.playtimeSeconds = std::max(0.0, m_round.playtimeSeconds - m_activeAttemptPlaytime);
    if (m_activeStage < m_stages.size()) {
        m_stages[m_activeStage].attempts = std::max(0, m_stages[m_activeStage].attempts - 1);
        m_stages[m_activeStage].playtimeSeconds = std::max(
            0.0,
            m_stages[m_activeStage].playtimeSeconds - m_activeAttemptPlaytime
        );
    }

    m_attemptActive = false;
    m_attemptSucceeded = false;
    m_activeAttemptPlaytime = 0.0;
}

RoundSummary TrainingStats::completeRound(int roundNumber) {
    // The final successful attempt belongs to the completed round, but it must
    // not later be treated as a failed attempt when the player eventually dies.
    m_attemptActive = false;
    m_attemptSucceeded = false;
    m_activeAttemptPlaytime = 0.0;

    RoundSummary summary;
    summary.round = std::max(1, roundNumber);
    summary.stats = m_round;
    m_history.push_back(summary);
    if (m_history.size() > kMaxRoundHistory) {
        m_history.erase(m_history.begin(), m_history.begin() + (m_history.size() - kMaxRoundHistory));
    }

    m_round = {};
    for (auto& stageStats : m_stages) {
        stageStats.currentStreak = 0;
    }
    return summary;
}

AggregateStats const& TrainingStats::stage(std::size_t index) const {
    static AggregateStats empty;
    if (index >= m_stages.size()) {
        return empty;
    }
    return m_stages[index];
}

std::string TrainingStats::encode() const {
    std::ostringstream out;
    out << "v1|" << encodeAggregate(m_lifetime)
        << '|' << encodeAggregate(m_round)
        << '|';

    for (std::size_t i = 0; i < m_stages.size(); ++i) {
        if (i != 0) out << ';';
        out << encodeAggregate(m_stages[i]);
    }

    out << '|';
    for (std::size_t i = 0; i < m_history.size(); ++i) {
        if (i != 0) out << ';';
        out << m_history[i].round << ':' << encodeAggregate(m_history[i].stats);
    }
    return out.str();
}

void TrainingStats::decode(std::string const& encoded) {
    if (encoded.empty()) {
        return;
    }

    auto sections = split(encoded, '|');
    if (sections.size() != 5 || sections[0] != "v1") {
        return;
    }

    AggregateStats lifetime;
    AggregateStats round;
    if (!decodeAggregate(sections[1], lifetime) || !decodeAggregate(sections[2], round)) {
        return;
    }

    std::vector<AggregateStats> stages;
    if (!sections[3].empty()) {
        for (auto const& item : split(sections[3], ';')) {
            AggregateStats stats;
            if (!decodeAggregate(item, stats)) {
                return;
            }
            stages.push_back(stats);
        }
    }

    std::vector<RoundSummary> history;
    if (!sections[4].empty()) {
        for (auto const& item : split(sections[4], ';')) {
            auto const pos = item.find(':');
            if (pos == std::string::npos) {
                return;
            }
            try {
                RoundSummary summary;
                summary.round = std::max(1, std::stoi(item.substr(0, pos)));
                if (!decodeAggregate(item.substr(pos + 1), summary.stats)) {
                    return;
                }
                history.push_back(summary);
            }
            catch (...) {
                return;
            }
        }
    }

    m_lifetime = lifetime;
    m_round = round;
    m_history = std::move(history);
    if (m_history.size() > kMaxRoundHistory) {
        m_history.erase(m_history.begin(), m_history.begin() + (m_history.size() - kMaxRoundHistory));
    }

    auto const keep = std::min(m_stages.size(), stages.size());
    for (std::size_t i = 0; i < keep; ++i) {
        m_stages[i] = stages[i];
    }

    m_attemptActive = false;
    m_attemptSucceeded = false;
    m_activeAttemptPlaytime = 0.0;
}

std::string formatDuration(double seconds) {
    seconds = std::max(0.0, seconds);
    auto total = static_cast<int>(std::round(seconds));
    auto const hours = total / 3600;
    auto const minutes = (total % 3600) / 60;
    auto const secs = total % 60;

    std::ostringstream out;
    if (hours > 0) {
        out << hours << 'h' << ' ' << std::setw(2) << std::setfill('0') << minutes << 'm';
    }
    else if (minutes > 0) {
        out << minutes << 'm' << ' ' << std::setw(2) << std::setfill('0') << secs << 's';
    }
    else {
        out << secs << 's';
    }
    return out.str();
}

} // namespace baconsistent::core
