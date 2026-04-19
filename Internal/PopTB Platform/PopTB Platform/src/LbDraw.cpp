#include <LbDraw.h>
#include <LbRect.h>
#include <LbSurface.h>
#include <cstdio>

UBYTE		*_lbCurrentGhostTable;
RenderBase*	_lbpRenderBase;
ULONG		_lbCurrentDrawFlags;

Pop3RenderArea	*_lbpCurrentRA;
Pop3Rect		_lbCurrentViewPort;
Pop3Rect		_lbCurrentClipRect;
UBYTE		*_lbCurrentOriginPtr;
UBYTE		*_lbpCurrentFadeTable;
UBYTE		*_lbpCurrentFadeRow;
Pop3PixelCaps	_lbCurrentPixelCaps;

// One instance of each render context (as declared in LbRender.h)
RenderContext8	_lbRenderContext8;
RenderContext16	_lbRenderContext16;
RenderContext24	_lbRenderContext24;
RenderContext32	_lbRenderContext32;


// Checks that some or all of the bound rectangle is visible within the current clip window.
BOOL LbDraw_IsVisible(const Pop3Rect &boundrect)
{
    Pop3Rect norm = boundrect;
    LbRect_Normalise(&norm);
    return LbRect_Intersects(&_lbCurrentClipRect, &norm);
}


//***************************************************************************
//	LbDraw_Pixel:
//	Attributes		: CLIPPED
//	Flags Tested	: GLASS,INVERT_GLASS, FADE
//***************************************************************************
void LbDraw_Pixel(SINT x, SINT y, TbColour Colour)
{
    ASSERTMSG(_lbpCurrentRA, _LBSTR("LbDraw_Pixel: No current render area"));
    _lbpRenderBase->LbDraw_Pixel(x, y, Colour);
}



//***************************************************************************
// LbDraw_HorizontalLine: Draw a horizontal line to the current surface.
//	Will draw left to right unless the length is negative.
//	Attributes		: CLIPPED, INCLUSIVE
//	Flags Tested	: GLASS, INVERT_GLASS, FADE
//***************************************************************************
void LbDraw_HorizontalLine(SINT x, SINT y, SINT Length, TbColour Colour)
{
    ASSERTMSG(_lbpCurrentRA, _LBSTR("LbDraw_HorizontalLine: No current render area"));
    Pop3Rect bounds(x, y, x + Length, y + 1);
    if (LbDraw_IsVisible(bounds))
        _lbpRenderBase->LbDraw_HorizontalLine(x, y, Length, Colour);
}



//***************************************************************************
// LbDraw_VerticalLine: Draw a vertical line to the current surface.
//	Will draw downwards unless the height is negative.
//	Attributes		: CLIPPED, INCLUSIVE
//	Flags Tested	: GLASS, INVERT_GLASS, FADE
//***************************************************************************
void LbDraw_VerticalLine(SINT x, SINT y, SINT Height, TbColour Colour)
{
    ASSERTMSG(_lbpCurrentRA, _LBSTR("LbDraw_VerticalLine: No current render area"));
    Pop3Rect bounds(x, y, x + 1, y + Height);
    if (LbDraw_IsVisible(bounds))
        _lbpRenderBase->LbDraw_VerticalLine(x, y, Height, Colour);
}



//***************************************************************************
// LbDraw_Line: Draw a line.
//	Attributes		: CLIPPED, INCLUSIVE
//	Flags Tested	: GLASS,INVERT_GLASS, FADE
//***************************************************************************
void LbDraw_Line(SINT x1, SINT y1, SINT x2, SINT y2, TbColour Colour)
{
    ASSERTMSG(_lbpCurrentRA, _LBSTR("LbDraw_Line: No current render area"));
    Pop3Rect bounds(x1, y1, x2, y2);
    if (LbDraw_IsVisible(bounds))
        _lbpRenderBase->LbDraw_Line(x1, y1, x2, y2, Colour);
}



//***************************************************************************
// LbDraw_Rectangle: Draw a rectangle on the current surface.
//	Attributes		: CLIPPED, EXCLUSIVE
//	Flags Tested	: OUTLINE,GLASS,INVERT_GLASS, FADE
//***************************************************************************
void LbDraw_Rectangle(const Pop3Rect *pRect, TbColour Colour)
{
    if (pRect)
        LbDraw_Rectangle(*pRect, Colour);
    else
        LbDraw_Rectangle(_lbCurrentClipRect, Colour);
}

void LbDraw_Rectangle(const Pop3Rect &Rect, TbColour Colour)
{
    ASSERTMSG(_lbpCurrentRA, _LBSTR("LbDraw_Rectangle: No current render area"));
    if (LbDraw_IsVisible(Rect))
        _lbpRenderBase->LbDraw_Rectangle(Rect, Colour);
}



//***************************************************************************
// Support rectangle functions. The passed rect must be normalised
// (i.e. Left <= Right, Top <= Bottom) and not NULL
// Attributes	: CLIPPED, EXCLUSIVE
//***************************************************************************
//***************************************************************************
// LbDraw_RectangleOutline:
// Flags Tested	: GLASS,INVERT_GLASS, FADE
//***************************************************************************
void LbDraw_RectangleOutline(const Pop3Rect *pRect, TbColour Colour)
{
    if (pRect)
        LbDraw_RectangleOutline(*pRect, Colour);
    else
        LbDraw_RectangleOutline(_lbCurrentClipRect, Colour);
}

void LbDraw_RectangleOutline(const Pop3Rect &pRect, TbColour Colour)
{
    ASSERTMSG(_lbpCurrentRA, _LBSTR("LbDraw_RectangleOutline: No current render area"));
    if (LbDraw_IsVisible(pRect))
        _lbpRenderBase->LbDraw_RectangleOutline(pRect, Colour);
}



//***************************************************************************
// LbDraw_RectangleSolid:
// Flags Tested	: GLASS,INVERT_GLASS, FADE
//***************************************************************************
void LbDraw_RectangleFilled(const Pop3Rect *pRect, TbColour Colour)
{
    if (pRect)
        LbDraw_RectangleFilled(*pRect, Colour);
    else
        LbDraw_RectangleFilled(_lbCurrentClipRect, Colour);
}

void LbDraw_RectangleFilled(const Pop3Rect &pRect, TbColour Colour)
{
    ASSERTMSG(_lbpCurrentRA, _LBSTR("LbDraw_RectangleFilled: No current render area"));
    if (LbDraw_IsVisible(pRect))
        _lbpRenderBase->LbDraw_RectangleFilled(pRect, Colour);
}

//***************************************************************************
// LbDraw_Circle: Draw a circle.
// Attributes: CLIPPED, INCLUSIVE
// Flags Tested	: OUTLINE, GLASS,INVERT_GLASS, FADE
//***************************************************************************
void LbDraw_Circle(SINT x, SINT y, UINT Radius, TbColour Colour)
{
    ASSERTMSG(_lbpCurrentRA, _LBSTR("LbDraw_Circle: No current render area"));
    ASSERTMSG(Radius < 0x7FFFFFFF, _LBSTR("LbDraw_Circle: Radius too large"));
    Pop3Rect bounds((SINT)(x - Radius), (SINT)(y - Radius), (SINT)(x + Radius), (SINT)(y + Radius));
    if (LbDraw_IsVisible(bounds))
        _lbpRenderBase->LbDraw_Circle(x, y, Radius, Colour);
}


//***************************************************************************
// LbDraw_CircleOutline: Draw an outline of a circle.
// Attributes: CLIPPED, INCLUSIVE
// Flags Tested	: GLASS,INVERT_GLASS, FADE
//***************************************************************************
void LbDraw_CircleOutline(SINT x, SINT y, UINT Radius, TbColour Colour)
{
    ASSERTMSG(_lbpCurrentRA, _LBSTR("LbDraw_CircleOutline: No current render area"));
    Pop3Rect bounds((SINT)(x - Radius), (SINT)(y - Radius), (SINT)(x + Radius), (SINT)(y + Radius));
    if (LbDraw_IsVisible(bounds))
        _lbpRenderBase->LbDraw_CircleOutline(x, y, Radius, Colour);
}


//***************************************************************************
// LbDraw_CircleFilled: Draw a filled circle.
// Attributes: CLIPPED, INCLUSIVE
// Flags Tested	: GLASS,INVERT_GLASS, FADE
//***************************************************************************
void LbDraw_CircleFilled(SINT x, SINT y, UINT Radius, TbColour Colour)
{
    ASSERTMSG(_lbpCurrentRA, _LBSTR("LbDraw_CircleFilled: No current render area"));
    Pop3Rect bounds((SINT)(x - Radius), (SINT)(y - Radius), (SINT)(x + Radius), (SINT)(y + Radius));
    if (LbDraw_IsVisible(bounds))
        _lbpRenderBase->LbDraw_CircleFilled(x, y, Radius, Colour);
}

//***************************************************************************
// LbDraw_Triangle: Draw a triangle.
// Attributes	: CLIPPED, INCLUSIVE
// Flags Tested	: GLASS, INVERT_GLASS, FADE, OUTLINE
//***************************************************************************
void LbDraw_Triangle(SINT x1, SINT y1, SINT x2, SINT y2, SINT x3, SINT y3, TbColour Colour)
{
    ASSERTMSG(_lbpCurrentRA, _LBSTR("LbDraw_Triangle: No current render area"));
    SINT minX = x1; if (x2 < minX) minX = x2; if (x3 < minX) minX = x3;
    SINT minY = y1; if (y2 < minY) minY = y2; if (y3 < minY) minY = y3;
    SINT maxX = x1; if (x2 > maxX) maxX = x2; if (x3 > maxX) maxX = x3;
    SINT maxY = y1; if (y2 > maxY) maxY = y2; if (y3 > maxY) maxY = y3;
    Pop3Rect bounds(minX, minY, maxX, maxY);
    if (LbDraw_IsVisible(bounds))
        _lbpRenderBase->LbDraw_Triangle(x1, y1, x2, y2, x3, y3, Colour);
}



//***************************************************************************
// LbDrawPalette: Draw the palette as a 16*16 block of individual colours.
// Each colour is draw as a rectangle with the size (xsize*ysize). If
// display_numbers is non-zero then the palette index is displayed on top of
// the colours rectangle in colour 0.
// Attributes	: CLIPPED
// Flags Tested	: NONE
//***************************************************************************
void LbDraw_Palette(SINT x, SINT y, UINT CellWidth, ULONG CellHeight, BOOL DrawNumbers)
{
    ASSERTMSG(_lbpCurrentRA, _LBSTR("LbDraw_Palette: No current render area"));
    ASSERTMSG(CellWidth, _LBSTR("LbDraw_Palette: CellWidth is zero"));
    ASSERTMSG(CellHeight, _LBSTR("LbDraw_Palette: CellHeight is zero"));

    ULONG savedFlags = _lbCurrentDrawFlags;
    _lbCurrentDrawFlags = 0;

    TbColour black(0, 0, 0);
    TbColour white(255, 255, 255);
    UBYTE index = 0;

    for (int row = 0; row < 16; row++)
    {
        SINT cx = x;
        for (int col = 0; col < 16; col++)
        {
            TbColour cellColour(index);
            Pop3Rect cellRect(cx, y, cx + CellWidth, y + CellHeight);
            LbDraw_RectangleFilled(&cellRect, cellColour);
            if (DrawNumbers)
            {
                char buf[4];
                sprintf(buf, "%d", index);
                if ((UINT)cellColour.Red + cellColour.Green + cellColour.Blue <= 255)
                    LbDraw_PropText(cx, y, buf, white);
                else
                    LbDraw_PropText(cx, y, buf, black);
            }
            cx += CellWidth;
            index++;
        }
        y += CellHeight;
    }

    _lbCurrentDrawFlags = savedFlags;
}


// Draws proportional text
void LbDraw_PropText(SINT x, SINT y, const TBCHAR *pText, TbColour colour)
{
    ASSERTMSG(_lbpCurrentRA, _LBSTR("LbDraw_PropText: No current render area"));
    _lbpRenderBase->LbDraw_PropText(x, y, pText, colour);
}

void LbDraw_UnicodePropText(SINT x, SINT y, const UNICHAR *pText, TbColour colour)
{
    ASSERTMSG(_lbpCurrentRA, _LBSTR("LbDraw_UnicodePropText: No current render area"));
    _lbpRenderBase->LbDraw_UnicodePropText(x, y, pText, colour);
}


// Sets the clip window
void LbDraw_SetClipRect(const Pop3Rect *clipwindow)
{
    ASSERTMSG(_lbpCurrentRA, _LBSTR("LbDraw_SetClipRect: No current render area"));

    // _lbCurrentClipRect is stored in VIEWPORT-LOCAL coordinates (that is the
    // coord system the renderers use).  The valid drawing area in those coords
    // is the viewport clamped against the render-area bounds.
    Pop3Point vpPos  = _lbCurrentViewPort.GetPosition();
    Pop3Size  vpSize = _lbCurrentViewPort.GetSize();
    Pop3Rect  raRect(Pop3Rect(_lbpCurrentRA->mSize));

    // Compute the visible portion of the viewport in RA-absolute coords
    Pop3Rect visibleInRa = _lbCurrentViewPort;
    if (!LbRect_Intersection(&visibleInRa, &raRect))
    {
        LbRect_Empty(&_lbCurrentClipRect);
        return;
    }

    if (clipwindow)
    {
        // Caller-supplied clip rect is in RA-absolute coords
        Pop3Rect requested = *clipwindow;
        LbRect_Normalise(&requested);
        if (!LbRect_Intersection(&visibleInRa, &requested))
        {
            LbRect_Empty(&_lbCurrentClipRect);
            return;
        }
    }

    // Convert RA-absolute -> viewport-local by subtracting viewport origin
    _lbCurrentClipRect = visibleInRa;
    LbRect_Offset(&_lbCurrentClipRect, -vpPos.X, -vpPos.Y);
}

// Sets the viewport
void LbDraw_SetViewPort(const Pop3Rect *viewport)
{
    ASSERTMSG(_lbpCurrentRA, _LBSTR("LbDraw_SetViewPort: No current render area"));

    if (viewport)
    {
        _lbCurrentViewPort = *viewport;
    }
    else
    {
        _lbCurrentViewPort = Pop3Rect(_lbpCurrentRA->mSize);
    }
    LbRect_Normalise(&_lbCurrentViewPort);

    _lbCurrentOriginPtr = _lbpCurrentRA->mpData
        + _lbCurrentViewPort.Top * _lbpCurrentRA->mPitch
        + _lbCurrentViewPort.Left * (_lbpCurrentRA->mFormat.Depth / 8);

    LbDraw_SetClipRect(NULL);
}

// Sets Clip window back to full viewport.
void LbDraw_ReleaseClipRect(void)
{
    ASSERTMSG(_lbpCurrentRA, _LBSTR("LbDraw_ReleaseClipRect: No current render area"));
    LbDraw_SetClipRect(NULL);
}

// Set the view port back to full surface
void LbDraw_ReleaseViewPort(void)
{
    ASSERTMSG(_lbpCurrentRA, _LBSTR("LbDraw_ReleaseViewPort: No current render area"));
    LbDraw_SetViewPort(NULL);
}

Pop3Rect * LbDraw_GetClipRect()
{
    return &_lbCurrentClipRect;
}

Pop3Rect * LbDraw_GetViewPort()
{
    return &_lbCurrentViewPort;
}

void LbDraw_ClearClipRect(TbColour Colour)
{
    LbDraw_RectangleFilled(_lbCurrentClipRect, Colour);
}

// Sets the current drawing destination
void LbDraw_SetCurrent(TbSurface *surface)
{
    if (surface)
    {
#if defined(_LB_DEBUG) && defined(_LB_LIBRARY)
        ASSERTMSG(surface->bExternal || LbList_NodeExists(&_lbSurfaceList, &surface->ListNode),
            _LBSTR("LbDraw_SetCurrent(TbSurface*): Surface not in surface list"));
#endif

        _lbpCurrentRA       = &surface->SurfaceArea;
        _lbCurrentOriginPtr = surface->SurfaceArea.mpData;
        _lbCurrentDrawFlags = surface->DrawFlags;
        _lbCurrentGhostTable = surface->GhostTable;
        _lbpCurrentFadeTable = surface->FadeTable;
        _lbpCurrentFadeRow   = surface->FadeStepRow;

        LbDraw_SetViewPort(NULL);

        switch (surface->SurfaceArea.mFormat.Depth)
        {
        case 8:  _lbpRenderBase = &_lbRenderContext8;  break;
        case 16: _lbpRenderBase = &_lbRenderContext16; break;
        case 24: _lbpRenderBase = &_lbRenderContext24; break;
        case 32: _lbpRenderBase = &_lbRenderContext32; break;
        default:
            ASSERTMSG(FALSE, _LBSTR("LbDraw_SetCurrent(TbSurface*): Unknown bit depth"));
            _lbpRenderBase = &_lbRenderContext8;
            break;
        }

        _lbCurrentPixelCaps.Set(surface->SurfaceArea.mFormat);
    }
    else
    {
        _lbpCurrentRA = NULL;
    }
}

void LbDraw_SetCurrent(Pop3RenderArea *renderarea)
{
    if (renderarea)
    {
        _lbpCurrentRA       = renderarea;
        _lbCurrentOriginPtr = renderarea->mpData;

        LbDraw_SetViewPort(NULL);

        switch (renderarea->mFormat.Depth)
        {
        case 8:  _lbpRenderBase = &_lbRenderContext8;  break;
        case 16: _lbpRenderBase = &_lbRenderContext16; break;
        case 24: _lbpRenderBase = &_lbRenderContext24; break;
        case 32: _lbpRenderBase = &_lbRenderContext32; break;
        default:
            ASSERTMSG(FALSE, _LBSTR("LbDraw_SetCurrent(Pop3RenderArea*): Unknown bit depth"));
            _lbpRenderBase = &_lbRenderContext8;
            break;
        }

        _lbCurrentPixelCaps.Set(renderarea->mFormat);
    }
    else
    {
        _lbpCurrentRA = NULL;
    }
}

