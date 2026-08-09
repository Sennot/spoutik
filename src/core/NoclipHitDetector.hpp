#pragma once

#include <cstdint>

namespace baconsistent::core {

// Filters PlayLayer::destroyPlayer noise from real noclip-assisted hits.
// Geometry Dash can call destroyPlayer during StartPos/reset plumbing while
// the player remains alive. Treating every such call as noclip causes false
// positives. This detector mirrors Death Tracker's conservative baseline idea
// and also ignores calls before the first gameplay update of a fresh attempt.
class NoclipHitDetector {
public:
    void reset();
    void advanceFrame();

    // Returns true only when the current call is strong evidence that a lethal
    // hit was suppressed. objectToken is normally the GameObject pointer cast
    // to uintptr_t; zero means no collision object and is always ignored.
    [[nodiscard]] bool observe(
        std::uintptr_t objectToken,
        bool wasAlive,
        bool aliveAfter,
        bool levelEnding,
        bool nativeIgnoreDamage
    );

    [[nodiscard]] bool armed() const { return m_framesSinceReset > 0; }

private:
    int m_framesSinceReset = 0;
    bool m_haveBaselineObject = false;
    std::uintptr_t m_baselineObject = 0;
};

} // namespace baconsistent::core
