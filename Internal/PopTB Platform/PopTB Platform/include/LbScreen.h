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

//***************************************************************************
// Graphics system
//***************************************************************************
void	LbScreen_Terminate();


//***************************************************************************
//LbScreen_WindowsProcessing
//***************************************************************************
void	LbScreen_WindowsProcessing();
BOOL	LbScreen_IsActive();

struct _GUID;
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

#endif // ifndef _LBSRCN_H

