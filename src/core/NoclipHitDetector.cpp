#include "NoclipHitDetector.hpp"

namespace baconsistent::core {

void NoclipHitDetector::reset() {
    m_framesSinceReset = 0;
    m_haveBaselineObject = false;
    m_baselineObject = 0;
}

void NoclipHitDetector::advanceFrame() {
    if (m_framesSinceReset < 1000000) {
        ++m_framesSinceReset;
    }
}

bool NoclipHitDetector::observe(
    std::uintptr_t objectToken,
    bool wasAlive,
    bool aliveAfter,
    bool levelEnding,
    bool nativeIgnoreDamage
) {
    // StartPos selection/reset can cause bookkeeping destroyPlayer calls before
    // gameplay has actually resumed. Ignore that whole bootstrap window.
    if (!armed() || objectToken == 0 || !wasAlive || !aliveAfter || levelEnding) {
        return false;
    }

    // Native/commonly-used noclip paths expose GD's ignore-damage flags. In
    // that case a live return from a real collision object is enough evidence
    // on the very first hit.
    if (nativeIgnoreDamage) {
        return true;
    }

    // Conservative fallback copied from Death Tracker's idea: the first
    // alive destroyPlayer object becomes a baseline instead of immediately
    // accusing noclip. A later different collision object surviving the same
    // attempt is strong evidence that another hook is suppressing deaths.
    if (!m_haveBaselineObject) {
        m_haveBaselineObject = true;
        m_baselineObject = objectToken;
        return false;
    }

    return objectToken != m_baselineObject;
}

} // namespace baconsistent::core
