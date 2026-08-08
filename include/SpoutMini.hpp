#pragma once
#ifdef GEODE_IS_WINDOWS
#include <windows.h>
#include <cstdint>

// ABI-compatible prefix of Spout2 2.007.017 SPOUTLIBRARY.
// The exact order below is verified against upstream SpoutLibrary.h.
// Keeping only a prefix avoids a static import library and lets the .geode
// carry/load SpoutLibrary.dll entirely from its own resources.
struct SpoutMini {
    virtual void SetSenderName(const char* sendername = nullptr) = 0;
    virtual void SetSenderFormat(DWORD format) = 0;
    virtual void ReleaseSender(DWORD msec = 0) = 0;
    virtual bool SendFbo(unsigned int fbo, unsigned int width, unsigned int height, bool invert = true) = 0;
    virtual bool SendTexture(unsigned int texture, unsigned int target, unsigned int width, unsigned int height, bool invert = true, unsigned int hostFbo = 0) = 0;
    virtual bool SendImage(const unsigned char* pixels, unsigned int width, unsigned int height, unsigned int glFormat = 0x1908, bool invert = false) = 0;
    virtual bool IsInitialized() = 0;
    virtual const char* GetName() = 0;
    virtual unsigned int GetWidth() = 0;
    virtual unsigned int GetHeight() = 0;
    virtual double GetFps() = 0;
    virtual long GetFrame() = 0;
    virtual HANDLE GetHandle() = 0;
    virtual bool GetCPU() = 0;
    virtual bool GetGLDX() = 0;
};

using GetSpoutFn = SpoutMini* (WINAPI*)();
#endif
