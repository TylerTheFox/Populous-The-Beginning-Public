#ifndef MOUSE_H
#define MOUSE_H

#include <windows.h>

void mouse_lock();
void mouse_unlock();

BOOL WINAPI fake_GetCursorPos(LPPOINT lpPoint);
BOOL WINAPI fake_ClipCursor(const RECT *lpRect);
int WINAPI fake_ShowCursor(BOOL bShow);
HCURSOR WINAPI fake_SetCursor(HCURSOR hCursor);
BOOL WINAPI fake_GetWindowRect(HWND hWnd, LPRECT lpRect);
BOOL WINAPI fake_GetClientRect(HWND hWnd, LPRECT lpRect);
BOOL WINAPI fake_ClientToScreen(HWND hWnd, LPPOINT lpPoint);
BOOL WINAPI fake_ScreenToClient(HWND hWnd, LPPOINT lpPoint);
BOOL WINAPI fake_SetCursorPos(int X, int Y);
HWND WINAPI fake_WindowFromPoint(POINT Point);
BOOL WINAPI fake_GetClipCursor(LPRECT lpRect);
BOOL WINAPI fake_GetCursorInfo(PCURSORINFO pci);
int WINAPI fake_GetSystemMetrics(int nIndex);
BOOL WINAPI fake_SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);
BOOL WINAPI fake_MoveWindow(HWND hWnd, int X, int Y, int nWidth, int nHeight, BOOL bRepaint);
LRESULT WINAPI fake_SendMessageA(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
LONG WINAPI fake_SetWindowLongA(HWND hWnd, int nIndex, LONG dwNewLong);
BOOL WINAPI fake_EnableWindow(HWND hWnd, BOOL bEnable);
BOOL WINAPI fake_DestroyWindow(HWND hWnd);
HWND WINAPI fake_CreateWindowExA(
    DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int X, int Y,
    int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam);

#endif
