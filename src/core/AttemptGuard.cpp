#include "AttemptGuard.hpp"

namespace baconsistent::core {

void AttemptGuard::begin(bool practiceMode, bool protectPractice, bool protectNoclip) {
    m_protectPractice = protectPractice;
    m_protectNoclip = protectNoclip;
    m_blockedByPractice = m_protectPractice && practiceMode;
    m_blockedByNoclip = false;
}

void AttemptGuard::observePracticeMode(bool practiceMode) {
    // Once practice was used during a protected attempt, keep the whole
    // attempt invalid until a real restart. Toggling practice back off cannot
    // retroactively make a checkpoint-assisted run legitimate.
    if (m_protectPractice && practiceMode) {
        m_blockedByPractice = true;
    }
}

void AttemptGuard::observeSuppressedDeath() {
    if (m_protectNoclip) {
        m_blockedByNoclip = true;
    }
}

bool AttemptGuard::countable() const {
    return !m_blockedByPractice && !m_blockedByNoclip;
}

} // namespace baconsistent::core
