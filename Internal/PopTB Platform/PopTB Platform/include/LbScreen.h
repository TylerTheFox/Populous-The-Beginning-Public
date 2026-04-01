//***************************************************************************
// LbScrn.h : Screens and surfaces
//***************************************************************************
#ifndef _LBSCRN_H
#define _LBSCRN_H

// includes
//***************************************************************************
#include	"Pop3Types.h"
#include	<LbError.h>
#include	<LbPal.h>
#include	<LbLists.h>
#include	<LbColour.h>
#include	<LbSurface.h>
#include	<LbPal.h>
#include	<Pop3ScreenMode.h>
#include    <Pop3Point.h>

//***************************************************************************
//***************************************************************************
#include <DDraw.h>

//***************************************************************************
// Graphics system
//***************************************************************************
void	LbScreen_Terminate();


//***************************************************************************
//LbScreen_WindowsProcessing
//***************************************************************************
void	LbScreen_WindowsProcessing();
BOOL	LbScreen_IsActive();

struct IDirectDraw;
struct _GUID;
void			LbScreen_SetDirectDrawGuid(_GUID *pGuid);
IDirectDraw*	LbScreen_GetDirectDraw();
TbError			LbScreen_Initialise(_GUID *pGuid = NULL, BOOL modex = FALSE, void *hWndView = NULL, void *hWndApplication = NULL);

//***************************************************************************
// LbScreen_SetSimulator : Set the next SetMode function to use the simulator or not
//	void LbScreen_SetSimulator(BOOL useSimulator);
//***************************************************************************
#if defined(_LB_WINDOWS) && defined(_LB_DEBUG)
void LbScreen_SetSimulator(BOOL useSimulator);
BOOL LbScreen_GetSimulator();
#else
LB_INLINE void LbScreen_SetSimulator(BOOL) {};
LB_INLINE BOOL LbScreen_GetSimulator()
{
    return FALSE;
};
#endif


//***************************************************************************
// LbScreen_SetPaletteEntries : Set a range of palette entries in the screen's palette
//	void LbScreen_SetPaletteEntries(const TbPaletteEntry *palette, UINT first, UINT num);
//***************************************************************************
void LbScreen_SetPaletteEntries(const TbPaletteEntry *lpPalette, UINT nFirst, UINT nEntries);



//***************************************************************************
// LbScreen_SetPalette : Sets the whole palette in one go
//***************************************************************************
#define LbScreen_SetPalette(pPal)	LbScreen_SetPaletteEntries((pPal)->Entry, 0, 256)



//***************************************************************************
// LbScreen_GetPaletteEntries & family : Retrieve the screen's palette RGB values
//	TbPaletteEntry* LbScreen_GetPaletteEntries(TbPaletteEntry *palette, UINT first, UINT num);
//***************************************************************************
TbPaletteEntry * LbScreen_GetPaletteEntries(TbPaletteEntry *palette, UINT first, UINT num);


//***************************************************************************
// LbScreen_GetPalette : get the whole palette in one go
//***************************************************************************
#define LbScreen_GetPalette(pPal)	(TbPalette *)LbScreen_GetPaletteEntries((pPal)->Entry, 0, 256)


//***************************************************************************
// LbScreen_SetMode : Set current video mode
//	TbError LbScreen_SetMode(UINT width, UINT height, UINT depth, ULONG Flags, void *palette, UBYTE *surfaceMem);
//***************************************************************************
#define	LB_SCREEN_MODE_SWAP				0x00000001 // Use flip swap page mode
#define	LB_SCREEN_MODE_FLIP				0x00000002 // Use flip page mode
#define	LB_SCREEN_MODE_NO_CHEAP_FLIP	0x00000004 // Use flip page and do it properly in DOS
#define	LB_SCREEN_3D					0x00000008 // Enable 3D
#define	LB_SCREEN_MODE_WINDOWED			0x00000010 // Enable drawing to a window
TbError LbScreen_SetMode(UINT nWidth, UINT nHeight, UINT nDepth, ULONG flags, const TbPalette *lpPalette);


//***************************************************************************
// LbScreen_RestoreMode : Set the screen mode back to that before the app started
//	TbError LbScreen_RestoreMode();
//***************************************************************************
TbError LbScreen_RestoreMode();


//***************************************************************************
// LbScreen_Swap : Copy or flip the back and front surfaces
//	TbError LbScreen_Swap(ULONG);
//***************************************************************************
#define LB_SURFACE_SWAP_NOWAIT			0x00000001 // Don't wait for previous async task to finish (Direct draw compatible)

TbError LbScreen_Swap(ULONG flags);



//***************************************************************************
// LbScreen_SwapClear : Copy or flip the Backsurface to the Frontsurface and clear the Backsurface.
//	void LbScreen_SwapClear(UINT colour);
//***************************************************************************
void LbScreen_SwapClear(TbColour colour);



//***************************************************************************
// LbScreen_PartSwap : Copy part of the Backsurface to the Frontsurface.
//	void LbScreen_PartSwap( const Pop3Point &destOffset, const Pop3Rect &srcRect, ULONG flags);
//***************************************************************************
TbError LbScreen_PartSwap(const Pop3Point &destOffset, const Pop3Rect &srcRect, ULONG flags = 0);



//***************************************************************************
// LbScreen_ModeAvailable : Is the given screen mode available?
//	BOOL LbScreen_ModeAvailable( UINT width, UINT height, UINT depth);
//***************************************************************************
BOOL LbScreen_IsModeAvailable(UINT width, UINT height, UINT depth);



//	<s>>TbScreenModeFind<<
#ifdef _LB_WINDOWS
typedef struct _Pop3ScreenModeFind
{
    TbListNode ListNode;
    Pop3ScreenMode Info;
} _Pop3ScreenModeFind;

typedef struct Pop3ScreenModeFind
{
    TbList ModeList;
    _Pop3ScreenModeFind *pCurrent;
} Pop3ScreenModeFind;
#else
typedef SINT Pop3ScreenModeFind;
#endif



//***************************************************************************
// LbScreen_ModeFindFirst:	Find the first available mode
// LbScreen_ModeFindNext:	Find the next available mode
// LbScreen_ModeFindEnd:	Finish the search
//***************************************************************************
#if 1
TbError LbScreen_ModeFindFirst(Pop3ScreenModeFind *lpScreenModeFind, Pop3ScreenMode *lpScreenMode);
TbError LbScreen_ModeFindNext(Pop3ScreenModeFind *lpScreenModeFind, Pop3ScreenMode *modeInfo);
void	LbScreen_ModeFindEnd(Pop3ScreenModeFind *lpScreenModeFind);
#else
inline TbError _LbScreen_ModeFindFirst(Pop3ScreenModeFind *lpScreenModeFind, Pop3ScreenMode *lpScreenMode) {
    DEVMODE dm;
    EnumDisplaySettings(NULL, 0, &dm);

    lpScreenMode->Height = dm.dmPelsHeight;
    lpScreenMode->Width = dm.dmPelsWidth;
    lpScreenMode->Depth = 8;

    _Pop3ScreenModeFind *pCurrent = new _Pop3ScreenModeFind;
    pCurrent->Info.Height = dm.dmPelsHeight;
    pCurrent->Info.Width = dm.dmPelsWidth;
    pCurrent->Info.Depth = dm.dmBitsPerPel;
    pCurrent->ListNode.lpNext = nullptr;
    pCurrent->ListNode.lpPrev = nullptr;
    lpScreenModeFind->pCurrent = pCurrent;
    return LB_OK;
}

inline TbError _LbScreen_ModeFindNext(Pop3ScreenModeFind *lpScreenModeFind, Pop3ScreenMode *modeInfo) {

    return LB_ERROR;
}

inline void	_LbScreen_ModeFindEnd(Pop3ScreenModeFind *lpScreenModeFind) {

}

#define LbScreen_ModeFindFirst(lpScreenModeFind,lpScreenMode)	_LbScreen_ModeFindFirst(lpScreenModeFind,lpScreenMode)
#define LbScreen_ModeFindNext(lpScreenModeFind,lpScreenMode)	_LbScreen_ModeFindNext(lpScreenModeFind,lpScreenMode)
#define LbScreen_ModeFindEnd(lpScreenModeFind)					_LbScreen_ModeFindEnd(lpScreenModeFind)
#endif
//***************************************************************************
// LbScreen_WaitVbi : Wait until Vertical Blank is active.
//	void LbScreen_WaitVbi();
//***************************************************************************
void LbScreen_WaitVbi();

//
// Palette Fading functions.
//
SINT	LbScreen_FadePalette(TbPalette *lpPaletteTo, UINT nSteps, TbPaletteFadeType PaletteFadeType);
void	LbScreen_StopOpenFadePalette();

//***************************************************************************
// LbScreen_SetActiveWindow: Sets active draw window in windowed mode.
//***************************************************************************
void LbScreen_SetActiveWindow(TbHandle	hwnd);

//***************************************************************************
// LbScreen_GetFrontSurface : Get pointer to the front (visible) surface
// LbScreen_GetBackSurface : Get pointer to the back surface for rendering
// LbSurface_GetCurrent : Get the last locked surface
//	TbSurface *LbScreen_GetFrontSurface();
//	TbSurface *LbScreen_GetBackSurface();
//	TbSurface *LbSurface_GetCurrent();
//****************************************************************************
#if !defined(_LB_DEBUG) || defined(_LB_LIBRARY)
extern TbSurface _lbFrontSurface;
extern TbSurface _lbBackSurface;
#endif
#ifndef _LB_DEBUG
inline TbSurface *LbScreen_GetFrontSurface()
{
    return (&_lbFrontSurface);
}
inline TbSurface *LbScreen_GetBackSurface()
{
    return (&_lbBackSurface);
}

#else
TbSurface * LbScreen_GetFrontSurface();
TbSurface * LbScreen_GetBackSurface();
TbSurface * LbScreen_GetFrontSurface();
TbSurface * LbScreen_GetBackSurface();
#endif

#if 0

extern IDirectDraw * _lbpDirectDraw; //                                         00000014        
extern IDirectDrawClipper * _lbpClipper; //                                     0000001C        
extern IDirectDrawPalette * _lbpPalette; //                                     00000018  
extern Pop3ScreenModeFind _lbDisplayModeList; //                                  00000FEC                
extern TbSurface _lbBackSurface; //                                             00001104        
extern TbSurface _lbFlipSurface; //                                             00000F1C        
extern TbSurface _lbFrontSurface; //                                            00000F8C        
extern _GUID  _LbScreenDDGUID; //                                              00000028   
extern int _LbModeXScreen; //                                                   0000002C        
extern int _LbScreenActive; //                                                  00000024        
extern int _LbWindowActive; //                                                  00000020        
//extern int _lbUsingSimulator; //                                                00000004        
extern int _lbWillUseSimulator; //                                              00000008        
extern int _lbPaletteFadeStarted; //                                            00000004        
extern int _lbIsScreenInitialised; //                                           00000000    
extern int _LbPopupMessageBox(void *, char const *, char const *, UINT); //     0000121E        
extern int _LbScreen_IsModeSet(void); //                                        00002BED      

extern IDirectDraw * _lbDirectDraw;                                         // 00000000
extern void _InitialiseSurface(TbSurface *, UINT, UINT, UINT, ULONG);           // 00001CD8
extern BOOL _CheckAndRestoreSurface(TbSurface const *);// 00001FC3


//extern struct TbList _lbSurfaceList;
extern struct TbPalette _lbGlobalPalette;
extern Pop3RenderArea * _lbpCurrentRA;
extern void LbApp_ShowMessage(const char *Format, ...);
//extern void ReleaseDirectDraw();
//extern int ReleaseMode();
//extern signed int __cdecl CreateDirectDrawObject(int a1, int a2, HMODULE *a3);

inline void _InitialiseSurface2(struct TbSurface *a1, unsigned int a2, unsigned int a3, unsigned int a4, unsigned __int32 a5)
{
    char *v5; // ebp@4
    struct Pop3PixFormat *v6; // esi@4
    IDirectDrawSurface *v7; // eax@9
    unsigned int v8; // ecx@10
    unsigned int v9; // edx@10
    unsigned int v10; // eax@10
    unsigned int v11; // ecx@10
    IDirectDrawSurface *v12; // eax@11
    unsigned int v13; // eax@11
    unsigned int v14; // edx@11
    int v15; // [sp+10h] [bp-8Ch]@9
    int v16; // [sp+14h] [bp-88h]@10
    unsigned int v17; // [sp+1Ch] [bp-80h]@10
    int v18; // [sp+20h] [bp-7Ch]@10
    unsigned int v19; // [sp+24h] [bp-78h]@10
    unsigned int v20; // [sp+28h] [bp-74h]@10
    unsigned int v21; // [sp+2Ch] [bp-70h]@10
    int v22; // [sp+30h] [bp-6Ch]@11
    int v23; // [sp+34h] [bp-68h]@11
    unsigned int v24; // [sp+38h] [bp-64h]@11
    unsigned int v25; // [sp+3Ch] [bp-60h]@11
    unsigned int v26; // [sp+40h] [bp-5Ch]@11

    a1->DrawFlags = 0;
    a1->GhostTable = 0;
    a1->FadeTable = 0;
    a1->SurfaceArea.mSize.Width = a2;
    a1->SurfaceArea.mSize.Height = a3;
    a1->SurfaceArea.mFormat.Depth = a4;
    a1->mpPalette = (TbPalette *)&_lbGlobalPalette;
    if (a4 != 8)
        a1->mpPalette = 0;
    switch (a4)
    {
    default:
        v5 = (char *)&a1->SurfaceArea.mFormat;
        v6 = &Pop3PixFormat8;
        break;
    case 8u:
        v5 = (char *)&a1->SurfaceArea.mFormat;
        v6 = &Pop3PixFormat8;
        break;
    case 0x10u:
        v5 = (char *)&a1->SurfaceArea.mFormat;
        v6 = &Pop3PixFormat16;
        break;
    case 0x18u:
        v5 = (char *)&a1->SurfaceArea.mFormat;
        v6 = &Pop3PixFormat24;
        break;
    case 0x20u:
        v5 = (char *)&a1->SurfaceArea.mFormat;
        v6 = &Pop3PixFormat32;
        break;
    }
    memcpy(v5, v6, 0x18u);
    memset(&v15, 0, 0x20u);
    v7 = a1->lpSurface;
    v15 = 32;
    if (!v7->GetPixelFormat(reinterpret_cast<LPDDPIXELFORMAT>(&v15)))
    {
        v8 = v19;
        v9 = v20;
        *(DWORD *)v5 = v18;
        v10 = v21;
        a1->SurfaceArea.mFormat.Green = v8;
        v11 = v17;
        a1->SurfaceArea.mFormat.Blue = v9;
        a1->SurfaceArea.mFormat.Alpha = v10;
        a1->SurfaceArea.mFormat.Depth = v11;
        a1->SurfaceArea.mFormat.bPalette = v16 & 0x20;
    }
    v12 = a1->lpSurface;
    v22 = 108;
    v23 = 14;
    v12->GetSurfaceDesc(reinterpret_cast<LPDDSURFACEDESC>(&v22));
    v13 = v25;
    v14 = v24;
    a1->SurfaceArea.mPitch = v26;
    a1->SurfaceArea.mSize.Width = v13;
    a1->SurfaceArea.mSize.Height = v14;
    a1->CreationFlags = a5;
}

inline int __cdecl sub_BE1E90(GUID *lpGUID, LPDIRECTDRAW *lplpDD)
{
    int result; // eax@2

    if (DirectDrawCreate(lpGUID, lplpDD, 0))
    {
        result = -1;
    }
    else
    {
       // if (dword_FF11D0)
        //    Pop3App::setHwnd(dword_FF11D0);
        result = 0;
    }
    return result;
}

inline signed int __cdecl LbScreen_SetMode2(unsigned int a1, unsigned int a2, unsigned int a3, unsigned __int32 a4, const struct TbPalette *a5)
{
    DDSURFACEDESC surDesc = {};

    // Create the primary surface

    /*
        ddsd.dwSize = sizeof(ddsd);   
        ddsd.dwFlags = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
        ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_FLIP | DDSCAPS_COMPLEX;
        ddsd.dwBackBufferCount = 1;
    */

    surDesc.dwSize = 108;
    surDesc.dwFlags = 1;
    surDesc.dwBackBufferCount = 512;

    auto v5 = a4;
    if (v5 & 8)
    {
        surDesc.dwFlags = 33;
        surDesc.dwBackBufferCount = 536;
        surDesc.lPitch = 1;
    }
    if (a4 & 8)
        surDesc.dwBackBufferCount |= 0x2000u;

    if (a4 & 4)
        v5 = a4 | 2;
    if (v5 & 0x10)
    {
        if (v5 & 2)
            return -1;
        v5 |= 1u;
    }
    sub_BE1E90((512 & 0x200000) /* Fancy way of saying nullptr */, &_lbpDirectDraw);
    LbSurface_RegisterDirectDraw(_lbpDirectDraw);
    auto hWnd = static_cast<HWND>(Pop3App::getHwnd());
    ShowWindow(hWnd, 5);
    _LbWindowActive = 1;
    _LbScreenActive = 1;
    _lbpDirectDraw->SetCooperativeLevel(hWnd, 0);
    SetCursor(nullptr);
    _lbpDirectDraw->SetDisplayMode(a1, a2, a3);
    memcpy(&_lbGlobalPalette, a5, 0x400u);
    _lbpDirectDraw->CreatePalette(68, reinterpret_cast<LPPALETTEENTRY>(&_lbGlobalPalette), &_lbpPalette, nullptr);
    _lbpDirectDraw->CreateSurface(&surDesc, &_lbFrontSurface.lpSurface, nullptr);
    _lbFrontSurface.lpSurface->GetSurfaceDesc(&surDesc);
    
    surDesc.dwFlags = 7;
    surDesc.dwHeight = a2;
    surDesc.dwWidth = a1;
    surDesc.dwBackBufferCount = 2112;

    _lbpDirectDraw->CreateSurface(&surDesc, &_lbBackSurface.lpSurface, nullptr);
    _lbFrontSurface.lpSurface->SetPalette(_lbpPalette);
    _InitialiseSurface2(&_lbFrontSurface, a1, a2, a3, v5);
    _InitialiseSurface2(&_lbBackSurface, a1, a2, a3, v5);
    return 0;
}
#endif

#endif // ifndef _LBSRCN_H

