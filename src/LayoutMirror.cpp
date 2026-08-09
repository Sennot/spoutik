#include "LayoutMirror.hpp"
#include "layout_mode.hpp"
#include <Geode/loader/Mod.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string_view>

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
    m_spatialEntries.clear();
    m_spatialOverflow.clear();
    m_entryIndex.clear();
    m_renderNodes.clear();
    m_pendingByObjectID.clear();
    m_layoutPalette.clear();
    m_savedStates.clear();
    m_savedSceneSprites.clear();
    m_frameSerial = 0;
    m_originalRecordCount = 0;
    m_transformedRecordCount = 0;
    m_classifiedKeepCount = 0;
    m_boundRecordCount = 0;
    m_boundKeepCount = 0;
    m_unclassifiedObjectCount = 0;
    m_frameNodeProbeCount = 0;
    m_frameMappedNodeCount = 0;
    m_frameStyledNodeCount = 0;
    m_frameSuppressedNodeCount = 0;
    m_frameCandidateObjectCount = 0;
    m_frameRetainedCandidateCount = 0;
    m_frameCompactCandidateCount = 0;
    m_frameSpatialCandidateCount = 0;
    m_frameFallbackCandidateCount = 0;
    m_frameForcedVisibleCount = 0;
    m_frameBatchedMutationCount = 0;
    m_frameViewportLeft = 0.f;
    m_frameViewportRight = 0.f;
    m_frameViewportBottom = 0.f;
    m_frameViewportTop = 0.f;
    m_frameViewportValid = false;
    m_reportedRenderCoverage = false;
    m_spatialIndexReady = false;
    m_renderMapReady = false;
    m_renderingLayout = false;
    m_reportedFramebufferSkip = false;
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
    m_entryIndex.reserve(originalRecords.size());

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
        if (pending.keep) ++m_boundKeepCount;
    }
    else {
        // This covers runtime-created objects that do not originate in the
        // serialized level. It is the exact pinned XDBot addObject decision.
        keep = !excludedTriggerIDs.contains(object->m_objectID);
        forceHidden = object->m_objectID == 2065;
        ++m_unclassifiedObjectCount;
    }

    // getModifiedString intentionally leaves gameplay triggers in the level
    // data, while XDBot's PlayLayer::addObject hook omits this exact set from
    // the visual world. Preserve that second half of the upstream behavior
    // without removing triggers from our one authoritative gameplay layer.
    if (excludedTriggerIDs.contains(object->m_objectID)) keep = false;

    auto const index = m_entries.size();
    // Mirror the exact upstream addObject baseline without mutating the real
    // decorated object. XDBot makes every retained object visible/opaque once;
    // later opacity calls update the logical alpha, while live disable flags
    // are read at render time independently from unreliable camera culling.
    auto const layoutOpacity = static_cast<unsigned char>(forceHidden ? 0 : 255);
    m_entries.push_back({ object, keep, forceHidden, layoutOpacity, 0 });
    m_entryIndex.insert_or_assign(object, index);
    if (m_renderMapReady) {
        registerRenderNodes(m_entries.back());
        // Objects created after PlayLayer::init are rare and usually moving.
        // Keep them in a tiny overflow instead of inserting into a 100k-entry
        // sorted vector and shifting it during gameplay.
        if (m_spatialIndexReady) m_spatialOverflow.push_back(index);
    }
}

void LayoutMirror::observeOpacity(GameObject* object, unsigned char opacity) {
    if (!object || m_renderingLayout) return;
    auto found = m_entryIndex.find(object);
    if (found == m_entryIndex.end()) return;
    m_entries[found->second].layoutOpacity = opacity;
}

void LayoutMirror::finishFor(PlayLayer* real) {
    if (!real || real != m_real || m_modifiedString.empty()) return;

    std::size_t pending = 0;
    std::size_t pendingKeep = 0;
    for (auto const& [id, records] : m_pendingByObjectID) {
        (void)id;
        pending += records.size();
        pendingKeep += static_cast<std::size_t>(std::count_if(
            records.begin(), records.end(), [](PendingRecord const& record) { return record.keep; }
        ));
    }

    log::info(
        "XDBot exact layout map ready: {} source records, {} transformed, {} bound, {} retained bound, {} runtime-only, {} pending ({} retained)",
        m_originalRecordCount,
        m_transformedRecordCount,
        m_boundRecordCount,
        m_boundKeepCount,
        m_unclassifiedObjectCount,
        pending,
        pendingKeep
    );
    if (m_classifiedKeepCount != m_transformedRecordCount || pendingKeep != 0) {
        log::warn(
            "Layout map is incomplete (retained bound {}/{}, retained pending {}). Ensure XDBot's own Layout Mode is OFF and include this line in the next log",
            m_boundKeepCount,
            m_transformedRecordCount,
            pendingKeep
        );
    }

    // Build one direct CCNode lookup after object construction. Main/detail/
    // glow sprites may be reparented, so index the actual render nodes. Pooled
    // particles are deliberately handled only from their current camera owner.
    m_renderNodes.clear();
    m_renderNodes.reserve(m_entries.size() * 3);
    for (auto const& entry : m_entries) registerRenderNodes(entry);

    // The engine's current-frame camera arrays are deliberately transient:
    // on several levels they contain only one or two objects during the first
    // visible frames, then admit decoration later. Build an immutable X-sorted
    // lookup once so the same local viewport is masked from frame one onward.
    m_spatialEntries.clear();
    m_spatialEntries.reserve(m_entries.size());
    for (std::size_t index = 0; index < m_entries.size(); ++index) {
        auto* object = m_entries[index].object;
        if (!object) continue;
        auto const position = object->getPosition();
        if (!std::isfinite(position.x) || !std::isfinite(position.y)) continue;
        m_spatialEntries.push_back({ position.x, position.y, index });
    }
    std::sort(
        m_spatialEntries.begin(), m_spatialEntries.end(),
        [](SpatialEntry const& left, SpatialEntry const& right) {
            if (left.x != right.x) return left.x < right.x;
            return left.y < right.y;
        }
    );
    m_spatialIndexReady = true;
    m_renderMapReady = true;

    if (!m_spatialEntries.empty()) {
        log::info(
            "Stable layout spatial index ready: {} objects, X {:.1f}..{:.1f}",
            m_spatialEntries.size(),
            m_spatialEntries.front().x,
            m_spatialEntries.back().x
        );
    }
}

void LayoutMirror::destroyFor(PlayLayer* real) {
    if (!real || real == m_real) clear();
}

void LayoutMirror::registerRenderNodes(LayoutEntry const& entry) {
    auto* object = entry.object;
    if (!object) return;

    auto registerNode = [&](cocos2d::CCNode* node, RenderNodeKind kind) {
        if (!node) return;
        auto rank = [](RenderNodeKind value) {
            switch (value) {
                case RenderNodeKind::Main: return 3;
                case RenderNodeKind::Detail: return 2;
                case RenderNodeKind::Suppress: return 1;
            }
            return 0;
        };
        auto found = m_renderNodes.find(node);
        if (found == m_renderNodes.end() || rank(kind) > rank(found->second.kind)) {
            m_renderNodes.insert_or_assign(node, RenderNodeEntry { object, kind });
        }
    };

    if (!entry.keep || entry.forceHidden) {
        registerNode(object, RenderNodeKind::Suppress);
        registerNode(object->m_colorSprite, RenderNodeKind::Suppress);
        registerNode(object->m_glowSprite, RenderNodeKind::Suppress);
        return;
    }

    registerNode(object, RenderNodeKind::Main);
    registerNode(object->m_colorSprite, RenderNodeKind::Detail);
    // This is the render-time equivalent of XDBot's m_hasNoGlow assignment.
    // Skipping the glow node avoids a pair of visibility setters every frame.
    registerNode(object->m_glowSprite, RenderNodeKind::Suppress);
}

LayoutMirror::NodeVisitAction LayoutMirror::beginNodeVisit(cocos2d::CCNode* node) {
    if (!m_renderingLayout || !node) return NodeVisitAction::PassThrough;
    auto const collectCoverage = !m_reportedRenderCoverage;
    if (collectCoverage) ++m_frameNodeProbeCount;

    auto found = m_renderNodes.find(node);
    if (found == m_renderNodes.end()) return NodeVisitAction::PassThrough;
    if (collectCoverage) ++m_frameMappedNodeCount;

    auto const& renderNode = found->second;
    auto const ownerDisabled = renderNode.owner && (
        renderNode.owner->m_isGroupDisabled || renderNode.owner->m_isGroupDisabledTemp ||
        renderNode.owner->m_isDisabled || renderNode.owner->m_isDisabled2
    );
    if (renderNode.kind == RenderNodeKind::Suppress || ownerDisabled || !node->isVisible()) {
        if (collectCoverage) ++m_frameSuppressedNodeCount;
        return NodeVisitAction::Skip;
    }

    auto* sprite = static_cast<cocos2d::CCSprite*>(node);
    auto const target = renderNode.kind == RenderNodeKind::Main
        ? (renderNode.owner->m_isObjectBlack ? kLayoutBlack : kLayoutWhite)
        : (renderNode.owner->m_isColorSpriteBlack ? kLayoutBlack : kLayoutWhite);
    auto const current = sprite->getColor();
    if (current.r == target.r && current.g == target.g && current.b == target.b) {
        return NodeVisitAction::PassThrough;
    }

    SavedVisualState state;
    state.node = node;
    state.sprite = sprite;
    state.color = current;
    state.restoreColor = true;
    m_savedStates.push_back(state);
    sprite->setColor(target);
    if (collectCoverage) ++m_frameStyledNodeCount;
    return NodeVisitAction::Styled;
}

void LayoutMirror::endNodeVisit(cocos2d::CCNode* node) {
    if (m_savedStates.empty()) return;
    auto const state = m_savedStates.back();
    if (state.node != node) return;
    if (state.sprite) state.sprite->setColor(state.color);
    m_savedStates.pop_back();
}

void LayoutMirror::touchCameraEntry(LayoutEntry& entry) {
    if (!entry.object || entry.touchedSerial == m_frameSerial) return;
    entry.touchedSerial = m_frameSerial;
    ++m_frameCandidateObjectCount;
    if (entry.keep && !entry.forceHidden) ++m_frameRetainedCandidateCount;

    auto* object = entry.object;
    auto const gameplayDisabled =
        object->m_isGroupDisabled || object->m_isGroupDisabledTemp ||
        object->m_isDisabled || object->m_isDisabled2;
    auto const shouldShow = entry.keep && !entry.forceHidden && !gameplayDisabled;

    auto setSpriteVisible = [&](cocos2d::CCSprite* sprite, bool visible, bool mainSprite) {
        if (!sprite || sprite->isVisible() == visible) return;
        SavedVisualState state;
        state.node = sprite;
        state.sprite = sprite;
        state.restoreVisible = true;
        state.visible = sprite->isVisible();
        m_savedStates.push_back(state);
        sprite->cocos2d::CCSprite::setVisible(visible);
        if (visible && mainSprite) ++m_frameForcedVisibleCount;
        if (sprite->getBatchNode()) ++m_frameBatchedMutationCount;
    };

    auto setSpriteOpacity = [&](cocos2d::CCSprite* sprite, unsigned char opacity) {
        if (!sprite || sprite->getOpacity() == opacity) return;
        SavedVisualState state;
        state.node = sprite;
        state.sprite = sprite;
        state.opacity = sprite->getOpacity();
        state.restoreOpacity = true;
        m_savedStates.push_back(state);
        sprite->cocos2d::CCSprite::setOpacity(opacity);
        if (sprite->getBatchNode()) ++m_frameBatchedMutationCount;
    };

    auto styleSprite = [&](cocos2d::CCSprite* sprite, cocos2d::ccColor3B target) {
        if (!sprite || !sprite->getBatchNode() || !sprite->isVisible()) return;
        auto const current = sprite->getColor();
        if (current.r == target.r && current.g == target.g && current.b == target.b) return;
        SavedVisualState state;
        state.node = sprite;
        state.sprite = sprite;
        state.color = current;
        state.restoreColor = true;
        m_savedStates.push_back(state);
        sprite->setColor(target);
        ++m_frameBatchedMutationCount;
    };

    auto suppressBatchedSprite = [&](cocos2d::CCSprite* sprite) {
        if (sprite && sprite->getBatchNode()) setSpriteVisible(sprite, false, false);
    };

    auto suppressBatchedParticle = [&](cocos2d::CCParticleSystemQuad* particle) {
        if (!particle || !particle->getBatchNode() || !particle->isVisible()) return;
        SavedVisualState state;
        state.node = particle;
        state.particle = particle;
        state.restoreVisible = true;
        state.visible = true;
        m_savedStates.push_back(state);
        particle->cocos2d::CCParticleSystem::setVisible(false);
        ++m_frameBatchedMutationCount;
    };

    if (!shouldShow) {
        suppressBatchedSprite(object);
        suppressBatchedSprite(object->m_colorSprite);
        suppressBatchedSprite(object->m_glowSprite);
        suppressBatchedParticle(object->m_particle);
        return;
    }

    // This is the scoped equivalent of XDBot's post-add setVisible(true) and
    // setOpacity(255). It revives hidden/invisible retained structures only in
    // the camera candidate set, then restores the decorated world this frame.
    setSpriteVisible(object, true, true);
    if (object->m_hasColorSprite) setSpriteVisible(object->m_colorSprite, true, false);
    setSpriteOpacity(object, entry.layoutOpacity);

    styleSprite(object, object->m_isObjectBlack ? kLayoutBlack : kLayoutWhite);
    styleSprite(
        object->m_colorSprite,
        object->m_isColorSpriteBlack ? kLayoutBlack : kLayoutWhite
    );
    suppressBatchedSprite(object->m_glowSprite);
}

void LayoutMirror::applyCameraOverrides(CCDirector* director, PlayLayer* real) {
    if (!director || !real || real != m_real) return;
    ++m_frameSerial;
    if (m_frameSerial == 0) ++m_frameSerial;

    auto touchEntry = [&](std::size_t index, std::size_t& sourceCount) {
        if (index >= m_entries.size()) return;
        auto& entry = m_entries[index];
        if (!entry.object || entry.touchedSerial == m_frameSerial) return;
        ++sourceCount;
        touchCameraEntry(entry);
    };

    auto touchObject = [&](GameObject* object, std::size_t& sourceCount) {
        if (!object) return;
        auto found = m_entryIndex.find(object);
        if (found == m_entryIndex.end()) return;
        touchEntry(found->second, sourceCount);
    };

    auto touchVector = [&](auto const& objects, int activeCount, std::size_t& sourceCount) {
        auto limit = objects.size();
        if (activeCount >= 0) {
            limit = std::min(limit, static_cast<std::size_t>(activeCount));
        }
        for (std::size_t i = 0; i < limit; ++i) touchObject(objects[i], sourceCount);
    };

    // Keep GD's compact vectors for moving/effect objects, but do not trust
    // them as the sole mask: some levels populate them several frames late.
    touchVector(
        real->m_calcNonEffectObjects,
        real->m_calcNonEffectObjectsSize,
        m_frameCompactCandidateCount
    );
    touchVector(
        real->m_visibleObjects,
        real->m_visibleObjectsCount,
        m_frameCompactCandidateCount
    );
    touchVector(
        real->m_visibleObjects2,
        real->m_visibleObjects2Count,
        m_frameCompactCandidateCount
    );

    // Convert all four logical screen corners through the real camera/object
    // transform. Querying our immutable X-sorted index makes removed batch
    // sprites deterministic from the first frame, independently of GD culling.
    if (m_spatialIndexReady && real->m_objectLayer) {
        auto const size = director->getWinSize();
        std::array<CCPoint, 4> const screenCorners {{
            { 0.f, 0.f },
            { size.width, 0.f },
            { 0.f, size.height },
            { size.width, size.height },
        }};

        auto left = std::numeric_limits<float>::infinity();
        auto right = -std::numeric_limits<float>::infinity();
        auto bottom = std::numeric_limits<float>::infinity();
        auto top = -std::numeric_limits<float>::infinity();
        for (auto const& screen : screenCorners) {
            auto const world = real->m_objectLayer->convertToNodeSpace(screen);
            left = std::min(left, world.x);
            right = std::max(right, world.x);
            bottom = std::min(bottom, world.y);
            top = std::max(top, world.y);
        }

        auto const width = right - left;
        auto const height = top - bottom;
        m_frameViewportValid =
            std::isfinite(left) && std::isfinite(right) &&
            std::isfinite(bottom) && std::isfinite(top) &&
            width > 1.f && height > 1.f && width < 100000.f && height < 100000.f;

        if (m_frameViewportValid) {
            // Overscan handles large sprites and objects whose visual bounds
            // extend beyond their origin, while remaining independent of total
            // level size. Moving objects are additionally covered above.
            auto const marginX = std::max(240.f, width * 0.75f);
            auto const marginY = std::max(180.f, height * 0.75f);
            m_frameViewportLeft = left - marginX;
            m_frameViewportRight = right + marginX;
            m_frameViewportBottom = bottom - marginY;
            m_frameViewportTop = top + marginY;

            auto first = std::lower_bound(
                m_spatialEntries.begin(), m_spatialEntries.end(), m_frameViewportLeft,
                [](SpatialEntry const& entry, float value) { return entry.x < value; }
            );
            for (auto it = first;
                 it != m_spatialEntries.end() && it->x <= m_frameViewportRight;
                 ++it) {
                if (it->y < m_frameViewportBottom || it->y > m_frameViewportTop) continue;
                touchEntry(it->entryIndex, m_frameSpatialCandidateCount);
            }

            for (auto index : m_spatialOverflow) {
                if (index >= m_entries.size() || !m_entries[index].object) continue;
                auto const position = m_entries[index].object->getPosition();
                if (position.x < m_frameViewportLeft || position.x > m_frameViewportRight ||
                    position.y < m_frameViewportBottom || position.y > m_frameViewportTop) {
                    continue;
                }
                touchEntry(index, m_frameSpatialCandidateCount);
            }
        }
    }

    auto touchSectionGrid = [&](auto const& grid) {
        if (grid.empty()) return;
        auto const left = std::max(0, real->m_leftSectionIndex - 1);
        auto const right = std::min(static_cast<int>(grid.size()) - 1, real->m_rightSectionIndex + 1);
        if (left > right) return;

        for (auto sectionX = left; sectionX <= right; ++sectionX) {
            auto* column = grid[static_cast<std::size_t>(sectionX)];
            if (!column || column->empty()) continue;
            auto const bottom = std::max(0, real->m_bottomSectionIndex - 1);
            auto const top = std::min(static_cast<int>(column->size()) - 1, real->m_topSectionIndex + 1);
            if (bottom > top) continue;

            for (auto sectionY = bottom; sectionY <= top; ++sectionY) {
                auto* objects = (*column)[static_cast<std::size_t>(sectionY)];
                if (!objects) continue;
                for (auto* object : *objects) touchObject(object, m_frameFallbackCandidateCount);
            }
        }
    };

    // Defensive binding fallback for an unexpected camera transform. It is
    // normally cold; unlike the old implementation, late-changing section
    // indices are no longer the primary source of local layout decisions.
    if (!m_frameViewportValid || m_frameCandidateObjectCount < 16) {
        touchSectionGrid(real->m_nonEffectObjects);
        touchSectionGrid(real->m_sections);
    }
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

void LayoutMirror::beginLayoutPass(CCDirector* director, PlayLayer* real) {
    if (!director || !real || real != m_real) return;
    m_savedStates.clear();
    m_frameNodeProbeCount = 0;
    m_frameMappedNodeCount = 0;
    m_frameStyledNodeCount = 0;
    m_frameSuppressedNodeCount = 0;
    m_frameCandidateObjectCount = 0;
    m_frameRetainedCandidateCount = 0;
    m_frameCompactCandidateCount = 0;
    m_frameSpatialCandidateCount = 0;
    m_frameFallbackCandidateCount = 0;
    m_frameForcedVisibleCount = 0;
    m_frameBatchedMutationCount = 0;
    m_frameViewportLeft = 0.f;
    m_frameViewportRight = 0.f;
    m_frameViewportBottom = 0.f;
    m_frameViewportTop = 0.f;
    m_frameViewportValid = false;
    m_renderingLayout = true;
    applyScenePalette(real);
    applyCameraOverrides(director, real);
}

void LayoutMirror::endLayoutPass() {
    restoreLayoutOverrides();
    m_renderingLayout = false;
    auto const reportFrame = !m_reportedRenderCoverage || m_frameSerial == 120;
    if (reportFrame) {
        log::info(
            "Layout stable mask frame {}: {} candidates ({} retained: {} compact + {} spatial + {} fallback), {} forced visible, {} batched mutations; viewport {} [{:.1f}..{:.1f}, {:.1f}..{:.1f}]; {} scene nodes, {} mapped, {} styled, {} skipped; {} live objects",
            m_frameSerial,
            m_frameCandidateObjectCount,
            m_frameRetainedCandidateCount,
            m_frameCompactCandidateCount,
            m_frameSpatialCandidateCount,
            m_frameFallbackCandidateCount,
            m_frameForcedVisibleCount,
            m_frameBatchedMutationCount,
            m_frameViewportValid ? "valid" : "fallback",
            m_frameViewportLeft,
            m_frameViewportRight,
            m_frameViewportBottom,
            m_frameViewportTop,
            m_frameNodeProbeCount,
            m_frameMappedNodeCount,
            m_frameStyledNodeCount,
            m_frameSuppressedNodeCount,
            m_entries.size()
        );
        m_reportedRenderCoverage = true;
    }
}

void LayoutMirror::restoreLayoutOverrides() {
    for (auto it = m_savedStates.rbegin(); it != m_savedStates.rend(); ++it) {
        if (it->restoreColor && it->sprite) it->sprite->setColor(it->color);
        if (it->restoreOpacity && it->sprite) {
            it->sprite->cocos2d::CCSprite::setOpacity(it->opacity);
        }
        if (it->restoreVisible && it->sprite) {
            it->sprite->cocos2d::CCSprite::setVisible(it->visible);
        }
        else if (it->restoreVisible && it->particle) {
            it->particle->cocos2d::CCParticleSystem::setVisible(it->visible);
        }
    }
    m_savedStates.clear();

    for (auto it = m_savedSceneSprites.rbegin(); it != m_savedSceneSprites.rend(); ++it) {
        if (!it->sprite) continue;
        it->sprite->setColor(it->color);
        it->sprite->setOpacity(it->opacity);
    }
    m_savedSceneSprites.clear();
}

bool LayoutMirror::isStableGameplayScene(CCDirector* director, PlayLayer* real) const {
    if (!director || !real || real != m_real) return false;
    if (!Mod::get()->getSettingValue<bool>("enabled")) return false;
    if (!Mod::get()->getSettingValue<bool>("layout-player-view")) return false;

    auto* scene = director->getRunningScene();
    if (!scene) return false;

    // CCTransitionScene renders retained input/output scenes through its own
    // textures and timing. Re-visiting it at swap time repeats the transition:
    // on entry that leaks the level card/menu, and on exit it can leave only
    // the transition clear color. A stable PlayLayer belongs to the director's
    // running scene itself; during either transition its root is a different
    // retained CCScene.
    cocos2d::CCNode* root = real;
    while (root->getParent()) root = root->getParent();
    return root == scene;
}

bool LayoutMirror::renderPlayerView(CCDirector* director, PlayLayer* real) {
#ifndef GEODE_IS_WINDOWS
    (void)director;
    (void)real;
    return false;
#else
    if (!isStableGameplayScene(director, real)) return false;
    auto* scene = director->getRunningScene();

    GLint framebuffer = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &framebuffer);
    if (framebuffer != 0) {
        if (!m_reportedFramebufferSkip) {
            m_reportedFramebufferSkip = true;
            log::warn(
                "Layout redraw skipped safely: presentation framebuffer is {}, not default FBO 0",
                framebuffer
            );
        }
        return false;
    }

    beginLayoutPass(director, real);

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

    endLayoutPass();
    return true;
#endif
}
