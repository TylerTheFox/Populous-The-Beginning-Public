#pragma once

#include <Windows.h>
#include <string>
typedef char                CHAR;
typedef short               SHORT;
typedef long                LONG;
typedef int                 BOOL;
typedef unsigned char       BYTE;
typedef unsigned short      WORD;
typedef float               FLOAT;
typedef DWORD               ULONG;
typedef WORD                UWORD;
typedef BYTE                UCHAR;
typedef int                 SINT;
typedef signed long         SLONG;
typedef signed char         SBYTE;
typedef unsigned char       UBYTE;
typedef signed short        SWORD;
typedef char                TBCHAR;
typedef UWORD               UNICHAR;
typedef wchar_t             UNICODE_CHAR;
typedef std::wstring        UNICODE_STRING;

// types for pointing to const strings
typedef const TBCHAR*       LPCTBCHAR;
typedef const CHAR*         LPCCHAR;
typedef const UNICHAR*      LPCUNICHAR;

// <t>>TbHandle<<
typedef void* TbHandle;

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
#define LB_API_VAR_IMPORT	extern
#define LB_API_VAR_EXPORT
#else
#define LB_API(ret) LB_EXTERNTYPE ret LB_CALLTYPE
#define LB_API_CLASS(classname) class classname
#define LB_API_VAR_IMPORT	extern
#define LB_API_VAR_EXPORT
#endif
//#endif

// Used by TbPtrList
// <t>>TBLISTPOS<<
typedef void* TBLISTPOS;

//***************************************************************************
