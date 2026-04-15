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
bool                Pop3Screen::s_hwComposite         = false;
bool                Pop3Screen::s_debugBackbufferPink = false;

void Pop3Screen::setHwCompositeActive(bool on) { s_hwComposite = on; }
bool Pop3Screen::hwCompositeActive()           { return s_hwComposite; }

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

    // A8R8G8B8 so the alpha channel survives — the Phase 7 composite keys
    // on palette-index-255 → alpha=0 to punch the SW quad through to HW
    // geometry in the world area. X8R8G8B8 would throw away the alpha
    // bits we wrote into the texture, leaving a fully-opaque quad that
    // hides HW behind it.
    HRESULT hr = s_pDevice->CreateTexture(
        width, height, 1,
        D3DUSAGE_DYNAMIC,
        D3DFMT_A8R8G8B8,
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

    // If the window has been resized since device creation, the D3D9
    // backbuffer stays at its original size and Present() scales it to
    // the new client rect via the driver — that bilinear stretch on top
    // of our already-point-upscaled quad causes visible shimmer around
    // fine UI edges (sidebar separators, icon borders). Keep the
    // backbuffer in sync with the window client rect so Present is 1:1.
    if (matchWindowSizeToBackbuffer())
        return; // Skip this frame, device was just reset

    // Ensure we have a texture of the right size
    if (!createFramebufferTexture(width, height))
        return;

    // Phase 7 option A: when HW rendering is active, we flip the frame
    // composite order so HW world geometry (terrain/sprites/etc.) draws
    // FIRST onto a clean backbuffer, then the SW back surface layers on
    // top as an alpha-keyed quad (palette index 255 = fully transparent,
    // everything else opaque). That way UI drawn into the SW back
    // surface (panels, minimap, text, ghost-blended sprites that bail
    // back to SW) appears on top of the HW world exactly where the SW
    // pipeline wrote pixels, and HW terrain shows through everywhere
    // else. Without the reorder (pre-Phase-7), HW geometry drew on top
    // of the SW composite, so anything rendered into SW in the world
    // area got overdrawn by HW terrain and was invisible.
    //
    // Key palette index 255 is chosen because the SW pipeline clears the
    // back surface to it after each LbScreen_Swap when HW is active
    // (see LbScreen.cpp). It's an uncommon UI colour so we don't false-
    // positive on legit UI pixels; if the rest of the SW pipeline ends
    // up legitimately emitting 255 somewhere that looks transparent, we
    // pick a different sentinel.
    const bool hwComposite = s_hwComposite;
    // Palette index 0 as the transparent key. Rationale: RLE-encoded
    // sprites (which is how nearly every UI element draws) use byte 0 as
    // "end of row" in the decoder, so decoded sprite pixels never write
    // index 0 to the back surface. The earlier choice of 255 false-
    // positived the shaman portrait in the sidebar because 255 is the
    // cycled-highlight palette slot (see cycle_clr_255 in Palettes.cpp).
    const unsigned int kAlphaKeyIndex = 0;

    // Upload 8bpp indexed pixels → 32bpp ARGB texture. In HW-composite
    // mode, palette index 0 becomes alpha=0 (transparent); everywhere
    // else stays alpha=255. In SW-only mode, always alpha=255.
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
                const unsigned char idx = srcRow[x];
                const unsigned int alpha =
                    (hwComposite && idx == kAlphaKeyIndex) ? 0x00u : 0xFFu;
                const unsigned char r = palette[idx * 4 + 0];
                const unsigned char g = palette[idx * 4 + 1];
                const unsigned char b = palette[idx * 4 + 2];
                dstRow[x] = (alpha << 24) | (r << 16) | (g << 8) | b;
            }
        }

        s_pFramebufferTex->UnlockRect(0);
    }

    if (SUCCEEDED(s_pDevice->BeginScene()))
    {
        // Phase 8.5 #12c diag: bright-pink clear instead of black so we
        // can tell whether "black halos around sprites" are HW writing
        // black explicitly (halos stay black even with pink clear) vs
        // alpha-test discarding pixels and revealing the cleared bg
        // (halos become pink).
        const D3DCOLOR clearColour = s_debugBackbufferPink
                                     ? 0xFFFF00FFu  // bright magenta
                                     : 0xFF000000u; // black
        s_pDevice->Clear(0, nullptr, D3DCLEAR_TARGET, clearColour, 1.0f, 0);

        if (hwComposite)
        {
            // HW first (world geometry onto the clean backbuffer), then
            // the SW back surface as an alpha-keyed overlay quad.
            if (s_overlayCallback)
                s_overlayCallback();
            drawFramebufferQuad(/*alphaBlend=*/true);
        }
        else
        {
            // Legacy order: SW framebuffer quad, then overlay (ImGui only).
            drawFramebufferQuad(/*alphaBlend=*/false);
            if (s_overlayCallback)
                s_overlayCallback();
        }

        s_pDevice->EndScene();
    }

    s_pDevice->Present(nullptr, nullptr, nullptr, nullptr);
}

void Pop3Screen::drawFramebufferQuad(bool alphaBlend)
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

    // Reset texture stage ops — HwDisplayList::applyState may have left
    // COLOROP=SELECTARG2 (from HWRMODE_TINT) which would ignore the
    // framebuffer texture entirely and paint the quad in the default
    // diffuse colour (white) since our FVF has no vertex colour.
    s_pDevice->SetTextureStageState(0, D3DTSS_COLOROP,   D3DTOP_SELECTARG1);
    s_pDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    s_pDevice->SetTextureStageState(0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG1);
    s_pDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);

    // Point sampling for crisp pixel art
    s_pDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    s_pDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    s_pDevice->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);

    // Alpha blending for the Phase 7 composite — SW framebuffer overlaid
    // on HW world geometry with palette-index-255 → alpha=0 pixels
    // showing HW through. For SW-only mode we disable it (opaque quad).
    if (alphaBlend)
    {
        s_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
        s_pDevice->SetRenderState(D3DRS_SRCBLEND,  D3DBLEND_SRCALPHA);
        s_pDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    }
    else
    {
        s_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
    }
    s_pDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    s_pDevice->SetRenderState(D3DRS_ZENABLE, FALSE);
    s_pDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

    s_pDevice->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, verts, sizeof(ScreenVertex));

    s_pDevice->SetTexture(0, nullptr);
    s_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
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

int Pop3Screen::getBackbufferWidth()   { return s_backbufferWidth; }
int Pop3Screen::getBackbufferHeight()  { return s_backbufferHeight; }
int Pop3Screen::getFramebufferWidth()  { return s_framebufferWidth; }
int Pop3Screen::getFramebufferHeight() { return s_framebufferHeight; }

void Pop3Screen::setDebugBackbufferPink(bool on) { s_debugBackbufferPink = on; }
bool Pop3Screen::debugBackbufferPink()           { return s_debugBackbufferPink; }

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

bool Pop3Screen::matchWindowSizeToBackbuffer()
{
    // Only applies to windowed mode — fullscreen has a fixed backbuffer
    // negotiated against the display mode, not the client rect.
    if (!s_pDevice || !s_windowed || !s_hWnd)
        return false;

    RECT rc;
    if (!GetClientRect((HWND)s_hWnd, &rc))
        return false;

    int newW = rc.right - rc.left;
    int newH = rc.bottom - rc.top;

    // Minimised / zero-sized: don't attempt a zero-dim backbuffer.
    if (newW <= 0 || newH <= 0)
        return false;

    if (newW == s_backbufferWidth && newH == s_backbufferHeight)
        return false; // No change

    // Release all D3DPOOL_DEFAULT resources before Reset.
    if (s_deviceDeinitCallback)
        s_deviceDeinitCallback();

    if (s_pFramebufferTex)
    {
        s_pFramebufferTex->Release();
        s_pFramebufferTex = nullptr;
        // Force a re-create at the next present() so dims are re-taken.
        s_framebufferWidth = 0;
        s_framebufferHeight = 0;
    }

    D3DPRESENT_PARAMETERS pp;
    ZeroMemory(&pp, sizeof(pp));
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferWidth = newW;
    pp.BackBufferHeight = newH;
    pp.BackBufferCount = 1;
    pp.hDeviceWindow = (HWND)s_hWnd;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    pp.EnableAutoDepthStencil = FALSE;

    HRESULT hr = s_pDevice->Reset(&pp);
    if (FAILED(hr))
    {
        // If reset fails (device lost during resize, driver rejection),
        // leave state as-is; handleDeviceLost will pick it up next frame.
        return true;
    }

    s_backbufferWidth = newW;
    s_backbufferHeight = newH;

    if (s_deviceInitCallback)
        s_deviceInitCallback();

    return true; // Skip this frame
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
