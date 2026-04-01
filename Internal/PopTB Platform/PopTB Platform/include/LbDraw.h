//***************************************************************************
// LbDraw.h : Basic drawing functions
//***************************************************************************
#ifndef _LBDRAW_H
#define _LBDRAW_H

#include    "Pop3Types.h"
#include	<lbError.h>
#include	<lbMacro.h>

#include	<lbColour.h>
#include	<LbPal.h>
#include	<LbText.h>
#include	"LbSpriteRender.h"
#include	<LbRender.h>
#include    <Pop3RenderArea.h>
#include    "Pop3Debug.h"

#include <_drawdef.h>


// The new globals.
extern Pop3RenderArea	*_lbpCurrentRA;			// Current render area to draw to.
extern Pop3Rect		_lbCurrentViewPort;		// Viewport (relative to surface)
extern Pop3Rect		_lbCurrentClipRect;		// Clip window (relative to origin)
extern UBYTE		*_lbCurrentOriginPtr;	// Origin Pointer
extern RenderBase*	_lbpRenderBase;			// The renderbase for the current renderarea
extern ULONG		_lbCurrentDrawFlags;	// Draw flags
extern UBYTE		*_lbCurrentGhostTable;
extern UBYTE		*_lbpCurrentFadeTable;
extern UBYTE		*_lbpCurrentFadeRow;
extern Pop3PixelCaps	_lbCurrentPixelCaps;


//***************************************************************************
// DrawFlags
//***************************************************************************
// These flags may be combined
const ULONG	LB_DRAW_FLAG_XFLIP				= 0x00000001;
const ULONG	LB_DRAW_FLAG_YFLIP				= 0x00000002;
const ULONG	LB_DRAW_FLAG_OUTLINE			= 0x00000004;

// These flags are mutually exclusive, listed in priority, first is highest
const ULONG	LB_DRAW_FLAG_GLASS				= 0x00000008;
const ULONG	LB_DRAW_FLAG_INVERT_GLASS		= 0x00000010;
const ULONG	LB_DRAW_FLAG_FADE				= 0x00000020;


//***************************************************************************
//	LbDraw_GetAddressOf:
//***************************************************************************
inline UBYTE *LbDraw_GetAddressOf(SINT x, SINT y)
{
    return (_CURRENT_ORIGIN_PTR+x+y*_CURRENT_RENDER_AREA->mPitch);
}

//***************************************************************************
//	LbDraw_GetCurrent:
//***************************************************************************
inline Pop3RenderArea *LbDraw_GetCurrent()
{
    return _CURRENT_RENDER_AREA;
}



//***************************************************************************
//	LbDraw_Pixel:
//	Attributes		: CLIPPED
//	Flags Tested	: GLASS,INVERT_GLASS, FADE
//***************************************************************************
void LbDraw_Pixel(SINT x, SINT y, TbColour Colour);



//***************************************************************************
// LbDraw_HorizontalLine: Draw a horizontal line to the current surface.
//	Will draw left to right unless the length is negative.
//	Attributes		: CLIPPED, INCLUSIVE
//	Flags Tested	: GLASS, INVERT_GLASS, FADE
//***************************************************************************
void LbDraw_HorizontalLine(SINT x, SINT y, SINT Length, TbColour Colour);



//***************************************************************************
// LbDraw_VerticalLine: Draw a vertical line to the current surface.
//	Will draw downwards unless the height is negative.
//	Attributes		: CLIPPED, INCLUSIVE
//	Flags Tested	: GLASS, INVERT_GLASS, FADE
//***************************************************************************
void LbDraw_VerticalLine(SINT x, SINT y, SINT Height, TbColour Colour);



//***************************************************************************
// LbDraw_Line: Draw a line.
//	Attributes		: CLIPPED, INCLUSIVE
//	Flags Tested	: GLASS,INVERT_GLASS, FADE
//***************************************************************************
void LbDraw_Line(SINT x1, SINT y1, SINT x2, SINT y2, TbColour Colour);



//***************************************************************************
// LbDraw_Rectangle: Draw a rectangle on the current surface.
//	Attributes		: CLIPPED, EXCLUSIVE
//	Flags Tested	: OUTLINE,GLASS,INVERT_GLASS, FADE
//***************************************************************************
void LbDraw_Rectangle(const Pop3Rect *pRect, TbColour Colour);
void LbDraw_Rectangle(const Pop3Rect &Rect, TbColour Colour);



//***************************************************************************
// Support rectangle functions. The passed rect must be normalised
// (i.e. Left <= Right, Top <= Bottom) and not NULL
// Attributes	: CLIPPED, EXCLUSIVE
//***************************************************************************
//***************************************************************************
// LbDraw_RectangleOutline:
// Flags Tested	: GLASS,INVERT_GLASS, FADE
//***************************************************************************
void LbDraw_RectangleOutline(const Pop3Rect *pRect, TbColour Colour);
void LbDraw_RectangleOutline(const Pop3Rect &pRect, TbColour Colour);



//***************************************************************************
// LbDraw_RectangleSolid:
// Flags Tested	: GLASS,INVERT_GLASS, FADE
//***************************************************************************
void LbDraw_RectangleFilled(const Pop3Rect *pRect, TbColour Colour);
void LbDraw_RectangleFilled(const Pop3Rect &pRect, TbColour Colour);

//***************************************************************************
// LbDraw_Circle: Draw a circle.
// Attributes: CLIPPED, INCLUSIVE
// Flags Tested	: OUTLINE, GLASS,INVERT_GLASS, FADE
//***************************************************************************
void	LbDraw_Circle(SINT x, SINT y, UINT Radius, TbColour Colour);


//***************************************************************************
// LbDraw_CircleOutline: Draw an outline of a circle.
// Attributes: CLIPPED, INCLUSIVE
// Flags Tested	: GLASS,INVERT_GLASS, FADE
//***************************************************************************
void	LbDraw_CircleOutline(SINT x, SINT y, UINT Radius, TbColour Colour);


//***************************************************************************
// LbDraw_CircleFilled: Draw a filled circle.
// Attributes: CLIPPED, INCLUSIVE
// Flags Tested	: GLASS,INVERT_GLASS, FADE
//***************************************************************************
void	LbDraw_CircleFilled(SINT x, SINT y, UINT Radius, TbColour Colour);


//***************************************************************************
// LbDraw_Triangle: Draw a triangle.
// Attributes	: CLIPPED, INCLUSIVE
// Flags Tested	: GLASS, INVERT_GLASS, FADE, OUTLINE
//***************************************************************************
void	LbDraw_Triangle(SINT x1, SINT y1, SINT x2, SINT y2, SINT x3, SINT y3, TbColour Colour);



//***************************************************************************
// LbDrawPalette: Draw the palette as a 16*16 block of individual colours.
// Each colour is draw as a rectangle with the size (xsize*ysize). If
// display_numbers is non-zero then the palette index is displayed on top of
// the colours rectangle in colour 0.
// Attributes	: CLIPPED
// Flags Tested	: NONE
//***************************************************************************
void	LbDraw_Palette(SINT x, SINT y, UINT CellWidth, ULONG CellHeight, BOOL DrawNumbers);




//***************************************************************************
//	Clear the current clip rect
//***************************************************************************
void	LbDraw_ClearClipRect(TbColour Colour);


// Draws proportional text
void LbDraw_PropText(SINT x, SINT y, const TBCHAR *pText, TbColour colour);
void LbDraw_UnicodePropText(SINT x, SINT y, const UNICHAR *pText, TbColour colour);


// Sets the clip window
void LbDraw_SetClipRect(const Pop3Rect *clipwindow);

// Sets the viewport
void LbDraw_SetViewPort(const Pop3Rect *viewport);

// Sets Clip window back to full viewport.
void LbDraw_ReleaseClipRect();

// Set the view port back to full surface
void LbDraw_ReleaseViewPort();

// Gets the current clip window
Pop3Rect * LbDraw_GetClipRect();

// Gets the current view port
Pop3Rect * LbDraw_GetViewPort();

// Sets the current drawing destination
void LbDraw_SetCurrent(TbSurface *surface);
void LbDraw_SetCurrent(Pop3RenderArea *renderarea);


// Checks that some or all of the bound rectangle is visible within the current clip window.
BOOL	LbDraw_IsVisible(const Pop3Rect &boundrect);

// Gets the current draw flags
inline ULONG LbDraw_GetFlags()
{
    return _CURRENT_DRAW_FLAGS;
}

// Sets the current draw flags
inline void	LbDraw_SetFlags(ULONG draw_flags)
{
    _CURRENT_DRAW_FLAGS = draw_flags;
}

// Sets the specifed draw flags according to mask
inline void LbDraw_SetFlagsOn(ULONG fMask)
{
    _CURRENT_DRAW_FLAGS |= fMask;
}

// Clears the specifed draw flags according to mask
inline void	LbDraw_SetFlagsOff(ULONG fMask)
{
    _CURRENT_DRAW_FLAGS &= ~fMask;
}

// Sets the current fade table
inline void	LbDraw_SetFadeTable(UBYTE *fade_table)
{
    _CURRENT_FADE_TABLE = fade_table;
    _CURRENT_FADE_ROW = NULL;
}

// Gets the current "fade step"
inline ULONG	LbDraw_GetFadeStep()
{
    ASSERTMSG(_CURRENT_FADE_TABLE, _LBSTR("LbDraw_GetFadeStep no current fade table set"));
    ASSERTMSG(_CURRENT_FADE_ROW, _LBSTR("LbDraw_GetFadeStep : Invalid fade step"));

    return (((_CURRENT_FADE_TABLE - _CURRENT_FADE_ROW) >> 8));
}

// Sets the current "fade step"
inline void LbDraw_SetFadeStep(ULONG step)
{
    ASSERTMSG(_CURRENT_FADE_TABLE, _LBSTR("LbDraw_SetFadeRow : No fade table set"));

    _CURRENT_FADE_ROW = _CURRENT_FADE_TABLE + (step << 8);
}

// Gets the current ghost table
inline UBYTE *LbDraw_GetGhostTable()
{
    return (_CURRENT_GHOST_TABLE);
}

// Sets the current ghost table
inline void	LbDraw_SetGhostTable(UBYTE *ghost_table)
{
    _CURRENT_GHOST_TABLE = ghost_table;
}

// Registers all Library8 drawing/screen functions into Pop3Draw.
// Call once before LbScreen_Initialise().
void LbDraw_RegisterBackend();

#endif
