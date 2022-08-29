#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

void CounterStart();
double CounterStop();
void DebugPrint(const char *format, ...);
void DrawFrameInfoStart();
void DrawFrameInfoEnd();
int dprintf(const char *fmt, ...);

extern double DebugFrameTime;
extern DWORD DebugFrameCount;

//#define _DEBUG 1

//use OutputDebugStringA rather than printf
#define _DEBUG_S 1

//log everything (slow)
//#define _DEBUG_X 1

#ifdef _DEBUG

#ifdef _DEBUG_S
#define printf(format, ...) DebugPrint("xDBG " format, ##__VA_ARGS__)
#else
#define printf(format, ...) dprintf(format, ##__VA_ARGS__) 
#endif 

#else 
#define printf(format, ...)
#endif 
