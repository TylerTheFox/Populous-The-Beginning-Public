#include "Pop3Platform_Win32.h"
#include "Pop3App.h"
#include "Pop3Debug.h"
#include "SDL.h"
#include "SDL_syswm.h"

#ifdef _WIN32
Pop3AppInstance   Pop3App::_hinst                    = nullptr;
Pop3WindowHandle Pop3App::_hwnd                     = nullptr;
poptb_callback*  Pop3App::_deviceLost               = nullptr;
#endif
int              Pop3App::_icon                     = 0;
bool             Pop3App::_active                   = true;
bool             Pop3App::_quitting                 = false;
std::string      Pop3App::_windowName;

Pop3AppEventCallback    Pop3App::_EventCallback     = nullptr;

Pop3SEHTranslator               Pop3App::_Pop3_SEH  = nullptr;
std::terminate_handler          Pop3App::_Pop3_Term  = nullptr;


static Pop3SEHTranslator    _Pop3_SEH;
static std::terminate_handler _Pop3_Term;

#define WM_AUTORENDERER WM_USER+111
#define WM_WINEFULLSCREEN WM_USER+112
#define WM_D3D9DEVICELOST WM_USER+113

#if POP3_BUILD_USE_SDL2
SDL_Window*  Pop3App::_window                       = nullptr;
#else
const char              className[]                 = "_Bullfrog0";
#endif

void Pop3App::init(const Pop3AppGameInitData & data)
{
    _EventCallback  = data.AppData.EventCallBack;
    _hinst          = data.AppData.hinst;
    _icon           = data.AppData.Icon;

#if POP3_BUILD_USE_SDL2
    // Init SDL
    VERIFY(SDL_Init(SDL_INIT_EVERYTHING) == FALSE);
#else
    HINSTANCE hInst = (HINSTANCE)_hinst;
    if (hInst)
        UnregisterClass(className, hInst);

    _hinst = data.AppData.hinst;
    hInst = (HINSTANCE)_hinst;

    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.lpfnWndProc = (WNDPROC)MainWindowProc;
    wcex.hInstance = hInst;
    wcex.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(_icon));
    wcex.lpszClassName = className;
    if (!RegisterClassEx(&wcex))
    {
        auto dw = GetLastError();
		Pop3Debug::fatalError("failed: GetLastError returned %u\n", dw);
    }
#endif

    // Init Other Subsystems
    VERIFY(Pop3Input::init(data.InputData));
}

void Pop3App::setIcon(const int & i)
{
    _icon = i;
}

bool Pop3App::isActive()
{
    return _active;
}

bool Pop3App::isQuitting()
{
    return _quitting;
}

void Pop3App::setWindowName(const std::string& name)
{
    _windowName = name;
}

const std::string & Pop3App::getWindowName()
{
    return _windowName;
}

void Pop3App::setQutting(bool s)
{
    _quitting = s;
}

#if POP3_BUILD_USE_SDL2
void Pop3App::CreateScreen()
{
    DestroyScreen();

    _window = SDL_CreateWindow(
        _windowName.c_str(),
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        1024,
        480,
        SDL_WINDOW_FULLSCREEN
    );

    VERIFY(_window != nullptr);

    // Required for legacy Bullfrog lib/D3D code.
    SDL_SysWMinfo systemInfo;
    SDL_VERSION(&systemInfo.version);
    SDL_GetWindowWMInfo(_window, &systemInfo);
    _hwnd = (Pop3WindowHandle)systemInfo.info.win.window;
    VERIFY(_hwnd != nullptr);
}
#else
Pop3WindowHandle Pop3App::CreateScreen()
{
    VERIFY(_hinst != nullptr);
    DestroyScreen();
    HWND hwnd = CreateWindowEx(
        0x40000u,
        className,
        "Populous: The Beginning 1.5",
        0x80080000,
        0,
        0,
        1280,
        1024,
        nullptr,
        nullptr,
        (HINSTANCE)_hinst,
        nullptr);
    _hwnd = (Pop3WindowHandle)hwnd;
    VERIFY(_hwnd != nullptr);
    return _hwnd;
}
#endif

void Pop3App::DestroyScreen()
{
#if POP3_BUILD_USE_SDL2
    if (_window)
    {
        SDL_DestroyWindow(_window);
        _window = nullptr;
    }
#else
    if (_hwnd)
    {
        ::DestroyWindow((HWND)_hwnd);
        _hwnd = nullptr;
    }
#endif
}

void Pop3App::Terminate()
{
    DestroyScreen();
#if POP3_BUILD_USE_SDL2
    SDL_Quit();
#endif
}

#if POP3_BUILD_USE_SDL2
SDL_Window* Pop3App::getWindow()
{
    return _window;
}
#endif

void Pop3App::ProcessWindowsMessages()
{
#if POP3_BUILD_USE_SDL2
	static SDL_Event ev;
	while (SDL_PollEvent(&ev))
	{
		switch (ev.type)
		{
		case SDL_TEXTINPUT:
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
		case SDL_KEYDOWN:
		case SDL_KEYUP:
		case SDL_MOUSEMOTION:
			Pop3Input::ProcessEvent(ev);
			break;
		case SDL_QUIT:
			_quitting = true;
			break;
		case SDL_WINDOWEVENT:
			switch (ev.window.event)
			{
			case SDL_WINDOWEVENT_FOCUS_LOST:
				_active = false;
				break;
			case SDL_WINDOWEVENT_FOCUS_GAINED:
				_active = true;
				break;
			case SDL_WINDOWEVENT_CLOSE:
				_quitting = true;
				break;
			default:;// dont care.
			}
			// Intentional fallthough, allows the rest of SDL_WINDOWEVENT to be handled by someone else.
		default:
			if (_EventCallback)
				_EventCallback(ev); // Not handled by us.
		}
	}
#else
	static MSG msg;
	static UINT count;
	count = 2000;
	while (PeekMessage(&msg, (HWND)_hwnd, 0, 0, PM_NOREMOVE) && count--)
	{
		if (!GetMessage(&msg, (HWND)_hwnd, 0, 0))
			break;
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
#endif
}

void Pop3App::ProcessWindowsMessages(const Pop3InputDispatchTable & dispatchTable)
{
    if (Pop3App::isActive())
	    Pop3Input::setDispatchTable(dispatchTable);
	Pop3App::ProcessWindowsMessages();
}

#ifdef _WIN32
Pop3AppInstance Pop3App::getHinstance()
{
    return _hinst;
}

void Pop3App::setHwnd(Pop3WindowHandle h)
{
    _hwnd = h;
}

Pop3WindowHandle Pop3App::getHwnd()
{
    return _hwnd;
}

#if !POP3_BUILD_USE_SDL2
// Clip the OS cursor to this window's client rect (in screen coords) while
// the window is focused. Released when focus is lost so the user can reach
// other windows, the taskbar, etc. Called from WM_SETFOCUS / WA_ACTIVE /
// WM_SIZE / WM_MOVE below. No-op in fullscreen (the clip would cover the
// whole monitor anyway) and on minimised windows (zero-size client rect).
static void clipCursorToClientRect(HWND hWnd)
{
    if (!hWnd || IsIconic(hWnd)) return;
    RECT rc;
    if (!GetClientRect(hWnd, &rc)) return;
    POINT tl = { rc.left, rc.top };
    POINT br = { rc.right, rc.bottom };
    if (!ClientToScreen(hWnd, &tl)) return;
    if (!ClientToScreen(hWnd, &br)) return;
    if (br.x <= tl.x || br.y <= tl.y) return;
    RECT screenRc = { tl.x, tl.y, br.x, br.y };
    ClipCursor(&screenRc);
}

static void releaseCursorClip()
{
    ClipCursor(nullptr);
}

Pop3Result POP3_CALLBACK Pop3App::MainWindowProc(Pop3WindowHandle hwnd, UINT msg, Pop3WParam wParam, Pop3LParam lParam)
{
    HWND hWnd = (HWND)hwnd;
    if (_quitting) return DefWindowProc(hWnd, msg, (WPARAM)wParam, (LPARAM)lParam);

    switch (msg)
    {
    case WM_MOUSEMOVE:
    case WM_INPUT:
        return Pop3Input::ProcessEvent(hwnd, msg, wParam, lParam);
    case WM_MOUSEACTIVATE:
    case WM_SYSKEYUP:
    case WM_KEYUP:
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        return 0;
    case WM_CHAR:
        return Pop3Input::ProcessEvent(hwnd, msg, wParam, lParam);;
    case WM_QUIT:
    case WM_ENDSESSION:
    case WM_CLOSE:
    case WM_DESTROY:
        // Causes issues with D3D which
        // _EventCallback is used for.
        _quitting = true;
        releaseCursorClip();
        return 0;
        break;
    case WM_CHILDACTIVATE:
    case WM_GETMINMAXINFO:
        Pop3Input::resetKeys();
        break;
    case WM_SETFOCUS:
        _active = true;
        Pop3Input::resetKeys();
        clipCursorToClientRect(hWnd);
        break;
    case WM_KILLFOCUS:
        _active = false;
        Pop3Input::resetKeys();
        releaseCursorClip();
        break;
    case WM_SIZE:
        switch (wParam)
        {
        case SIZE_MAXHIDE:
        case SIZE_RESTORED:
        case SIZE_MAXIMIZED:
        case SIZE_MAXSHOW:
            _active = true;
            break;
        case SIZE_MINIMIZED:
            _active = false;
            break;
        default:;
        }
        Pop3Input::resetKeys();
        // Client rect may have moved in screen coords — re-clip.
        if (_active)
            clipCursorToClientRect(hWnd);
        else
            releaseCursorClip();
        break;
    case WM_MOVE:
        // Window moved — the client rect's screen position changed, so the
        // existing clip is no longer aligned. Re-clip if we still own focus.
        if (_active)
            clipCursorToClientRect(hWnd);
        break;
    case WM_ACTIVATE:
        switch (wParam)
        {
        case WA_ACTIVE:
        case WA_CLICKACTIVE:
            _active = true;
            clipCursorToClientRect(hWnd);
            break;
        case WA_INACTIVE:
            _active = false;
            releaseCursorClip();
            break;
        default:;
        }
        Pop3Input::resetKeys();
        break;
    case WM_ENTERSIZEMOVE:
        // User grabbed the title bar or a resize edge — release so they can
        // drag past the window's own rect. We'll re-clip on WM_EXITSIZEMOVE.
        releaseCursorClip();
        break;
    case WM_EXITSIZEMOVE:
        if (_active)
            clipCursorToClientRect(hWnd);
        break;
    case WM_SETCURSOR:
        SetCursor(nullptr);
		return TRUE;
	case WM_SYSCOMMAND:
	if (wParam == SC_CLOSE)
	{
		_quitting = true;
		return 0;
	}
    default:;
    }
    if (_EventCallback)
        return _EventCallback(hwnd, msg, wParam, lParam);
    return DefWindowProc(hWnd, msg, (WPARAM)wParam, (LPARAM)lParam);
}
#endif
#endif
