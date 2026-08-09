#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace baconsistent::core {

inline double legacy21PercentFromX(double x, double levelLength) {
    if (!std::isfinite(x) || !std::isfinite(levelLength) || levelLength <= 0.0) {
        return 0.0;
    }
    return std::clamp((x / levelLength) * 100.0, 0.0, 100.0);
}

inline double modern22PercentFromTime(double timeAtX, double totalTime) {
    if (!std::isfinite(timeAtX) || !std::isfinite(totalTime) || totalTime <= 0.0) {
        return 0.0;
    }
    return std::clamp((timeAtX / totalTime) * 100.0, 0.0, 100.0);
}

struct DualPercentBoundaries {
    std::vector<double> legacy21;
    std::vector<double> modern22;
};

// Normalize a physical StartPos list as a PAIR of percentage systems. A marker
// is kept only when it creates a non-zero segment in both systems, so indexes
// remain identical when the user switches 2.1 <-> 2.2 and saved counters stay
// attached to the same physical StartPos interval.
inline DualPercentBoundaries normalizeDualPercentMarkers(
    std::vector<double> legacy21Markers,
    std::vector<double> modern22Markers
) {
    constexpr double epsilon = 0.01;
    constexpr std::size_t maxInnerMarkers = 99; // 100 trainable parts max.

    DualPercentBoundaries out;
    if (legacy21Markers.size() != modern22Markers.size()) {
        return out;
    }

    std::vector<std::pair<double, double>> pairs;
    pairs.reserve(legacy21Markers.size());
    for (std::size_t i = 0; i < legacy21Markers.size(); ++i) {
        auto const legacy = legacy21Markers[i];
        auto const modern = modern22Markers[i];
        if (!std::isfinite(legacy) || !std::isfinite(modern)) {
            continue;
        }
        pairs.emplace_back(
            std::clamp(legacy, 0.0, 100.0),
            std::clamp(modern, 0.0, 100.0)
        );
    }

    // X-based 2.1 percentage represents physical left-to-right ordering.
    std::sort(pairs.begin(), pairs.end(), [](auto const& a, auto const& b) {
        if (std::abs(a.first - b.first) <= epsilon) {
            return a.second < b.second;
        }
        return a.first < b.first;
    });

    out.legacy21.push_back(0.0);
    out.modern22.push_back(0.0);

    for (auto const& [legacy, modern] : pairs) {
        if (out.legacy21.size() > maxInnerMarkers) {
            break;
        }

        // 0 and 100 are implicit. Also drop duplicate / near-duplicate physical
        // markers, or a marker that collapses to the same 2.2 time percentage.
        if (legacy <= epsilon || modern <= epsilon ||
            legacy >= 100.0 - epsilon || modern >= 100.0 - epsilon) {
            continue;
        }
        if (legacy - out.legacy21.back() <= epsilon ||
            modern - out.modern22.back() <= epsilon) {
            continue;
        }

        out.legacy21.push_back(legacy);
        out.modern22.push_back(modern);
    }

    out.legacy21.push_back(100.0);
    out.modern22.push_back(100.0);
    return out;
}

} // namespace baconsistent::core
