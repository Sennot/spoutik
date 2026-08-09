#include "StartPosAnalyzer.hpp"
#include "../core/PercentageMath.hpp"

#include <algorithm>
#include <cmath>

using namespace geode::prelude;

namespace baconsistent {

StartPosAnalysis analyzeStartPositions(PlayLayer* layer) {
    StartPosAnalysis result;
    if (!layer || layer->m_levelLength <= 0.f || !layer->m_objects) {
        return result;
    }

    auto const levelLength = layer->m_levelLength;
    auto const levelTime = layer->timeForPos({levelLength, 0.f}, 0.f, 0.f, true, 0.f);

    struct Marker {
        float x = 0.f;
        StartPosObject* object = nullptr;
        double legacy21 = 0.0;
        double modern22 = 0.0;
    };

    std::vector<Marker> markers;
    for (auto child : CCArrayExt<CCObject*>(layer->m_objects)) {
        auto startPos = typeinfo_cast<StartPosObject*>(child);
        if (!startPos) {
            continue;
        }

        auto const startPosX = startPos->getPositionX();
        auto const legacy21 = core::legacy21PercentFromX(startPosX, levelLength);

        auto modern22 = legacy21;
        if (levelTime > 0.f) {
            auto const startPosTime = layer->timeForPos({startPosX, 0.f}, 0.f, 0.f, true, 0.f);
            modern22 = core::modern22PercentFromTime(startPosTime, levelTime);
        }
        markers.push_back({startPosX, startPos, legacy21, modern22});
    }

    std::sort(markers.begin(), markers.end(), [](Marker const& a, Marker const& b) {
        return a.x < b.x;
    });

    // Keep runtime objects in physical order. The persistent boundary arrays
    // are normalized later; activateSelectedStartPos resolves by percentage,
    // so near-duplicate StartPos objects cannot shift stage indexes.
    for (auto const& marker : markers) {
        result.legacy21Percentages.push_back(marker.legacy21);
        result.modern22Percentages.push_back(marker.modern22);
        result.runtimeMarkers.push_back({marker.object, marker.legacy21, marker.modern22});
    }
    return result;
}

double currentPercentForMode(PlayLayer* layer, PercentageMode mode) {
    if (!layer || !layer->m_player1 || layer->m_levelLength <= 0.f) {
        return 0.0;
    }

    auto const playerX = layer->m_player1->getPositionX();
    if (mode == PercentageMode::Legacy21) {
        return core::legacy21PercentFromX(
            static_cast<double>(playerX),
            static_cast<double>(layer->m_levelLength)
        );
    }

    // Use the exact same timeForPos scale as the StartPos scanner. This keeps
    // runtime progress aligned with 2.2 boundaries even around speed portals.
    auto const totalTime = layer->timeForPos({layer->m_levelLength, 0.f}, 0.f, 0.f, true, 0.f);
    if (totalTime <= 0.f) {
        return core::legacy21PercentFromX(
            static_cast<double>(playerX),
            static_cast<double>(layer->m_levelLength)
        );
    }

    auto const currentTime = layer->timeForPos({playerX, 0.f}, 0.f, 0.f, true, 0.f);
    return core::modern22PercentFromTime(currentTime, totalTime);
}

} // namespace baconsistent
