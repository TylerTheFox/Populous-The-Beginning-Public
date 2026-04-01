// Pop3Screen.cpp
//
// Native Direct3D 9 display backend implementation.
// Manages D3D9 device lifecycle, presents the 8bpp software
// framebuffer as a textured quad, and provides overlay hooks.

#include "Pop3Screen.h"
#include "Pop3Debug.h"
#include <d3d9.h>
#include <windows.h>

#pragma comment(lib, "d3d9.lib")

// ---------------------------------------------------------------
//  Static member definitions
// ---------------------------------------------------------------
IDirect3D9*         Pop3Screen::s_pD3D              = nullptr;
IDirect3DDevice9*   Pop3Screen::s_pDevice            = nullptr;
IDirect3DTexture9*  Pop3Screen::s_pFramebufferTex    = nullptr;

Pop3ScreenCallback  Pop3Screen::s_overlayCallback    = nullptr;
Pop3ScreenCallback  Pop3Screen::s_deviceInitCallback = nullptr;
Pop3ScreenCallback  Pop3Screen::s_deviceDeinitCallback = nullptr;

void*               Pop3Screen::s_hWnd               = nullptr;
int                 Pop3Screen::s_backbufferWidth     = 0;
int                 Pop3Screen::s_backbufferHeight    = 0;
int                 Pop3Screen::s_framebufferWidth    = 0;
int                 Pop3Screen::s_framebufferHeight   = 0;
bool                Pop3Screen::s_windowed            = true;
bool                Pop3Screen::s_fullscreen          = false;
bool                Pop3Screen::s_border              = true;
bool                Pop3Screen::s_ready               = false;

// ---------------------------------------------------------------
//  Textured-quad vertex format for presenting the framebuffer
// ---------------------------------------------------------------
struct ScreenVertex
{
    float x, y, z, rhw;
    float u, v;
};
static const DWORD SCREEN_FVF = D3DFVF_XYZRHW | D3DFVF_TEX1;

// ---------------------------------------------------------------
//  Lifecycle
// ---------------------------------------------------------------

bool Pop3Screen::create(void* hWnd, int backbufferWidth, int backbufferHeight, bool windowed)
{
    s_hWnd = hWnd;
    s_framebufferWidth = backbufferWidth;
    s_framebufferHeight = backbufferHeight;
    s_windowed = windowed;
    s_fullscreen = !windowed;
    s_border = windowed;

    // In windowed mode, set the D3D9 backbuffer to the window's client size
    // so our point-filtered quad handles the upscaling (not the GPU driver).
    if (windowed)
    {
        RECT rc;
        GetClientRect((HWND)hWnd, &rc);
        s_backbufferWidth = rc.right - rc.left;
        s_backbufferHeight = rc.bottom - rc.top;
    }
    else
    {
        s_backbufferWidth = backbufferWidth;
        s_backbufferHeight = backbufferHeight;
    }

    s_pD3D = Direct3DCreate9(D3D_SDK_VERSION);
    if (!s_pD3D)
    {
        Pop3Debug::fatalError_NoReport("Pop3Screen: Failed to create Direct3D9 object");
        return false;
    }

    if (!createDevice())
        return false;

    s_ready = true;

    if (s_deviceInitCallback)
        s_deviceInitCallback();

    return true;
}

void Pop3Screen::destroy()
{
    if (s_deviceDeinitCallback)
        s_deviceDeinitCallback();

    releaseDevice();

    if (s_pD3D)
    {
        s_pD3D->Release();
        s_pD3D = nullptr;
    }

    s_ready = false;
}

bool Pop3Screen::isReady()
{
    return s_ready && s_pDevice != nullptr;
}

// ---------------------------------------------------------------
//  Internal device management
// ---------------------------------------------------------------

bool Pop3Screen::createDevice()
{
    D3DPRESENT_PARAMETERS pp;
    ZeroMemory(&pp, sizeof(pp));

    pp.Windowed = s_windowed ? TRUE : FALSE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferWidth = s_backbufferWidth;
    pp.BackBufferHeight = s_backbufferHeight;
    pp.BackBufferCount = 1;
    pp.hDeviceWindow = (HWND)s_hWnd;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    pp.EnableAutoDepthStencil = FALSE;

    if (!s_windowed)
    {
        pp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
    }

    HRESULT hr = s_pD3D->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        (HWND)s_hWnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING,
        &pp,
        &s_pDevice);

    if (FAILED(hr))
    {
        Pop3Debug::fatalError_NoReport("Pop3Screen: Failed to create D3D9 device (hr=0x%08X)", hr);
        return false;
    }

    return true;
}

void Pop3Screen::releaseDevice()
{
    if (s_pFramebufferTex)
    {
        s_pFramebufferTex->Release();
        s_pFramebufferTex = nullptr;
    }

    if (s_pDevice)
    {
        s_pDevice->Release();
        s_pDevice = nullptr;
    }
}

bool Pop3Screen::createFramebufferTexture(int width, int height)
{
    if (s_pFramebufferTex)
    {
        if (s_framebufferWidth == width && s_framebufferHeight == height)
            return true;
        s_pFramebufferTex->Release();
        s_pFramebufferTex = nullptr;
    }

    HRESULT hr = s_pDevice->CreateTexture(
        width, height, 1,
        D3DUSAGE_DYNAMIC,
        D3DFMT_X8R8G8B8,
        D3DPOOL_DEFAULT,
        &s_pFramebufferTex,
        nullptr);

    if (FAILED(hr))
        return false;

    s_framebufferWidth = width;
    s_framebufferHeight = height;
    return true;
}

// ---------------------------------------------------------------
//  Framebuffer presentation
// ---------------------------------------------------------------

void Pop3Screen::present(const unsigned char* pixels, int pitch,
                         int width, int height,
                         const unsigned char* palette)
{
    if (!s_pDevice || !pixels || !palette)
        return;

    // Handle device lost
    if (handleDeviceLost())
        return; // Skip this frame, device was just reset

    // Ensure we have a texture of the right size
    if (!createFramebufferTexture(width, height))
        return;

    // Upload 8bpp indexed pixels → 32bpp XRGB texture
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(s_pFramebufferTex->LockRect(0, &lr, nullptr, D3DLOCK_DISCARD)))
    {
        for (int y = 0; y < height; y++)
        {
            const unsigned char* srcRow = pixels + y * pitch;
            unsigned int* dstRow = reinterpret_cast<unsigned int*>(
                static_cast<unsigned char*>(lr.pBits) + y * lr.Pitch);

            for (int x = 0; x < width; x++)
            {
                // Palette entries are RGBX (R,G,B,reserved).
                // D3D expects XRGB (0x00RRGGBB).
                unsigned char idx = srcRow[x];
                unsigned char r = palette[idx * 4 + 0];
                unsigned char g = palette[idx * 4 + 1];
                unsigned char b = palette[idx * 4 + 2];
                dstRow[x] = (0xFF000000u) | (r << 16) | (g << 8) | b;
            }
        }

        s_pFramebufferTex->UnlockRect(0);
    }

    // Begin scene → draw framebuffer quad → overlay → end → present
    if (SUCCEEDED(s_pDevice->BeginScene()))
    {
        s_pDevice->Clear(0, nullptr, D3DCLEAR_TARGET, 0xFF000000, 1.0f, 0);

        drawFramebufferQuad();

        // Fire overlay callback (ImGui renders here)
        if (s_overlayCallback)
            s_overlayCallback();

        s_pDevice->EndScene();
    }

    s_pDevice->Present(nullptr, nullptr, nullptr, nullptr);
}

void Pop3Screen::drawFramebufferQuad()
{
    if (!s_pFramebufferTex || !s_pDevice)
        return;

    float w = static_cast<float>(s_backbufferWidth);
    float h = static_cast<float>(s_backbufferHeight);

    // Pixel-perfect fullscreen quad
    ScreenVertex verts[4] =
    {
        { -0.5f,     -0.5f,     0.0f, 1.0f,   0.0f, 0.0f },
        { w - 0.5f,  -0.5f,     0.0f, 1.0f,   1.0f, 0.0f },
        { -0.5f,     h - 0.5f,  0.0f, 1.0f,   0.0f, 1.0f },
        { w - 0.5f,  h - 0.5f,  0.0f, 1.0f,   1.0f, 1.0f },
    };

    // Set minimal render state for the framebuffer quad
    s_pDevice->SetTexture(0, s_pFramebufferTex);
    s_pDevice->SetFVF(SCREEN_FVF);

    // Point sampling for crisp pixel art
    s_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    s_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    s_pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

    // Disable alpha blending for the base framebuffer
    s_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    s_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    s_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    s_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

    s_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(ScreenVertex));

    s_pDevice->SetTexture(0, nullptr);
}

// ---------------------------------------------------------------
//  Callbacks
// ---------------------------------------------------------------

void Pop3Screen::setOverlayCallback(Pop3ScreenCallback cb)
{
    s_overlayCallback = cb;
}

void Pop3Screen::setDeviceInitCallback(Pop3ScreenCallback cb)
{
    s_deviceInitCallback = cb;
    // If device already exists, fire immediately
    if (s_ready && cb)
        cb();
}

void Pop3Screen::setDeviceDeinitCallback(Pop3ScreenCallback cb)
{
    s_deviceDeinitCallback = cb;
}

// ---------------------------------------------------------------
//  D3D9 device access
// ---------------------------------------------------------------

IDirect3DDevice9* Pop3Screen::getDevice()
{
    return s_pDevice;
}

// ---------------------------------------------------------------
//  Display state
// ---------------------------------------------------------------

void Pop3Screen::toggleFullscreen()
{
    if (!s_pDevice || !s_pD3D)
        return;

    // Notify deinit
    if (s_deviceDeinitCallback)
        s_deviceDeinitCallback();

    releaseDevice();

    s_windowed = !s_windowed;
    s_fullscreen = !s_windowed;

    HWND hWnd = (HWND)s_hWnd;

    if (s_windowed)
    {
        SetWindowLongPtr(hWnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
        SetWindowPos(hWnd, HWND_NOTOPMOST,
            100, 100, s_backbufferWidth, s_backbufferHeight,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        s_border = true;
    }
    else
    {
        SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hWnd, HWND_TOPMOST,
            0, 0,
            GetSystemMetrics(SM_CXSCREEN),
            GetSystemMetrics(SM_CYSCREEN),
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        s_border = false;
    }

    createDevice();

    // Notify init
    if (s_deviceInitCallback)
        s_deviceInitCallback();
}

int Pop3Screen::getRenderWidth()
{
    if (s_windowed)
    {
        RECT rc;
        ::GetClientRect((HWND)s_hWnd, &rc);
        return rc.right - rc.left;
    }
    return s_backbufferWidth;
}

int Pop3Screen::getRenderHeight()
{
    if (s_windowed)
    {
        RECT rc;
        ::GetClientRect((HWND)s_hWnd, &rc);
        return rc.bottom - rc.top;
    }
    return s_backbufferHeight;
}

bool Pop3Screen::isWindowed()
{
    return s_windowed;
}

bool Pop3Screen::isFullscreen()
{
    return s_fullscreen;
}

bool Pop3Screen::hasBorder()
{
    return s_border;
}

void Pop3Screen::getWindowRect(int& left, int& top, int& right, int& bottom)
{
    RECT rc;
    ::GetClientRect((HWND)s_hWnd, &rc);
    left = rc.left;
    top = rc.top;
    right = rc.right;
    bottom = rc.bottom;
}

bool Pop3Screen::handleDeviceLost()
{
    if (!s_pDevice)
        return true;

    HRESULT hr = s_pDevice->TestCooperativeLevel();

    if (hr == D3DERR_DEVICELOST)
        return true; // Can't reset yet, skip frame

    if (hr == D3DERR_DEVICENOTRESET)
    {
        // Release resources before reset
        if (s_deviceDeinitCallback)
            s_deviceDeinitCallback();

        if (s_pFramebufferTex)
        {
            s_pFramebufferTex->Release();
            s_pFramebufferTex = nullptr;
        }

        D3DPRESENT_PARAMETERS pp;
        ZeroMemory(&pp, sizeof(pp));
        pp.Windowed = s_windowed ? TRUE : FALSE;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.BackBufferFormat = D3DFMT_X8R8G8B8;
        pp.BackBufferWidth = s_backbufferWidth;
        pp.BackBufferHeight = s_backbufferHeight;
        pp.BackBufferCount = 1;
        pp.hDeviceWindow = (HWND)s_hWnd;
        pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

        hr = s_pDevice->Reset(&pp);
        if (FAILED(hr))
            return true;

        if (s_deviceInitCallback)
            s_deviceInitCallback();

        return true; // Skip this frame after reset
    }

    return false; // Device is fine
}
