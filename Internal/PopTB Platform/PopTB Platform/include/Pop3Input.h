#pragma once
#include "Pop3Keys.h"
#include "Pop3Point.h"
#include "Pop3Types.h"
#include <functional>
#include "SDL.h"
#include <EASTL/array.h>

#define	SHIFT_FLAG				(1<<0)
#define	CONTROL_FLAG			(1<<1)
#define	ALT_FLAG				(1<<2)
#define	DONT_CARE_SHIFT_FLAG	(1<<3)
#define	DONT_CARE_CONTROL_FLAG	(1<<4)
#define	DONT_CARE_ALT_FLAG		(1<<5)
#define	DONT_CARE_FLAG			(1<<6)

#define	DONT_CARE_FLAGS_SHIFT	(3)
#define	MOD_FLAGS_MASK			((1<<DONT_CARE_FLAGS_SHIFT)-1)
#define LB_KEY_MOUSE_LEFT		LB_KEY_MOUSE0
#define LB_KEY_MOUSE_RIGHT		LB_KEY_MOUSE1
#define LB_KEY_MOUSE_MIDDLE		LB_KEY_MOUSE2

typedef std::function<BOOL(Pop3InputKey, BOOL, void *)>             OnKeyChangeFunc;
typedef std::function<BOOL(Pop3InputKey, BOOL, SINT, SINT, void *)> OnMouseChangeFunc;
typedef std::function<void(const char &)>                           OnCharMsg;

typedef struct Pop3InputDispatchTable
{
    OnKeyChangeFunc     OnKeyChange;        // Called on a key up or down event
    OnMouseChangeFunc   OnMouseChange;      // Called on a mouse button up or down event
} Pop3InputDispatchTable;

typedef struct Pop3InputGameData
{
	const struct Pop3InputDispatchTable*    PDT;
    OnCharMsg                               Onmsg;
    SWORD*                                  ScreenW;
    SWORD*                                  ScreenH;
    bool*                                   InvertMouseButtons;
} Pop3InputGameData;

struct Pop3MouseWheel
{
    float V;
    float H;
};

class Pop3Input
{
public:
    Pop3Input() = delete;

    static bool                             init(const Pop3InputGameData & data);
	static void                             setDispatchTable(const Pop3InputDispatchTable & data);
    static void                             setDispatchTable(const Pop3InputDispatchTable * data);
    static const Pop3InputDispatchTable *   getDispatchTable();
    static bool                             IS_KEY_DOWN(const Pop3InputKey & key);
    static bool                             IS_KEY_UP(const Pop3InputKey & key);
    static void                             setKeyDownState(const Pop3InputKey & key, const bool & b);
    static void                             resetKeys();
    static const UBYTE &                    getFlags();
    static Pop3Point*                       getMouseXY();
    static void                             setMousePointer(const Pop3Point *pos);
    static char                             waitForKeyChar();
    static Pop3InputKey                     waitForKeyLbKey();
    static bool                             getInputModes();
	static void								setRawInput();
	static void								setDirectInput();
    static void                             ToggleInputModes();

    static bool                             isFirstChar();
    static const eastl::array<bool, MAX_KEYS> & getState();
    static const Pop3MouseWheel&            getMouseWheel();
    static void                             resetMouseWheel();
#if POP3_BUILD_USE_SDL2
    static void                             ProcessEvent(const SDL_Event & ev);
#else
    static LRESULT                          ProcessEvent(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

    static void                             EnterTextMode();
    static void                             ExitTextMode();
private:
#if POP3_BUILD_USE_SDL2
    static void                             sortOutSpecialKeys(Pop3InputKey code, Uint32 down);
#else
    static Pop3InputKey                     sortOutSpecialKeys(const WPARAM & wParam, const USHORT & KeyboardFlags, const USHORT & makeCode, const bool & down);
#endif
    static void                             handleTextInput(const char & c);

    static bool                             _UseWindowsMessages;
    static bool                             _firstChar;
    static char                             _waitChar;
    static bool                             _waiting;
    static bool                             _waitingKey;
    static bool                             _textMode;
    static Pop3Point                        _mousePos;
    static Pop3MouseWheel                   _mouseWheel;
    static UBYTE                            _flags;
    static eastl::array<bool, MAX_KEYS>      _keysDwn;
    static Pop3InputGameData                _ptrs;
    static Pop3InputKey                     _waitKey;
};

#define	_IsKeyDown(k)			            (Pop3Input::IS_KEY_DOWN(k))
#define	SHIFT_ON				            (_IsKeyDown(LB_KEY_LSHIFT)   || _IsKeyDown(LB_KEY_RSHIFT))
#define	CONTROL_ON				            (_IsKeyDown(LB_KEY_LCONTROL) || _IsKeyDown(LB_KEY_RCONTROL))
#define	ALT_ON					            (_IsKeyDown(LB_KEY_LALT)     || _IsKeyDown(LB_KEY_RALT))
