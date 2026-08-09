#pragma once

#include <Geode/Geode.hpp>

#include <vector>

namespace baconsistent {

enum class PercentageMode {
    Modern22,
    Legacy21,
};

struct StartPosRuntimeMarker {
    StartPosObject* object = nullptr;
    double legacy21 = 0.0;
    double modern22 = 0.0;
};

struct StartPosAnalysis {
    std::vector<double> legacy21Percentages;
    std::vector<double> modern22Percentages;
    std::vector<StartPosRuntimeMarker> runtimeMarkers;

    [[nodiscard]] bool foundAny() const {
        return !legacy21Percentages.empty() || !modern22Percentages.empty();
    }
};

// Scan the already-created PlayLayer objects and calculate each StartPos in
// both Geometry Dash percentage systems:
// - 2.1: X / levelLength
// - 2.2: timeForPos(X) / timeForPos(levelLength)
StartPosAnalysis analyzeStartPositions(PlayLayer* layer);

// Runtime progress matching the selected percentage system.
double currentPercentForMode(PlayLayer* layer, PercentageMode mode);

} // namespace baconsistent
