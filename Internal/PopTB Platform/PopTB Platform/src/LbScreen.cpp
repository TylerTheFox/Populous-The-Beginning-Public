// includes
//***************************************************************************
#include "Pop3Debug.h"
#include <Pop3Rect.h>
#include    <LbScreen.h>
#include    <LbSCBack.h>
#include    <Pop3Screen.h>
#include    "Pop3Platform_Win32.h"

// Forward declarations for external functions (defined in Library8/src)
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
int _LbWindowActive;
int _LbScreenActive;
int _lbIsScreenInitialised;
int _LbModeXScreen;
int _lbUsingSimulator;
int _lbWillUseSimulator = 1;
TbPalette _lbGlobalPalette;

static int _lbPaletteFadeStarted;
static TbList _lbDisplayModeList;
static TbList _lbModeEnumList;
static HWND _lbhWndMain;
static HWND _lbhWndCreated;
static char _lbWindowClassName[128];

// FadePalette static state
static TbPaletteEntry gPaletteFrom[256];
static TbPalette gPaletteTo;
static int gFadeCount;

//***************************************************************************
// Internal helper forward declarations
//***************************************************************************
static int ReleaseMode();
static int CreateScreenModeList();
static void RegisterBullfrogWindowClass();
static LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void _InitialiseSurface(TbSurface* surface, UINT width, UINT height, UINT depth, ULONG flags);

//***************************************************************************
// Internal helper implementations
//***************************************************************************

static void _InitialiseSurface(TbSurface* surface, UINT width, UINT height, UINT depth, ULONG flags)
{
    surface->DrawFlags = 0;
    surface->GhostTable = 0;
    surface->FadeTable = 0;
    surface->SurfaceArea.mPitch = width * (depth / 8);
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

    surface->CreationFlags = flags;
}

static int ReleaseMode()
{
    _LbWindowActive = 0;
    _LbScreenActive = 0;

    if (!_lbFrontSurface.SurfaceArea.mpData)
        return 0;

    _CBINFO cbInfo(_CBINFO::POINTER_RELEASE, (ULONG)0);
    _LbGCBS.EventNotification(LBCB_POINTER_CB_EVENT, &cbInfo);

    // Free software-allocated surface memory
    if (_lbBackSurface.SurfaceArea.mpData)
    {
        free(_lbBackSurface.SurfaceArea.mpData);
        _lbBackSurface.SurfaceArea.mpData = NULL;
    }

    if (_lbFrontSurface.SurfaceArea.mpData)
    {
        free(_lbFrontSurface.SurfaceArea.mpData);
        _lbFrontSurface.SurfaceArea.mpData = NULL;
    }

    return 0;
}

static int CreateScreenModeList()
{
    LbList_Init(&_lbDisplayModeList);

    // Helper: insert a (w, h, depth) triple if not already present.
    auto insert_mode = [](DWORD w, DWORD h, DWORD depth)
    {
        TbListNode* node = LbList_GetFirst(&_lbDisplayModeList);
        while (node && !LbList_IsLast(node))
        {
            _Pop3ScreenModeFind* entry = (_Pop3ScreenModeFind*)node;
            if (entry->Info.Width == w &&
                entry->Info.Height == h &&
                entry->Info.Depth == depth)
            {
                return;
            }
            node = LbList_GetNext(node);
        }
        _Pop3ScreenModeFind* pNode = new _Pop3ScreenModeFind;
        if (pNode)
        {
            pNode->Info.Width = w;
            pNode->Info.Height = h;
            pNode->Info.Depth = depth;
            LbList_Insert(&_lbDisplayModeList, _lbDisplayModeList.lpLast, (TbListNode*)pNode);
        }
    };

    DEVMODE dm;
    memset(&dm, 0, sizeof(dm));
    dm.dmSize = sizeof(dm);

    for (DWORD i = 0; EnumDisplaySettingsA(NULL, i, &dm); i++)
    {
        if (dm.dmBitsPerPel == 8 || dm.dmBitsPerPel == 16 || dm.dmBitsPerPel == 32)
        {
            insert_mode(dm.dmPelsWidth, dm.dmPelsHeight, dm.dmBitsPerPel);

            // Modern Windows (10/11) does not enumerate 8bpp display modes
            // through EnumDisplaySettings — the driver stack only exposes
            // 32bpp (and sometimes 16bpp). The game's internal rasterizer
            // is 8bpp regardless; Pop3Screen handles the palette->texture
            // upload at present time. Mirror every reported resolution as
            // a synthetic 8bpp entry so enum_screen_modes (Utils.cpp) can
            // still find the 640x480/800x600 it needs without requiring
            // actual 8bpp support from the display driver.
            if (dm.dmBitsPerPel != 8)
                insert_mode(dm.dmPelsWidth, dm.dmPelsHeight, 8);
        }

        memset(&dm, 0, sizeof(dm));
        dm.dmSize = sizeof(dm);
    }

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
        Pop3Screen::destroy();
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

    // Prevent DWM from bitmap-stretching the window on high-DPI displays.
    // Must be called before any window creation.
    {
        HMODULE hUser32 = GetModuleHandleA("user32.dll");
        if (hUser32)
        {
            typedef BOOL(WINAPI* PFN_SetProcessDPIAware)(void);
            PFN_SetProcessDPIAware fn = (PFN_SetProcessDPIAware)GetProcAddress(hUser32, "SetProcessDPIAware");
            if (fn) fn();
        }
    }

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

    if (CreateScreenModeList())
        return LB_ERROR;

    _lbIsScreenInitialised = 1;
    return LB_OK;
}

void LbScreen_SetPaletteEntries(const TbPaletteEntry *lpPalette, UINT nFirst, UINT nEntries)
{
    memcpy(&_lbGlobalPalette.Entry[nFirst], lpPalette, 4 * nEntries);
}

TbPaletteEntry * LbScreen_GetPaletteEntries(TbPaletteEntry *palette, UINT first, UINT num)
{
    memcpy(palette, &_lbGlobalPalette.Entry[first], 4 * num);
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

    if (!_lbUsingSimulator)
        SetCursor(NULL);

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

    // Allocate software memory for front and back surfaces
    UINT bytesPerPixel = nDepth / 8;
    UINT pitch = nWidth * bytesPerPixel;
    UINT bufSize = pitch * nHeight;

    _lbFrontSurface.SurfaceArea.mpData = (UBYTE*)malloc(bufSize);
    if (!_lbFrontSurface.SurfaceArea.mpData)
        return LB_ERROR;
    memset(_lbFrontSurface.SurfaceArea.mpData, 0, bufSize);

    _lbBackSurface.SurfaceArea.mpData = (UBYTE*)malloc(bufSize);
    if (!_lbBackSurface.SurfaceArea.mpData)
    {
        free(_lbFrontSurface.SurfaceArea.mpData);
        _lbFrontSurface.SurfaceArea.mpData = NULL;
        return LB_ERROR;
    }
    memset(_lbBackSurface.SurfaceArea.mpData, 0, bufSize);

    _lbFrontSurface.lpSurface = NULL;
    _lbBackSurface.lpSurface = NULL;

    _LbModeXScreen = 0;

    _InitialiseSurface(&_lbFrontSurface, nWidth, nHeight, nDepth, flags);
    _InitialiseSurface(&_lbBackSurface, nWidth, nHeight, nDepth, flags);

    // Create D3D9 device via Pop3Screen (windowed mode by default)
    bool windowed = (flags & LB_SCREEN_MODE_WINDOWED) != 0;

    // Adjust window style for windowed vs fullscreen
    if (windowed)
    {
        DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        SetWindowLongA(_lbhWndMain, GWL_STYLE, style);

        // Scale the window up so the game isn't tiny on modern monitors.
        // Find the largest integer scale that fits comfortably on screen.
        int deskW = GetSystemMetrics(SM_CXSCREEN);
        int deskH = GetSystemMetrics(SM_CYSCREEN);
        int scale = 1;
        while ((int)nWidth * (scale + 1) <= deskW * 9 / 10 &&
               (int)nHeight * (scale + 1) <= deskH * 9 / 10)
            scale++;

        RECT rc = { 0, 0, (LONG)(nWidth * scale), (LONG)(nHeight * scale) };
        AdjustWindowRect(&rc, style, FALSE);
        SetWindowPos(_lbhWndMain, HWND_TOP,
            (deskW - (rc.right - rc.left)) / 2,
            (deskH - (rc.bottom - rc.top)) / 2,
            rc.right - rc.left, rc.bottom - rc.top,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
    else
    {
        DWORD style = WS_POPUP | WS_VISIBLE;
        SetWindowLongA(_lbhWndMain, GWL_STYLE, style);
        SetWindowPos(_lbhWndMain, HWND_TOP, 0, 0, nWidth, nHeight,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }

    if (!Pop3Screen::isReady())
    {
        if (!Pop3Screen::create(_lbhWndMain, nWidth, nHeight, windowed))
        {
            free(_lbFrontSurface.SurfaceArea.mpData);
            free(_lbBackSurface.SurfaceArea.mpData);
            _lbFrontSurface.SurfaceArea.mpData = NULL;
            _lbBackSurface.SurfaceArea.mpData = NULL;
            return LB_ERROR;
        }
    }

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
    if (!_lbBackSurface.SurfaceArea.mpData || !_lbFrontSurface.SurfaceArea.mpData)
        return LB_ERROR;

    _LbGCBS.EventNotification(LBCB_SCREEN_BEGIN_SWAP, NULL);

    // Copy back surface → front surface (software blit)
    UINT pitch = _lbBackSurface.SurfaceArea.mPitch;
    UINT height = _lbBackSurface.SurfaceArea.mSize.Height;
    memcpy(_lbFrontSurface.SurfaceArea.mpData, _lbBackSurface.SurfaceArea.mpData, pitch * height);

    // Present the front surface through D3D9
    Pop3Screen::present(
        _lbFrontSurface.SurfaceArea.mpData,
        _lbFrontSurface.SurfaceArea.mPitch,
        _lbFrontSurface.SurfaceArea.mSize.Width,
        _lbFrontSurface.SurfaceArea.mSize.Height,
        (const unsigned char*)&_lbGlobalPalette.Entry[0]);

    _LbGCBS.EventNotification(LBCB_SCREEN_END_SWAP, NULL);

    return LB_OK;
}

void LbScreen_SwapClear(TbColour colour)
{
    LbScreen_Swap(0);
    LbSurface_ClearRect(&_lbBackSurface, NULL, colour, 0);
}

TbError LbScreen_PartSwap(const Pop3Point &destOffset, const Pop3Rect &srcRect, ULONG flags)
{
    if (!_lbFrontSurface.SurfaceArea.mpData || !_lbBackSurface.SurfaceArea.mpData)
        return LB_ERROR;

    _LbGCBS.EventNotification(LBCB_SCREEN_BEGIN_SWAP, NULL);

    UINT bytesPerPixel = _lbBackSurface.SurfaceArea.mFormat.Depth / 8;
    UINT srcPitch = _lbBackSurface.SurfaceArea.mPitch;
    UINT dstPitch = _lbFrontSurface.SurfaceArea.mPitch;

    int copyWidth = (srcRect.Right - srcRect.Left) * bytesPerPixel;
    int copyHeight = srcRect.Bottom - srcRect.Top;

    for (int y = 0; y < copyHeight; y++)
    {
        UBYTE* src = _lbBackSurface.SurfaceArea.mpData + (srcRect.Top + y) * srcPitch + srcRect.Left * bytesPerPixel;
        UBYTE* dst = _lbFrontSurface.SurfaceArea.mpData + (destOffset.Y + y) * dstPitch + destOffset.X * bytesPerPixel;
        memcpy(dst, src, copyWidth);
    }

    // Present the front surface through D3D9
    Pop3Screen::present(
        _lbFrontSurface.SurfaceArea.mpData,
        _lbFrontSurface.SurfaceArea.mPitch,
        _lbFrontSurface.SurfaceArea.mSize.Width,
        _lbFrontSurface.SurfaceArea.mSize.Height,
        (const unsigned char*)&_lbGlobalPalette.Entry[0]);

    _LbGCBS.EventNotification(LBCB_SCREEN_END_SWAP, NULL);

    return LB_OK;
}

BOOL LbScreen_IsModeAvailable(UINT width, UINT height, UINT depth)
{
    BOOL found = FALSE;
    Pop3ScreenModeFind modeFind;
    Pop3ScreenMode mode;

    // The game renders at 8bpp internally but Pop3Screen converts to 32bpp.
    // Modern hardware only enumerates 32bpp modes, so map 8/16 → 32.
    UINT hwDepth = (depth <= 16) ? 32 : depth;

    if (LbScreen_ModeFindFirst(&modeFind, &mode) == LB_OK)
    {
        do
        {
            if (mode.Width == width && mode.Height == height && mode.Depth == hwDepth)
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
    // D3D9 PresentationInterval handles vsync; this is now a no-op.
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
