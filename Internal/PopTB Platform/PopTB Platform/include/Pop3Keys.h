#pragma once
#include "Pop3Build.h"
#if POP3_BUILD_USE_SDL2
#include <SDL.h>
#define MOUSE_ENUM_OFFSET SDL_NUM_SCANCODES
typedef enum Pop3InputKey
{
    LB_KEY_NONE = SDL_SCANCODE_UNKNOWN,

    // Keyboard codes
    LB_KEY_ESC = SDL_SCANCODE_ESCAPE,
    LB_KEY_1 = SDL_SCANCODE_1,
    LB_KEY_2 = SDL_SCANCODE_2,
    LB_KEY_3 = SDL_SCANCODE_3,
    LB_KEY_4 = SDL_SCANCODE_4,
    LB_KEY_5 = SDL_SCANCODE_5,
    LB_KEY_6 = SDL_SCANCODE_6,
    LB_KEY_7 = SDL_SCANCODE_7,
    LB_KEY_8 = SDL_SCANCODE_8,
    LB_KEY_9 = SDL_SCANCODE_9,
    LB_KEY_0 = SDL_SCANCODE_0,
    LB_KEY_MINUS = SDL_SCANCODE_MINUS,
    LB_KEY_EQUAL = SDL_SCANCODE_EQUALS,
    LB_KEY_BACKSPACE = SDL_SCANCODE_BACKSPACE,
    LB_KEY_TAB = SDL_SCANCODE_TAB,
    LB_KEY_Q = SDL_SCANCODE_Q,
    LB_KEY_W = SDL_SCANCODE_W,
    LB_KEY_E = SDL_SCANCODE_E,
    LB_KEY_R = SDL_SCANCODE_R,
    LB_KEY_T = SDL_SCANCODE_T,
    LB_KEY_Y = SDL_SCANCODE_Y,
    LB_KEY_U = SDL_SCANCODE_U,
    LB_KEY_I = SDL_SCANCODE_I,
    LB_KEY_O = SDL_SCANCODE_O,
    LB_KEY_P = SDL_SCANCODE_P,
    LB_KEY_LSBRACKET = SDL_SCANCODE_LEFTBRACKET,
    LB_KEY_RSBRACKET = SDL_SCANCODE_RIGHTBRACKET,
    LB_KEY_RETURN = SDL_SCANCODE_RETURN,
    LB_KEY_LCONTROL = SDL_SCANCODE_LCTRL,
    LB_KEY_A = SDL_SCANCODE_A,
    LB_KEY_S = SDL_SCANCODE_S,
    LB_KEY_D = SDL_SCANCODE_D,
    LB_KEY_F = SDL_SCANCODE_F,
    LB_KEY_G = SDL_SCANCODE_G,
    LB_KEY_H = SDL_SCANCODE_H,
    LB_KEY_J = SDL_SCANCODE_J,
    LB_KEY_K = SDL_SCANCODE_K,
    LB_KEY_L = SDL_SCANCODE_L,
    LB_KEY_COLON = SDL_SCANCODE_SEMICOLON,
    LB_KEY_QUOTE = SDL_SCANCODE_APOSTROPHE,
    LB_KEY_QUOTE2 = SDL_SCANCODE_GRAVE,
    LB_KEY_LSHIFT = SDL_SCANCODE_LSHIFT,
    LB_KEY_HASH = SDL_SCANCODE_NONUSHASH,
    LB_KEY_Z = SDL_SCANCODE_Z,
    LB_KEY_X = SDL_SCANCODE_X,
    LB_KEY_C = SDL_SCANCODE_C,
    LB_KEY_V = SDL_SCANCODE_V,
    LB_KEY_B = SDL_SCANCODE_B,
    LB_KEY_N = SDL_SCANCODE_N,
    LB_KEY_M = SDL_SCANCODE_M,
    LB_KEY_COMMA = SDL_SCANCODE_COMMA,
    LB_KEY_DOT = SDL_SCANCODE_PERIOD,
    LB_KEY_SLASH = SDL_SCANCODE_SLASH,
    LB_KEY_RSHIFT = SDL_SCANCODE_RSHIFT,
    LB_KEY_RESERVED1 = SDL_SCANCODE_LANG6,
    LB_KEY_LALT = SDL_SCANCODE_LALT,
    LB_KEY_SPACE = SDL_SCANCODE_SPACE,
    LB_KEY_CAPS = SDL_SCANCODE_CAPSLOCK,
    LB_KEY_F1 = SDL_SCANCODE_F1,
    LB_KEY_F2 = SDL_SCANCODE_F2,
    LB_KEY_F3 = SDL_SCANCODE_F3,
    LB_KEY_F4 = SDL_SCANCODE_F4,
    LB_KEY_F5 = SDL_SCANCODE_F5,
    LB_KEY_F6 = SDL_SCANCODE_F6,
    LB_KEY_F7 = SDL_SCANCODE_F7,
    LB_KEY_F8 = SDL_SCANCODE_F8,
    LB_KEY_F9 = SDL_SCANCODE_F9,
    LB_KEY_F10 = SDL_SCANCODE_F10,

    LB_KEY_NUM_ASTERISK = SDL_SCANCODE_KP_MULTIPLY,
    LB_KEY_NUM_LOCK = SDL_SCANCODE_NUMLOCKCLEAR,
    LB_KEY_SCROLL_LOCK = SDL_SCANCODE_SCROLLLOCK,
    LB_KEY_NUM_7 = SDL_SCANCODE_KP_7,
    LB_KEY_NUM_8 = SDL_SCANCODE_KP_8,
    LB_KEY_NUM_9 = SDL_SCANCODE_KP_9,
    LB_KEY_NUM_MINUS = SDL_SCANCODE_KP_MINUS,
    LB_KEY_NUM_4 = SDL_SCANCODE_KP_4,
    LB_KEY_NUM_5 = SDL_SCANCODE_KP_5,
    LB_KEY_NUM_6 = SDL_SCANCODE_KP_6,
    LB_KEY_NUM_PLUS = SDL_SCANCODE_KP_PLUS,
    LB_KEY_NUM_1 = SDL_SCANCODE_KP_1,
    LB_KEY_NUM_2 = SDL_SCANCODE_KP_2,
    LB_KEY_NUM_3 = SDL_SCANCODE_KP_3,
    LB_KEY_NUM_0 = SDL_SCANCODE_KP_0,
    LB_KEY_NUM_INSERT = LB_KEY_NUM_0,
    LB_KEY_NUM_DOT = SDL_SCANCODE_KP_PERIOD,
    LB_KEY_NUM_DELETE = LB_KEY_NUM_DOT,
    LB_KEY_BACKSLASH = SDL_SCANCODE_NONUSBACKSLASH,
    LB_KEY_F11 = SDL_SCANCODE_F11,
    LB_KEY_F12 = SDL_SCANCODE_F12,

    LB_KEY_NUM_ENTER = SDL_SCANCODE_KP_ENTER,
    LB_KEY_RCONTROL = SDL_SCANCODE_RCTRL,
    LB_KEY_NUM_SLASH = SDL_SCANCODE_KP_DIVIDE,
    LB_KEY_PRINT_SCR = SDL_SCANCODE_PRINTSCREEN,
    LB_KEY_RALT = SDL_SCANCODE_RALT,
    LB_KEY_BREAK = SDL_SCANCODE_PAUSE,
    LB_KEY_PAUSE = LB_KEY_BREAK,
    LB_KEY_HOME = SDL_SCANCODE_HOME,
    LB_KEY_UP = SDL_SCANCODE_UP,
    LB_KEY_PGUP = SDL_SCANCODE_PAGEUP,
    LB_KEY_LEFT = SDL_SCANCODE_LEFT,
    LB_KEY_RIGHT = SDL_SCANCODE_RIGHT,
    LB_KEY_END = SDL_SCANCODE_END,
    LB_KEY_DOWN = SDL_SCANCODE_DOWN,
    LB_KEY_PGDN = SDL_SCANCODE_PAGEDOWN,
    LB_KEY_INSERT = SDL_SCANCODE_INSERT,
    LB_KEY_DELETE = SDL_SCANCODE_DELETE,

    // Mouse input
    LB_KEY_MOUSE0 = (SDL_BUTTON_LEFT         + MOUSE_ENUM_OFFSET),
    LB_KEY_MOUSE1 = (SDL_BUTTON_RIGHT        + MOUSE_ENUM_OFFSET),
    LB_KEY_MOUSE2 = (SDL_BUTTON_MIDDLE       + MOUSE_ENUM_OFFSET),
    LB_KEY_MOUSE3 = (SDL_BUTTON_X1           + MOUSE_ENUM_OFFSET),
    LB_KEY_MOUSE4 = (SDL_BUTTON_X2           + MOUSE_ENUM_OFFSET)
} TbInputKey;
#define MAX_KEYS				LB_KEY_MOUSE4
#define LB_KEY_MOUSE_LEFT       LB_KEY_MOUSE0
#define LB_KEY_MOUSE_RIGHT      LB_KEY_MOUSE1
#define LB_KEY_MOUSE_MIDDLE     LB_KEY_MOUSE2
#else
#include <windows.h>
#define MOUSE_ENUM_OFFSET 256
typedef enum Pop3InputKey
{
    LB_KEY_NONE = 0,

    // Keyboard codes
    LB_KEY_ESC = VK_ESCAPE,
    LB_KEY_MINUS = VK_OEM_MINUS,
    LB_KEY_NUM_MINUS = LB_KEY_MINUS,
    LB_KEY_EQUAL = VK_OEM_PLUS,
    LB_KEY_NUM_PLUS = LB_KEY_EQUAL,
    LB_KEY_BACKSPACE = VK_BACK,
    LB_KEY_TAB = VK_TAB,

    LB_KEY_0 = 0x30,
    LB_KEY_1,
    LB_KEY_2,
    LB_KEY_3,
    LB_KEY_4,
    LB_KEY_5,
    LB_KEY_6,
    LB_KEY_7,
    LB_KEY_8,
    LB_KEY_9,

    LB_KEY_A = 0x41,
    LB_KEY_B,
    LB_KEY_C,
    LB_KEY_D,
    LB_KEY_E,
    LB_KEY_F,
    LB_KEY_G,
    LB_KEY_H,
    LB_KEY_I,
    LB_KEY_J,
    LB_KEY_K,
    LB_KEY_L,
    LB_KEY_M,
    LB_KEY_N,
    LB_KEY_O,
    LB_KEY_P,
    LB_KEY_Q,
    LB_KEY_R,
    LB_KEY_S,
    LB_KEY_T,
    LB_KEY_U,
    LB_KEY_V,
    LB_KEY_W,
    LB_KEY_X,
    LB_KEY_Y,
    LB_KEY_Z,

    LB_KEY_LSBRACKET = VK_OEM_4,
    LB_KEY_RSBRACKET = VK_OEM_6,
    LB_KEY_RETURN = VK_RETURN,
    LB_KEY_NUM_ENTER = LB_KEY_RETURN,
    LB_KEY_LCONTROL = VK_LCONTROL,
    LB_KEY_COLON = VK_OEM_1,
    LB_KEY_QUOTE = VK_OEM_8,
    LB_KEY_QUOTE2 = VK_OEM_3,
    LB_KEY_LSHIFT = VK_LSHIFT,
    LB_KEY_HASH = VK_OEM_6,
    LB_KEY_COMMA = VK_OEM_COMMA,
    LB_KEY_DOT = VK_OEM_PERIOD,
    LB_KEY_NUM_DOT = LB_KEY_DOT,
    LB_KEY_SLASH = VK_OEM_2,
    LB_KEY_RSHIFT = VK_RSHIFT,
    LB_KEY_LALT = VK_LMENU,
    LB_KEY_SPACE = VK_SPACE,
    LB_KEY_CAPS = VK_CAPITAL,

    LB_KEY_F1 = 0x70,
    LB_KEY_F2,
    LB_KEY_F3,
    LB_KEY_F4,
    LB_KEY_F5,
    LB_KEY_F6,
    LB_KEY_F7,
    LB_KEY_F8,
    LB_KEY_F9,
    LB_KEY_F10,
    LB_KEY_F11,
    LB_KEY_F12,

    LB_KEY_NUM_0 = VK_NUMPAD0,
    LB_KEY_NUM_INSERT = LB_KEY_NUM_0,
    LB_KEY_NUM_1,
    LB_KEY_NUM_2,
    LB_KEY_NUM_3,
    LB_KEY_NUM_4,
    LB_KEY_NUM_5,
    LB_KEY_NUM_6,
    LB_KEY_NUM_7,
    LB_KEY_NUM_8,
    LB_KEY_NUM_9,
    LB_KEY_NUM_SLASH = VK_DIVIDE,

    LB_KEY_NUM_ASTERISK = VK_MULTIPLY,
    LB_KEY_NUM_LOCK = VK_NUMLOCK,
    LB_KEY_SCROLL_LOCK = VK_SCROLL,
    LB_KEY_BACKSLASH = VK_OEM_5,

    LB_KEY_RCONTROL = VK_RCONTROL,
    LB_KEY_PRINT_SCR = VK_SNAPSHOT,
    LB_KEY_RALT = VK_RMENU,
    LB_KEY_BREAK = VK_PAUSE,
    LB_KEY_PAUSE = LB_KEY_BREAK,
    LB_KEY_HOME = VK_HOME,
    LB_KEY_UP = VK_UP,
    LB_KEY_PGUP = VK_PRIOR,
    LB_KEY_LEFT = VK_LEFT,
    LB_KEY_RIGHT = VK_RIGHT,
    LB_KEY_END = VK_END,
    LB_KEY_DOWN = VK_DOWN,
    LB_KEY_PGDN = VK_NEXT,
    LB_KEY_INSERT = VK_INSERT,
    LB_KEY_DELETE = VK_DELETE,
    LB_KEY_NUM_DELETE = LB_KEY_DELETE,

    // Mouse input
    LB_KEY_MOUSE0 = MOUSE_ENUM_OFFSET,
    LB_KEY_MOUSE1,
    LB_KEY_MOUSE2,
    LB_KEY_MOUSE3,
    LB_KEY_MOUSE4,

    LB_KEY_MAX
} TbInputKey;

#define MAX_KEYS				LB_KEY_MAX
#define LB_KEY_MOUSE_LEFT       LB_KEY_MOUSE0
#define LB_KEY_MOUSE_RIGHT      LB_KEY_MOUSE1
#define LB_KEY_MOUSE_MIDDLE     LB_KEY_MOUSE2
#endif