#pragma once

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace layout_companion {

inline constexpr std::uint32_t kMagic = 0x59414C53; // "SLAY"
inline constexpr std::uint32_t kProtocolVersion = 1;
inline constexpr wchar_t kSharedMemoryName[] = L"Local\\SpoutLayoutCompanion-v1";
inline constexpr std::size_t kMaximumQuads = 16384;

enum FrameFlags : std::uint32_t {
    FrameActive = 1u << 0,
    FrameTruncated = 1u << 1,
};

enum class QuadKind : std::uint16_t {
    ObjectMain = 0,
    ObjectDetail = 1,
    PlayerOne = 2,
    PlayerTwo = 3,
};

struct Color final {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
    std::uint8_t alpha = 255;
};

struct Quad final {
    float x0 = 0.f;
    float y0 = 0.f;
    float x1 = 0.f;
    float y1 = 0.f;
    float x2 = 0.f;
    float y2 = 0.f;
    float x3 = 0.f;
    float y3 = 0.f;
    std::int32_t zOrder = 0;
    QuadKind kind = QuadKind::ObjectMain;
    std::uint16_t reserved = 0;
    Color color {};
};

struct alignas(64) SharedFrame final {
    // Odd means that Geometry Dash is writing. The companion accepts a frame
    // only when two reads observe the same even sequence number.
    alignas(8) std::uint64_t sequence = 0;
    std::uint32_t magic = kMagic;
    std::uint32_t protocolVersion = kProtocolVersion;
    std::uint32_t byteSize = 0;
    std::uint32_t producerProcessId = 0;
    std::uint64_t frameNumber = 0;
    std::uint64_t producedAtMilliseconds = 0;
    std::uint32_t flags = 0;
    float logicalWidth = 0.f;
    float logicalHeight = 0.f;
    float groundTop = 0.f;
    Color background {40, 125, 255, 255};
    Color ground {0, 102, 255, 255};
    Color groundLine {255, 255, 255, 255};
    std::uint32_t quadCount = 0;
    std::uint32_t droppedQuadCount = 0;
    std::uint32_t sourceObjectCount = 0;
    std::uint32_t retainedObjectCount = 0;
    Quad quads[kMaximumQuads] {};
};

static_assert(std::is_standard_layout_v<Color>);
static_assert(std::is_standard_layout_v<Quad>);
static_assert(std::is_standard_layout_v<SharedFrame>);
static_assert(std::is_trivially_copyable_v<SharedFrame>);
static_assert(offsetof(SharedFrame, sequence) == 0);

} // namespace layout_companion
