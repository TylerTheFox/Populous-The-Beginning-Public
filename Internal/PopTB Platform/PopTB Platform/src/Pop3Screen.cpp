// Pop3Screen.cpp
//
// Native Direct3D 9 display backend implementation.
// Manages D3D9 device lifecycle, presents the 8bpp software
// framebuffer as a textured quad, and provides overlay hooks.

#include "Pop3Screen.h"
#include "Pop3Debug.h"
#include <d3d9.h>
#include <windows.h>
#include <d3dx9.h>
#include <cstring>
#include <cstdio>
#include "../../../../Tracy.h"
#include "../../../../TracyWin.h"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9.lib")

// ---------------------------------------------------------------
//  Static member definitions
// ---------------------------------------------------------------
IDirect3D9*         Pop3Screen::s_pD3D              = nullptr;
IDirect3DDevice9*   Pop3Screen::s_pDevice            = nullptr;
IDirect3DTexture9*  Pop3Screen::s_pFramebufferTex    = nullptr;

Pop3ScreenCallback  Pop3Screen::s_overlayCallback    = nullptr;
Pop3ScreenCallback  Pop3Screen::s_hwUiOverlayCallback = nullptr;
Pop3ScreenCallback  Pop3Screen::s_deviceInitCallback = nullptr;
Pop3ScreenCallback  Pop3Screen::s_deviceDeinitCallback = nullptr;
Pop3ScreenCallback  Pop3Screen::s_deviceDestroyCallback = nullptr;

void*               Pop3Screen::s_hWnd               = nullptr;
int                 Pop3Screen::s_backbufferWidth     = 0;
int                 Pop3Screen::s_backbufferHeight    = 0;
int                 Pop3Screen::s_framebufferWidth    = 0;
int                 Pop3Screen::s_framebufferHeight   = 0;
bool                Pop3Screen::s_windowed            = true;
bool                Pop3Screen::s_fullscreen          = false;
bool                Pop3Screen::s_border              = true;
// The user's windowed border PREFERENCE, distinct from the border the window
// happens to have right now: an Alt+Enter round trip must come back to the
// style the user configured, not to a hardcoded bordered window.
bool                Pop3Screen::s_borderPref          = true;
bool                Pop3Screen::s_ready               = false;
bool                Pop3Screen::s_hwComposite         = false;
bool                Pop3Screen::s_debugBackbufferPink = false;

bool                Pop3Screen::s_vsync               = true;   // default: vsync on
int                 Pop3Screen::s_maxFps              = 0;
bool                Pop3Screen::s_maintas             = false;  // ddraw.ini maintas (letterbox)
IDirect3DTexture9*      Pop3Screen::s_pIndexTex       = nullptr;
IDirect3DTexture9*      Pop3Screen::s_pPaletteTex     = nullptr;
IDirect3DPixelShader9*  Pop3Screen::s_pPalettePS      = nullptr;
int                 Pop3Screen::s_indexTexW           = 0;
int                 Pop3Screen::s_indexTexH           = 0;
bool                Pop3Screen::s_gpuPalette          = true;
bool                Pop3Screen::s_gpuActiveFrame      = false;
// ---------------------------------------------------------------
//  Device identity latch (telemetry)
//
//  D3DADAPTER_IDENTIFIER9 is owned by s_pD3D, which destroy() releases,
//  and the vertex-processing mode is a local in createDevice. The
//  diagnostics report is built after teardown, so sample once here and
//  report the copy - the same rule the am_i_host field learned.
//  Written only in createDevice (boot / device rebuild), read only by the
//  accessors. Display-only: never read by the sim.
// ---------------------------------------------------------------
namespace {
    char         _id_adapterDesc[MAX_DEVICE_IDENTIFIER_STRING]   = { 0 };
    char         _id_adapterDriver[MAX_DEVICE_IDENTIFIER_STRING] = { 0 };
    char         _id_driverVersion[40]                           = { 0 };
    unsigned int _id_vendorId    = 0;
    unsigned int _id_deviceId    = 0;
    unsigned int _id_subSysId    = 0;
    unsigned int _id_maxTexW     = 0;
    unsigned int _id_maxTexH     = 0;
    unsigned int _id_availTexMem = 0;   // bytes, sampled right after create
    int          _id_vpMode      = -1;  // 2=HW 1=MIXED 0=SW, -1 = never created
}

// ---------------------------------------------------------------
//  Frame-timing introspection (LOCAL-ONLY; QueryPerformanceCounter).
//  Never read from deterministic sim/lockstep code - us values differ
//  per machine. Reported once/sec via Pop3Debug::trace and exposed via
//  getFrameTimingMs() for an optional on-screen overlay.
// ---------------------------------------------------------------
namespace {
    inline long long _pp_qpc()
    {
        LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart;
    }
    inline long long _pp_freq()
    {
        static long long f = []{ LARGE_INTEGER x; QueryPerformanceFrequency(&x); return x.QuadPart; }();
        return f;
    }
    inline double _pp_ms(long long a, long long b)
    {
        return (double)(b - a) * 1000.0 / (double)_pp_freq();
    }

    long long _pp_prevPresent  = 0;   // present-to-present anchor
    long long _pp_lastCapTicks = 0;   // maxfps limiter anchor
    long long _pp_winStart     = 0;   // 1s report window anchor

    double _pp_accConvert = 0.0, _pp_accDraw = 0.0, _pp_accPresent = 0.0, _pp_accFrame = 0.0;
    int    _pp_accCount = 0;

    double _pp_avgConvert = 0.0, _pp_avgDraw = 0.0, _pp_avgPresent = 0.0, _pp_avgFrame = 0.0, _pp_avgFps = 0.0;

    // Suppress the once/sec [frametime] fps log line by default - it spammed
    // the debug output. Set true to re-enable. The averages above are still
    // computed for the on-screen overlay (getFrameTimingMs()).
    bool   _pp_logFps = false;
}

void Pop3Screen::setPresentMode(bool vsync, int maxFps)
{
    s_vsync  = vsync;
    s_maxFps = (maxFps > 0) ? maxFps : 0;
}

void Pop3Screen::applyPresentMode(bool vsync, int maxFps)
{
    const bool intervalChanged = (vsync != s_vsync);
    setPresentMode(vsync, maxFps);

    // The CPU cap is read live each present; only the D3D presentation
    // interval is baked into the device and needs a Reset.
    if (!intervalChanged || !isReady())
        return;

    // Reset-grade teardown, mirroring matchWindowSizeToBackbuffer: release
    // DEFAULT-pool resources, Reset to the SAME dims with the new interval.
    if (s_deviceDeinitCallback)
        s_deviceDeinitCallback();

    if (s_pFramebufferTex)
    {
        s_pFramebufferTex->Release();
        s_pFramebufferTex = nullptr;
    }
    if (s_pIndexTex) { s_pIndexTex->Release(); s_pIndexTex = nullptr; s_indexTexW = s_indexTexH = 0; }
    // Force a re-create at the next present() so dims are re-taken.
    s_framebufferWidth = 0;
    s_framebufferHeight = 0;

    D3DPRESENT_PARAMETERS pp;
    ZeroMemory(&pp, sizeof(pp));
    pp.Windowed = s_windowed ? TRUE : FALSE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferWidth = s_backbufferWidth;
    pp.BackBufferHeight = s_backbufferHeight;
    pp.BackBufferCount = 2;
    pp.hDeviceWindow = (HWND)s_hWnd;
    pp.PresentationInterval = s_vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    if (!s_windowed)
        pp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;

    HRESULT hr = s_pDevice->Reset(&pp);
    if (FAILED(hr))
    {
        // LOST device: do NOT rebuild (an unfocused exclusive CreateDevice
        // fails too). s_vsync is already latched; handleDeviceLost's next
        // successful Reset picks it up.
        if (s_pDevice->TestCooperativeLevel() != D3D_OK)
        {
            // Leave state as-is (no init callback); handleDeviceLost
            // recovers next frame and fires the init callback then.
            return;
        }
        // Genuine parameter rejection on a healthy device: full rebuild,
        // same fallback as toggleFullscreen.
        if (s_deviceDestroyCallback)
            s_deviceDestroyCallback();
        else if (s_deviceDeinitCallback)
            s_deviceDeinitCallback();
        releaseDevice();
        createDevice();
    }

    if (s_deviceInitCallback)
        s_deviceInitCallback();
}

void Pop3Screen::setMaintainAspect(bool on) { s_maintas = on; }
bool Pop3Screen::maintainAspect()           { return s_maintas; }

void Pop3Screen::getPresentRect(int& x, int& y, int& w, int& h)
{
    x = 0;
    y = 0;
    w = s_backbufferWidth;
    h = s_backbufferHeight;

    if (!s_maintas || s_framebufferWidth <= 0 || s_framebufferHeight <= 0 ||
        s_backbufferWidth <= 0 || s_backbufferHeight <= 0)
        return;

    // Aspect-fit: scale the framebuffer uniformly to the largest size that
    // fits the backbuffer, centered. Bars are the regular black Clear.
    const float scaleX = (float)s_backbufferWidth  / (float)s_framebufferWidth;
    const float scaleY = (float)s_backbufferHeight / (float)s_framebufferHeight;
    const float scale  = (scaleX < scaleY) ? scaleX : scaleY;

    const int destW = (int)((float)s_framebufferWidth  * scale + 0.5f);
    const int destH = (int)((float)s_framebufferHeight * scale + 0.5f);
    x = (s_backbufferWidth  - destW) / 2;
    y = (s_backbufferHeight - destH) / 2;
    w = destW;
    h = destH;
}

void Pop3Screen::getFrameTimingMs(double& convertUpload, double& hwDraw,
                                  double& present, double& frameTotal, double& fps)
{
    convertUpload = _pp_avgConvert;
    hwDraw        = _pp_avgDraw;
    present       = _pp_avgPresent;
    frameTotal    = _pp_avgFrame;
    fps           = _pp_avgFps;
}
// Device identity latch readers - see the latch block near the top of this
// file. All valid after destroy(); "" / 0 / -1 before the first createDevice.
const char*  Pop3Screen::adapterDescription()        { return _id_adapterDesc; }
const char*  Pop3Screen::adapterDriver()             { return _id_adapterDriver; }
const char*  Pop3Screen::adapterDriverVersion()      { return _id_driverVersion; }
unsigned int Pop3Screen::adapterVendorId()           { return _id_vendorId; }
unsigned int Pop3Screen::adapterDeviceId()           { return _id_deviceId; }
unsigned int Pop3Screen::adapterSubSysId()           { return _id_subSysId; }
unsigned int Pop3Screen::deviceMaxTextureWidth()     { return _id_maxTexW; }
unsigned int Pop3Screen::deviceMaxTextureHeight()    { return _id_maxTexH; }
unsigned int Pop3Screen::availableTextureMemAtInit() { return _id_availTexMem; }
int          Pop3Screen::vertexProcessingMode()      { return _id_vpMode; }

bool Pop3Screen::presentVsync()  { return s_vsync; }
int  Pop3Screen::presentMaxFps() { return s_maxFps; }

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

bool Pop3Screen::create(void* hWnd, int backbufferWidth, int backbufferHeight, bool windowed, bool borderless)
{
    s_hWnd = hWnd;
    s_framebufferWidth = backbufferWidth;
    s_framebufferHeight = backbufferHeight;
    s_windowed = windowed;
    s_fullscreen = !windowed || borderless;
    s_border = windowed && !borderless;

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
    // Full device teardown (see toggleFullscreen): use the destroy callback so
    // every device-owned resource is released, not just the DEFAULT pool.
    if (s_deviceDestroyCallback)
        s_deviceDestroyCallback();
    else if (s_deviceDeinitCallback)
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
    pp.BackBufferCount = 2; // one queued frame so Present() does not stall the
                            // CPU-bound frame loop; worst case +<=1 vblank latency, only when the queue is full
    pp.hDeviceWindow = (HWND)s_hWnd;
    pp.PresentationInterval = s_vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24S8;

    if (!s_windowed)
    {
        pp.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
    }

    // Prefer hardware vertex processing: under SWVP the HwGpuLand vs_2_0
    // shader runs on the CPU. NEVER add D3DCREATE_MULTITHREADED (hard
    // project invariant) and never PUREDEVICE (imgui calls GetTransform).
    // On failure fall back MIXED then SOFTWARE, rebuilding pp each retry
    // (CreateDevice may scribble on it).
    const D3DPRESENT_PARAMETERS pp_pristine = pp;
    DWORD behaviour = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
    {
        D3DCAPS9 caps;
        if (SUCCEEDED(s_pD3D->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps)) &&
            (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT))
        {
            behaviour = (caps.VertexShaderVersion >= D3DVS_VERSION(2, 0))
                ? D3DCREATE_HARDWARE_VERTEXPROCESSING
                : D3DCREATE_MIXED_VERTEXPROCESSING;
        }
    }

    HRESULT hr = s_pD3D->CreateDevice(
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        (HWND)s_hWnd,
        behaviour,
        &pp,
        &s_pDevice);

    if (FAILED(hr) && behaviour == D3DCREATE_HARDWARE_VERTEXPROCESSING)
    {
        pp = pp_pristine;
        behaviour = D3DCREATE_MIXED_VERTEXPROCESSING;
        hr = s_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
            (HWND)s_hWnd, behaviour, &pp, &s_pDevice);
    }
    if (FAILED(hr) && behaviour != D3DCREATE_SOFTWARE_VERTEXPROCESSING)
    {
        pp = pp_pristine;
        behaviour = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
        hr = s_pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
            (HWND)s_hWnd, behaviour, &pp, &s_pDevice);
    }
    if (SUCCEEDED(hr))
    {

        Pop3Debug::trace("Pop3Screen: vertex processing mode = %s",
            behaviour == D3DCREATE_HARDWARE_VERTEXPROCESSING ? "hardware" :
            behaviour == D3DCREATE_MIXED_VERTEXPROCESSING ? "mixed" : "software");
        // Latch (see the device-identity block near the top of this file):
        // `behaviour` dies with this call and GetAvailableTextureMem needs a
        // live device, but diagnostics run long after teardown.
        _id_vpMode = (behaviour == D3DCREATE_HARDWARE_VERTEXPROCESSING) ? 2
                   : (behaviour == D3DCREATE_MIXED_VERTEXPROCESSING)    ? 1 : 0;
        _id_availTexMem = s_pDevice->GetAvailableTextureMem();

        // 0002585: one-time device caps dump so a user log shows POW2 support,
        // max texture size and adapter - needed to interpret HwTexture::create
        // failures (e.g. NPOT rejects on a strict-POW2 device).
        D3DCAPS9 devCaps;
        if (SUCCEEDED(s_pDevice->GetDeviceCaps(&devCaps)))
        {
            _id_maxTexW = (unsigned int)devCaps.MaxTextureWidth;
            _id_maxTexH = (unsigned int)devCaps.MaxTextureHeight;

            D3DADAPTER_IDENTIFIER9 adapterId;
            const char* adapter = "?";
            if (SUCCEEDED(s_pD3D->GetAdapterIdentifier(D3DADAPTER_DEFAULT, 0, &adapterId)))
            {
                adapter = adapterId.Description;
                // Latch the whole identifier: s_pD3D is Release()d in
                // destroy() and the diagnostics report is built after that.
                strncpy(_id_adapterDesc, adapterId.Description, sizeof(_id_adapterDesc) - 1);
                _id_adapterDesc[sizeof(_id_adapterDesc) - 1] = '\0';
                strncpy(_id_adapterDriver, adapterId.Driver, sizeof(_id_adapterDriver) - 1);
                _id_adapterDriver[sizeof(_id_adapterDriver) - 1] = '\0';
                _id_vendorId = (unsigned int)adapterId.VendorId;
                _id_deviceId = (unsigned int)adapterId.DeviceId;
                _id_subSysId = (unsigned int)adapterId.SubSysId;
                // DriverVersion is the usual packed
                // product.version.subversion.build QWORD.
                _snprintf_s(_id_driverVersion, sizeof(_id_driverVersion), _TRUNCATE,
                            "%u.%u.%u.%u",
                            (unsigned int)HIWORD(adapterId.DriverVersion.HighPart),
                            (unsigned int)LOWORD(adapterId.DriverVersion.HighPart),
                            (unsigned int)HIWORD(adapterId.DriverVersion.LowPart),
                            (unsigned int)LOWORD(adapterId.DriverVersion.LowPart));
            }
            Pop3Debug::trace(
                "Pop3Screen: device caps - adapter='%s' vendor=0x%04X device=0x%04X drv=%s "
                "maxTex=%lux%lu POW2=%d NONPOW2COND=%d vram_est=%uMB",
                adapter, _id_vendorId, _id_deviceId,
                _id_driverVersion[0] ? _id_driverVersion : "?",
                devCaps.MaxTextureWidth, devCaps.MaxTextureHeight,
                (devCaps.TextureCaps & D3DPTEXTURECAPS_POW2) ? 1 : 0,
                (devCaps.TextureCaps & D3DPTEXTURECAPS_NONPOW2CONDITIONAL) ? 1 : 0,
                _id_availTexMem >> 20);
        }
    }

    if (FAILED(hr))
    {
        // NOT fatal here: exclusive devices can be enumerable but uncreatable
        // (RDP, another exclusive app, hybrid GPUs). Return false so
        // LbScreen_SetMode reports LB_ERROR and the caller falls back; the
        // startup path still hard-fails in setup_host.
        Pop3Debug::trace("Pop3Screen: Failed to create D3D9 device (hr=0x%08X)", hr);
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

    releaseGpuPaletteResources();

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

    // Must be A8R8G8B8: the HW composite keys the transparent palette index
    // to alpha=0 to punch the SW quad through to HW geometry; X8R8G8B8
    // would drop those alpha bits and hide HW behind an opaque quad.
    // Refuse (loudly) framebuffer textures beyond the device's texture
    // caps - some drivers cap at 4096/8192 and a silent CreateTexture
    // failure produces a black screen with no log.
    {
        D3DCAPS9 caps;
        if (SUCCEEDED(s_pDevice->GetDeviceCaps(&caps)) &&
            ((DWORD)width > caps.MaxTextureWidth || (DWORD)height > caps.MaxTextureHeight))
        {
            Pop3Debug::trace("Pop3Screen: framebuffer %dx%d exceeds device texture caps %lux%lu",
                width, height, caps.MaxTextureWidth, caps.MaxTextureHeight);
            return false;
        }
    }

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
//  GPU palette-lookup path (offloads the per-frame CPU convert):
//  raw 8bpp indices in an L8 texture + a 256x1 LUT (rebuilt each frame -
//  it cycles for water), ps_2_0 shader does the lookup. The index-0 ->
//  alpha-0 composite key is baked into the LUT alpha. Falls back to the
//  CPU convert if any resource cannot be created.
// ---------------------------------------------------------------
bool Pop3Screen::createPaletteShader()
{
    if (s_pPalettePS) return true;
    if (!s_pDevice)   return false;

    static const char* kSrc =
        "sampler2D idxTex : register(s0);\n"
        "sampler2D palTex : register(s1);\n"
        "float4 main(float2 uv : TEXCOORD0) : COLOR0 {\n"
        "  float idx = tex2D(idxTex, uv).r;\n"
        "  float u = idx * (255.0/256.0) + (0.5/256.0);\n"
        "  return tex2D(palTex, float2(u, 0.5));\n"
        "}\n";

    LPD3DXBUFFER code = nullptr, err = nullptr;
    HRESULT hr = D3DXCompileShader(kSrc, (UINT)strlen(kSrc), nullptr, nullptr,
                                   "main", "ps_2_0", 0, &code, &err, nullptr);
    if (FAILED(hr) || !code)
    {
        if (err) { Pop3Debug::trace("[gpupal] ps compile failed: %s\n", (const char*)err->GetBufferPointer()); err->Release(); }
        if (code) code->Release();
        return false;
    }
    hr = s_pDevice->CreatePixelShader(reinterpret_cast<const DWORD*>(code->GetBufferPointer()), &s_pPalettePS);
    code->Release();
    if (err) err->Release();
    return SUCCEEDED(hr) && s_pPalettePS != nullptr;
}

bool Pop3Screen::ensurePaletteTexture()
{
    if (s_pPaletteTex) return true;
    if (!s_pDevice)    return false;
    HRESULT hr = s_pDevice->CreateTexture(256, 1, 1, 0, D3DFMT_A8R8G8B8,
                                          D3DPOOL_MANAGED, &s_pPaletteTex, nullptr);
    return SUCCEEDED(hr) && s_pPaletteTex != nullptr;
}

bool Pop3Screen::createIndexTexture(int width, int height)
{
    if (s_pIndexTex)
    {
        if (s_indexTexW == width && s_indexTexH == height) return true;
        s_pIndexTex->Release();
        s_pIndexTex = nullptr;
    }
    // Caps guard + loud failure (see createFramebufferTexture)
    {
        D3DCAPS9 caps;
        if (SUCCEEDED(s_pDevice->GetDeviceCaps(&caps)) &&
            ((DWORD)width > caps.MaxTextureWidth || (DWORD)height > caps.MaxTextureHeight))
        {
            Pop3Debug::trace("Pop3Screen: index texture %dx%d exceeds device texture caps %lux%lu",
                width, height, caps.MaxTextureWidth, caps.MaxTextureHeight);
            return false;
        }
    }
    HRESULT hr = s_pDevice->CreateTexture(width, height, 1, D3DUSAGE_DYNAMIC,
                                          D3DFMT_L8, D3DPOOL_DEFAULT, &s_pIndexTex, nullptr);
    if (FAILED(hr))
    {
        s_pIndexTex = nullptr;
        Pop3Debug::trace("Pop3Screen: index texture CreateTexture(%dx%d) failed", width, height);
        return false;
    }
    s_indexTexW = width; s_indexTexH = height;
    return true;
}

void Pop3Screen::uploadIndices(const unsigned char* pixels, int pitch, int width, int height)
{
    if (!s_pIndexTex) return;
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(s_pIndexTex->LockRect(0, &lr, nullptr, D3DLOCK_DISCARD)))
    {
        unsigned char* dst = static_cast<unsigned char*>(lr.pBits);
        for (int y = 0; y < height; y++)
            memcpy(dst + y * lr.Pitch, pixels + y * pitch, width);
        s_pIndexTex->UnlockRect(0);
    }
}

void Pop3Screen::uploadPalette(const unsigned char* palette, bool composite)
{
    if (!s_pPaletteTex) return;
    D3DLOCKED_RECT lr;
    if (SUCCEEDED(s_pPaletteTex->LockRect(0, &lr, nullptr, 0)))
    {
        unsigned int* dst = static_cast<unsigned int*>(lr.pBits);
        for (int i = 0; i < 256; i++)
        {
            const unsigned char r = palette[i * 4 + 0];
            const unsigned char g = palette[i * 4 + 1];
            const unsigned char b = palette[i * 4 + 2];
            const unsigned int a = (composite && i == 0) ? 0x00u : 0xFFu;
            dst[i] = (a << 24) | (r << 16) | (g << 8) | b;
        }
        s_pPaletteTex->UnlockRect(0);
    }
}

void Pop3Screen::releaseGpuPaletteResources()
{
    if (s_pIndexTex)   { s_pIndexTex->Release();   s_pIndexTex = nullptr; }
    if (s_pPaletteTex) { s_pPaletteTex->Release(); s_pPaletteTex = nullptr; }
    if (s_pPalettePS)  { s_pPalettePS->Release();  s_pPalettePS = nullptr; }
    s_indexTexW = s_indexTexH = 0;
}

// ---------------------------------------------------------------
//  Framebuffer presentation
// ---------------------------------------------------------------

void Pop3Screen::present(const unsigned char* pixels, int pitch,
                         int width, int height,
                         const unsigned char* palette)
{
    ZoneScopedN("render.d3d9.present_frame");
    // --- frame-timing: present-to-present period (LOCAL-ONLY diag) ---
    const long long _pp_fpNow = _pp_qpc();
    const double _pp_framePeriodMs = _pp_prevPresent ? _pp_ms(_pp_prevPresent, _pp_fpNow) : 0.0;
    _pp_prevPresent = _pp_fpNow;

    if (!s_pDevice || !pixels || !palette)
        return;

    // Handle device lost
    if (handleDeviceLost())
        return; // Skip this frame, device was just reset

    // Keep the backbuffer in sync with the window client rect so Present
    // is 1:1 - the driver's bilinear stretch on a stale-size backbuffer
    // shimmers around fine UI edges.
    if (matchWindowSizeToBackbuffer())
        return; // Skip this frame, device was just reset

    // The framebuffer/index texture is created lazily below, per render path.

    // HW composite: HW world geometry draws FIRST onto a clean backbuffer,
    // then the SW back surface layers on top as an alpha-keyed quad so SW
    // UI (panels, minimap, text) covers the HW world only where SW wrote
    // pixels. The SW pipeline clears the back surface to the key index
    // after each LbScreen_Swap when HW is active (see LbScreen.cpp).
    const bool hwComposite = s_hwComposite;
    // Transparent key must stay index 0: RLE sprite decoding (nearly every
    // UI element) uses byte 0 as "end of row", so decoded sprite pixels
    // never write index 0. Index 255 is unusable - it is the cycled-
    // highlight slot (cycle_clr_255 in Palettes.cpp) and false-positives
    // the sidebar shaman portrait.
    const unsigned int kAlphaKeyIndex = 0;

    // Upload 8bpp indexed pixels → 32bpp ARGB texture. In HW-composite
    // mode, palette index 0 becomes alpha=0 (transparent); everywhere
    // else stays alpha=255. In SW-only mode, always alpha=255.
    const long long _pp_tConv0 = _pp_qpc();   // start of convert+upload

    // GPU palette path: upload raw 8bpp indices + a 256-entry LUT and let a
    // pixel shader do the lookup, instead of converting every pixel on the
    // CPU. Falls back to the CPU convert if the shader/textures fail.
    {
        ZoneScopedN("render.d3d9.framebuffer_upload");
        bool gpuOk = false;
        if (s_gpuPalette)
        {
            if (createPaletteShader() && ensurePaletteTexture() && createIndexTexture(width, height))
            {
                // Keep the SW-dim source (getFramebufferWidth/Height) current -
                // the HW renderer scales its geometry by it. createFramebufferTexture
                // (which normally sets these) is skipped on the GPU path.
                s_framebufferWidth  = width;
                s_framebufferHeight = height;
                uploadIndices(pixels, pitch, width, height);
                uploadPalette(palette, hwComposite);
                gpuOk = true;
            }
            else
            {
                s_gpuPalette = false;   // stop retrying every frame
                Pop3Debug::trace("[gpupal] GPU palette path unavailable; using CPU convert\n");
            }
        }
        s_gpuActiveFrame = gpuOk;

        if (!gpuOk)
        {
            ZoneScopedN("render.d3d9.framebuffer_cpu_convert");
            if (!createFramebufferTexture(width, height))
                return;
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
        }
    }

    const long long _pp_tConv1 = _pp_qpc();   // convert+upload done

    if (SUCCEEDED(s_pDevice->BeginScene()))
    {
        ZoneScopedN("render.d3d9.scene");
        // Diagnostic: bright-pink clear distinguishes HW writing black
        // explicitly (halos stay black) from alpha-test discard revealing
        // the cleared background (halos turn pink).
        const D3DCOLOR clearColour = s_debugBackbufferPink
                                     ? 0xFFFF00FFu  // bright magenta
                                     : 0xFF000000u; // black
        s_pDevice->Clear(0, nullptr, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, clearColour, 1.0f, 0);

        if (hwComposite)
        {
            // Composite order:
            //   1. HW world geometry into the clean backbuffer (scissored
            //      to the world rect by HwRender::drawOverlay).
            //   2. SW back surface as an alpha-keyed overlay quad (panels
            //      and UI draw here; pal-0 keys through to HW world).
            //   3. HW UI overlay (shaman portrait, minimap 3D, debug —
            //      scissor disabled so they can land on sidebar pixels).
            //   4. ImGui (sits on top of everything — fires from the
            //      game's draw_callback alongside the HW world pass).
            if (s_overlayCallback)
            { ZoneScopedN("render.d3d9.world_overlay"); s_overlayCallback(); }
            { ZoneScopedN("render.d3d9.framebuffer_composite"); drawFramebufferQuad(/*alphaBlend=*/true); }
            if (s_hwUiOverlayCallback)
            { ZoneScopedN("render.d3d9.hw_ui_overlay"); s_hwUiOverlayCallback(); }
        }
        else
        {
            // Legacy order: SW framebuffer quad, then overlay (ImGui only).
            { ZoneScopedN("render.d3d9.framebuffer_composite"); drawFramebufferQuad(/*alphaBlend=*/false); }
            if (s_overlayCallback)
            { ZoneScopedN("render.d3d9.overlay"); s_overlayCallback(); }
        }

        s_pDevice->EndScene();
    }

    const long long _pp_tDraw1 = _pp_qpc();   // HW draw done

    // Optional CPU frame cap (only meaningful with vsync off).
    if (!s_vsync && s_maxFps > 0)
    {
        const double _pp_targetMs = 1000.0 / (double)s_maxFps;
        while (_pp_lastCapTicks && _pp_ms(_pp_lastCapTicks, _pp_qpc()) < _pp_targetMs)
        {
            if (_pp_targetMs - _pp_ms(_pp_lastCapTicks, _pp_qpc()) > 2.0) Sleep(1);
        }
        _pp_lastCapTicks = _pp_qpc();
    }

    { ZoneScopedN("render.d3d9.present"); s_pDevice->Present(nullptr, nullptr, nullptr, nullptr); }
    const long long _pp_tPres1 = _pp_qpc();   // Present() returned

    TracyPlot("d3d9.upload_ms", _pp_ms(_pp_tConv0, _pp_tConv1));
    TracyPlot("d3d9.scene_ms", _pp_ms(_pp_tConv1, _pp_tDraw1));
    TracyPlot("d3d9.present_ms", _pp_ms(_pp_tDraw1, _pp_tPres1));
    TracyPlot("d3d9.frame_ms", _pp_framePeriodMs);
    // The FrameMark belongs after Present so a driver/vsync stall remains part
    // of the rendered frame rather than being attributed to the next one.
    tracy_frame_end();

    // --- accumulate rolling 1s frame-timing (LOCAL-ONLY diagnostic) ---
    _pp_accConvert += _pp_ms(_pp_tConv0, _pp_tConv1);
    _pp_accDraw    += _pp_ms(_pp_tConv1, _pp_tDraw1);
    _pp_accPresent += _pp_ms(_pp_tDraw1, _pp_tPres1);
    _pp_accFrame   += _pp_framePeriodMs;
    _pp_accCount++;
    if (_pp_winStart == 0) _pp_winStart = _pp_fpNow;
    if (_pp_ms(_pp_winStart, _pp_tPres1) >= 1000.0 && _pp_accCount > 0)
    {
        _pp_avgConvert = _pp_accConvert / _pp_accCount;
        _pp_avgDraw    = _pp_accDraw    / _pp_accCount;
        _pp_avgPresent = _pp_accPresent / _pp_accCount;
        _pp_avgFrame   = _pp_accFrame   / _pp_accCount;
        _pp_avgFps     = _pp_avgFrame > 0.0 ? 1000.0 / _pp_avgFrame : 0.0;
        if (_pp_logFps)
        Pop3Debug::trace("[frametime] fps=%.1f frame=%.2fms | convert+upload=%.2f draw=%.2f present=%.2f  (n=%d vsync=%d maxfps=%d)\n",
                         _pp_avgFps, _pp_avgFrame, _pp_avgConvert, _pp_avgDraw, _pp_avgPresent,
                         _pp_accCount, s_vsync ? 1 : 0, s_maxFps);
        _pp_accConvert = _pp_accDraw = _pp_accPresent = _pp_accFrame = 0.0;
        _pp_accCount = 0;
        _pp_winStart = _pp_tPres1;
    }
}

void Pop3Screen::drawFramebufferQuad(bool alphaBlend)
{
    if (!s_pDevice)
        return;

    // Destination rect: full backbuffer, or the centered aspect-fit rect
    // when maintas is on (getPresentRect). The bars around a letterboxed
    // quad stay the frame's black Clear.
    int rx, ry, rw, rh;
    getPresentRect(rx, ry, rw, rh);
    const float x0 = (float)rx - 0.5f;
    const float y0 = (float)ry - 0.5f;
    const float x1 = (float)(rx + rw) - 0.5f;
    const float y1 = (float)(ry + rh) - 0.5f;

    // Pixel-perfect quad
    ScreenVertex verts[4] =
    {
        { x0,  y0,  0.0f, 1.0f,   0.0f, 0.0f },
        { x1,  y0,  0.0f, 1.0f,   1.0f, 0.0f },
        { x0,  y1,  0.0f, 1.0f,   0.0f, 1.0f },
        { x1,  y1,  0.0f, 1.0f,   1.0f, 1.0f },
    };

    // Set minimal render state for the framebuffer quad
    s_pDevice->SetTexture(0, s_pFramebufferTex);
    s_pDevice->SetFVF(SCREEN_FVF);

    // GPU palette path: index texture on s0, palette LUT on s1; the pixel
    // shader does the lookup. Self-contained (own blend/draw/cleanup, then
    // return) so the fixed-function fallback below is unchanged.
    if (s_gpuActiveFrame && s_pPalettePS && s_pIndexTex && s_pPaletteTex)
    {
        s_pDevice->SetPixelShader(s_pPalettePS);
        s_pDevice->SetTexture(0, s_pIndexTex);
        s_pDevice->SetTexture(1, s_pPaletteTex);
        for (DWORD st = 0; st < 2; ++st)
        {
            s_pDevice->SetSamplerState(st, D3DSAMP_MINFILTER, D3DTEXF_POINT);
            s_pDevice->SetSamplerState(st, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
            s_pDevice->SetSamplerState(st, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
            s_pDevice->SetSamplerState(st, D3DSAMP_ADDRESSU,  D3DTADDRESS_CLAMP);
            s_pDevice->SetSamplerState(st, D3DSAMP_ADDRESSV,  D3DTADDRESS_CLAMP);
        }

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

        s_pDevice->SetPixelShader(nullptr);
        s_pDevice->SetTexture(1, nullptr);
        s_pDevice->SetTexture(0, nullptr);
        s_pDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
        return;
    }

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

    // Alpha blending for the HW composite - SW framebuffer overlaid on HW
    // world geometry, key-index pixels (alpha=0) showing HW through. For
    // SW-only mode we disable it (opaque quad).
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

void Pop3Screen::setHwUiOverlayCallback(Pop3ScreenCallback cb)
{
    s_hwUiOverlayCallback = cb;
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

void Pop3Screen::setDeviceDestroyCallback(Pop3ScreenCallback cb)
{
    s_deviceDestroyCallback = cb;
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

    // "Fullscreen" here is BORDERLESS: a desktop-size WS_POPUP window over a
    // WINDOWED device. Must RESET the existing device, not rebuild it - a
    // Reset preserves MANAGED textures/shaders that onDeviceLost keeps as
    // stale pointers across it. Never request an exclusive device here: the
    // client-size backbuffer is rarely an enumerable mode and that path is a
    // hard exit.
    s_fullscreen = !s_fullscreen;
    s_windowed = true;

    HWND hWnd = (HWND)s_hWnd;

    if (!s_fullscreen)
    {
        DWORD style = (s_borderPref ? WS_OVERLAPPEDWINDOW : WS_POPUP) | WS_VISIBLE;
        SetWindowLongPtr(hWnd, GWL_STYLE, style);

        // Scale the game up to a comfortable integer multiple that fits
        // the screen, matching the logic in LbScreen_SetMode. Guard the
        // framebuffer dims: matchWindowSizeToBackbuffer zeroes them for a
        // frame after a resize, and 0 would spin this loop forever.
        int deskW = GetSystemMetrics(SM_CXSCREEN);
        int deskH = GetSystemMetrics(SM_CYSCREEN);
        int fbW = (s_framebufferWidth  > 0) ? s_framebufferWidth  : 640;
        int fbH = (s_framebufferHeight > 0) ? s_framebufferHeight : 480;
        int scale = 1;
        while (fbW * (scale + 1) <= deskW * 9 / 10 &&
               fbH * (scale + 1) <= deskH * 9 / 10)
            scale++;

        RECT rc = { 0, 0, (LONG)(fbW * scale), (LONG)(fbH * scale) };
        AdjustWindowRect(&rc, style, FALSE);
        int winW = rc.right - rc.left;
        int winH = rc.bottom - rc.top;
        SetWindowPos(hWnd, HWND_NOTOPMOST,
            (deskW - winW) / 2, (deskH - winH) / 2, winW, winH,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        s_border = s_borderPref;

        // Re-take the backbuffer from the REAL client rect now; waiting
        // for matchWindowSizeToBackbuffer next frame would present the
        // first frame driver-stretched at the stale fullscreen size.
        RECT crc;
        if (GetClientRect(hWnd, &crc) &&
            crc.right > crc.left && crc.bottom > crc.top)
        {
            s_backbufferWidth = crc.right - crc.left;
            s_backbufferHeight = crc.bottom - crc.top;
        }
    }
    else
    {
        int deskW = GetSystemMetrics(SM_CXSCREEN);
        int deskH = GetSystemMetrics(SM_CYSCREEN);
        SetWindowLongPtr(hWnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
        SetWindowPos(hWnd, HWND_TOPMOST,
            0, 0, deskW, deskH,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        s_border = false;

        // Desktop-size backbuffer: always presentable on a windowed
        // device, so the exclusive-mode failure path is unreachable.
        s_backbufferWidth = deskW;
        s_backbufferHeight = deskH;
    }

    // Release DEFAULT-pool resources (Reset-grade: MANAGED textures + shaders
    // survive), then Reset the device to the new backbuffer size. Mirrors
    // matchWindowSizeToBackbuffer.
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
    if (s_pIndexTex) { s_pIndexTex->Release(); s_pIndexTex = nullptr; s_indexTexW = s_indexTexH = 0; }

    D3DPRESENT_PARAMETERS pp;
    ZeroMemory(&pp, sizeof(pp));
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferWidth = s_backbufferWidth;
    pp.BackBufferHeight = s_backbufferHeight;
    pp.BackBufferCount = 2;
    pp.hDeviceWindow = (HWND)s_hWnd;
    pp.PresentationInterval = s_vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24S8;

    HRESULT hr = s_pDevice->Reset(&pp);
    if (FAILED(hr))
    {
        // Reset rejected the new mode: full rebuild. The DESTROY callback
        // must release EVERY device-owned resource (MANAGED + shaders too)
        // so nothing dangles; caches repopulate lazily.
        if (s_deviceDestroyCallback)
            s_deviceDestroyCallback();
        else if (s_deviceDeinitCallback)
            s_deviceDeinitCallback();
        releaseDevice();
        createDevice();
    }

    // Notify init (rebinds ImGui and other per-device consumers to the live
    // device; fires for both the Reset and the rebuild fallback).
    if (s_deviceInitCallback)
        s_deviceInitCallback();
}

void Pop3Screen::setWindowModeState(bool windowed, bool borderless, bool border)
{
    // Same derivation as create(): "fullscreen" covers exclusive AND
    // borderless. Unlike create() the caller's border request is honored -
    // create() only sees (windowed, borderless) and cannot see
    // LB_SCREEN_MODE_NO_BORDER, so a border=false window used to report
    // hasBorder()==true and host_change_res re-added the frame.
    s_windowed   = windowed;
    s_fullscreen = !windowed || borderless;
    s_border     = windowed && !borderless && border;
    // Only a genuine windowed mode expresses a border preference; the
    // borderless/exclusive states say nothing about what the user wants when
    // they come back to a window.
    if (windowed && !borderless)
        s_borderPref = border;
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
    }
    // Release s_pIndexTex UNCONDITIONALLY (not nested under s_pFramebufferTex):
    // on the GPU-palette path s_pFramebufferTex is never created, and a live
    // DEFAULT-pool index texture makes Reset() fail with D3DERR_INVALIDCALL.
    if (s_pIndexTex) { s_pIndexTex->Release(); s_pIndexTex = nullptr; s_indexTexW = s_indexTexH = 0; }
    // Force a re-create at the next present() so dims are re-taken.
    s_framebufferWidth = 0;
    s_framebufferHeight = 0;

    D3DPRESENT_PARAMETERS pp;
    ZeroMemory(&pp, sizeof(pp));
    pp.Windowed = TRUE;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferWidth = newW;
    pp.BackBufferHeight = newH;
    pp.BackBufferCount = 2; // one queued frame so Present() does not stall the
                            // CPU-bound frame loop; worst case +<=1 vblank latency, only when the queue is full
    pp.hDeviceWindow = (HWND)s_hWnd;
    pp.PresentationInterval = s_vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
    pp.EnableAutoDepthStencil = TRUE;
    pp.AutoDepthStencilFormat = D3DFMT_D24S8;

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
        if (s_pIndexTex) { s_pIndexTex->Release(); s_pIndexTex = nullptr; s_indexTexW = s_indexTexH = 0; }

        D3DPRESENT_PARAMETERS pp;
        ZeroMemory(&pp, sizeof(pp));
        pp.Windowed = s_windowed ? TRUE : FALSE;
        pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
        pp.BackBufferFormat = D3DFMT_X8R8G8B8;
        pp.BackBufferWidth = s_backbufferWidth;
        pp.BackBufferHeight = s_backbufferHeight;
        pp.BackBufferCount = 2; // one queued frame so Present() does not stall the
                            // CPU-bound frame loop; worst case +<=1 vblank latency, only when the queue is full
        pp.hDeviceWindow = (HWND)s_hWnd;
        pp.PresentationInterval = s_vsync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
        pp.EnableAutoDepthStencil = TRUE;
        pp.AutoDepthStencilFormat = D3DFMT_D24S8;

        hr = s_pDevice->Reset(&pp);
        if (FAILED(hr))
            return true;

        if (s_deviceInitCallback)
            s_deviceInitCallback();

        return true; // Skip this frame after reset
    }

    return false; // Device is fine
}
