#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_opengl.h>
#include <Windows.h>

#include "CompanionProtocol.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

namespace {
using namespace layout_companion;

class SharedReader final {
public:
    ~SharedReader() { close(); }

    bool connect() {
        if (m_view) return true;
        auto handle = OpenFileMappingW(FILE_MAP_READ, FALSE, kSharedMemoryName);
        if (!handle) return false;
        auto* view = static_cast<SharedFrame*>(MapViewOfFile(
            handle, FILE_MAP_READ, 0, 0, sizeof(SharedFrame)
        ));
        if (!view) {
            CloseHandle(handle);
            return false;
        }
        m_handle = handle;
        m_view = view;
        return true;
    }

    void close() {
        if (m_view) UnmapViewOfFile(m_view);
        if (m_handle) CloseHandle(m_handle);
        m_view = nullptr;
        m_handle = nullptr;
        m_lastSequence = 0;
    }

    bool read(SharedFrame& destination) {
        if (!m_view && !connect()) return false;
        auto* sequence = reinterpret_cast<volatile LONG64*>(&m_view->sequence);
        auto const before = static_cast<std::uint64_t>(InterlockedCompareExchange64(
            sequence, 0, 0
        ));
        if ((before & 1u) != 0 || before == m_lastSequence) return false;

        MemoryBarrier();
        auto count = std::min<std::uint32_t>(m_view->quadCount, kMaximumQuads);
        auto const bytes = offsetof(SharedFrame, quads) + sizeof(Quad) * count;
        std::memcpy(&destination, m_view, bytes);
        MemoryBarrier();

        auto const after = static_cast<std::uint64_t>(InterlockedCompareExchange64(
            sequence, 0, 0
        ));
        if (before != after || (after & 1u) != 0) return false;
        if (destination.magic != kMagic ||
            destination.protocolVersion != kProtocolVersion ||
            destination.byteSize != sizeof(SharedFrame)) {
            close();
            return false;
        }
        destination.quadCount = count;
        m_lastSequence = after;
        return true;
    }

private:
    HANDLE m_handle = nullptr;
    SharedFrame* m_view = nullptr;
    std::uint64_t m_lastSequence = 0;
};

void setColor(Color color) {
    glColor4ub(color.red, color.green, color.blue, color.alpha);
}

void drawRect(float left, float bottom, float right, float top, Color color) {
    setColor(color);
    glBegin(GL_QUADS);
    glVertex2f(left, bottom);
    glVertex2f(right, bottom);
    glVertex2f(right, top);
    glVertex2f(left, top);
    glEnd();
}

void drawQuad(Quad const& quad) {
    setColor(quad.color);
    glBegin(GL_QUADS);
    glVertex2f(quad.x0, quad.y0);
    glVertex2f(quad.x1, quad.y1);
    glVertex2f(quad.x2, quad.y2);
    glVertex2f(quad.x3, quad.y3);
    glEnd();

    if (quad.kind == QuadKind::PlayerOne || quad.kind == QuadKind::PlayerTwo) {
        glColor4ub(0, 0, 0, 255);
        glLineWidth(2.f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(quad.x0, quad.y0);
        glVertex2f(quad.x1, quad.y1);
        glVertex2f(quad.x2, quad.y2);
        glVertex2f(quad.x3, quad.y3);
        glEnd();
    }
}

bool hasArgument(int argc, char** argv, char const* wanted) {
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], wanted) == 0) return true;
    }
    return false;
}

bool frameIsActive(SharedFrame const& frame, bool connected, std::uint64_t now) {
    auto const fresh = connected && frame.producedAtMilliseconds <= now &&
        now - frame.producedAtMilliseconds < 1500;
    return fresh && (frame.flags & FrameActive) != 0 &&
        frame.logicalWidth > 1.f && frame.logicalHeight > 1.f;
}

struct FindWindowContext {
    DWORD processId = 0;
    HWND result = nullptr;
};

BOOL CALLBACK findProcessWindow(HWND candidate, LPARAM value) {
    auto& context = *reinterpret_cast<FindWindowContext*>(value);
    DWORD processId = 0;
    GetWindowThreadProcessId(candidate, &processId);
    if (processId != context.processId || !IsWindowVisible(candidate) ||
        GetWindow(candidate, GW_OWNER) != nullptr) {
        return TRUE;
    }
    RECT client {};
    if (!GetClientRect(candidate, &client) || client.right <= 1 || client.bottom <= 1) {
        return TRUE;
    }
    context.result = candidate;
    return FALSE;
}

HWND findProducerWindow(std::uint32_t processId) {
    FindWindowContext context {processId, nullptr};
    EnumWindows(findProcessWindow, reinterpret_cast<LPARAM>(&context));
    return context.result;
}

HWND getNativeWindow(SDL_Window* window) {
    return static_cast<HWND>(SDL_GetPointerProperty(
        SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr
    ));
}

void configureClickThrough(SDL_Window* window) {
    auto native = getNativeWindow(window);
    if (!native) return;
    auto const style = GetWindowLongPtrW(native, GWL_EXSTYLE);
    SetWindowLongPtrW(
        native,
        GWL_EXSTYLE,
        style | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE
    );
}

bool syncOverlayToProducer(SDL_Window* window, std::uint32_t processId) {
    auto overlay = getNativeWindow(window);
    auto producer = findProducerWindow(processId);
    if (!overlay || !producer) return false;

    RECT client {};
    POINT topLeft {};
    if (!GetClientRect(producer, &client) || !ClientToScreen(producer, &topLeft)) return false;
    auto const width = client.right - client.left;
    auto const height = client.bottom - client.top;
    if (width <= 1 || height <= 1) return false;
    return SetWindowPos(
        overlay,
        HWND_TOPMOST,
        topLeft.x,
        topLeft.y,
        width,
        height,
        SWP_NOACTIVATE | SWP_SHOWWINDOW
    ) != FALSE;
}

void renderFrame(SDL_Window* window, SharedFrame& frame, bool connected) {
    int pixelWidth = 0;
    int pixelHeight = 0;
    SDL_GetWindowSizeInPixels(window, &pixelWidth, &pixelHeight);
    pixelWidth = std::max(pixelWidth, 1);
    pixelHeight = std::max(pixelHeight, 1);
    glViewport(0, 0, pixelWidth, pixelHeight);
    glClearColor(0.025f, 0.035f, 0.055f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    auto const now = GetTickCount64();
    auto const active = frameIsActive(frame, connected, now);
    if (!active) {
        SDL_GL_SwapWindow(window);
        return;
    }

    auto const sourceAspect = frame.logicalWidth / frame.logicalHeight;
    auto const windowAspect = static_cast<float>(pixelWidth) / static_cast<float>(pixelHeight);
    int viewportWidth = pixelWidth;
    int viewportHeight = pixelHeight;
    int viewportX = 0;
    int viewportY = 0;
    if (windowAspect > sourceAspect) {
        viewportWidth = static_cast<int>(pixelHeight * sourceAspect);
        viewportX = (pixelWidth - viewportWidth) / 2;
    }
    else {
        viewportHeight = static_cast<int>(pixelWidth / sourceAspect);
        viewportY = (pixelHeight - viewportHeight) / 2;
    }
    glViewport(viewportX, viewportY, viewportWidth, viewportHeight);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, frame.logicalWidth, 0.0, frame.logicalHeight, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);

    drawRect(0.f, 0.f, frame.logicalWidth, frame.logicalHeight, frame.background);
    if (frame.groundTop > 0.f) {
        drawRect(0.f, 0.f, frame.logicalWidth, frame.groundTop, frame.ground);
        drawRect(
            0.f,
            std::max(0.f, frame.groundTop - 2.f),
            frame.logicalWidth,
            frame.groundTop,
            frame.groundLine
        );
    }

    std::stable_sort(
        frame.quads,
        frame.quads + frame.quadCount,
        [](Quad const& left, Quad const& right) {
            if (left.kind == QuadKind::PlayerOne || left.kind == QuadKind::PlayerTwo) return false;
            if (right.kind == QuadKind::PlayerOne || right.kind == QuadKind::PlayerTwo) return true;
            return left.zOrder < right.zOrder;
        }
    );
    for (std::uint32_t index = 0; index < frame.quadCount; ++index) {
        drawQuad(frame.quads[index]);
    }
    SDL_GL_SwapWindow(window);
}
}

int main(int argc, char** argv) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    auto flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (hasArgument(argc, argv, "--always-on-top")) flags |= SDL_WINDOW_ALWAYS_ON_TOP;
    auto const overlayMode = hasArgument(argc, argv, "--overlay");
    if (overlayMode) {
        flags |= SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP |
            SDL_WINDOW_NOT_FOCUSABLE | SDL_WINDOW_HIDDEN;
    }

    auto* window = SDL_CreateWindow("Layout Companion - waiting for Geometry Dash", 1280, 720, flags);
    if (!window) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    if (overlayMode) configureClickThrough(window);
    auto context = SDL_GL_CreateContext(window);
    if (!context) {
        std::fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    SDL_GL_SetSwapInterval(1);

    SharedReader reader;
    auto frame = std::make_unique<SharedFrame>();
    bool running = true;
    std::uint64_t nextConnectAttempt = 0;
    std::uint64_t nextTitleUpdate = 0;
    bool connected = false;
    bool overlayVisible = false;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) running = false;
        }

        auto const now = GetTickCount64();
        if (!connected && now >= nextConnectAttempt) {
            connected = reader.connect();
            nextConnectAttempt = now + 500;
        }
        if (connected) reader.read(*frame);

        auto const active = frameIsActive(*frame, connected, now);
        if (overlayMode) {
            if (active && syncOverlayToProducer(window, frame->producerProcessId)) {
                if (!overlayVisible) {
                    SDL_ShowWindow(window);
                    configureClickThrough(window);
                    overlayVisible = true;
                }
                renderFrame(window, *frame, connected);
            }
            else {
                if (overlayVisible) SDL_HideWindow(window);
                overlayVisible = false;
                SDL_Delay(4);
            }
        }
        else {
            renderFrame(window, *frame, connected);
        }
        if (now >= nextTitleUpdate) {
            auto const fresh = connected && frame->producedAtMilliseconds <= now &&
                now - frame->producedAtMilliseconds < 1500;
            std::string title;
            if (!connected) title = "Layout Companion - waiting for Geometry Dash";
            else if (!fresh) title = "Layout Companion - bridge idle";
            else if ((frame->flags & FrameActive) == 0) title = "Layout Companion - open a level";
            else {
                title = "Layout Companion - " + std::to_string(frame->quadCount) + " quads";
                if ((frame->flags & FrameTruncated) != 0) title += " (truncated)";
            }
            SDL_SetWindowTitle(window, title.c_str());
            nextTitleUpdate = now + 500;
        }
        SDL_Delay(1);
    }

    SDL_GL_DestroyContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
