#include "Pop3Input.h"
#include "Pop3App.h"
#include "Pop3Debug.h"
#include "SDL.h"

#define GET_X_LPARAM(lp)                        ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp)                        ((int)(short)HIWORD(lp))

eastl::array<bool, MAX_KEYS> Pop3Input::_keysDwn = {};
UBYTE                      Pop3Input::_flags = 0;
bool                       Pop3Input::_waiting = false;
bool                       Pop3Input::_textMode = false;
bool                       Pop3Input::_firstChar = false;
bool                       Pop3Input::_waitingKey = false;
bool                       Pop3Input::_UseWindowsMessages = false;
char                       Pop3Input::_waitChar = 0;
Pop3Point                  Pop3Input::_mousePos;
Pop3InputKey               Pop3Input::_waitKey = LB_KEY_NONE;
Pop3InputGameData          Pop3Input::_ptrs;
Pop3MouseWheel             Pop3Input::_mouseWheel = { 0.0f, 0.0f };

enum key_flags
{
    SHIFT = SHIFT_FLAG,
    CONTROL = CONTROL_FLAG,
    ALT = ALT_FLAG,
};

bool Pop3Input::init(const Pop3InputGameData & data)
{
    _ptrs = data;
#if POP3_BUILD_USE_SDL2
    SDL_SetRelativeMouseMode(SDL_TRUE);
#else
    RAWINPUTDEVICE ri[2] = {};

    // Mouse
    ri[0].usUsagePage = 0x01;
    ri[0].usUsage = 0x02;
    ri[1].dwFlags = RIDEV_NOLEGACY;

    // Keyboard.
    ri[1].usUsagePage = 0x01;
    ri[1].usUsage = 0x06;
    ri[1].dwFlags = 0;//RIDEV_NOLEGACY; 

                      // Do not use RIDEV_NOLEGACY, this will block WM_CHAR
                      // and make it hard to process chat.

    if (RegisterRawInputDevices(ri, 2, sizeof(ri[0])) == FALSE)
        return false; // :(

                      // Hide the Windows cursor.
    ShowCursor(false);
#endif
    return true;
}

void Pop3Input::setDispatchTable(const Pop3InputDispatchTable & data)
{
    _ptrs.PDT = &data;
}

void Pop3Input::setDispatchTable(const Pop3InputDispatchTable * data)
{
    _ptrs.PDT = data;
}

const Pop3InputDispatchTable * Pop3Input::getDispatchTable()
{
    if (_ptrs.PDT)
    {
        return _ptrs.PDT;
    }
    return nullptr;
}

bool Pop3Input::IS_KEY_DOWN(const Pop3InputKey& key)
{
    return _keysDwn[key];
}

bool Pop3Input::IS_KEY_UP(const Pop3InputKey& key)
{
    return !IS_KEY_DOWN(key);
}

void Pop3Input::setKeyDownState(const Pop3InputKey & key, const bool & b)
{
    _keysDwn[key] = b;
}

void Pop3Input::resetKeys()
{
    if (_ptrs.PDT)
    {
        if (_ptrs.PDT->OnKeyChange)
        {
            for (int key = 0; key < MAX_KEYS; key++)
                if (_keysDwn[key])
                {
                    _ptrs.PDT->OnKeyChange(static_cast<Pop3InputKey>(key), false, nullptr);
                    _keysDwn[key] = false;
                }
        }
        else
            _keysDwn.fill(false);
        _flags = 0;
    }
}

const UBYTE & Pop3Input::getFlags()
{
    return _flags;
}

Pop3Point * Pop3Input::getMouseXY()
{
    return &_mousePos;
}

void Pop3Input::setMousePointer(const Pop3Point *pos)
{
    _mousePos.X = pos->X;
    _mousePos.Y = pos->Y;
}
typedef void(__stdcall *poptb_callback)(void);
typedef DWORD(__stdcall *poptb_renderer)(void);
typedef DWORD(WINAPI *ccdraw_renderer)(void);

extern ccdraw_renderer poptb_directx_renderer;
char Pop3Input::waitForKeyChar()
{
    _waiting = true;
    EnterTextMode();
    while (_waiting)
    {
        poptb_directx_renderer();
        Pop3App::ProcessWindowsMessages();
        Sleep(1); // Just wait.
    }
    ExitTextMode();
    return _waitChar;
}

Pop3InputKey Pop3Input::waitForKeyLbKey()
{
    _waitingKey = true;
    while (_waitingKey)
    {
        poptb_directx_renderer();
        Pop3App::ProcessWindowsMessages();
        Sleep(1); // Just wait.
    }
    return _waitKey;
}

bool Pop3Input::getInputModes()
{
    return _UseWindowsMessages;
}

void Pop3Input::setRawInput()
{
	_UseWindowsMessages = false;
}

void Pop3Input::setDirectInput()
{
	_UseWindowsMessages = true;
}

void Pop3Input::ToggleInputModes()
{
    _UseWindowsMessages = !_UseWindowsMessages;
}

bool Pop3Input::isFirstChar()
{
    return _firstChar;
}

const eastl::array<bool, MAX_KEYS>& Pop3Input::getState()
{
    return _keysDwn;
}

const Pop3MouseWheel& Pop3Input::getMouseWheel()
{
    return _mouseWheel;
}

void Pop3Input::resetMouseWheel()
{
    _mouseWheel = { 0.0f, 0.0f };
}

#if POP3_BUILD_USE_SDL2
#define _IsKeyDown(type) ((type == SDL_KEYDOWN || (type == SDL_MOUSEBUTTONDOWN)) ? true : false)
void Pop3Input::sortOutSpecialKeys(Pop3InputKey code, Uint32 type)
{
    static auto processKeyFlags = [](enum key_flags flag, const bool & down, UBYTE & f)
    {
        if (down)
        {
            f |= flag;
            return;
        }
        f &= ~flag;
    };
    switch (code)
    {
    case LB_KEY_RSHIFT:
    case LB_KEY_LSHIFT:
        processKeyFlags(key_flags::SHIFT, _IsKeyDown(type), _flags);
        break;
    case LB_KEY_RCONTROL:
    case LB_KEY_LCONTROL:
        processKeyFlags(key_flags::CONTROL, _IsKeyDown(type), _flags);
        break;
    case LB_KEY_LALT:
    case LB_KEY_RALT:
        processKeyFlags(key_flags::ALT, _IsKeyDown(type), _flags);
        break;
    default:;
    }
}
#else
Pop3InputKey Pop3Input::sortOutSpecialKeys(const WPARAM & wParam, const USHORT & KeyboardFlags, const USHORT & makeCode, const bool & down)
{
    int extended = ((KeyboardFlags & RI_KEY_E0) != 0);
    static const int SC_SHIFT_R = 54;
    static auto inputKey = Pop3InputKey::LB_KEY_NONE;
    static auto processKeyFlags = [](enum key_flags flag, const bool & down, UBYTE & f)
    {
        if (down)
        {
            f |= flag;
            return;
        }
        f &= ~flag;
    };
    inputKey = static_cast<Pop3InputKey>(wParam);

    switch (wParam)
    {
    case VK_SHIFT:
    {
        inputKey = (makeCode == SC_SHIFT_R) ? LB_KEY_RSHIFT : LB_KEY_LSHIFT;
        processKeyFlags(key_flags::SHIFT, down, _flags);
        break;
    }
    case VK_CONTROL:
    {
        inputKey = extended ? LB_KEY_RCONTROL : LB_KEY_LCONTROL;
        processKeyFlags(key_flags::CONTROL, down, _flags);
        break;
    }
    case VK_MENU:
    {
        inputKey = extended ? LB_KEY_RALT : LB_KEY_LALT;
        processKeyFlags(key_flags::ALT, down, _flags);
        break;
    }
    default:;
    }
    return inputKey;
}
#endif

void Pop3Input::handleTextInput(const char & c)
{
    if (_textMode)
    {
        //if (!_firstChar)
        {
            // Exit any waiting.
            if (_waiting)
            {
                _waitChar = c;
                _waiting = false;
            }

            _ptrs.Onmsg(c);
        }
        /*else*/ _firstChar = false;
    }
}

#if !POP3_BUILD_USE_SDL2
LRESULT Pop3Input::ProcessEvent(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static const Pop3InputDispatchTable * dispatchPtr;
    static Pop3InputKey currKey;

    // RawInput
    static UINT dwSize;
    static LPBYTE lpb = nullptr;
    static RAWINPUT * raw;


    static auto KeyButtonPressed = [](const WPARAM & param, const USHORT & KeyboardFlags, const USHORT & makeCode, const bool & down)
    {
        currKey = sortOutSpecialKeys(param, KeyboardFlags, makeCode, down);
        
        if (_waitingKey)
        {
            _waitKey = currKey;
            _waitingKey = false;
        }

        if (currKey == LB_KEY_ESC)
            Pop3Input::resetKeys();

        _keysDwn[currKey] = down;
        if (_ptrs.PDT && _ptrs.PDT->OnKeyChange)
            _ptrs.PDT->OnKeyChange(currKey, _keysDwn[currKey], nullptr);
    };
    static auto MouseButtonPressed = [](const WPARAM & param, const USHORT & KeyboardFlags, const USHORT & makeCode, const bool & down, const Pop3Point & pos)
    {
        currKey = sortOutSpecialKeys(param, KeyboardFlags, makeCode, down);
        _keysDwn[currKey] = down;
        if (_ptrs.PDT && _ptrs.PDT->OnMouseChange)
            _ptrs.PDT->OnMouseChange(currKey, _keysDwn[currKey], pos.X, pos.Y, nullptr);
    };
    switch (msg)
    {
    case WM_MOUSEMOVE:
        if (_UseWindowsMessages)
        {
            // Update Pointer Position
            _mousePos.X = GET_X_LPARAM(lParam);
            _mousePos.Y = GET_Y_LPARAM(lParam);

            // Check mouse coords.
            if (_mousePos.X < 0)
                _mousePos.X = 0;
            else if (_mousePos.X > *_ptrs.ScreenW)
                _mousePos.X = *_ptrs.ScreenW;

            if (_mousePos.Y < 0)
                _mousePos.Y = 0;
            else if (_mousePos.Y > *_ptrs.ScreenH)
                _mousePos.Y = *_ptrs.ScreenH;
        }
        return 0;
    case WM_INPUT:
        if (!lpb)
        {
            GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
            lpb = new BYTE[dwSize];
        }
        GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER));
        raw = reinterpret_cast<RAWINPUT*>(lpb);
        switch (raw->header.dwType)
        {
        case RIM_TYPEKEYBOARD: 
            switch (raw->data.keyboard.Message)
            {
            case WM_SYSKEYDOWN:
            case WM_KEYDOWN:
            case WM_SYSKEYUP:
            case WM_KEYUP:
                KeyButtonPressed(raw->data.keyboard.VKey, raw->data.keyboard.Flags, raw->data.keyboard.MakeCode,(raw->data.keyboard.Flags & RI_KEY_BREAK) == 0);
                return 0;
            default:; // Unsupported state.
            }
            break;
        case RIM_TYPEMOUSE:
            if (!_UseWindowsMessages)
            {
                // Update Pointer Position
                _mousePos.X += raw->data.mouse.lLastX;
                _mousePos.Y += raw->data.mouse.lLastY;

                // Check mouse coords.
                if (_mousePos.X < 0)
                    _mousePos.X = 0;
                else if (_mousePos.X > *_ptrs.ScreenW)
                    _mousePos.X = *_ptrs.ScreenW;

                if (_mousePos.Y < 0)
                    _mousePos.Y = 0;
                else if (_mousePos.Y > *_ptrs.ScreenH)
                    _mousePos.Y = *_ptrs.ScreenH;
            }

            // Check Mouse Buttons
            if ((raw->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN) > 0)
            {
                MouseButtonPressed((*_ptrs.InvertMouseButtons) ? Pop3InputKey::LB_KEY_MOUSE1 : Pop3InputKey::LB_KEY_MOUSE0, raw->data.keyboard.Flags, raw->data.keyboard.MakeCode, true, _mousePos);
            }
            else if ((raw->data.mouse.usButtonFlags & RI_MOUSE_LEFT_BUTTON_UP) > 0)
            {
                MouseButtonPressed((*_ptrs.InvertMouseButtons) ? Pop3InputKey::LB_KEY_MOUSE1 : Pop3InputKey::LB_KEY_MOUSE0, raw->data.keyboard.Flags, raw->data.keyboard.MakeCode, false, _mousePos);
            }

            if ((raw->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_DOWN) > 0)
            {
                MouseButtonPressed((*_ptrs.InvertMouseButtons) ? Pop3InputKey::LB_KEY_MOUSE0 : Pop3InputKey::LB_KEY_MOUSE1, raw->data.keyboard.Flags, raw->data.keyboard.MakeCode, true, _mousePos);
            }
            else if ((raw->data.mouse.usButtonFlags & RI_MOUSE_RIGHT_BUTTON_UP) > 0)
            {
                MouseButtonPressed((*_ptrs.InvertMouseButtons) ? Pop3InputKey::LB_KEY_MOUSE0 : Pop3InputKey::LB_KEY_MOUSE1, raw->data.keyboard.Flags, raw->data.keyboard.MakeCode, false, _mousePos);
            }
             
            if ((raw->data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_DOWN) > 0)
            {
                MouseButtonPressed(Pop3InputKey::LB_KEY_MOUSE2, raw->data.keyboard.Flags, raw->data.keyboard.MakeCode, true, _mousePos);
            }
            else if ((raw->data.mouse.usButtonFlags & RI_MOUSE_MIDDLE_BUTTON_UP) > 0)
            {
                MouseButtonPressed(Pop3InputKey::LB_KEY_MOUSE2, raw->data.keyboard.Flags, raw->data.keyboard.MakeCode, false, _mousePos);
            }
            
            if ((raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_DOWN) > 0)
            {
                MouseButtonPressed(Pop3InputKey::LB_KEY_MOUSE3, raw->data.keyboard.Flags, raw->data.keyboard.MakeCode, true, _mousePos);
            }
            else if ((raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_4_UP) > 0)
            {
                MouseButtonPressed(Pop3InputKey::LB_KEY_MOUSE3, raw->data.keyboard.Flags, raw->data.keyboard.MakeCode, false, _mousePos);
            }
            
            if ((raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_DOWN) > 0)
            {
                MouseButtonPressed(Pop3InputKey::LB_KEY_MOUSE4, raw->data.keyboard.Flags, raw->data.keyboard.MakeCode, true, _mousePos);
            }
            else if ((raw->data.mouse.usButtonFlags & RI_MOUSE_BUTTON_5_UP) > 0)
            {
                MouseButtonPressed(Pop3InputKey::LB_KEY_MOUSE4, raw->data.keyboard.Flags, raw->data.keyboard.MakeCode, false, _mousePos);
            }
            else if ((raw->data.mouse.usButtonFlags & RI_MOUSE_WHEEL) > 0)
            {
                _mouseWheel.V += static_cast<float>(static_cast<signed short>(raw->data.mouse.usButtonData)) / static_cast<float>(WHEEL_DELTA);
            }
            else if ((raw->data.mouse.usButtonFlags & RI_MOUSE_HWHEEL) > 0)
            {
                _mouseWheel.H += static_cast<float>(static_cast<signed short>(raw->data.mouse.usButtonData)) / static_cast<float>(WHEEL_DELTA);
            }
        default:;// Unsupported  device.
        }
        return 0;
    case WM_CHAR:
        Pop3Input::handleTextInput(static_cast<char>(wParam));
        return 0;
    default:;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
#else
void Pop3Input::ProcessEvent(const SDL_Event & ev)
{
    static auto inputKey = Pop3InputKey::LB_KEY_NONE;
    static const Pop3InputDispatchTable * dispatchPtr;
    static SDL_Scancode code;

    code = ev.key.keysym.scancode;

    switch (ev.type)
    {
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        code = static_cast<SDL_Scancode>(ev.button.button + MOUSE_ENUM_OFFSET);
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        inputKey = static_cast<Pop3InputKey>(code);
        dispatchPtr = GetCurrentDisPatchPtr();
        sortOutSpecialKeys(inputKey, ev.type);
        _keysDwn[inputKey] = _IsKeyDown(ev.type);
        break;
    default:;
    }

    switch (ev.type)
    {
    case SDL_MOUSEMOTION:
        // Update Pointer Position
        _mousePos.X = ev.button.x;
        _mousePos.Y = ev.button.y;
        break;
    case SDL_KEYDOWN:
    case SDL_KEYUP:
        dispatchPtr->OnKeyChange(inputKey, _keysDwn[inputKey], nullptr);
        break;
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP:
        dispatchPtr->OnMouseChange(inputKey, _keysDwn[inputKey], ev.button.x, ev.button.y, nullptr);
        break;
    case SDL_TEXTINPUT:
        handleTextInput(ev.text.text[0]);
        break;
    default: ASSERT(false);
    }
}
#endif

void Pop3Input::EnterTextMode()
{
#if POP3_BUILD_USE_SDL2
    SDL_StartTextInput();
#endif
    _textMode = true;
    _firstChar = true;
}

void Pop3Input::ExitTextMode()
{
#if POP3_BUILD_USE_SDL2
    SDL_StopTextInput();
#endif
    _textMode = false;
}
