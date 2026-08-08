#include "LayoutMirror.hpp"
#include "layout_mode.hpp"
#include <Geode/loader/Mod.hpp>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <limits>
#include <string_view>
#include <unordered_map>

#ifdef GEODE_IS_WINDOWS
#include <GL/gl.h>
#endif

using namespace geode::prelude;

namespace {
    constexpr double kPositionQuantization = 1000.0;

    struct ObjectKey {
        int id = 0;
        long long x = 0;
        long long y = 0;

        bool operator==(ObjectKey const& other) const {
            return id == other.id && x == other.x && y == other.y;
        }
    };

    struct ObjectKeyHash {
        std::size_t operator()(ObjectKey const& key) const noexcept {
            auto h1 = std::hash<int>{}(key.id);
            auto h2 = std::hash<long long>{}(key.x);
            auto h3 = std::hash<long long>{}(key.y);
            return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2))
                      ^ (h3 + 0x9e3779b97f4a7c15ULL + (h2 << 6) + (h2 >> 2));
        }
    };

    struct LayoutRecord {
        bool hidden = false;
    };

    long long quantize(double value) {
        return std::llround(value * kPositionQuantization);
    }

    std::vector<std::string_view> splitView(std::string const& value, char delimiter) {
        std::vector<std::string_view> out;
        std::size_t begin = 0;
        while (begin <= value.size()) {
            auto end = value.find(delimiter, begin);
            if (end == std::string::npos) end = value.size();
            out.emplace_back(value.data() + begin, end - begin);
            if (end == value.size()) break;
            begin = end + 1;
        }
        return out;
    }

    bool parseInt(std::string_view text, int& out) {
        if (text.empty()) return false;
        std::string tmp(text);
        char* end = nullptr;
        auto value = std::strtol(tmp.c_str(), &end, 10);
        if (!end || *end != '\0') return false;
        out = static_cast<int>(value);
        return true;
    }

    bool parseDouble(std::string_view text, double& out) {
        if (text.empty()) return false;
        std::string tmp(text);
        char* end = nullptr;
        auto value = std::strtod(tmp.c_str(), &end);
        if (!end || *end != '\0') return false;
        out = value;
        return true;
    }

    bool parseLayoutRecord(std::string_view record, ObjectKey& key, bool& hidden) {
        if (record.empty()) return false;
        std::vector<std::string_view> parts;
        std::size_t begin = 0;
        while (begin <= record.size()) {
            auto end = record.find(',', begin);
            if (end == std::string_view::npos) end = record.size();
            parts.emplace_back(record.substr(begin, end - begin));
            if (end == record.size()) break;
            begin = end + 1;
        }

        int id = 0;
        double x = std::numeric_limits<double>::quiet_NaN();
        double y = std::numeric_limits<double>::quiet_NaN();
        hidden = false;

        for (std::size_t i = 0; i + 1 < parts.size(); i += 2) {
            int property = 0;
            if (!parseInt(parts[i], property)) continue;
            if (property == 1) parseInt(parts[i + 1], id);
            else if (property == 2) parseDouble(parts[i + 1], x);
            else if (property == 3) parseDouble(parts[i + 1], y);
            else if (property == 135) {
                int v = 0;
                if (parseInt(parts[i + 1], v)) hidden = v != 0;
            }
        }

        if (id == 0 || !std::isfinite(x) || !std::isfinite(y)) return false;
        key = { id, quantize(x), quantize(y) };
        return true;
    }
}

LayoutMirror& LayoutMirror::get() {
    static LayoutMirror instance;
    return instance;
}

void LayoutMirror::clear() {
    restoreLayoutOverrides();
    m_real = nullptr;
    m_modifiedString.clear();
    m_entries.clear();
    m_entryIndex.clear();
    m_savedStates.clear();
    m_frameSerial = 0;
    m_renderingLayout = false;
    m_backgroundTouched = false;
}

void LayoutMirror::createFor(PlayLayer* real, GJGameLevel* level) {
    clear();
    if (!real || !level) return;
    if (!Mod::get()->getSettingValue<bool>("enabled")) return;

    m_real = real;
    try {
        // This is the full pinned XDBot preprocessing path. Unlike the old
        // implementation, its output is used as a render mask over the one
        // authoritative PlayLayer instead of booting a second gameplay world.
        m_modifiedString = LayoutMode::getModifiedString(std::string(level->m_levelString));
    }
    catch (std::exception const& e) {
        log::error("XDBot LayoutMode preprocessing failed: {}", e.what());
        clear();
        return;
    }

    buildLayoutMap(real);
}

void LayoutMirror::destroyFor(PlayLayer* real) {
    if (!real || real == m_real) clear();
}

void LayoutMirror::buildLayoutMap(PlayLayer* real) {
    if (!real || real != m_real || m_modifiedString.empty()) return;

    std::unordered_map<ObjectKey, std::deque<LayoutRecord>, ObjectKeyHash> records;
    auto strings = splitView(m_modifiedString, ';');
    std::size_t parsedRecords = 0;
    for (std::size_t i = 1; i < strings.size(); ++i) {
        ObjectKey key;
        bool hidden = false;
        if (!parseLayoutRecord(strings[i], key, hidden)) continue;
        records[key].push_back({ hidden });
        ++parsedRecords;
    }

    auto* objects = real->m_objects;
    if (!objects) {
        log::error("Authoritative PlayLayer has no object array; Layout render mask disabled");
        return;
    }

    m_entries.reserve(objects->count());
    m_entryIndex.reserve(objects->count());
    std::size_t matched = 0;
    std::size_t keptVisible = 0;

    for (unsigned i = 0; i < objects->count(); ++i) {
        auto* object = typeinfo_cast<GameObject*>(objects->objectAtIndex(i));
        if (!object) continue;

        auto const start = object->m_startPosition;
        ObjectKey key { object->m_objectID, quantize(start.x), quantize(start.y) };
        bool keep = false;
        bool forceHidden = false;

        auto consume = [&](ObjectKey const& candidate) -> bool {
            auto found = records.find(candidate);
            if (found == records.end() || found->second.empty()) return false;
            auto rec = found->second.front();
            found->second.pop_front();
            keep = true;
            forceHidden = rec.hidden || object->m_objectID == 2065;
            ++matched;
            if (!forceHidden) ++keptVisible;
            return true;
        };

        if (!consume(key)) {
            // Accommodate the tiny float round-trip differences between the
            // serialized level and GameObject::m_startPosition.
            bool consumed = false;
            for (long long dx = -1; dx <= 1 && !consumed; ++dx) {
                for (long long dy = -1; dy <= 1 && !consumed; ++dy) {
                    if (dx == 0 && dy == 0) continue;
                    // Windows headers define `near` as an empty legacy macro.
                    // Keep this identifier macro-safe for the Win64 build.
                    ObjectKey nearbyKey { key.id, key.x + dx, key.y + dy };
                    consumed = consume(nearbyKey);
                }
            }
        }

        auto index = m_entries.size();
        m_entries.push_back({ object, keep, forceHidden, 0 });
        m_entryIndex.emplace(object, index);
    }

    log::info(
        "XDBot visual layout map ready: {} real objects, {} transformed records, {} matched, {} locally visible",
        m_entries.size(), parsedRecords, matched, keptVisible
    );
    if (parsedRecords && matched * 100 < parsedRecords * 90) {
        log::warn(
            "Only {} of {} XDBot layout records mapped to live objects; please include this line in the next test log",
            matched, parsedRecords
        );
    }
}

void LayoutMirror::touchEntry(LayoutEntry& entry) {
    if (!entry.object || entry.touchedSerial == m_frameSerial) return;
    entry.touchedSerial = m_frameSerial;
    auto* object = entry.object;

    SavedVisualState state;
    state.object = object;
    state.visible = object->isVisible();
    state.opacity = object->getOpacity();
    state.activeMainColorID = object->m_activeMainColorID;
    state.activeDetailColorID = object->m_activeDetailColorID;
    state.detailUsesHSV = object->m_detailUsesHSV;
    state.baseUsesHSV = object->m_baseUsesHSV;
    state.hasNoGlow = object->m_hasNoGlow;
    state.isHide = object->m_isHide;
    state.hadGlow = object->m_glowSprite != nullptr;
    state.glowVisible = state.hadGlow && object->m_glowSprite->isVisible();
    state.hadParticle = object->m_particle != nullptr;
    state.particleVisible = state.hadParticle && object->m_particle->isVisible();
    m_savedStates.push_back(state);

    if (!entry.keep || entry.forceHidden) {
        object->setVisible(false);
        if (object->m_glowSprite) object->m_glowSprite->setVisible(false);
        if (object->m_particle) object->m_particle->setVisible(false);
        return;
    }

    // Exact XDBot addObject visual mutation. Excluded triggers and removed deco
    // are represented by entry.keep == false, so they never reach this block.
    object->m_activeMainColorID = -1;
    object->m_activeDetailColorID = -1;
    object->m_detailUsesHSV = false;
    object->m_baseUsesHSV = false;
    object->m_hasNoGlow = true;
    object->m_isHide = object->m_objectID == 2065;
    object->setOpacity(object->m_objectID == 2065 ? 0 : 255);
    object->setVisible(object->m_objectID != 2065);
    if (object->m_glowSprite) object->m_glowSprite->setVisible(false);
}

void LayoutMirror::applyLayoutOverrides(PlayLayer* real) {
    if (!real || real != m_real) return;
    ++m_frameSerial;
    if (m_frameSerial == 0) ++m_frameSerial;
    m_savedStates.clear();

    // Kept XDBot objects must be made visible even if an original alpha/toggle
    // trigger hid them. This list is normally far smaller than the decorated
    // source level, keeping the per-frame work bounded by layout complexity.
    for (auto& entry : m_entries) {
        if (entry.keep) touchEntry(entry);
    }

    // Removed decoration only needs touching when GD currently considers it
    // visible. This avoids toggling tens of thousands of off-camera deco nodes.
    auto hideVisibleVector = [&](auto const& vec) {
        for (auto* object : vec) {
            if (!object) continue;
            auto found = m_entryIndex.find(object);
            if (found == m_entryIndex.end()) continue;
            auto& entry = m_entries[found->second];
            if (!entry.keep) touchEntry(entry);
        }
    };
    hideVisibleVector(real->m_visibleObjects);
    hideVisibleVector(real->m_visibleObjects2);

    if (real->m_background) {
        m_savedBackgroundColor = real->m_background->getColor();
        m_backgroundTouched = true;
        real->m_background->setColor({40, 125, 255});
    }
}

void LayoutMirror::restoreLayoutOverrides() {
    for (auto it = m_savedStates.rbegin(); it != m_savedStates.rend(); ++it) {
        auto& state = *it;
        auto* object = state.object;
        if (!object) continue;
        object->m_activeMainColorID = state.activeMainColorID;
        object->m_activeDetailColorID = state.activeDetailColorID;
        object->m_detailUsesHSV = state.detailUsesHSV;
        object->m_baseUsesHSV = state.baseUsesHSV;
        object->m_hasNoGlow = state.hasNoGlow;
        object->m_isHide = state.isHide;
        object->setOpacity(state.opacity);
        object->setVisible(state.visible);
        if (state.hadGlow && object->m_glowSprite) object->m_glowSprite->setVisible(state.glowVisible);
        if (state.hadParticle && object->m_particle) object->m_particle->setVisible(state.particleVisible);
    }
    m_savedStates.clear();

    if (m_backgroundTouched && m_real && m_real->m_background) {
        m_real->m_background->setColor(m_savedBackgroundColor);
    }
    m_backgroundTouched = false;
}

void LayoutMirror::renderPlayerView(CCDirector* director, PlayLayer* real) {
#ifndef GEODE_IS_WINDOWS
    (void)director;
    (void)real;
#else
    if (!director || !real || real != m_real) return;
    if (!Mod::get()->getSettingValue<bool>("enabled")) return;
    if (!Mod::get()->getSettingValue<bool>("layout-player-view")) return;
    auto* scene = director->getRunningScene();
    if (!scene) return;

    applyLayoutOverrides(real);
    m_renderingLayout = true;

    glDisable(GL_SCISSOR_TEST);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    director->setViewport();
    director->setProjection(director->getProjection());

    // Render the SAME authoritative scene a second time. Physics, camera,
    // practice checkpoints, StartPos, music time and mod HUD therefore remain
    // byte-for-byte the real game state. ShaderLayer::visit is bypassed only
    // while m_renderingLayout is true (see main.cpp).
    scene->visit();
    if (auto* notification = director->getNotificationNode(); notification && notification->isVisible()) {
        notification->visit();
    }

    m_renderingLayout = false;
    restoreLayoutOverrides();
#endif
}
