#pragma once

// =============================================================================
// Pop3Types.h — Platform-independent type definitions
//
// This header deliberately does NOT include <Windows.h>. All types are defined
// from standard C++ headers so that game code consuming this header is not
// exposed to the Win32 API surface. When <Windows.h> IS included elsewhere
// (e.g. by platform .cpp files that need Win32 APIs), the guards below prevent
// double-definition conflicts.
// =============================================================================

#include <cstdint>
#include <string>

// ---- Windows-compatible base types ----------------------------------------
// These match the Windows SDK definitions exactly (size, signedness) so that
// code which was written against Win32 types continues to compile unchanged.
// The #ifndef guards use the same macros that the Windows SDK sets, so if
// <Windows.h> is pulled in before or after this header, only one definition
// of each type exists.
//
// minwindef.h guard: _MINWINDEF_
// windef.h guard:    _WINDEF_
// winnt.h guard:     _WINNT_
// basetsd.h guard:   _BASETSD_H_
// guiddef.h guard:   GUID_DEFINED
// --------------------------------------------------------------------------

#ifndef _MINWINDEF_
typedef unsigned long       DWORD;
typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef unsigned int        UINT;
typedef float               FLOAT;
#ifndef FALSE
#define FALSE               0
#endif
#ifndef TRUE
#define TRUE                1
#endif
#endif

#ifndef _WINNT_
typedef char                CHAR;
typedef short               SHORT;
typedef long                LONG;
typedef unsigned long long  ULONGLONG;
typedef unsigned short      USHORT;
#endif

// ---- Bullfrog / PopTB game types ------------------------------------------
// These are the canonical types used throughout the game codebase. They are
// defined in terms of the base types above (or directly from C++ fundamentals)
// so they work with or without <Windows.h>.

typedef unsigned long       ULONG;
typedef unsigned short      UWORD;
typedef unsigned char       UCHAR;
typedef int                 SINT;
typedef signed long         SLONG;
typedef signed char         SBYTE;
typedef unsigned char       UBYTE;
typedef signed short        SWORD;
typedef char                TBCHAR;
typedef UWORD               UNICHAR;
typedef wchar_t             UNICODE_CHAR;
typedef std::wstring        UNICODE_STRING;

// String literal pass-through used by Library8 assertion macros.
#ifndef _LBSTR
#define _LBSTR(x) x
#endif

// types for pointing to const strings
typedef const TBCHAR*       LPCTBCHAR;
typedef const CHAR*         LPCCHAR;
typedef const UNICHAR*      LPCUNICHAR;

// <t>>TbHandle<<
typedef void* TbHandle;

// ---- Opaque platform handle types ----------------------------------------
// These replace raw Windows types (HWND, HINSTANCE, etc.) in the public API.
// On Windows they are pointer-sized opaque handles; the platform .cpp files
// cast them to/from the real Windows types internally.
typedef void*               Pop3WindowHandle;   // HWND
typedef void*               Pop3AppInstance;     // HINSTANCE
typedef long                Pop3Result;          // LRESULT (LONG_PTR on x86)
typedef unsigned int        Pop3WParam;          // WPARAM  (UINT_PTR on x86)
typedef long                Pop3LParam;          // LPARAM  (LONG_PTR on x86)

// ---- Calling convention macros -------------------------------------------
// Abstracts __stdcall / __cdecl so headers compile without <Windows.h>.
#ifdef _MSC_VER
#define POP3_CALLBACK       __stdcall
#define POP3_CDECL          __cdecl
#else
#define POP3_CALLBACK
#define POP3_CDECL
#endif

// ---- GUID ----------------------------------------------------------------
// Platform-independent GUID structure compatible with the Windows SDK layout.
#ifndef GUID_DEFINED
#define GUID_DEFINED
typedef struct {
    unsigned long  Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char  Data4[8];
} GUID;
#endif

// ---- MAX_PATH ------------------------------------------------------------
#ifndef MAX_PATH
#define MAX_PATH 260
#endif

#define OBJECTS_EXPLICT_TYPE 0
#if OBJECTS_EXPLICT_TYPE
template <typename V, class D>
class StrongType
{
public:
    inline explicit StrongType()
        : m_v(0)
    {}

    inline explicit StrongType(V const &v)
        : m_v(v)
    {}

    inline operator V () const
    {
        return m_v;
    }

private:
    ULONG m_v;
};
class ThingNumTag;
typedef StrongType<UWORD, ThingNumTag> ThingNum;
#else
typedef UWORD ThingNum;
#endif
#define THING_NULL static_cast<ThingNum>(0)

//	Microsoft
//***************************************************************************
#ifdef _MSC_VER
#define __MSC__
#endif

//	Microsoft - more defines
//***************************************************************************
#if !defined(_LB_WINDOWS) && (defined(_WIN32) || defined(WIN32) || defined(__WINDOW_386__) || defined(__WINDOWS__))
#define _LB_WINDOWS
#endif


//	what platform exactly did you want to run on?
//***************************************************************************
#if !defined(_LB_WINDOWS) && !defined(_LB_DOS)
#error Please specify which platform with one of the above defines.
#endif

#if defined(_LB_DOS)
#error DOS no longer supported in library
#endif


//	Make your mine up time
//***************************************************************************
#ifdef _LB_DEBUG
#ifdef _LB_NDEBUG
#error Cant compile both _LB_DEBUG and _LB_NDEBUG at the same time
#endif
#endif


// calltypes and externs
//***************************************************************************
#define LB_CALLTYPE		__cdecl
#define LB_EXTERNTYPE	extern
#define	LB_EXTERN		extern
#ifdef __cplusplus
#define	LB_CEXTERN		extern "C"
#else
#define	LB_CEXTERN		LB_EXTERN
#endif


// callback
//***************************************************************************
#define LB_CALLBACK LB_CALLTYPE

//***************************************************************************
//	Structure and class member arrangement code
//***************************************************************************
#ifdef __cplusplus
#define PRIVATE_MEMBERS			private:
#define PROTECTED_MEMBERS		protected:
#define PUBLIC_MEMBERS			public:
#else
#define PRIVATE_MEMBERS
#define PROTECTED_MEMBERS
#define PUBLIC_MEMBERS
#endif
#if defined(_LB_LIBRARY) || !defined(_LB_DEBUG)
#define LB_PRIVATE_MEMBERS
#define LB_PROTECTED_MEMBERS
#define LB_PUBLIC_MEMBERS		PUBLIC_MEMBERS
#else
#define LB_PRIVATE_MEMBERS		PRIVATE_MEMBERS
#define LB_PROTECTED_MEMBERS	PROTECTED_MEMBERS
#define LB_PUBLIC_MEMBERS		PUBLIC_MEMBERS
#endif

#ifdef __cplusplus
#define LB_INLINE	inline
#define LB_FRIEND	friend
#else
#define LB_INLINE
#define LB_FRIEND	//
#endif

//***************************************************************************
// DLL and library call types : DLL imports, exports and externs.
//***************************************************************************
//
//	 Library Macros: Used for library-supplied functionality.
//
//		LIBEXPORT : Used to export a function from the library.
//		LIBIMPORT : Used to export a variable from the library.
//		LIBCLASSEXPORT : Used to export a class from the library.
//
//	 DLL-Only Macros: Only valid when using DLLs.
//
//		DLLIMPORT : Import function or variable from a DLL. (Used in header files)
//
//	 DLL-Only Macros: Only valid in DLL code.
//
//		DLLEXPORT :	Export the given function or variable from a DLL. (Used in source code)
//		DLLCLASSEXPORT : Export the given class from a DLL.
//
//***************************************************************************
#if defined(__MSC__) || defined(__GNUC__)
#define DLLEXPORT(ret)	__declspec(dllexport) ret
#define DLLIMPORT(ret)	__declspec(dllimport) ret
#define DLLVAREXPORT	__declspec(dllexport)
#define DLLVARIMPORT	__declspec(dllimport)
#define DLLCLASSEXPORT	__declspec(dllexport)
#define DLLCLASSIMPORT	__declspec(dllimport)
#else
#error Dont know how to declare exports for functions and classes
#endif

// Only set for building of the library
//***************************************************************************
#ifdef _LB_LIBRARY
#define LB_API(ret) LB_EXTERNTYPE ret LB_CALLTYPE
#define LB_API_CLASS(classname) class classname
#define extern	extern
#define LB_API_VAR_EXPORT
#else
#define LB_API(ret) LB_EXTERNTYPE ret LB_CALLTYPE
#define LB_API_CLASS(classname) class classname
#define extern	extern
#define LB_API_VAR_EXPORT
#endif
//#endif

// Used by TbPtrList
// <t>>TBLISTPOS<<
typedef void* TBLISTPOS;

//***************************************************************************
