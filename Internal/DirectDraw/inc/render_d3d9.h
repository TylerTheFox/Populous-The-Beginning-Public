#pragma once

typedef struct CUSTOMVERTEX { float x, y, z, rhw, u, v; } CUSTOMVERTEX;

DWORD WINAPI render_d3d9_main(void);
BOOL Direct3D9_Create();
BOOL Direct3D9_Reset();
BOOL Direct3D9_Release();
BOOL Direct3D9_OnDeviceLost();

extern HMODULE Direct3D9_hModule;