#include "TrainingManager.hpp"
#include "../core/PercentageMath.hpp"

#include <Geode/loader/Mod.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

using namespace geode::prelude;

namespace baconsistent {

namespace {
std::uint64_t fnv1a(std::string_view text, std::uint64_t seed = 14695981039346656037ull) {
    auto hash = seed;
    for (auto const ch : text) {
        hash ^= static_cast<unsigned char>(ch);
        hash *= 1099511628211ull;
    }
    return hash;
}

int configuredFallbackParts() {
    return static_cast<int>(Mod::get()->getSettingValue<int64_t>("parts"));
}

int configuredTarget() {
    return static_cast<int>(Mod::get()->getSettingValue<int64_t>("repetitions"));
}

std::string encodeDoubles(std::vector<double> const& values) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(6);
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i != 0) {
            out << ',';
        }
        out << values[i];
    }
    return out.str();
}

std::vector<double> decodeDoubles(std::string const& encoded) {
    std::vector<double> values;
    if (encoded.empty()) {
        return values;
    }

    std::istringstream in(encoded);
    std::string token;
    while (std::getline(in, token, ',')) {
        try {
            auto value = std::stod(token);
            if (std::isfinite(value)) {
                values.push_back(value);
            }
        }
        catch (...) {
            return {};
        }
    }
    return values;
}

std::string formatPercent(double value) {
    auto const rounded = std::round(value);
    if (std::abs(value - rounded) < 0.05) {
        return fmt::format("{:.0f}", rounded);
    }
    return fmt::format("{:.1f}", value);
}
} // namespace

TrainingManager& TrainingManager::get() {
    static TrainingManager instance;
    return instance;
}

TrainingManager::TrainingManager() : m_plan(10, 20), m_session(&m_plan) {}

bool TrainingManager::enabled() const {
    return m_loaded && Mod::get()->getSettingValue<bool>("enabled");
}

PercentageMode TrainingManager::percentageMode() const {
    return Mod::get()->getSettingValue<bool>("legacy-2-1-percentages")
        ? PercentageMode::Legacy21
        : PercentageMode::Modern22;
}

int TrainingManager::detectedStartPosCount() const {
    if (!m_hasStartPosProfile || m_startPos21Boundaries.size() < 2) {
        return 0;
    }
    return std::max(0, static_cast<int>(m_startPos21Boundaries.size()) - 2);
}

std::string TrainingManager::sourceLabel() const {
    if (!m_usingStartPosBoundaries) {
        return "Equal fallback";
    }
    return percentageMode() == PercentageMode::Legacy21 ? "StartPos / 2.1%" : "StartPos / 2.2%";
}

void TrainingManager::loadLevel(GJGameLevel* level, StartPosAnalysis const& analysis) {
    m_levelKey = makeLevelKey(level);
    m_loaded = !m_levelKey.empty();
    m_activePlanSignature.clear();
    m_startPos21Boundaries.clear();
    m_startPos22Boundaries.clear();
    m_runtimeStartPositions.clear();
    m_hasStartPosProfile = false;
    m_usingStartPosBoundaries = false;
    m_recentCompletedRound = 0;
    m_stats = {};

    if (!m_loaded) {
        return;
    }

    loadCachedStartPosProfile();
    updateStartPosProfile(analysis);
    auto const savedTargets = Mod::get()->getSavedValue<std::string>(saveKey("targets"), "");
    auto const savedCounts = Mod::get()->getSavedValue<std::string>(saveKey("counts"), "");
    auto const savedSignature = Mod::get()->getSavedValue<std::string>(saveKey("plan-signature"), "");
    applyConfiguredPlan(true);
    m_plan.decodeTargets(savedTargets);

    // applyConfiguredPlan loads legacy/default-target counts first. Re-read
    // them after restoring custom targets so a stage such as 25/30 is not
    // temporarily clamped to the global 20 target and lose progress.
    auto const legacyV01EqualCounts = savedSignature.empty() && !m_usingStartPosBoundaries;
    if (savedSignature == m_activePlanSignature || legacyV01EqualCounts) {
        m_plan.decodeCounts(savedCounts);
    }

    m_stats.configureStages(m_plan.size());
    m_stats.decode(Mod::get()->getSavedValue<std::string>(saveKey("stats-v1"), ""));
    m_roundNumber = std::max(1, Mod::get()->getSavedValue<int>(saveKey("round"), 1));

    auto const savedSelected = Mod::get()->getSavedValue<int>(saveKey("selected"), -1);
    if (savedSelected >= 0 && savedSelected < static_cast<int>(m_plan.size())) {
        m_session.setSelected(savedSelected);
    }
    else {
        m_session.setSelected(m_plan.recommendedBackwards());
    }

    // Migration from pre-round versions: a fully completed old plan becomes a
    // finished Round 1 rather than leaving the user stuck at 20/20 everywhere.
    if (m_plan.allCompleted()) {
        advanceRound();
    }

    saveSelected();
    saveProgress();
    saveStats();
    saveRound();
    flushCrashSafe();
}

void TrainingManager::unloadLevel() {
    if (m_loaded) {
        finishAttempt();
        saveProgress();
        saveStats();
        saveRound();
        flushCrashSafe();
    }

    m_loaded = false;
    m_levelKey.clear();
    m_activePlanSignature.clear();
    m_startPos21Boundaries.clear();
    m_startPos22Boundaries.clear();
    m_runtimeStartPositions.clear();
    m_hasStartPosProfile = false;
    m_usingStartPosBoundaries = false;
    m_recentCompletedRound = 0;
}

void TrainingManager::refreshSettings() {
    if (!m_loaded) {
        return;
    }
    applyConfiguredPlan(false);
}

void TrainingManager::beginAttempt(double percent, bool practiceMode) {
    if (!enabled()) {
        return;
    }

    refreshSettings();
    maybeAutoMatchStart(percent);
    m_session.beginAttempt(percent);
    m_attemptProgressCounted = false;
    m_attemptGuard.begin(
        practiceMode,
        Mod::get()->getSettingValue<bool>("practice-mode-protection"),
        Mod::get()->getSettingValue<bool>("noclip-protection")
    );

    m_stats.configureStages(m_plan.size());
    if (m_attemptGuard.countable()) {
        m_stats.beginAttempt(static_cast<std::size_t>(m_session.selected()));
    }
    // Do not update the persisted save container yet: an unfinished attempt
    // should simply disappear if GD crashes. It becomes durable on success or
    // when the attempt ends normally.
}

void TrainingManager::observePracticeMode(bool practiceMode) {
    if (!m_loaded || m_attemptProgressCounted) {
        return;
    }

    auto const wasCountable = m_attemptGuard.countable();
    m_attemptGuard.observePracticeMode(practiceMode);
    if (wasCountable && !m_attemptGuard.countable()) {
        m_stats.cancelAttempt();
        saveStats();
        flushCrashSafe();
    }
}

void TrainingManager::markSuppressedDeath() {
    if (!m_loaded || m_attemptProgressCounted) {
        return;
    }

    auto const wasCountable = m_attemptGuard.countable();
    m_attemptGuard.observeSuppressedDeath();
    if (wasCountable && !m_attemptGuard.countable()) {
        // Death Tracker-style semantics: noclip may be enabled, but the run is
        // invalid only once GD actually attempted a lethal collision and a
        // different hook/mod kept the player alive. Invalid attempts are
        // removed from stats instead of being counted as failures.
        m_stats.cancelAttempt();
        saveStats();
        flushCrashSafe();
    }
}

void TrainingManager::tick(double dt) {
    if (!enabled() || !m_attemptGuard.countable()) {
        return;
    }
    m_stats.tick(dt);
}

void TrainingManager::finishAttempt() {
    if (!m_loaded) {
        return;
    }

    if (m_attemptGuard.countable()) {
        m_stats.finishAttempt();
    }
    else {
        m_stats.cancelAttempt();
    }
    saveStats();
    // Geode saved values normally reach disk on game shutdown. Force an early
    // write at attempt boundaries so a GD crash cannot wipe the last training
    // progress/statistics batch.
    flushCrashSafe();
}

std::optional<ProgressEvent> TrainingManager::update(double currentPercent) {
    if (!enabled() || !m_attemptGuard.countable()) {
        return std::nullopt;
    }

    auto const indexBefore = m_session.selected();
    auto const target = m_plan.target(static_cast<std::size_t>(indexBefore));
    auto result = m_session.update(
        currentPercent,
        Mod::get()->getSettingValue<bool>("strict-start"),
        Mod::get()->getSettingValue<double>("start-tolerance")
    );

    if (!result) {
        return std::nullopt;
    }

    // The requested A -> B range is already clean at this exact moment. Any
    // later noclip collision or Practice toggle during the optional continued
    // long run must not revoke the fixed-stage completion.
    m_attemptProgressCounted = true;
    m_stats.markSuccess(static_cast<std::size_t>(indexBefore));

    ProgressEvent event;
    event.segmentIndex = indexBefore;
    event.repetitions = *result;
    event.target = target;
    event.segmentCompleted = m_plan.completed(static_cast<std::size_t>(indexBefore));
    event.planCompleted = m_plan.allCompleted();
    event.newRound = m_roundNumber;

    if (event.planCompleted) {
        event.roundCompleted = true;
        event.completedRound = m_roundNumber;
        advanceRound();
        event.newRound = m_roundNumber;
    }
    else if (event.segmentCompleted && Mod::get()->getSettingValue<bool>("auto-previous")) {
        m_session.setSelected(m_plan.previousIncomplete(indexBefore));
        saveSelected();
    }

    saveProgress();
    saveStats();
    saveRound();
    // Progress is the most valuable data in the mod. Flush it immediately
    // after every successful fixed-part completion instead of waiting for GD
    // to exit cleanly.
    flushCrashSafe();
    return event;
}

core::Segment TrainingManager::selectedSegment() const {
    return m_plan.segment(static_cast<std::size_t>(m_session.selected()));
}

int TrainingManager::liveSegmentIndex() const {
    if (!m_loaded || m_plan.size() == 0) {
        return 0;
    }

    auto* layer = PlayLayer::get();
    if (!layer || !m_usingStartPosBoundaries || m_runtimeStartPositions.empty()) {
        return std::clamp(m_session.selected(), 0, static_cast<int>(m_plan.size()) - 1);
    }

    auto* active = layer->m_startPosObject;
    if (!active) {
        // No StartPos selected means a run from zero: that is stage 0.
        return 0;
    }

    auto marker = std::find_if(
        m_runtimeStartPositions.begin(),
        m_runtimeStartPositions.end(),
        [active](StartPosRuntimeMarker const& value) { return value.object == active; }
    );

    if (marker == m_runtimeStartPositions.end()) {
        return std::clamp(m_session.selected(), 0, static_cast<int>(m_plan.size()) - 1);
    }

    auto const markerPercent = percentageMode() == PercentageMode::Legacy21
        ? marker->legacy21
        : marker->modern22;

    auto bestIndex = 0;
    auto bestDistance = std::numeric_limits<double>::infinity();
    for (int i = 0; i < static_cast<int>(m_plan.size()); ++i) {
        auto const distance = std::abs(m_plan.segment(static_cast<std::size_t>(i)).start - markerPercent);
        if (distance < bestDistance) {
            bestDistance = distance;
            bestIndex = i;
        }
    }

    return bestIndex;
}

void TrainingManager::select(int index) {
    if (!m_loaded) {
        return;
    }
    m_session.setSelected(index);
    saveSelected();
}

void TrainingManager::selectRelative(int delta) {
    if (!m_loaded || m_plan.size() == 0) {
        return;
    }

    auto index = m_session.selected() + delta;
    auto const size = static_cast<int>(m_plan.size());
    while (index < 0) index += size;
    while (index >= size) index -= size;
    select(index);
}

void TrainingManager::selectRecommended() {
    if (!m_loaded) {
        return;
    }
    m_session.setSelected(m_plan.recommendedBackwards());
    saveSelected();
}

void TrainingManager::resetSelected() {
    if (!m_loaded) {
        return;
    }
    m_plan.reset(static_cast<std::size_t>(m_session.selected()));
    saveProgress();
    flushCrashSafe();
}

void TrainingManager::resetRoundProgress() {
    if (!m_loaded) {
        return;
    }
    m_plan.resetAll();
    m_stats.resetCurrentRound();
    m_recentCompletedRound = 0;
    m_session.setSelected(m_plan.recommendedBackwards());
    saveSelected();
    saveProgress();
    saveStats();
    flushCrashSafe();
}

void TrainingManager::adjustSelectedTarget(int delta) {
    if (!m_loaded || m_plan.size() == 0 || delta == 0) {
        return;
    }
    auto const index = static_cast<std::size_t>(m_session.selected());
    m_plan.setTarget(index, m_plan.target(index) + delta);
    if (m_plan.allCompleted()) {
        advanceRound();
    }
    else {
        saveProgress();
        flushCrashSafe();
    }
}

void TrainingManager::resetSelectedTarget() {
    if (!m_loaded || m_plan.size() == 0) {
        return;
    }
    auto const index = static_cast<std::size_t>(m_session.selected());
    m_plan.setTarget(index, m_plan.target());
    if (m_plan.allCompleted()) {
        advanceRound();
    }
    else {
        saveProgress();
        flushCrashSafe();
    }
}

int TrainingManager::selectedTarget() const {
    if (!m_loaded || m_plan.size() == 0) {
        return m_plan.target();
    }
    return m_plan.target(static_cast<std::size_t>(m_session.selected()));
}

bool TrainingManager::canActivateSelectedStartPos() const {
    if (!m_loaded || !m_usingStartPosBoundaries || !PlayLayer::get()) {
        return false;
    }
    if (m_session.selected() == 0) {
        return true;
    }
    return markerForSegmentStart(m_session.selected()) != nullptr;
}

bool TrainingManager::activateSelectedStartPos() {
    auto layer = PlayLayer::get();
    if (!layer || !m_loaded || !m_usingStartPosBoundaries) {
        return false;
    }

    if (m_session.selected() == 0) {
        layer->setStartPosObject(nullptr);
        layer->resetLevel();
        return true;
    }

    auto marker = markerForSegmentStart(m_session.selected());
    if (!marker || !marker->object) {
        return false;
    }

    layer->setStartPosObject(marker->object);
    layer->resetLevel();
    return true;
}

std::string TrainingManager::segmentRangeText(int index) const {
    if (m_plan.size() == 0) {
        return "-";
    }
    index = std::clamp(index, 0, static_cast<int>(m_plan.size()) - 1);
    auto const seg = m_plan.segment(static_cast<std::size_t>(index));
    return fmt::format("{}-{}%", formatPercent(seg.start), formatPercent(seg.end));
}

std::string TrainingManager::compactSegmentText(int index) const {
    if (m_plan.size() == 0) {
        return "-";
    }
    index = std::clamp(index, 0, static_cast<int>(m_plan.size()) - 1);
    return fmt::format(
        "{}  {}/{}",
        segmentRangeText(index),
        m_plan.count(static_cast<std::size_t>(index)),
        m_plan.target(static_cast<std::size_t>(index))
    );
}

std::string TrainingManager::makeLevelKey(GJGameLevel* level) const {
    if (!level) {
        return {};
    }

    auto const originalID = level->m_originalLevel.value();
    if (originalID > 0) {
        return fmt::format("online:{}", originalID);
    }

    auto const id = level->m_levelID.value();
    if (id > 0) {
        return fmt::format("online:{}", id);
    }

    std::string const name = level->m_levelName.c_str();
    std::string const data = level->m_levelString.c_str();
    auto hash = fnv1a(name);
    hash = fnv1a(data, hash);
    return fmt::format("local:{:016x}", hash);
}

std::string TrainingManager::saveKey(std::string_view suffix) const {
    return fmt::format("levels.{}.{}", m_levelKey, suffix);
}

void TrainingManager::saveProgress() {
    if (!m_loaded) {
        return;
    }
    Mod::get()->setSavedValue(saveKey("counts"), m_plan.encodeCounts());
    Mod::get()->setSavedValue(saveKey("targets"), m_plan.encodeTargets());
    Mod::get()->setSavedValue(saveKey("plan-signature"), m_activePlanSignature);
}

void TrainingManager::saveSelected() {
    if (!m_loaded) {
        return;
    }
    Mod::get()->setSavedValue(saveKey("selected"), m_session.selected());
}

void TrainingManager::saveStats() {
    if (!m_loaded) {
        return;
    }
    Mod::get()->setSavedValue(saveKey("stats-v1"), m_stats.encode());
}

void TrainingManager::saveRound() {
    if (!m_loaded) {
        return;
    }
    Mod::get()->setSavedValue(saveKey("round"), m_roundNumber);
}

void TrainingManager::flushCrashSafe() {
    if (!m_loaded) {
        return;
    }

    if (auto result = Mod::get()->saveData(); result.isErr()) {
        log::warn("Baconsistent: early save failed: {}", result.unwrapErr());
    }
}

void TrainingManager::maybeAutoMatchStart(double percent) {
    if (!Mod::get()->getSettingValue<bool>("auto-match-start") || percent <= 0.5 || m_plan.size() == 0) {
        return;
    }

    auto const tolerance = Mod::get()->getSettingValue<double>("start-tolerance");
    auto bestDistance = std::numeric_limits<double>::infinity();
    auto candidate = -1;

    for (int i = 0; i < static_cast<int>(m_plan.size()); ++i) {
        auto const start = m_plan.segment(static_cast<std::size_t>(i)).start;
        auto const distance = std::abs(percent - start);
        if (distance < bestDistance) {
            bestDistance = distance;
            candidate = i;
        }
    }

    if (candidate >= 0 && bestDistance <= tolerance) {
        m_session.setSelected(candidate);
        saveSelected();
    }
}

void TrainingManager::advanceRound() {
    auto const completedRound = m_roundNumber;
    m_stats.completeRound(completedRound);
    m_recentCompletedRound = completedRound;
    ++m_roundNumber;

    m_plan.resetAll();
    m_session.setSelected(m_plan.recommendedBackwards());
    saveSelected();
    saveProgress();
    saveStats();
    saveRound();
    flushCrashSafe();
}

void TrainingManager::loadCachedStartPosProfile() {
    auto legacy = decodeDoubles(Mod::get()->getSavedValue<std::string>(saveKey("startpos-2-1"), ""));
    auto modern = decodeDoubles(Mod::get()->getSavedValue<std::string>(saveKey("startpos-2-2"), ""));

    if (legacy.size() >= 3 && modern.size() == legacy.size()) {
        legacy.erase(legacy.begin());
        legacy.pop_back();
        modern.erase(modern.begin());
        modern.pop_back();
        auto normalized = core::normalizeDualPercentMarkers(std::move(legacy), std::move(modern));
        if (normalized.legacy21.size() >= 3 && normalized.modern22.size() == normalized.legacy21.size()) {
            m_startPos21Boundaries = std::move(normalized.legacy21);
            m_startPos22Boundaries = std::move(normalized.modern22);
            m_hasStartPosProfile = true;
        }
    }
}

void TrainingManager::updateStartPosProfile(StartPosAnalysis const& analysis) {
    m_runtimeStartPositions = analysis.runtimeMarkers;
    if (!analysis.foundAny()) {
        return;
    }

    auto normalized = core::normalizeDualPercentMarkers(
        analysis.legacy21Percentages,
        analysis.modern22Percentages
    );

    if (normalized.legacy21.size() < 3 || normalized.modern22.size() != normalized.legacy21.size()) {
        return;
    }

    m_startPos21Boundaries = std::move(normalized.legacy21);
    m_startPos22Boundaries = std::move(normalized.modern22);
    m_hasStartPosProfile = true;

    Mod::get()->setSavedValue(saveKey("startpos-2-1"), encodeDoubles(m_startPos21Boundaries));
    Mod::get()->setSavedValue(saveKey("startpos-2-2"), encodeDoubles(m_startPos22Boundaries));
}

std::vector<double> TrainingManager::activeBoundaries() const {
    auto const useStartPos = Mod::get()->getSettingValue<bool>("auto-detect-startposes") && m_hasStartPosProfile;
    if (useStartPos) {
        return percentageMode() == PercentageMode::Legacy21
            ? m_startPos21Boundaries
            : m_startPos22Boundaries;
    }

    auto const parts = std::clamp(configuredFallbackParts(), 2, 20);
    std::vector<double> equal;
    equal.reserve(static_cast<std::size_t>(parts) + 1);
    for (int i = 0; i <= parts; ++i) {
        equal.push_back(i == parts ? 100.0 : (100.0 * static_cast<double>(i) / static_cast<double>(parts)));
    }
    return equal;
}

std::string TrainingManager::configuredPlanSignature() const {
    auto const useStartPos = Mod::get()->getSettingValue<bool>("auto-detect-startposes") && m_hasStartPosProfile;
    if (useStartPos) {
        return "startpos:" + encodeDoubles(m_startPos21Boundaries);
    }
    return fmt::format("equal:{}", std::clamp(configuredFallbackParts(), 2, 20));
}

void TrainingManager::applyConfiguredPlan(bool initialLoad) {
    auto const target = configuredTarget();
    auto const boundaries = activeBoundaries();
    auto const signature = configuredPlanSignature();
    auto const savedSignature = Mod::get()->getSavedValue<std::string>(saveKey("plan-signature"), "");

    auto const layoutChanged = initialLoad
        ? (!savedSignature.empty() && savedSignature != signature)
        : (!m_activePlanSignature.empty() && m_activePlanSignature != signature);

    m_plan.reconfigureBoundaries(boundaries, target);
    m_session.attach(&m_plan);
    m_usingStartPosBoundaries = Mod::get()->getSettingValue<bool>("auto-detect-startposes") && m_hasStartPosProfile;

    if (initialLoad) {
        m_plan.resetAll();
        auto const legacyV01EqualCounts = savedSignature.empty() && !m_usingStartPosBoundaries;
        if ((!layoutChanged && savedSignature == signature) || legacyV01EqualCounts) {
            m_plan.decodeCounts(Mod::get()->getSavedValue<std::string>(saveKey("counts"), ""));
        }
    }
    else if (layoutChanged) {
        m_plan.resetAll();
        m_plan.resetTargetsToDefault();
        m_session.setSelected(m_plan.recommendedBackwards());
        m_stats.clearStageStats();
        saveSelected();
    }

    m_stats.configureStages(m_plan.size());
    m_activePlanSignature = signature;
    saveProgress();
}

StartPosRuntimeMarker const* TrainingManager::markerForSegmentStart(int segmentIndex) const {
    if (segmentIndex <= 0 || m_runtimeStartPositions.empty() || m_plan.size() == 0) {
        return nullptr;
    }

    auto const start = m_plan.segment(static_cast<std::size_t>(segmentIndex)).start;
    auto const mode = percentageMode();
    StartPosRuntimeMarker const* best = nullptr;
    auto bestDistance = std::numeric_limits<double>::infinity();

    for (auto const& marker : m_runtimeStartPositions) {
        auto const value = mode == PercentageMode::Legacy21 ? marker.legacy21 : marker.modern22;
        auto const distance = std::abs(value - start);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = &marker;
        }
    }

    // Boundary normalization only removes markers within 0.01%. Keep a wider
    // runtime tolerance for float/timeForPos rounding without selecting a
    // genuinely different StartPos.
    return bestDistance <= 0.25 ? best : nullptr;
}

} // namespace baconsistent
