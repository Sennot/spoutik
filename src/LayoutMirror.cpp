#include "LayoutMirror.hpp"
#include "layout_mode.hpp"
#include <Geode/cocos/layers_scenes_transitions_nodes/CCTransition.h>
#include <Geode/loader/Mod.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string_view>

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
    auto const value = std::strtol(tmp.c_str(), &end, 10);
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

layout_companion::Color toProtocolColor(cocos2d::ccColor3B color) {
    return {color.r, color.g, color.b, 255};
}

bool finitePoint(cocos2d::CCPoint const& point) {
    return std::isfinite(point.x) && std::isfinite(point.y) &&
        std::abs(point.x) < 1000000.f && std::abs(point.y) < 1000000.f;
}
}

LayoutMirror& LayoutMirror::get() {
    static LayoutMirror instance;
    return instance;
}

void LayoutMirror::clear() {
    m_real = nullptr;
    m_modifiedString.clear();
    m_entries.clear();
    m_spatialEntries.clear();
    m_spatialOverflow.clear();
    m_entryIndex.clear();
    m_pendingByObjectID.clear();
    m_layoutPalette.clear();
    m_exportSerial = 0;
    m_originalRecordCount = 0;
    m_transformedRecordCount = 0;
    m_classifiedKeepCount = 0;
    m_boundRecordCount = 0;
    m_boundKeepCount = 0;
    m_unclassifiedObjectCount = 0;
    m_spatialIndexReady = false;
}

void LayoutMirror::prepareFor(PlayLayer* real, GJGameLevel* level) {
    clear();
    if (!real || !level || !Mod::get()->getSettingValue<bool>("enabled")) return;

    m_real = real;
    try {
        // The pinned XDBot transform is used only as classification metadata.
        // It never creates another PlayLayer or mutates the decorated one.
        m_modifiedString = LayoutMode::getModifiedString(std::string(level->m_levelString));
        buildLayoutPlan(level);
    }
    catch (std::exception const& error) {
        log::error("XDBot LayoutMode preprocessing failed: {}", error.what());
        clear();
    }
}

void LayoutMirror::buildLayoutPlan(GJGameLevel* level) {
    if (!level || m_modifiedString.empty()) return;

    auto const originalString = ZipUtils::decompressString(level->m_levelString.c_str(), true, 0);
    auto const originalRecords = parseObjectRecords(originalString);
    auto const transformedRecords = parseObjectRecords(m_modifiedString);
    m_originalRecordCount = originalRecords.size();
    m_transformedRecordCount = transformedRecords.size();
    m_entries.reserve(originalRecords.size());
    m_entryIndex.reserve(originalRecords.size());

    for (auto record : splitView(newColors, '|')) {
        int channel = 0;
        cocos2d::ccColor3B color;
        if (parsePaletteRecord(record, channel, color)) m_layoutPalette[channel] = color;
    }

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
        m_pendingByObjectID[original.objectID].push_back({keep, forceHidden});
    }

    if (transformedIndex != transformedRecords.size()) {
        log::error(
            "XDBot serialized layout alignment incomplete: consumed {} of {} transformed records",
            transformedIndex,
            transformedRecords.size()
        );
    }
}

void LayoutMirror::observeObject(PlayLayer* real, GameObject* object) {
    if (!real || real != m_real || !object || m_modifiedString.empty()) return;

    bool keep = true;
    bool forceHidden = object->m_objectID == 2065;
    auto found = m_pendingByObjectID.find(object->m_objectID);
    if (found != m_pendingByObjectID.end() && !found->second.empty()) {
        auto const pending = found->second.front();
        found->second.pop_front();
        keep = pending.keep;
        forceHidden = pending.forceHidden;
        ++m_boundRecordCount;
        if (pending.keep) ++m_boundKeepCount;
    }
    else {
        keep = !excludedTriggerIDs.contains(object->m_objectID);
        ++m_unclassifiedObjectCount;
    }
    if (excludedTriggerIDs.contains(object->m_objectID)) keep = false;

    auto const index = m_entries.size();
    m_entries.push_back({object, keep, forceHidden, 0});
    m_entryIndex.insert_or_assign(object, index);
    if (m_spatialIndexReady) m_spatialOverflow.push_back(index);
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
        "Companion XDBot map: {} source, {} transformed, {} bound, {} retained, {} runtime-only, {} pending",
        m_originalRecordCount,
        m_transformedRecordCount,
        m_boundRecordCount,
        m_boundKeepCount,
        m_unclassifiedObjectCount,
        pending
    );
    if (m_classifiedKeepCount != m_transformedRecordCount || pendingKeep != 0) {
        log::warn(
            "Companion layout map incomplete: retained bound {}/{}, retained pending {}",
            m_boundKeepCount,
            m_transformedRecordCount,
            pendingKeep
        );
    }

    m_spatialEntries.clear();
    m_spatialEntries.reserve(m_entries.size());
    for (std::size_t index = 0; index < m_entries.size(); ++index) {
        auto* object = m_entries[index].object;
        if (!object) continue;
        auto const position = object->getPosition();
        if (!finitePoint(position)) continue;
        m_spatialEntries.push_back({position.x, position.y, index});
    }
    std::sort(
        m_spatialEntries.begin(), m_spatialEntries.end(),
        [](SpatialEntry const& left, SpatialEntry const& right) {
            if (left.x != right.x) return left.x < right.x;
            return left.y < right.y;
        }
    );
    m_spatialIndexReady = true;
    log::info("Companion spatial index ready: {} objects", m_spatialEntries.size());
}

void LayoutMirror::destroyFor(PlayLayer* real) {
    if (!real || real == m_real) clear();
}

bool LayoutMirror::isStableGameplayScene(CCDirector* director, PlayLayer* real) const {
    if (!director || !real || real != m_real || m_modifiedString.empty()) return false;
    if (!Mod::get()->getSettingValue<bool>("enabled")) return false;
    auto* scene = director->getRunningScene();
    if (!scene || typeinfo_cast<cocos2d::CCTransitionScene*>(scene) || director->getNextScene()) {
        return false;
    }

    cocos2d::CCNode* root = real;
    while (root->getParent()) root = root->getParent();
    return root == scene;
}

bool LayoutMirror::writeCompanionFrame(
    layout_companion::SharedFrame& frame,
    CCDirector* director,
    PlayLayer* real
) {
    using namespace layout_companion;
    if (!isStableGameplayScene(director, real) || !real->m_objectLayer) return false;

    ++m_exportSerial;
    if (m_exportSerial == 0) ++m_exportSerial;
    auto const size = director->getWinSize();
    if (size.width <= 1.f || size.height <= 1.f) return false;

    auto paletteColor = [&](int channel, cocos2d::ccColor3B fallback) {
        auto found = m_layoutPalette.find(channel);
        return found == m_layoutPalette.end() ? fallback : found->second;
    };

    frame.flags = FrameActive;
    frame.logicalWidth = size.width;
    frame.logicalHeight = size.height;
    frame.groundTop = std::min(34.f, size.height * 0.14f);
    frame.background = toProtocolColor(paletteColor(kBackgroundChannel, {40, 125, 255}));
    frame.ground = toProtocolColor(paletteColor(kGround1Channel, {0, 102, 255}));
    frame.groundLine = toProtocolColor(paletteColor(kLineChannel, kLayoutWhite));
    frame.quadCount = 0;
    frame.droppedQuadCount = 0;
    frame.sourceObjectCount = static_cast<std::uint32_t>(std::min<std::size_t>(
        m_entries.size(), std::numeric_limits<std::uint32_t>::max()
    ));
    frame.retainedObjectCount = 0;

    auto appendNode = [&](cocos2d::CCNode* node, Color color, QuadKind kind) {
        if (!node) return;
        auto content = node->getContentSize();
        float width = content.width;
        float height = content.height;
        float left = 0.f;
        float bottom = 0.f;
        if (!std::isfinite(width) || !std::isfinite(height) || width < 1.f || height < 1.f) {
            width = 30.f;
            height = 30.f;
            left = -15.f;
            bottom = -15.f;
        }

        std::array<CCPoint, 4> points {{
            node->convertToWorldSpace({left, bottom}),
            node->convertToWorldSpace({left + width, bottom}),
            node->convertToWorldSpace({left + width, bottom + height}),
            node->convertToWorldSpace({left, bottom + height}),
        }};
        if (!std::all_of(points.begin(), points.end(), finitePoint)) return;

        float minX = points[0].x;
        float maxX = points[0].x;
        float minY = points[0].y;
        float maxY = points[0].y;
        for (auto const& point : points) {
            minX = std::min(minX, point.x);
            maxX = std::max(maxX, point.x);
            minY = std::min(minY, point.y);
            maxY = std::max(maxY, point.y);
        }
        if (maxX < -60.f || minX > size.width + 60.f ||
            maxY < -60.f || minY > size.height + 60.f) return;

        if (frame.quadCount >= kMaximumQuads) {
            ++frame.droppedQuadCount;
            frame.flags |= FrameTruncated;
            return;
        }

        auto& quad = frame.quads[frame.quadCount++];
        quad = {
            points[0].x, points[0].y,
            points[1].x, points[1].y,
            points[2].x, points[2].y,
            points[3].x, points[3].y,
            node->getZOrder(), kind, 0, color,
        };
    };

    std::size_t candidateCount = 0;
    auto exportEntry = [&](std::size_t index) {
        if (index >= m_entries.size()) return;
        auto& entry = m_entries[index];
        if (!entry.object || entry.exportedSerial == m_exportSerial) return;
        entry.exportedSerial = m_exportSerial;
        ++candidateCount;
        if (!entry.keep || entry.forceHidden) return;

        // Deliberately ignore decorated-world visibility, opacity and toggle
        // state. XDBot removes those visual triggers from its Layout world;
        // reproducing them here was why invisible levels stayed invisible.
        ++frame.retainedObjectCount;
        appendNode(
            entry.object,
            toProtocolColor(entry.object->m_isObjectBlack ? kLayoutBlack : kLayoutWhite),
            QuadKind::ObjectMain
        );
        appendNode(
            entry.object->m_colorSprite,
            toProtocolColor(entry.object->m_isColorSpriteBlack ? kLayoutBlack : kLayoutWhite),
            QuadKind::ObjectDetail
        );
    };

    auto exportObject = [&](GameObject* object) {
        if (!object) return;
        auto found = m_entryIndex.find(object);
        if (found != m_entryIndex.end()) exportEntry(found->second);
    };

    auto exportVector = [&](auto const& objects, int activeCount) {
        auto limit = objects.size();
        if (activeCount >= 0) limit = std::min(limit, static_cast<std::size_t>(activeCount));
        for (std::size_t index = 0; index < limit; ++index) exportObject(objects[index]);
    };

    exportVector(real->m_calcNonEffectObjects, real->m_calcNonEffectObjectsSize);
    exportVector(real->m_visibleObjects, real->m_visibleObjectsCount);
    exportVector(real->m_visibleObjects2, real->m_visibleObjects2Count);

    bool viewportValid = false;
    float viewportLeft = 0.f;
    float viewportRight = 0.f;
    float viewportBottom = 0.f;
    float viewportTop = 0.f;
    if (m_spatialIndexReady) {
        std::array<CCPoint, 4> const screenCorners {{
            {0.f, 0.f}, {size.width, 0.f}, {0.f, size.height}, {size.width, size.height},
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
        viewportValid = std::isfinite(left) && std::isfinite(right) &&
            std::isfinite(bottom) && std::isfinite(top) &&
            width > 1.f && height > 1.f && width < 100000.f && height < 100000.f;
        if (viewportValid) {
            auto const marginX = std::max(240.f, width * 0.75f);
            auto const marginY = std::max(180.f, height * 0.75f);
            viewportLeft = left - marginX;
            viewportRight = right + marginX;
            viewportBottom = bottom - marginY;
            viewportTop = top + marginY;

            auto first = std::lower_bound(
                m_spatialEntries.begin(), m_spatialEntries.end(), viewportLeft,
                [](SpatialEntry const& entry, float value) { return entry.x < value; }
            );
            for (auto it = first;
                 it != m_spatialEntries.end() && it->x <= viewportRight;
                 ++it) {
                if (it->y >= viewportBottom && it->y <= viewportTop) {
                    exportEntry(it->entryIndex);
                }
            }
            for (auto index : m_spatialOverflow) {
                if (index >= m_entries.size() || !m_entries[index].object) continue;
                auto const position = m_entries[index].object->getPosition();
                if (position.x >= viewportLeft && position.x <= viewportRight &&
                    position.y >= viewportBottom && position.y <= viewportTop) {
                    exportEntry(index);
                }
            }
        }
    }

    auto exportSectionGrid = [&](auto const& grid) {
        if (grid.empty()) return;
        auto const left = std::max(0, real->m_leftSectionIndex - 1);
        auto const right = std::min(static_cast<int>(grid.size()) - 1, real->m_rightSectionIndex + 1);
        if (left > right) return;
        for (auto sectionX = left; sectionX <= right; ++sectionX) {
            auto* column = grid[static_cast<std::size_t>(sectionX)];
            if (!column || column->empty()) continue;
            auto const bottom = std::max(0, real->m_bottomSectionIndex - 1);
            auto const top = std::min(static_cast<int>(column->size()) - 1, real->m_topSectionIndex + 1);
            for (auto sectionY = bottom; sectionY <= top; ++sectionY) {
                auto* objects = (*column)[static_cast<std::size_t>(sectionY)];
                if (!objects) continue;
                for (auto* object : *objects) exportObject(object);
            }
        }
    };
    if (!viewportValid || candidateCount < 16) {
        exportSectionGrid(real->m_nonEffectObjects);
        exportSectionGrid(real->m_sections);
    }

    // Player roots are exported after objects so they remain readable even
    // before sprite-atlas transport is introduced in a later protocol.
    appendNode(real->m_player1, {80, 255, 255, 255}, QuadKind::PlayerOne);
    appendNode(real->m_player2, {255, 230, 80, 255}, QuadKind::PlayerTwo);
    return true;
}
