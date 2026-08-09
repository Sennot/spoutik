#include "CompanionBridge.hpp"
#include "CompanionProtocol.hpp"
#include "LayoutMirror.hpp"
#include <Geode/loader/Mod.hpp>
#include <cstring>

#ifdef GEODE_IS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

using namespace geode::prelude;

CompanionBridge& CompanionBridge::get() {
    static CompanionBridge instance;
    return instance;
}

CompanionBridge::~CompanionBridge() {
    shutdown();
}

bool CompanionBridge::ensureMapping() {
#ifndef GEODE_IS_WINDOWS
    return false;
#else
    if (m_view) return true;

    auto handle = CreateFileMappingW(
        INVALID_HANDLE_VALUE,
        nullptr,
        PAGE_READWRITE,
        0,
        static_cast<DWORD>(sizeof(layout_companion::SharedFrame)),
        layout_companion::kSharedMemoryName
    );
    if (!handle) {
        log::error("Companion shared-memory creation failed: Win32 {}", GetLastError());
        return false;
    }

    auto* view = MapViewOfFile(
        handle,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        sizeof(layout_companion::SharedFrame)
    );
    if (!view) {
        log::error("Companion shared-memory mapping failed: Win32 {}", GetLastError());
        CloseHandle(handle);
        return false;
    }

    m_mappingHandle = handle;
    m_view = view;
    auto* frame = static_cast<layout_companion::SharedFrame*>(m_view);
    std::memset(frame, 0, sizeof(*frame));
    frame->magic = layout_companion::kMagic;
    frame->protocolVersion = layout_companion::kProtocolVersion;
    frame->byteSize = sizeof(layout_companion::SharedFrame);
    frame->producerProcessId = GetCurrentProcessId();
    if (!m_reportedReady) {
        log::info(
            "Companion bridge ready: {} KiB shared, protocol {}",
            sizeof(layout_companion::SharedFrame) / 1024,
            layout_companion::kProtocolVersion
        );
        m_reportedReady = true;
    }
    return true;
#endif
}

void CompanionBridge::publish(CCDirector* director, PlayLayer* real) {
#ifdef GEODE_IS_WINDOWS
    if (!Mod::get()->getSettingValue<bool>("enabled")) {
        suspend();
        return;
    }
    if (!real) {
        suspend();
        return;
    }
    auto const now = GetTickCount64();
    // The external renderer does not benefit from hundreds of IPC updates per
    // second. A 125 Hz cap protects high-FPS gameplay from bridge CPU churn.
    if (m_active && now - m_lastPublishAtMilliseconds < 8) return;
    if (!ensureMapping()) return;
    m_lastPublishAtMilliseconds = now;

    auto* frame = static_cast<layout_companion::SharedFrame*>(m_view);
    auto* sequence = reinterpret_cast<volatile LONG64*>(&frame->sequence);
    InterlockedIncrement64(sequence);
    MemoryBarrier();

    frame->magic = layout_companion::kMagic;
    frame->protocolVersion = layout_companion::kProtocolVersion;
    frame->byteSize = sizeof(layout_companion::SharedFrame);
    frame->producerProcessId = GetCurrentProcessId();
    frame->frameNumber = ++m_frameNumber;
    frame->producedAtMilliseconds = now;
    auto const active = LayoutMirror::get().writeCompanionFrame(*frame, director, real);
    if (!active) {
        frame->flags = 0;
        frame->quadCount = 0;
        frame->droppedQuadCount = 0;
        frame->sourceObjectCount = 0;
        frame->retainedObjectCount = 0;
    }

    MemoryBarrier();
    InterlockedIncrement64(sequence);
    m_active = active;
#else
    (void)director;
    (void)real;
#endif
}

void CompanionBridge::suspend() {
#ifdef GEODE_IS_WINDOWS
    if (!m_view || !m_active) return;
    auto* frame = static_cast<layout_companion::SharedFrame*>(m_view);
    auto* sequence = reinterpret_cast<volatile LONG64*>(&frame->sequence);
    InterlockedIncrement64(sequence);
    MemoryBarrier();
    frame->flags = 0;
    frame->quadCount = 0;
    frame->droppedQuadCount = 0;
    frame->sourceObjectCount = 0;
    frame->retainedObjectCount = 0;
    frame->frameNumber = ++m_frameNumber;
    frame->producedAtMilliseconds = GetTickCount64();
    MemoryBarrier();
    InterlockedIncrement64(sequence);
    m_active = false;
#endif
}

void CompanionBridge::shutdown() {
#ifdef GEODE_IS_WINDOWS
    suspend();
    if (m_view) UnmapViewOfFile(m_view);
    if (m_mappingHandle) CloseHandle(static_cast<HANDLE>(m_mappingHandle));
    m_view = nullptr;
    m_mappingHandle = nullptr;
#endif
}
