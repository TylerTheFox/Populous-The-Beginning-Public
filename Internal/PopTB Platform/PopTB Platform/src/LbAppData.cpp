// LbAppData.cpp - Application data management
// Reimplemented from IDA disassembly of Bullfrog Library 8.1
#include <windows.h>
#include "Pop3Types.h"
#include <LbError.h>
#include <Pop3ScreenMode.h>

static TbHandle _lbAppInstance;
static TbHandle _lbAppHWnd;
static BOOL _lbAppCloseDown;
static void *_lbAppWinProc;
static short _lbAppIcon;
static char _lbAppName[64];

TbHandle LbApp_GetHInstance(void)
{
    return _lbAppInstance;
}

void LbApp_SetHInstance(TbHandle handle)
{
    _lbAppInstance = handle;
}

TbHandle LbApp_GetHWnd(void)
{
    return _lbAppHWnd;
}

void LbApp_SetHWnd(TbHandle handle)
{
    _lbAppHWnd = handle;
}

void LbApp_ProcessWindowMessages()
{
    MSG msg;
    int limit = 2000;
    while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE))
    {
        if (!limit--)
            break;
        TranslateMessage(&msg);
        DispatchMessageA(&msg);
    }
}

BOOL LbApp_GetCloseDownRequest()
{
    return _lbAppCloseDown;
}

void LbApp_SetCloseDownRequest(BOOL bFlag)
{
    _lbAppCloseDown = bFlag;
}

void LbApp_SetUserWndProc(void *pWinProc)
{
    _lbAppWinProc = pWinProc;
}

void *LbApp_GetUserWndProc()
{
    return _lbAppWinProc;
}

TbError LbApp_EnableWindowsScreenSaver(BOOL enable)
{
    if (SystemParametersInfoA(SPI_SETSCREENSAVEACTIVE, enable, NULL, 0))
        return LB_OK;
    return LB_ERROR;
}

TbError LbApp_SetWindowIcon(short resourceID)
{
    _lbAppIcon = resourceID;
    return LB_OK;
}

short LbApp_GetWindowIcon(void)
{
    return _lbAppIcon;
}

TbError LbApp_SetWindowName(const char *windowName)
{
    if (windowName)
        strncpy(_lbAppName, windowName, 64);
    else
        strncpy(_lbAppName, "Bullfrog", 64);
    return LB_OK;
}

const char *LbApp_GetWindowName(void)
{
    return _lbAppName;
}

void LbApp_GetWindowsScreenMode(Pop3ScreenMode *mode)
{
    HDC hdc = CreateDCA("DISPLAY", NULL, NULL, NULL);
    if (hdc)
    {
        mode->Depth = GetDeviceCaps(hdc, BITSPIXEL);
        mode->Width = GetDeviceCaps(hdc, HORZRES);
        mode->Height = GetDeviceCaps(hdc, VERTRES);
        DeleteDC(hdc);
    }
}
