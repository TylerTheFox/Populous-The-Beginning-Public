// includes
//***************************************************************************
#include "Pop3Debug.h"
#include <Pop3Rect.h>
#include    <LbScreen.h>
#include    <LbSCBack.h>
#include    "Pop3Platform_Win32.h"

// Forward declarations for external functions (defined in Library8/src)
extern void LbSurface_RegisterDirectDraw(IDirectDraw *pDirectDraw);
extern void LbSurface_Unlock(TbSurface *surface);
extern TbError LbSurface_ClearRect(const TbSurface *surface, const Pop3Rect *rect, TbColour colour, ULONG flags);

// LbAppData functions (defined in Library8/src/LbAppData.cpp)
extern TbHandle LbApp_GetHInstance(void);
extern TbHandle LbApp_GetHWnd(void);
extern void LbApp_SetHWnd(TbHandle handle);
extern const char* LbApp_GetWindowName(void);
extern short LbApp_GetWindowIcon(void);
extern void LbApp_SetCloseDownRequest(BOOL bFlag);

//***************************************************************************
// Global surface instances
//***************************************************************************
TbSurface _lbBackSurface;
TbSurface _lbFrontSurface;

//***************************************************************************
// Internal globals
//***************************************************************************
static TbSurface _lbFlipSurface;
IDirectDraw* _lbpDirectDraw;
IDirectDrawPalette* _lbpPalette;
IDirectDrawClipper* _lbpClipper;
int _LbWindowActive;
int _LbScreenActive;
int _lbIsScreenInitialised;
_GUID* _LbScreenDDGUID;
int _LbModeXScreen;
int _lbUsingSimulator;
int _lbWillUseSimulator = 1;
TbPalette _lbGlobalPalette;

static int _lbPaletteFadeStarted;
static TbList _lbDisplayModeList;
static TbList _lbModeEnumList;
static HWND _lbhWndMain;
static HWND _lbhWndCreated;
static HMODULE _lbhDDrawDll;
static char _lbWindowClassName[128];

// FadePalette static state
static TbPaletteEntry gPaletteFrom[256];
static TbPalette gPaletteTo;
static int gFadeCount;

//***************************************************************************
// Internal helper forward declarations
//***************************************************************************
static int ReleaseMode();
static void ReleaseDirectDraw();
static int CreateDirectDrawObject(_GUID* guid, IDirectDraw** ppDD, HMODULE* phDll);
static int CreateScreenModeList(int modex, HWND hwnd, _GUID* guid);
static void RegisterBullfrogWindowClass();
static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static HRESULT WINAPI EnumModesCallback(LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext);
static void _InitialiseSurface(TbSurface* surface, UINT width, UINT height, UINT depth, ULONG flags);
static BOOL _CheckAndRestoreSurface(const TbSurface* surface);

//***************************************************************************
// Internal helper implementations
//***************************************************************************

static void _InitialiseSurface(TbSurface* surface, UINT width, UINT height, UINT depth, ULONG flags)
{
    surface->DrawFlags = 0;
    surface->GhostTable = 0;
    surface->FadeTable = 0;
    surface->SurfaceArea.mpData = NULL;
    surface->SurfaceArea.mPitch = 0;
    surface->SurfaceArea.mSize.Width = width;
    surface->SurfaceArea.mSize.Height = height;
    surface->SurfaceArea.mFormat.Depth = depth;
    surface->mpPalette = (depth == 8) ? &_lbGlobalPalette : NULL;

    switch (depth)
    {
    case 16: surface->SurfaceArea.mFormat = Pop3PixFormat16; break;
    case 24: surface->SurfaceArea.mFormat = Pop3PixFormat24; break;
    case 32: surface->SurfaceArea.mFormat = Pop3PixFormat32; break;
    default: surface->SurfaceArea.mFormat = Pop3PixFormat8; break;
    }

    // Query actual pixel format from DirectDraw surface
    if (surface->lpSurface)
    {
        DDPIXELFORMAT ddpf;
        memset(&ddpf, 0, sizeof(ddpf));
        ddpf.dwSize = sizeof(ddpf);
        if (surface->lpSurface->GetPixelFormat(&ddpf) == DD_OK)
        {
            surface->SurfaceArea.mFormat.Red = ddpf.dwRBitMask;
            surface->SurfaceArea.mFormat.Green = ddpf.dwGBitMask;
            surface->SurfaceArea.mFormat.Blue = ddpf.dwBBitMask;
            surface->SurfaceArea.mFormat.Alpha = ddpf.dwRGBAlphaBitMask;
            surface->SurfaceArea.mFormat.Depth = ddpf.dwRGBBitCount;
            surface->SurfaceArea.mFormat.bPalette = (ddpf.dwFlags & DDPF_PALETTEINDEXED8) ? 1 : 0;
        }

        DDSURFACEDESC ddsd;
        memset(&ddsd, 0, sizeof(ddsd));
        ddsd.dwSize = sizeof(ddsd);
        ddsd.dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH;
        surface->lpSurface->GetSurfaceDesc(&ddsd);
        surface->SurfaceArea.mPitch = ddsd.lPitch;
        surface->SurfaceArea.mSize.Width = ddsd.dwWidth;
        surface->SurfaceArea.mSize.Height = ddsd.dwHeight;
    }

    surface->CreationFlags = flags;
}

static BOOL _CheckAndRestoreSurface(const TbSurface* surface)
{
    if (!surface->lpSurface)
        return FALSE;
    if (surface->lpSurface->IsLost() == DDERR_SURFACELOST)
    {
        surface->lpSurface->Restore();
        return TRUE;
    }
    return FALSE;
}

static int ReleaseMode()
{
    _LbWindowActive = 0;
    _LbScreenActive = 0;

    if (!_lbFrontSurface.lpSurface)
        return 0;

    _CBINFO cbInfo(_CBINFO::POINTER_RELEASE, (ULONG)0);
    _LbGCBS.EventNotification(LBCB_POINTER_CB_EVENT, &cbInfo);

    if (_lbpDirectDraw)
        _lbpDirectDraw->SetCooperativeLevel(_lbhWndMain, DDSCL_NORMAL);

    if (_lbBackSurface.lpSurface)
    {
        _lbBackSurface.lpSurface->Release();
        _lbBackSurface.lpSurface = NULL;
    }

    if (_lbFlipSurface.lpSurface)
    {
        _lbFlipSurface.lpSurface->Release();
        _lbFlipSurface.lpSurface = NULL;
    }

    if (_lbFrontSurface.lpSurface)
    {
        _lbFrontSurface.lpSurface->Release();
        _lbFrontSurface.lpSurface = NULL;
    }

    if (_lbpPalette)
    {
        _lbpPalette->Release();
        _lbpPalette = NULL;
    }

    if (_lbpClipper)
    {
        _lbpClipper->Release();
        _lbpClipper = NULL;
    }

    return 0;
}

static void ReleaseDirectDraw()
{
    if (_lbpDirectDraw)
    {
        _lbpDirectDraw->RestoreDisplayMode();
        _lbpDirectDraw->Release();
        _lbpDirectDraw = NULL;
        LbSurface_RegisterDirectDraw(NULL);
    }
    if (_lbhDDrawDll)
    {
        FreeLibrary(_lbhDDrawDll);
        _lbhDDrawDll = NULL;
    }
}

static int CreateDirectDrawObject(_GUID* guid, IDirectDraw** ppDD, HMODULE* phDll)
{
    HMODULE hDll;
    if (_lbUsingSimulator)
        hDll = LoadLibraryA("SIMULATOR.DLL");
    else
        hDll = LoadLibraryA("ddraw.dll");

    if (!hDll)
        return -1;

    typedef HRESULT(WINAPI *LPDIRECTDRAWCREATEFN)(GUID*, LPDIRECTDRAW*, IUnknown*);
    LPDIRECTDRAWCREATEFN pfnCreate = (LPDIRECTDRAWCREATEFN)GetProcAddress(hDll, "DirectDrawCreate");

    if (!pfnCreate || pfnCreate(guid, ppDD, NULL) != DD_OK)
    {
        FreeLibrary(hDll);
        return -1;
    }

    *phDll = hDll;
    return 0;
}

static HRESULT WINAPI EnumModesCallback(LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext)
{
    TbList* pList = (TbList*)lpContext;

    _Pop3ScreenModeFind* pNode = new _Pop3ScreenModeFind;
    if (!pNode)
        return DDENUMRET_CANCEL;

    pNode->Info.Width = lpDDSurfaceDesc->dwWidth;
    pNode->Info.Height = lpDDSurfaceDesc->dwHeight;
    pNode->Info.Depth = lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount;

    LbList_Insert(pList, pList->lpLast, (TbListNode*)pNode);
    return DDENUMRET_OK;
}

static int CreateScreenModeList(int modex, HWND hwnd, _GUID* guid)
{
    IDirectDraw* pDD = NULL;
    HMODULE hDll = NULL;

    if (CreateDirectDrawObject(guid, &pDD, &hDll))
        return -1;

    LbList_Init(&_lbDisplayModeList);

    if (modex)
    {
        HWND hWndTemp = hwnd;
        if (!hWndTemp)
        {
            RegisterBullfrogWindowClass();
            HINSTANCE hInst = (HINSTANCE)LbApp_GetHInstance();
            hWndTemp = CreateWindowExA(0, _lbWindowClassName, "",
                WS_POPUP | WS_DISABLED, 0, 0, 1280, 1024,
                NULL, NULL, hInst, NULL);
            if (!hWndTemp)
            {
                pDD->Release();
                FreeLibrary(hDll);
                return -1;
            }
        }

        pDD->SetCooperativeLevel(hWndTemp, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWMODEX);
        pDD->EnumDisplayModes(0, NULL, &_lbDisplayModeList, EnumModesCallback);
        pDD->SetCooperativeLevel(hWndTemp, DDSCL_NORMAL);

        if (!hwnd)
            DestroyWindow(hWndTemp);
    }
    else
    {
        pDD->EnumDisplayModes(0, NULL, &_lbDisplayModeList, EnumModesCallback);
    }

    pDD->Release();
    FreeLibrary(hDll);
    return 0;
}

static void RegisterBullfrogWindowClass()
{
    static int registered = 0;
    if (!registered)
    {
        registered = 1;
        WNDCLASSA wc;
        memset(&wc, 0, sizeof(wc));
        wc.lpfnWndProc = WindowProc;
        wc.hInstance = (HINSTANCE)LbApp_GetHInstance();
        if (LbApp_GetWindowIcon())
            wc.hIcon = LoadIconA(wc.hInstance, MAKEINTRESOURCEA(LbApp_GetWindowIcon()));
        sprintf(_lbWindowClassName, "_%s%p", "BullfrogWindow", LbApp_GetHInstance());
        wc.lpszClassName = _lbWindowClassName;
        RegisterClassA(&wc);
    }
}

static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_ACTIVATE:
        if (_lbUsingSimulator)
            return DefWindowProcA(hWnd, uMsg, wParam, lParam);
        if (LOWORD(wParam) == WA_INACTIVE)
        {
            _LbWindowActive = 0;
            _LbScreenActive = 0;
        }
        else
        {
            _LbWindowActive = 1;
            _LbScreenActive = 1;
        }
        return 0;

    case WM_CLOSE:
        LbApp_SetCloseDownRequest(TRUE);
        return 0;

    case WM_SYSCOMMAND:
        if (wParam == SC_SCREENSAVE)
            return 0;
        break;
    }

    return DefWindowProcA(hWnd, uMsg, wParam, lParam);
}

//***************************************************************************
// Public API implementations
//***************************************************************************

void LbScreen_Terminate(void)
{
    _CBINFO cbInfo(_CBINFO::POINTER_HIDE, (ULONG)0);
    _LbGCBS.EventNotification(LBCB_POINTER_CB_EVENT, &cbInfo);

    if (_lbIsScreenInitialised)
    {
        _lbIsScreenInitialised = 0;
        ReleaseMode();
        ReleaseDirectDraw();
        if (_lbhWndCreated)
            DestroyWindow(_lbhWndCreated);
        _lbhWndCreated = NULL;
    }

    // Free the display mode list
    TbListNode* node;
    while ((node = LbList_GetFirst(&_lbDisplayModeList)) != NULL)
    {
        LbList_Remove(node);
        delete node;
    }
}

BOOL LbScreen_IsActive(void)
{
    return _LbWindowActive;
}

TbError LbScreen_Initialise(_GUID *pGuid, BOOL modex, void *hWndView, void *hWndApplication)
{
    if (_lbIsScreenInitialised)
        return LB_OK;

    LbList_Init(&_lbModeEnumList);

    LbApp_SetHWnd(hWndView);

    if (!hWndView)
    {
        RegisterBullfrogWindowClass();
        HINSTANCE hInst = (HINSTANCE)LbApp_GetHInstance();
        const char* winName = LbApp_GetWindowName();
        _lbhWndCreated = CreateWindowExA(
            WS_EX_APPWINDOW, _lbWindowClassName, winName ? winName : "",
            WS_POPUP | WS_DISABLED, 0, 0, 1280, 1024,
            NULL, NULL, hInst, NULL);
        if (!_lbhWndCreated)
            return LB_ERROR;
        LbApp_SetHWnd(_lbhWndCreated);
    }

    if (CreateScreenModeList(modex, (HWND)hWndApplication, pGuid))
        return LB_ERROR;

    _LbScreenDDGUID = pGuid;
    _lbIsScreenInitialised = 1;
    return LB_OK;
}

void LbScreen_SetPaletteEntries(const TbPaletteEntry *lpPalette, UINT nFirst, UINT nEntries)
{
    memcpy(&_lbGlobalPalette.Entry[nFirst], lpPalette, 4 * nEntries);

    if (_lbpPalette && _lbFrontSurface.lpSurface)
        _lbpPalette->SetEntries(0, nFirst, nEntries, (LPPALETTEENTRY)lpPalette);
}

TbPaletteEntry * LbScreen_GetPaletteEntries(TbPaletteEntry *palette, UINT first, UINT num)
{
    if (_lbpPalette)
    {
        if (_lbpPalette->GetEntries(0, first, num, (LPPALETTEENTRY)&_lbGlobalPalette.Entry[first]) == DD_OK)
            memcpy(palette, &_lbGlobalPalette.Entry[first], 4 * num);
    }
    else
    {
        memcpy(palette, &_lbGlobalPalette.Entry[first], 4 * num);
    }
    return palette;
}

TbError LbScreen_SetMode(UINT nWidth, UINT nHeight, UINT nDepth, ULONG flags, const TbPalette *lpPalette)
{
    if (flags & LB_SCREEN_MODE_NO_CHEAP_FLIP)
        flags |= LB_SCREEN_MODE_FLIP;

    if (flags & LB_SCREEN_MODE_WINDOWED)
    {
        if (flags & LB_SCREEN_MODE_FLIP)
            return LB_ERROR;
        flags |= LB_SCREEN_MODE_SWAP;
    }

    if ((flags & (LB_SCREEN_MODE_SWAP | LB_SCREEN_MODE_FLIP)) == 0)
        return LB_ERROR;

    _CBINFO cbRestore(_CBINFO::POINTER_BACKUP, (ULONG)0);
    _LbGCBS.EventNotification(LBCB_POINTER_CB_EVENT, &cbRestore);

    ReleaseMode();

    if (!LbScreen_IsModeAvailable(nWidth, nHeight, nDepth))
        return LB_ERROR;

    if (!_lbpDirectDraw)
    {
        if (CreateDirectDrawObject(_LbScreenDDGUID, &_lbpDirectDraw, &_lbhDDrawDll))
        {
            ReleaseDirectDraw();
            return LB_ERROR;
        }
    }

    LbSurface_RegisterDirectDraw(_lbpDirectDraw);

    if (_lbhWndCreated)
        _lbhWndMain = _lbhWndCreated;
    else
        _lbhWndMain = (HWND)LbApp_GetHWnd();

    if (!_lbUsingSimulator)
        ShowWindow(_lbhWndMain, SW_SHOW);

    // Process pending messages
    MSG msg;
    while (PeekMessageA(&msg, _lbhWndMain, 0, 0, PM_REMOVE))
        DefWindowProcA(msg.hwnd, msg.message, msg.wParam, msg.lParam);

    _LbWindowActive = 1;
    _LbScreenActive = 1;

    // Set cooperative level
    DWORD coopFlags;
    if (flags & LB_SCREEN_MODE_WINDOWED)
        coopFlags = DDSCL_NORMAL;
    else
        coopFlags = DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWMODEX;

    if (_lbpDirectDraw->SetCooperativeLevel(_lbhWndMain, coopFlags) != DD_OK)
    {
        ReleaseDirectDraw();
        return LB_ERROR;
    }

    // Create clipper for windowed mode
    if (flags & LB_SCREEN_MODE_WINDOWED)
    {
        if (_lbpDirectDraw->CreateClipper(0, &_lbpClipper, NULL) != DD_OK)
        {
            ReleaseDirectDraw();
            return LB_ERROR;
        }
        if (_lbpClipper->SetHWnd(0, _lbhWndMain) != DD_OK)
        {
            ReleaseDirectDraw();
            return LB_ERROR;
        }
    }

    if (!_lbUsingSimulator)
        SetCursor(NULL);

    // Set display mode for fullscreen
    if (!(flags & LB_SCREEN_MODE_WINDOWED))
    {
        if (_lbpDirectDraw->SetDisplayMode(nWidth, nHeight, nDepth) != DD_OK)
        {
            ReleaseDirectDraw();
            return LB_ERROR;
        }
    }

    // Setup palette
    if (lpPalette)
        memcpy(&_lbGlobalPalette, lpPalette, sizeof(TbPalette));
    else
    {
        for (UINT i = 0; i < 256; i++)
        {
            _lbGlobalPalette.Entry[i].Red = (UBYTE)i;
            _lbGlobalPalette.Entry[i].Green = 0;
            _lbGlobalPalette.Entry[i].Blue = (UBYTE)i;
            _lbGlobalPalette.Entry[i].Reserved = 0;
        }
    }

    if (nDepth == 8)
    {
        const TbPaletteEntry* palSrc = lpPalette ? lpPalette->Entry : _lbGlobalPalette.Entry;
        if (_lbpDirectDraw->CreatePalette(DDPCAPS_8BIT | DDPCAPS_ALLOW256,
            (LPPALETTEENTRY)palSrc, &_lbpPalette, NULL) != DD_OK)
        {
            ReleaseDirectDraw();
            return LB_ERROR;
        }
    }
    else
    {
        if (_lbpPalette)
        {
            _lbpPalette->Release();
            _lbpPalette = NULL;
        }
    }

    // Create primary surface
    DDSURFACEDESC ddsd;
    memset(&ddsd, 0, sizeof(ddsd));
    ddsd.dwSize = sizeof(ddsd);
    ddsd.dwFlags = DDSD_CAPS;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

    if (flags & LB_SCREEN_MODE_FLIP)
    {
        ddsd.dwFlags |= DDSD_BACKBUFFERCOUNT;
        ddsd.ddsCaps.dwCaps |= DDSCAPS_FLIP | DDSCAPS_COMPLEX;
        ddsd.dwBackBufferCount = 1;
    }

    if (flags & LB_SCREEN_3D)
        ddsd.ddsCaps.dwCaps |= DDSCAPS_3DDEVICE;

    if (_lbpDirectDraw->CreateSurface(&ddsd, &_lbFrontSurface.lpSurface, NULL) != DD_OK)
    {
        _lbFrontSurface.lpSurface = NULL;
        ReleaseDirectDraw();
        return LB_ERROR;
    }

    // Get surface desc to check for ModeX
    _lbFrontSurface.lpSurface->GetSurfaceDesc(&ddsd);
    _LbModeXScreen = ddsd.ddsCaps.dwCaps & DDSCAPS_MODEX;

    // If ModeX and swap mode, recreate with flip chain
    if (_LbModeXScreen && (flags & LB_SCREEN_MODE_SWAP))
    {
        _lbFrontSurface.lpSurface->Release();
        _lbFrontSurface.lpSurface = NULL;

        memset(&ddsd, 0, sizeof(ddsd));
        ddsd.dwSize = sizeof(ddsd);
        ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
        ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
        ddsd.dwBackBufferCount = 1;

        if (_lbpDirectDraw->CreateSurface(&ddsd, &_lbFrontSurface.lpSurface, NULL) != DD_OK)
        {
            _lbFrontSurface.lpSurface = NULL;
            ReleaseDirectDraw();
            return LB_ERROR;
        }

        DDSCAPS caps;
        caps.dwCaps = DDSCAPS_BACKBUFFER;
        _lbFrontSurface.lpSurface->GetAttachedSurface(&caps, &_lbFlipSurface.lpSurface);
    }

    // Get or create back buffer
    if (flags & LB_SCREEN_MODE_FLIP)
    {
        DDSCAPS caps;
        caps.dwCaps = DDSCAPS_BACKBUFFER;
        _lbFrontSurface.lpSurface->GetAttachedSurface(&caps, &_lbBackSurface.lpSurface);
    }
    else
    {
        memset(&ddsd, 0, sizeof(ddsd));
        ddsd.dwSize = sizeof(ddsd);
        ddsd.dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
        ddsd.dwWidth = nWidth;
        ddsd.dwHeight = nHeight;
        ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_SYSTEMMEMORY;

        if (flags & LB_SCREEN_3D)
            ddsd.ddsCaps.dwCaps |= DDSCAPS_3DDEVICE;

        _lbpDirectDraw->CreateSurface(&ddsd, &_lbBackSurface.lpSurface, NULL);
    }

    if (!_lbBackSurface.lpSurface)
    {
        if (_lbFrontSurface.lpSurface)
        {
            _lbFrontSurface.lpSurface->Release();
            _lbFrontSurface.lpSurface = NULL;
        }
        ReleaseDirectDraw();
        return LB_ERROR;
    }

    // Set clipper on front surface for windowed mode
    if (flags & LB_SCREEN_MODE_WINDOWED)
        _lbFrontSurface.lpSurface->SetClipper(_lbpClipper);

    // Set palette on front surface
    if (nDepth == 8 && _lbpPalette)
        _lbFrontSurface.lpSurface->SetPalette(_lbpPalette);

    _InitialiseSurface(&_lbFrontSurface, nWidth, nHeight, nDepth, flags);
    _InitialiseSurface(&_lbBackSurface, nWidth, nHeight, nDepth, flags);

    Pop3Size size;
    size.Width = nWidth;
    size.Height = nHeight;
    _CBINFO cbSize(_CBINFO::POINTER_RESTORE, size);
    _LbGCBS.EventNotification(LBCB_POINTER_CB_EVENT, &cbSize);
    _LbGCBS.EventNotification(LBCB_SCREEN_MODECHANGE, (void*)(intptr_t)_lbUsingSimulator);

    return LB_OK;
}

TbError LbScreen_Swap(ULONG flags)
{
    ULONG waitFlag = flags ^ LB_SURFACE_SWAP_NOWAIT;

    if (_lbFrontSurface.CreationFlags & LB_SCREEN_MODE_FLIP)
    {
        _LbGCBS.EventNotification(LBCB_SCREEN_BEGIN_FLIP, NULL);
        HRESULT hr = _lbFrontSurface.lpSurface->Flip(_lbBackSurface.lpSurface, (waitFlag ? DDFLIP_WAIT : 0));
        _LbGCBS.EventNotification(LBCB_SCREEN_END_FLIP, NULL);

        if (hr == DDERR_SURFACELOST)
        {
            _CheckAndRestoreSurface(&_lbFrontSurface);
            _CheckAndRestoreSurface(&_lbBackSurface);
        }
        if (hr != DD_OK)
            return LB_ERROR;
        return LB_OK;
    }
    else
    {
        _LbGCBS.EventNotification(LBCB_SCREEN_BEGIN_SWAP, NULL);
        HRESULT hr;

        if (_lbFrontSurface.CreationFlags & LB_SCREEN_MODE_WINDOWED)
        {
            RECT rc;
            HWND hWnd = (HWND)LbApp_GetHWnd();
            GetWindowRect(hWnd, &rc);
            hr = _lbFrontSurface.lpSurface->Blt(&rc, _lbBackSurface.lpSurface,
                NULL, DDBLT_WAIT, NULL);
        }
        else if (_LbModeXScreen)
        {
            hr = _lbFlipSurface.lpSurface->BltFast(0, 0, _lbBackSurface.lpSurface,
                NULL, DDBLTFAST_WAIT * waitFlag);
            hr = _lbFrontSurface.lpSurface->Flip(_lbFlipSurface.lpSurface, (waitFlag ? DDFLIP_WAIT : 0));
        }
        else
        {
            hr = _lbFrontSurface.lpSurface->BltFast(0, 0, _lbBackSurface.lpSurface,
                NULL, DDBLTFAST_WAIT * waitFlag);
        }

        _LbGCBS.EventNotification(LBCB_SCREEN_END_SWAP, NULL);

        if (hr == DDERR_SURFACELOST)
        {
            _CheckAndRestoreSurface(&_lbFrontSurface);
            _CheckAndRestoreSurface(&_lbBackSurface);
        }
        if (hr != DD_OK)
            return LB_ERROR;
        return LB_OK;
    }
}

void LbScreen_SwapClear(TbColour colour)
{
    LbScreen_Swap(0);
    LbSurface_ClearRect(&_lbBackSurface, NULL, colour, 0);
}

TbError LbScreen_PartSwap(const Pop3Point &destOffset, const Pop3Rect &srcRect, ULONG flags)
{
    if (!_lbFrontSurface.lpSurface)
        return LB_ERROR;

    if (_lbFrontSurface.CreationFlags & LB_SCREEN_MODE_FLIP)
        return LB_ERROR;
    if (_lbFrontSurface.CreationFlags & LB_SCREEN_MODE_WINDOWED)
        return LB_ERROR;
    if (_LbModeXScreen)
        return LB_ERROR;

    ULONG waitFlag = flags ^ LB_SURFACE_SWAP_NOWAIT;

    _LbGCBS.EventNotification(LBCB_SCREEN_BEGIN_SWAP, NULL);

    RECT rc;
    rc.left = srcRect.Left;
    rc.top = srcRect.Top;
    rc.right = srcRect.Right;
    rc.bottom = srcRect.Bottom;

    HRESULT hr = _lbFrontSurface.lpSurface->BltFast(
        destOffset.X, destOffset.Y,
        _lbBackSurface.lpSurface, &rc,
        DDBLTFAST_WAIT * waitFlag);

    _LbGCBS.EventNotification(LBCB_SCREEN_END_SWAP, NULL);

    if (hr == DDERR_SURFACELOST)
    {
        _CheckAndRestoreSurface(&_lbFrontSurface);
        _CheckAndRestoreSurface(&_lbBackSurface);
    }
    if (hr != DD_OK)
        return LB_ERROR;

    return LB_OK;
}

BOOL LbScreen_IsModeAvailable(UINT width, UINT height, UINT depth)
{
    BOOL found = FALSE;
    Pop3ScreenModeFind modeFind;
    Pop3ScreenMode mode;

    if (LbScreen_ModeFindFirst(&modeFind, &mode) == LB_OK)
    {
        do
        {
            if (mode.Width == width && mode.Height == height && mode.Depth == depth)
            {
                found = TRUE;
                break;
            }
        } while (LbScreen_ModeFindNext(&modeFind, &mode) == LB_OK);

        LbScreen_ModeFindEnd(&modeFind);
    }

    return found;
}

TbError LbScreen_ModeFindFirst(Pop3ScreenModeFind *lpScreenModeFind, Pop3ScreenMode *lpScreenMode)
{
    lpScreenModeFind->pCurrent = (_Pop3ScreenModeFind*)LbList_GetFirst(&_lbDisplayModeList);
    return LbScreen_ModeFindNext(lpScreenModeFind, lpScreenMode);
}

TbError LbScreen_ModeFindNext(Pop3ScreenModeFind *lpScreenModeFind, Pop3ScreenMode *modeInfo)
{
    TbListNode* current = (TbListNode*)lpScreenModeFind->pCurrent;

    if (!current || LbList_IsLast(current))
        return LB_ERROR;

    _Pop3ScreenModeFind* entry = (_Pop3ScreenModeFind*)current;
    *modeInfo = entry->Info;

    lpScreenModeFind->pCurrent = (_Pop3ScreenModeFind*)LbList_GetNext(current);
    return LB_OK;
}

void LbScreen_ModeFindEnd(Pop3ScreenModeFind *lpScreenModeFind)
{
    // Nothing to clean up
}

void LbScreen_WaitVbi(void)
{
    if (_lbpDirectDraw)
        _lbpDirectDraw->WaitForVerticalBlank(DDWAITVB_BLOCKBEGIN, NULL);
}

SINT LbScreen_FadePalette(TbPalette *lpPaletteTo, UINT nSteps, TbPaletteFadeType PaletteFadeType)
{
    TbPaletteEntry* palTo;

    if (!lpPaletteTo)
    {
        memset(&gPaletteTo, 0, sizeof(TbPalette));
        palTo = gPaletteTo.Entry;
    }
    else
    {
        palTo = lpPaletteTo->Entry;
    }

    TbPaletteEntry localPal[256];

    if (PaletteFadeType == LB_PALETTE_FADE_CLOSED)
    {
        // Blocking fade: iterate through all steps
        LbScreen_GetPaletteEntries(gPaletteFrom, 0, 256);

        for (gFadeCount = 1; (UINT)gFadeCount < nSteps; gFadeCount++)
        {
            for (UINT i = 0; i < 256; i++)
            {
                localPal[i].Red   = (UBYTE)(gPaletteFrom[i].Red   + gFadeCount * ((int)palTo[i].Red   - (int)gPaletteFrom[i].Red)   / (int)nSteps);
                localPal[i].Green = (UBYTE)(gPaletteFrom[i].Green + gFadeCount * ((int)palTo[i].Green - (int)gPaletteFrom[i].Green) / (int)nSteps);
                localPal[i].Blue  = (UBYTE)(gPaletteFrom[i].Blue  + gFadeCount * ((int)palTo[i].Blue  - (int)gPaletteFrom[i].Blue)  / (int)nSteps);
            }
            LbScreen_WaitVbi();
            LbScreen_SetPaletteEntries(localPal, 0, 256);
        }

        // Set final palette
        LbScreen_SetPaletteEntries(palTo, 0, 256);
        _lbPaletteFadeStarted = 0;
    }
    else
    {
        // Incremental fade: one step per call
        if (!_lbPaletteFadeStarted)
        {
            gFadeCount = 0;
            _lbPaletteFadeStarted = 1;
            LbScreen_GetPaletteEntries(gPaletteFrom, 0, 256);
        }

        gFadeCount++;

        if ((UINT)gFadeCount == nSteps)
            _lbPaletteFadeStarted = 0;

        for (UINT i = 0; i < 256; i++)
        {
            localPal[i].Red   = (UBYTE)(gPaletteFrom[i].Red   + gFadeCount * ((int)palTo[i].Red   - (int)gPaletteFrom[i].Red)   / (int)nSteps);
            localPal[i].Green = (UBYTE)(gPaletteFrom[i].Green + gFadeCount * ((int)palTo[i].Green - (int)gPaletteFrom[i].Green) / (int)nSteps);
            localPal[i].Blue  = (UBYTE)(gPaletteFrom[i].Blue  + gFadeCount * ((int)palTo[i].Blue  - (int)gPaletteFrom[i].Blue)  / (int)nSteps);
        }

        LbScreen_WaitVbi();
        LbScreen_SetPaletteEntries(localPal, 0, 256);
    }

    return gFadeCount;
}

void LbScreen_StopOpenFadePalette(void)
{
    _lbPaletteFadeStarted = 0;
}
