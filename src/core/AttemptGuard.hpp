#pragma once

namespace baconsistent::core {

// Per-attempt validity gate used by runtime hooks. It deliberately does not
// care whether a noclip mod is merely enabled: an attempt is blocked only when
// GD actually tried to kill the player and the death was suppressed.
class AttemptGuard {
public:
    void begin(bool practiceMode, bool protectPractice, bool protectNoclip);
    void observePracticeMode(bool practiceMode);
    void observeSuppressedDeath();

    [[nodiscard]] bool countable() const;
    [[nodiscard]] bool blockedByPractice() const { return m_blockedByPractice; }
    [[nodiscard]] bool blockedByNoclip() const { return m_blockedByNoclip; }
    [[nodiscard]] bool practiceProtectionEnabled() const { return m_protectPractice; }
    [[nodiscard]] bool noclipProtectionEnabled() const { return m_protectNoclip; }

private:
    bool m_protectPractice = true;
    bool m_protectNoclip = true;
    bool m_blockedByPractice = false;
    bool m_blockedByNoclip = false;
};

} // namespace baconsistent::core
