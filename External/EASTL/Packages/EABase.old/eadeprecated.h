/*-----------------------------------------------------------------------------
 * eadeprecated.h
 *
 * Copyright (c) Electronic Arts. All rights reserved.
 *---------------------------------------------------------------------------*/


#ifndef INCLUDED_eadeprecated_H
#define INCLUDED_eadeprecated_H

#include <EABase/eabase.h>

#if defined(EA_PRAGMA_ONCE_SUPPORTED)
	#pragma once
#endif


// ------------------------------------------------------------------------
// EA_DEPRECATED / EA_PREFIX_DEPRECATED / EA_POSTFIX_DEPRECATED
//
// Used to signify that a function, variable, or type is deprecated.
// Usage:
//     EA_DEPRECATED void Function();
//     EA_PREFIX_DEPRECATED void Function() EA_POSTFIX_DEPRECATED;
//
#ifndef EA_DEPRECATED
	#if defined(EA_COMPILER_CPP14_ENABLED) || (defined(_MSC_VER) && (_MSC_VER >= 1900)) || (defined(__GNUC__) && (__GNUC__ >= 5)) || defined(__clang__)
		#define EA_DEPRECATED [[deprecated]]
	#elif defined(_MSC_VER) && (_MSC_VER > 1300)
		#define EA_DEPRECATED __declspec(deprecated)
	#elif defined(__GNUC__) && (__GNUC__ >= 4)
		#define EA_DEPRECATED __attribute__((deprecated))
	#else
		#define EA_DEPRECATED
	#endif
#endif

#ifndef EA_PREFIX_DEPRECATED
	#if defined(_MSC_VER) && (_MSC_VER > 1300)
		#define EA_PREFIX_DEPRECATED __declspec(deprecated)
		#define EA_POSTFIX_DEPRECATED
	#elif defined(__GNUC__) && (__GNUC__ >= 4)
		#define EA_PREFIX_DEPRECATED
		#define EA_POSTFIX_DEPRECATED __attribute__((deprecated))
	#else
		#define EA_PREFIX_DEPRECATED
		#define EA_POSTFIX_DEPRECATED
	#endif
#endif


// ------------------------------------------------------------------------
// EA_DEPRECATED_MESSAGE
//
// Used to signify that a function, variable, or type is deprecated, with a
// message that may be displayed by the compiler.
// Usage:
//     EA_DEPRECATED_MESSAGE("Use Bar instead") void Foo();
//
#ifndef EA_DEPRECATED_MESSAGE
	#if defined(EA_COMPILER_CPP14_ENABLED) || (defined(_MSC_VER) && (_MSC_VER >= 1900)) || (defined(__GNUC__) && (__GNUC__ >= 5)) || defined(__clang__)
		#define EA_DEPRECATED_MESSAGE(msg) [[deprecated(#msg)]]
	#elif defined(_MSC_VER) && (_MSC_VER >= 1400)
		#define EA_DEPRECATED_MESSAGE(msg) __declspec(deprecated(#msg))
	#else
		#define EA_DEPRECATED_MESSAGE(msg) EA_DEPRECATED
	#endif
#endif


// ------------------------------------------------------------------------
// EA_DEPRECATED_TYPEDEF
//
// Typedefs an old type name to a new type name, and marks the old name
// deprecated. Some compilers don't support deprecating typedefs, so on
// those this reduces to a plain typedef.
// Usage:
//     EA_DEPRECATED_TYPEDEF(OldName, NewName);
//
#ifndef EA_DEPRECATED_TYPEDEF
	#if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ >= 5)) || (defined(_MSC_VER) && (_MSC_VER >= 1900))
		#define EA_DEPRECATED_TYPEDEF(oldType, newType) using oldType EA_DEPRECATED = newType
	#else
		#define EA_DEPRECATED_TYPEDEF(oldType, newType) typedef newType oldType
	#endif
#endif


// ------------------------------------------------------------------------
// EA_DISABLE_VC_WARNING_DEPRECATION / EA_RESTORE_VC_WARNING_DEPRECATION
//
// Push/pop suppression of MSVC's deprecation warnings (C4995/C4996).
//
#ifndef EA_DISABLE_VC_WARNING_DEPRECATION
	#if defined(_MSC_VER)
		#define EA_DISABLE_VC_WARNING_DEPRECATION() \
			__pragma(warning(push))                 \
			__pragma(warning(disable: 4995 4996))
	#else
		#define EA_DISABLE_VC_WARNING_DEPRECATION()
	#endif
#endif

#ifndef EA_RESTORE_VC_WARNING_DEPRECATION
	#if defined(_MSC_VER)
		#define EA_RESTORE_VC_WARNING_DEPRECATION() __pragma(warning(pop))
	#else
		#define EA_RESTORE_VC_WARNING_DEPRECATION()
	#endif
#endif


// ------------------------------------------------------------------------
// EA_DEPRECATED_ENUM_ITEM / EA_NON_DEPRECATED_ENUM_ITEM
//
// Used to mark individual enumerators deprecated (C++14 and later).
//
#ifndef EA_DEPRECATED_ENUM_ITEM
	#if defined(EA_COMPILER_CPP14_ENABLED) || (defined(_MSC_VER) && (_MSC_VER >= 1900)) || (defined(__GNUC__) && (__GNUC__ >= 6)) || defined(__clang__)
		#define EA_DEPRECATED_ENUM_ITEM(item) item [[deprecated]]
		#define EA_DEPRECATED_ENUM_ITEM_MESSAGE(item, msg) item [[deprecated(#msg)]]
	#else
		#define EA_DEPRECATED_ENUM_ITEM(item) item
		#define EA_DEPRECATED_ENUM_ITEM_MESSAGE(item, msg) item
	#endif
#endif

#ifndef EA_NON_DEPRECATED_ENUM_ITEM
	#define EA_NON_DEPRECATED_ENUM_ITEM(item) item
#endif


#endif // INCLUDED_eadeprecated_H
