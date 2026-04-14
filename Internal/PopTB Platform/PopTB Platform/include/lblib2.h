#pragma once
// Do this to set up any necessary defines (i.e. _LB_DEBUG)
#include <LbTypes.h>
#include <stdio.h>

#if defined(_AFXDLL)
#pragma comment(linker, "/NODEFAULTLIB:libcmtd.lib")
#else
// Check we are using the multithreaded libraries
#if !defined(_MT)
#if defined(_LB_DEBUG)
#error Please use the  "Debug Multitreaded" runtime library
#else
#error Please use the "Multithreaded" runtime library
#endif
#endif
#endif