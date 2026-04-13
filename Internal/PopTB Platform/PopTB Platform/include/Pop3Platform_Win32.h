#pragma once
// =============================================================================
// Pop3Platform_Win32.h — Controlled Windows.h inclusion
//
// Game code that needs Win32 APIs should include this header instead of
// including <Windows.h> directly. This ensures consistent define-before-include
// order (NOMINMAX, WIN32_LEAN_AND_MEAN, winsock2 ordering, etc.) and gives us
// a single chokepoint to audit Win32 usage across the codebase.
//
// Files that do NOT need Win32 APIs should NOT include this header — they
// get platform-independent types from Pop3Types.h via Pop3Lib.h.
// =============================================================================

#ifdef _WIN32

// Ensure Winsock2 is included before Windows.h to avoid redefinition warnings.
// The game.h PCH already force-includes winsock2.h via ForcedIncludeFiles, but
// this is a safety net for files compiled without the PCH.
#ifndef _WINSOCKAPI_
#include <WinSock2.h>
#include <ws2tcpip.h>
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <Windows.h>

#endif // _WIN32
