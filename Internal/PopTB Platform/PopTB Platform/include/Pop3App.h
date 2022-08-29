#pragma once
#ifdef _WIN32
#include  "Windows.h"
#endif
#include "Pop3Build.h"
#include "Pop3Input.h"
#include <string>

typedef void(__stdcall *poptb_callback)(void);

#if POP3_BUILD_USE_SDL2
typedef void(__cdecl *Pop3AppEventCallback)(const SDL_Event & ev);
#else
typedef LONG(_stdcall *Pop3AppEventCallback)(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

struct Pop3AppGameData
{
    int                             Icon;
    HINSTANCE                       hinst;
    Pop3AppEventCallback            EventCallBack;
	_se_translator_function			Seh;
	terminate_handler				Term;
};

struct Pop3AppGameInitData
{
    Pop3AppGameData     AppData;
    Pop3InputGameData   InputData;
};

struct SDL_Window;

class Pop3App
{
public:
    Pop3App() = delete;
    static void init(const Pop3AppGameInitData & data);

#ifdef _WIN32
	static void         ProcessWindowsMessages();
    static void         ProcessWindowsMessages(const struct Pop3InputDispatchTable & dispatchTable);
    static HINSTANCE    getHinstance();
    static void         setHwnd(HWND h);
    static HWND         getHwnd();
#if !POP3_BUILD_USE_SDL2
    static HWND         CreateScreen();
#endif
#endif

    static void setIcon(const int & i);
    static bool isActive();
    static bool isQuitting();
    static void setWindowName(const std::string & name);
    static const std::string & getWindowName();
    static void  setQutting(bool s);
    // SDL
#if POP3_BUILD_USE_SDL2
    static void CreateScreen();
    static SDL_Window* getWindow();
#endif
    static void DestroyScreen();
    static void Terminate();

	// Exception Handlers
	static _se_translator_function	_Pop3_SEH;
	static terminate_handler		_Pop3_Term;
private:
    // General info
    static std::string              _windowName;
    static int                      _icon;
    static bool                     _active;
    static bool                     _quitting;

    // SDL
#if POP3_BUILD_USE_SDL2
    static SDL_Window*              _window;
#endif
    static Pop3AppEventCallback     _EventCallback;

#if !POP3_BUILD_USE_SDL2
    static LRESULT CALLBACK MainWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

    // Old Win32 shit
#ifdef _WIN32
    static poptb_callback*          _deviceLost;
    static HINSTANCE                _hinst;
    static HWND                     _hwnd;
#endif
};