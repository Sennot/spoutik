#include "LayoutMirror.hpp"
#include "layout_mode.hpp"
#include <Geode/loader/Mod.hpp>
#include <algorithm>
#include <cstdlib>
#include <string_view>
#include <unordered_set>

#ifdef GEODE_IS_WINDOWS
#include <GL/gl.h>
#endif

using namespace geode::prelude;

namespace {
    constexpr cocos2d::ccColor3B kLayoutWhite {255, 255, 255};
    constexpr cocos2d::ccColor3B kLayoutBlack {0, 0, 0};
    constexpr int kBackgroundChannel = 1000;
    constexpr int kGround1Channel = 1001;
    constexpr int kGround2Channel = 1009;
    constexpr int kLineChannel = 1002;
    constexpr int kMG1Channel = 1013;
    constexpr int kMG2Channel = 1014;

    struct ParsedRecord {
        int objectID = 0;
        bool hidden = false;
        std::string canonicalWithoutHidden;
    };

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

    bool parseObjectRecord(std::string_view record, ParsedRecord& out) {
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

        out = {};
        out.canonicalWithoutHidden.reserve(record.size());
        for (std::size_t i = 0; i + 1 < parts.size(); i += 2) {
            int property = 0;
            if (!parseInt(parts[i], property)) continue;

            if (property == 1) parseInt(parts[i + 1], out.objectID);
            if (property == 135) {
                int value = 0;
                if (parseInt(parts[i + 1], value)) out.hidden = value != 0;
                continue;
            }

            // XDBot getModifiedString preserves every property except 135 for
            // retained records. Keeping their serialized order gives us an
            // exact, table-independent subsequence comparison.
            out.canonicalWithoutHidden.append(parts[i]);
            out.canonicalWithoutHidden.push_back('\x1f');
            out.canonicalWithoutHidden.append(parts[i + 1]);
            out.canonicalWithoutHidden.push_back('\x1e');
        }
        return out.objectID != 0;
    }

    std::vector<ParsedRecord> parseObjectRecords(std::string const& levelString) {
        std::vector<ParsedRecord> records;
        auto strings = splitView(levelString, ';');
        if (strings.size() <= 1) return records;

        records.reserve(strings.size() - 1);
        for (std::size_t i = 1; i < strings.size(); ++i) {
            ParsedRecord record;
            if (parseObjectRecord(strings[i], record)) records.push_back(std::move(record));
        }
        return records;
    }

    bool parsePaletteRecord(std::string_view record, int& channel, cocos2d::ccColor3B& color) {
        if (record.empty()) return false;
        std::vector<std::string_view> parts;
        std::size_t begin = 0;
        while (begin <= record.size()) {
            auto end = record.find('_', begin);
            if (end == std::string_view::npos) end = record.size();
            parts.emplace_back(record.substr(begin, end - begin));
            if (end == record.size()) break;
            begin = end + 1;
        }

        int red = -1;
        int green = -1;
        int blue = -1;
        channel = 0;
        for (std::size_t i = 0; i + 1 < parts.size(); i += 2) {
            int property = 0;
            int value = 0;
            if (!parseInt(parts[i], property) || !parseInt(parts[i + 1], value)) continue;
            if (property == 1) red = value;
            else if (property == 2) green = value;
            else if (property == 3) blue = value;
            else if (property == 6) channel = value;
        }

        if (channel == 0 || red < 0 || green < 0 || blue < 0) return false;
        color = {
            static_cast<unsigned char>(std::clamp(red, 0, 255)),
            static_cast<unsigned char>(std::clamp(green, 0, 255)),
            static_cast<unsigned char>(std::clamp(blue, 0, 255)),
        };
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
    m_pendingByObjectID.clear();
    m_layoutPalette.clear();
    m_savedStates.clear();
    m_savedSceneSprites.clear();
    m_frameSerial = 0;
    m_originalRecordCount = 0;
    m_transformedRecordCount = 0;
    m_classifiedKeepCount = 0;
    m_boundRecordCount = 0;
    m_unclassifiedObjectCount = 0;
    m_renderingLayout = false;
}

void LayoutMirror::prepareFor(PlayLayer* real, GJGameLevel* level) {
    clear();
    if (!real || !level) return;
    if (!Mod::get()->getSettingValue<bool>("enabled")) return;

    m_real = real;
    try {
        // Run the exact pinned XDBot preprocessing before the authoritative
        // PlayLayer begins adding objects. We only derive render metadata from
        // it; the real level string and gameplay world are never replaced.
        m_modifiedString = LayoutMode::getModifiedString(std::string(level->m_levelString));
        buildLayoutPlan(level);
    }
    catch (std::exception const& e) {
        log::error("XDBot LayoutMode preprocessing failed: {}", e.what());
        clear();
    }
}

void LayoutMirror::buildLayoutPlan(GJGameLevel* level) {
    if (!level || m_modifiedString.empty()) return;

    auto originalString = ZipUtils::decompressString(level->m_levelString.c_str(), true, 0);
    auto originalRecords = parseObjectRecords(originalString);
    auto transformedRecords = parseObjectRecords(m_modifiedString);

    m_originalRecordCount = originalRecords.size();
    m_transformedRecordCount = transformedRecords.size();
    m_entries.reserve(originalRecords.size());

    // Consume the upstream palette itself instead of copying its RGB values.
    // Channel IDs are the six GD special channels emitted by XDBot newColors.
    for (auto record : splitView(newColors, '|')) {
        int channel = 0;
        cocos2d::ccColor3B color;
        if (parsePaletteRecord(record, channel, color)) m_layoutPalette[channel] = color;
    }

    // XDBot emits a stable subsequence of the source records. The only object
    // property it changes is 135 (Hide), so comparing every other serialized
    // property classifies removed deco without duplicating any XDBot ID table.
    std::size_t transformedIndex = 0;
    for (auto const& original : originalRecords) {
        bool keep = false;
        bool forceHidden = false;
        if (transformedIndex < transformedRecords.size()) {
            auto const& transformed = transformedRecords[transformedIndex];
            if (original.canonicalWithoutHidden == transformed.canonicalWithoutHidden) {
                keep = true;
                forceHidden = transformed.hidden || original.objectID == 2065;
                ++transformedIndex;
                ++m_classifiedKeepCount;
            }
        }
        m_pendingByObjectID[original.objectID].push_back({ keep, forceHidden });
    }

    if (transformedIndex != transformedRecords.size()) {
        log::error(
            "XDBot serialized layout alignment incomplete: consumed {} of {} transformed records",
            transformedIndex, transformedRecords.size()
        );
    }
}

void LayoutMirror::observeObject(PlayLayer* real, GameObject* object) {
    if (!real || real != m_real || !object || m_modifiedString.empty()) return;

    bool keep = true;
    bool forceHidden = object->m_objectID == 2065;
    auto found = m_pendingByObjectID.find(object->m_objectID);
    if (found != m_pendingByObjectID.end() && !found->second.empty()) {
        auto pending = found->second.front();
        found->second.pop_front();
        keep = pending.keep;
        forceHidden = pending.forceHidden;
        ++m_boundRecordCount;
    }
    else {
        // This covers runtime-created objects that do not originate in the
        // serialized level. It is the exact pinned XDBot addObject decision.
        keep = !excludedTriggerIDs.contains(object->m_objectID);
        forceHidden = object->m_objectID == 2065;
        ++m_unclassifiedObjectCount;
    }

    m_entries.push_back({ object, keep, forceHidden, 0 });
}

void LayoutMirror::finishFor(PlayLayer* real) {
    if (!real || real != m_real || m_modifiedString.empty()) return;

    std::size_t pending = 0;
    for (auto const& [id, records] : m_pendingByObjectID) {
        (void)id;
        pending += records.size();
    }

    log::info(
        "XDBot exact layout map ready: {} source records, {} transformed, {} bound, {} kept, {} runtime-only, {} pending",
        m_originalRecordCount,
        m_transformedRecordCount,
        m_boundRecordCount,
        m_classifiedKeepCount,
        m_unclassifiedObjectCount,
        pending
    );
    if (m_classifiedKeepCount != m_transformedRecordCount || pending != 0) {
        log::warn(
            "Layout map is incomplete (kept {}/{}, pending {}). Ensure XDBot's own Layout Mode is OFF and include this line in the next log",
            m_classifiedKeepCount,
            m_transformedRecordCount,
            pending
        );
    }
}

void LayoutMirror::destroyFor(PlayLayer* real) {
    if (!real || real == m_real) clear();
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
    state.mainColor = object->getColor();
    state.hadColorSprite = object->m_colorSprite != nullptr;
    state.detailColor = state.hadColorSprite ? object->m_colorSprite->getColor() : kLayoutWhite;
    state.hadGlow = object->m_glowSprite != nullptr;
    state.glowVisible = state.hadGlow && object->m_glowSprite->isVisible();
    state.glowColor = state.hadGlow ? object->m_glowSprite->getColor() : kLayoutWhite;
    state.hadParticle = object->m_particle != nullptr;
    state.particleVisible = state.hadParticle && object->m_particle->isVisible();
    m_savedStates.push_back(state);

    if (!entry.keep || entry.forceHidden) {
        object->setVisible(false);
        if (object->m_glowSprite) object->m_glowSprite->setVisible(false);
        if (object->m_particle) object->m_particle->setVisible(false);
        return;
    }

    // Exact pinned XDBot addObject state, plus an immediate color refresh for
    // objects that were originally initialized with the decorated color map.
    object->m_activeMainColorID = -1;
    object->m_activeDetailColorID = -1;
    object->m_detailUsesHSV = false;
    object->m_baseUsesHSV = false;
    object->m_hasNoGlow = true;
    object->m_isHide = object->m_objectID == 2065;
    object->setOpacity(object->m_objectID == 2065 ? 0 : 255);
    object->setVisible(object->m_objectID != 2065);
    object->setObjectColor(object->m_isObjectBlack ? kLayoutBlack : kLayoutWhite);
    object->setChildColor(object->m_isColorSpriteBlack ? kLayoutBlack : kLayoutWhite);
    if (object->m_glowSprite) object->m_glowSprite->setVisible(false);
}

void LayoutMirror::saveAndColorSceneSprite(CCSprite* sprite, ccColor3B color) {
    if (!sprite) return;
    for (auto const& state : m_savedSceneSprites) {
        if (state.sprite == sprite) return;
    }
    m_savedSceneSprites.push_back({ sprite, sprite->getColor(), sprite->getOpacity() });
    sprite->setColor(color);
    sprite->setOpacity(255);
}

void LayoutMirror::applyScenePalette(PlayLayer* real) {
    if (!real) return;
    m_savedSceneSprites.clear();

    auto colorFor = [&](int channel, ccColor3B fallback) {
        auto found = m_layoutPalette.find(channel);
        return found == m_layoutPalette.end() ? fallback : found->second;
    };

    // Read all six special-channel colors directly from pinned XDBot newColors.
    auto background = colorFor(kBackgroundChannel, {40, 125, 255});
    auto ground1 = colorFor(kGround1Channel, {0, 102, 255});
    auto ground2 = colorFor(kGround2Channel, {0, 102, 255});
    auto line = colorFor(kLineChannel, kLayoutWhite);
    auto mg1 = colorFor(kMG1Channel, {40, 125, 255});
    auto mg2 = colorFor(kMG2Channel, {40, 125, 255});
    saveAndColorSceneSprite(real->m_background, background);

    auto applyGround = [&](GJGroundLayer* ground) {
        if (!ground) return;
        saveAndColorSceneSprite(ground->m_ground1Sprite, ground1);
        saveAndColorSceneSprite(ground->m_ground2Sprite, ground2);
        saveAndColorSceneSprite(ground->m_lineSprite, line);
    };
    applyGround(real->m_groundLayer);
    applyGround(real->m_groundLayer2);

    if (real->m_middleground) {
        saveAndColorSceneSprite(real->m_middleground->m_ground1Sprite, mg1);
        saveAndColorSceneSprite(real->m_middleground->m_ground2Sprite, mg2);
    }
}

void LayoutMirror::applyLayoutOverrides(PlayLayer* real) {
    if (!real || real != m_real) return;
    ++m_frameSerial;
    if (m_frameSerial == 0) ++m_frameSerial;
    m_savedStates.clear();

    // Scan the complete classified object set. Restricting removed decoration
    // to GD's two visible caches leaked nodes whenever another mod or a camera
    // transition changed cache membership between the two render passes.
    for (auto& entry : m_entries) {
        if (entry.keep || (entry.object && entry.object->isVisible())) touchEntry(entry);
    }
    applyScenePalette(real);
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
        object->setObjectColor(state.mainColor);
        object->setChildColor(state.detailColor);
        object->setOpacity(state.opacity);
        object->setVisible(state.visible);
        if (state.hadGlow && object->m_glowSprite) {
            object->m_glowSprite->setColor(state.glowColor);
            object->m_glowSprite->setVisible(state.glowVisible);
        }
        if (state.hadParticle && object->m_particle) object->m_particle->setVisible(state.particleVisible);
    }
    m_savedStates.clear();

    for (auto it = m_savedSceneSprites.rbegin(); it != m_savedSceneSprites.rend(); ++it) {
        if (!it->sprite) continue;
        it->sprite->setColor(it->color);
        it->sprite->setOpacity(it->opacity);
    }
    m_savedSceneSprites.clear();
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
    // the real game state. ShaderLayer::visit is bypassed only for this pass.
    scene->visit();
    if (auto* notification = director->getNotificationNode(); notification && notification->isVisible()) {
        notification->visit();
    }

    m_renderingLayout = false;
    restoreLayoutOverrides();
#endif
}
