#pragma once

// Pop3Screen.h
//
// Native Direct3D 9 display backend for PopTB Platform.
// Replaces the CNCDDraw wrapper DLL with direct D3D9 device management,
// framebuffer presentation (8bpp palette → D3D9 texture), and overlay
// rendering callbacks for ImGui.

struct IDirect3D9;
struct IDirect3DDevice9;
struct IDirect3DTexture9;

typedef void(__stdcall* Pop3ScreenCallback)(void);

class Pop3Screen
{
public:
    Pop3Screen() = delete;

    // ---------------------------------------------------------------
    //  Lifecycle
    // ---------------------------------------------------------------

    // Create the D3D9 device for the given window.
    // Call once after the HWND is created (LbScreen_Initialise).
    static bool create(void* hWnd, int backbufferWidth, int backbufferHeight, bool windowed);

    // Tear down the D3D9 device and release all resources.
    static void destroy();

    // Returns true if the D3D9 device is valid and ready.
    static bool isReady();

    // ---------------------------------------------------------------
    //  Framebuffer presentation
    // ---------------------------------------------------------------

    // Present the software-rendered 8bpp framebuffer to the screen.
    //   pixels      - pointer to 8bpp indexed pixel data
    //   pitch       - row stride in bytes
    //   width/height- framebuffer dimensions
    //   palette     - 256-entry RGBX palette (4 bytes per entry: R,G,B,X)
    //
    // This uploads the framebuffer into a D3D9 texture, draws a
    // fullscreen quad, invokes the overlay callback, and calls Present().
    static void present(const unsigned char* pixels, int pitch,
                        int width, int height,
                        const unsigned char* palette);

    // ---------------------------------------------------------------
    //  Overlay rendering callback
    // ---------------------------------------------------------------

    // Register a callback invoked between the framebuffer quad draw
    // and the Present() call — used for ImGui overlay rendering.
    static void setOverlayCallback(Pop3ScreenCallback cb);

    // Register callbacks for D3D9 device init and shutdown —
    // used for ImGui device setup/teardown.
    static void setDeviceInitCallback(Pop3ScreenCallback cb);
    static void setDeviceDeinitCallback(Pop3ScreenCallback cb);

    // ---------------------------------------------------------------
    //  D3D9 device access
    // ---------------------------------------------------------------

    // Returns the D3D9 device pointer (for ImGui, textures, etc.)
    static IDirect3DDevice9* getDevice();

    // ---------------------------------------------------------------
    //  Display state
    // ---------------------------------------------------------------

    // Toggle between windowed and fullscreen mode.
    static void toggleFullscreen();

    // Current render dimensions (output/window size after upscale).
    static int getRenderWidth();
    static int getRenderHeight();

    // True if running in windowed mode.
    static bool isWindowed();

    // True if running in fullscreen mode.
    static bool isFullscreen();

    // True if windowed mode has a border.
    static bool hasBorder();

    // Get the window client rect (output size).
    static void getWindowRect(int& left, int& top, int& right, int& bottom);

    // Handle device lost / reset (e.g. Alt+Tab).
    static bool handleDeviceLost();

    // Phase 7: game calls this when HwRender::setActive flips. When on,
    // present() flips composite order (HW first, SW alpha-keyed quad on
    // top) and clears the back surface to the alpha-key palette index
    // after every LbScreen_Swap so each frame starts fully transparent.
    static void setHwCompositeActive(bool on);
    static bool hwCompositeActive();

private:
    static IDirect3D9*          s_pD3D;
    static IDirect3DDevice9*    s_pDevice;
    static IDirect3DTexture9*   s_pFramebufferTex;

    static Pop3ScreenCallback   s_overlayCallback;
    static Pop3ScreenCallback   s_deviceInitCallback;
    static Pop3ScreenCallback   s_deviceDeinitCallback;

    static void*                s_hWnd;
    static int                  s_backbufferWidth;
    static int                  s_backbufferHeight;
    static int                  s_framebufferWidth;
    static int                  s_framebufferHeight;
    static bool                 s_windowed;
    static bool                 s_fullscreen;
    static bool                 s_border;
    static bool                 s_ready;
    static bool                 s_hwComposite;    // Phase 7 composite-order flip

    static bool createDevice();
    static void releaseDevice();
    static bool createFramebufferTexture(int width, int height);
    static void drawFramebufferQuad(bool alphaBlend = false);
    // Detect a window resize and Reset() the device so the backbuffer tracks
    // the window client rect. Returns true if the device was just reset and
    // the caller should skip this frame (mirrors handleDeviceLost).
    static bool matchWindowSizeToBackbuffer();
};
