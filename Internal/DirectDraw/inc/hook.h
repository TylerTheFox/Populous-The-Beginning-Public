#ifndef HOOK_H
#define HOOK_H

#include <windows.h>

typedef BOOL (WINAPI* GETCURSORPOSPROC)(LPPOINT);
typedef BOOL(WINAPI* CLIPCURSORPROC)(const RECT*);
typedef int (WINAPI* SHOWCURSORPROC)(BOOL);
typedef HCURSOR (WINAPI* SETCURSORPROC)(HCURSOR);
typedef BOOL (WINAPI* GETWINDOWRECTPROC)(HWND, LPRECT);
typedef BOOL (WINAPI* GETCLIENTRECTPROC)(HWND, LPRECT);
typedef BOOL (WINAPI* CLIENTTOSCREENPROC)(HWND, LPPOINT);
typedef BOOL (WINAPI* SCREENTOCLIENTPROC)(HWND, LPPOINT);
typedef BOOL (WINAPI* SETCURSORPOSPROC)(int, int);
typedef HWND (WINAPI* WINDOWFROMPOINTPROC)(POINT);
typedef BOOL (WINAPI* GETCLIPCURSORPROC)(LPRECT);
typedef BOOL (WINAPI* GETCURSORINFOPROC)(PCURSORINFO);
typedef int (WINAPI* GETSYSTEMMETRICSPROC)(int);
typedef BOOL (WINAPI* SETWINDOWPOSPROC)(HWND, HWND, int, int, int, int, UINT);
typedef BOOL (WINAPI* MOVEWINDOWPROC)(HWND, int, int, int, int, BOOL);
typedef LRESULT (WINAPI* SENDMESSAGEAPROC)(HWND, UINT, WPARAM, LPARAM);
typedef LONG (WINAPI* SETWINDOWLONGAPROC)(HWND, int, LONG);
typedef BOOL (WINAPI* ENABLEWINDOWPROC)(HWND, BOOL);
typedef HWND (WINAPI* CREATEWINDOWEXAPROC)(DWORD, LPCSTR, LPCSTR, DWORD, int, int, int, int, HWND, HMENU, HINSTANCE, LPVOID);
typedef BOOL (WINAPI* DESTROYWINDOWPROC)(HWND);

extern GETCURSORPOSPROC real_GetCursorPos;
extern CLIPCURSORPROC real_ClipCursor;
extern SHOWCURSORPROC real_ShowCursor;
extern SETCURSORPROC real_SetCursor;
extern GETWINDOWRECTPROC real_GetWindowRect;
extern GETCLIENTRECTPROC real_GetClientRect;
extern CLIENTTOSCREENPROC real_ClientToScreen;
extern SCREENTOCLIENTPROC real_ScreenToClient;
extern SETCURSORPOSPROC real_SetCursorPos;
extern WINDOWFROMPOINTPROC real_WindowFromPoint;
extern GETCLIPCURSORPROC real_GetClipCursor;
extern GETCURSORINFOPROC real_GetCursorInfo;
extern GETSYSTEMMETRICSPROC real_GetSystemMetrics;
extern SETWINDOWPOSPROC real_SetWindowPos;
extern MOVEWINDOWPROC real_MoveWindow;
extern SENDMESSAGEAPROC real_SendMessageA;
extern SETWINDOWLONGAPROC real_SetWindowLongA;
extern ENABLEWINDOWPROC real_EnableWindow;
extern CREATEWINDOWEXAPROC real_CreateWindowExA;
extern DESTROYWINDOWPROC real_DestroyWindow;

extern int HookingMethod;
extern BOOL Hook_Active;

void Hook_Init();
void Hook_Exit();
void Hook_PatchIAT(HMODULE hMod, char *moduleName, char *functionName, PROC newFunction);
void Hook_Create(char *moduleName, char *functionName, PROC newFunction, PROC *function);
void Hook_Revert(char *moduleName, char *functionName, PROC newFunction, PROC *function);

#endif
