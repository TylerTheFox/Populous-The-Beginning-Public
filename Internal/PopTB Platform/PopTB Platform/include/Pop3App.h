#pragma once
#include "Pop3Build.h"
#include "Pop3Input.h"
#include "Pop3MajorFault.h"
#include <string>
#include <exception>

typedef void(POP3_CALLBACK *poptb_callback)(void);

#if POP3_BUILD_USE_SDL2
typedef void(POP3_CDECL *Pop3AppEventCallback)(const SDL_Event & ev);
#else
typedef Pop3Result(POP3_CALLBACK *Pop3AppEventCallback)(Pop3WindowHandle hwnd, UINT msg, Pop3WParam wParam, Pop3LParam lParam);
#endif

struct Pop3AppGameData
{
    int                             Icon;
    Pop3AppInstance                  hinst;
    Pop3AppEventCallback            EventCallBack;
    Pop3SEHTranslator               Seh;
    std::terminate_handler          Term;
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
    static void             ProcessWindowsMessages();
    static void             ProcessWindowsMessages(const struct Pop3InputDispatchTable & dispatchTable);
    static Pop3AppInstance  getHinstance();
    static void             setHwnd(Pop3WindowHandle h);
    static Pop3WindowHandle getHwnd();
#if !POP3_BUILD_USE_SDL2
    static Pop3WindowHandle CreateScreen();
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
    static Pop3SEHTranslator        _Pop3_SEH;
    static std::terminate_handler   _Pop3_Term;
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
    static Pop3Result POP3_CALLBACK MainWindowProc(Pop3WindowHandle hwnd, UINT msg, Pop3WParam wParam, Pop3LParam lParam);
#endif

    // Old Win32 shit
#ifdef _WIN32
    static poptb_callback*          _deviceLost;
    static Pop3AppInstance          _hinst;
    static Pop3WindowHandle        _hwnd;
#endif
};
