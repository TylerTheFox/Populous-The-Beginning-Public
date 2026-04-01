// LbRendef_impl.inl — method implementations for RENDERCLASS
// Included multiple times from LbRenderContexts.cpp with:
//   #define RENDERCLASS  RenderContext8 (or 16/24/32)
//   #define RENDER_BPP   1             (or 2/3/4)

// ---- One-time includes (guarded) ----------------------------------------
#ifndef LBRENDEF_IMPL_INL_ONCE
#define LBRENDEF_IMPL_INL_ONCE
#include <LbDraw.h>
#include <LbRect.h>
#include <LbText.h>
#include <cstring>
#include <cstdlib>
#endif // LBRENDEF_IMPL_INL_ONCE

// ---- Per-BPP pixel access macros (re-evaluated on every include) ---------
#undef _RC_STRIDE
#undef _RC_OFF
#undef _RC_COLVAL
#undef _RC_FADECOLVAL
#undef _RC_PUT
#undef _RC_GET
#undef _RC_GLASS
#undef _RC_IGLASS

#if RENDER_BPP == 1
#  define _RC_STRIDE         1
#  define _RC_OFF(x,y)       ((x) + (y)*(SINT)_lbpCurrentRA->mPitch)
#  define _RC_COLVAL(c)      ((ULONG)(c).Index)
#  define _RC_FADECOLVAL(c)  ((ULONG)TbColour((int)_lbpCurrentFadeRow[(c).Index]).Index)
#  define _RC_PUT(o,v)       (_lbCurrentOriginPtr[(o)] = (UBYTE)(v))
#  define _RC_GET(o)         ((ULONG)_lbCurrentOriginPtr[(o)])
#  define _RC_GLASS(o,v)     (_lbCurrentOriginPtr[(o)] = _lbCurrentGhostTable[256*(UBYTE)(v) + _lbCurrentOriginPtr[(o)]])
#  define _RC_IGLASS(o,v)    (_lbCurrentOriginPtr[(o)] = _lbCurrentGhostTable[256*_lbCurrentOriginPtr[(o)] + (UBYTE)(v)])
#elif RENDER_BPP == 2
#  define _RC_STRIDE         2
#  define _RC_OFF(x,y)       (2*(x) + (y)*(SINT)_lbpCurrentRA->mPitch)
#  define _RC_COLVAL(c)      ((ULONG)(USHORT)(c).GetFormattedValue(_lbCurrentPixelCaps))
#  define _RC_FADECOLVAL(c)  ((ULONG)(USHORT)TbColour((int)_lbpCurrentFadeRow[(c).Index]).GetFormattedValue(_lbCurrentPixelCaps))
#  define _RC_PUT(o,v)       (*(USHORT*)&_lbCurrentOriginPtr[(o)] = (USHORT)(v))
#  define _RC_GET(o)         ((ULONG)*(USHORT*)&_lbCurrentOriginPtr[(o)])
#  define _RC_GLASS(o,v)     _RC_PUT(o,v)
#  define _RC_IGLASS(o,v)    _RC_PUT(o,v)
#elif RENDER_BPP == 3
#  define _RC_STRIDE         3
#  define _RC_OFF(x,y)       (3*(x) + (y)*(SINT)_lbpCurrentRA->mPitch)
#  define _RC_COLVAL(c)      ((c).GetFormattedValue(_lbCurrentPixelCaps))
#  define _RC_FADECOLVAL(c)  (TbColour((int)_lbpCurrentFadeRow[(c).Index]).Packed)
#  define _RC_PUT(o,v)       do { *(USHORT*)&_lbCurrentOriginPtr[(o)] = (USHORT)(v); \
                                  _lbCurrentOriginPtr[(o)+2] = (UBYTE)((ULONG)(v) >> 16); } while(0)
#  define _RC_GET(o)         ((ULONG)*(USHORT*)&_lbCurrentOriginPtr[(o)] | ((ULONG)_lbCurrentOriginPtr[(o)+2] << 16))
#  define _RC_GLASS(o,v)     _RC_PUT(o,v)
#  define _RC_IGLASS(o,v)    _RC_PUT(o,v)
#elif RENDER_BPP == 4
#  define _RC_STRIDE         4
#  define _RC_OFF(x,y)       (4*(x) + (y)*(SINT)_lbpCurrentRA->mPitch)
#  define _RC_COLVAL(c)      ((c).Get32bitValue())
#  define _RC_FADECOLVAL(c)  (TbColour((int)_lbpCurrentFadeRow[(c).Index]).Get32bitValue())
#  define _RC_PUT(o,v)       (*(ULONG*)&_lbCurrentOriginPtr[(o)] = (ULONG)(v))
#  define _RC_GET(o)         (*(ULONG*)&_lbCurrentOriginPtr[(o)])
#  define _RC_GLASS(o,v)     _RC_PUT(o,v)
#  define _RC_IGLASS(o,v)    _RC_PUT(o,v)
#endif

//***************************************************************************
// LbDraw_Pixel
//***************************************************************************
void RENDERCLASS::LbDraw_Pixel(SINT x, SINT y, TbColour colour)
{
    if (x < _lbCurrentClipRect.Left || x >= _lbCurrentClipRect.Right ||
        y < _lbCurrentClipRect.Top  || y >= _lbCurrentClipRect.Bottom)
        return;

    SINT ofs = _RC_OFF(x, y);
    ULONG cv = _RC_COLVAL(colour);

    if (_lbCurrentDrawFlags & LB_DRAW_FLAG_GLASS)
        _RC_GLASS(ofs, cv);
    else if (_lbCurrentDrawFlags & LB_DRAW_FLAG_INVERT_GLASS)
        _RC_IGLASS(ofs, cv);
    else if (_lbCurrentDrawFlags & LB_DRAW_FLAG_FADE)
        _RC_PUT(ofs, _RC_FADECOLVAL(colour));
    else
        _RC_PUT(ofs, cv);
}

//***************************************************************************
// LbDraw_Rectangle
//***************************************************************************
void RENDERCLASS::LbDraw_Rectangle(const TbRect &rect, TbColour col)
{
    if (_lbCurrentDrawFlags & LB_DRAW_FLAG_OUTLINE)
        LbDraw_RectangleOutline(rect, col);
    else
        LbDraw_RectangleFilled(rect, col);
}

//***************************************************************************
// LbDraw_RectangleOutline
//***************************************************************************
void RENDERCLASS::LbDraw_RectangleOutline(const TbRect &rect, TbColour col)
{
    SINT w = LbRect_Width(&rect);
    SINT h = LbRect_Height(&rect);

    LbDraw_HorizontalLine(rect.Left,     rect.Top,      w - 1, col);
    LbDraw_VerticalLine  (rect.Right-1,  rect.Top,      h - 1, col);
    LbDraw_HorizontalLine(rect.Left + 1, rect.Bottom-1, w - 1, col);
    LbDraw_VerticalLine  (rect.Left,     rect.Top + 1,  h - 1, col);
}

//***************************************************************************
// LbDraw_RectangleFilled
//***************************************************************************
void RENDERCLASS::LbDraw_RectangleFilled(const TbRect &rect, TbColour colour)
{
    SINT pitch = (SINT)_lbpCurrentRA->mPitch;
    TbRect clipped = rect.Intersection(_lbCurrentClipRect);
    SINT w = LbRect_Width(&clipped);
    SINT h = LbRect_Height(&clipped);
    if (w <= 0 || h <= 0) return;

    ULONG cv = _RC_COLVAL(colour);

    if (_lbCurrentDrawFlags & LB_DRAW_FLAG_GLASS)
    {
        for (SINT row = 0; row < h; row++)
        {
            SINT ofs = _RC_OFF(clipped.Left, clipped.Top + row);
            for (SINT col = 0; col < w; col++, ofs += _RC_STRIDE)
                _RC_GLASS(ofs, cv);
        }
    }
    else if (_lbCurrentDrawFlags & LB_DRAW_FLAG_INVERT_GLASS)
    {
        for (SINT row = 0; row < h; row++)
        {
            SINT ofs = _RC_OFF(clipped.Left, clipped.Top + row);
            for (SINT col = 0; col < w; col++, ofs += _RC_STRIDE)
                _RC_IGLASS(ofs, cv);
        }
    }
    else
    {
        if (_lbCurrentDrawFlags & LB_DRAW_FLAG_FADE)
            cv = _RC_FADECOLVAL(colour);
#if RENDER_BPP == 1
        UBYTE *pDst = &_lbCurrentOriginPtr[_RC_OFF(clipped.Left, clipped.Top)];
        for (SINT row = 0; row < h; row++, pDst += pitch)
            memset(pDst, (UBYTE)cv, w);
#else
        for (SINT row = 0; row < h; row++)
        {
            SINT ofs = _RC_OFF(clipped.Left, clipped.Top + row);
            for (SINT col = 0; col < w; col++, ofs += _RC_STRIDE)
                _RC_PUT(ofs, cv);
        }
#endif
    }
}

//***************************************************************************
// LbDraw_Circle
//***************************************************************************
void RENDERCLASS::LbDraw_Circle(SINT x, SINT y, UINT radius, TbColour col)
{
    if (_lbCurrentDrawFlags & LB_DRAW_FLAG_OUTLINE)
        LbDraw_CircleOutline(x, y, radius, col);
    else
        LbDraw_CircleFilled(x, y, radius, col);
}

//***************************************************************************
// LbDraw_CircleOutline — Bresenham midpoint circle
//***************************************************************************
void RENDERCLASS::LbDraw_CircleOutline(SINT xpos, SINT ypos, UINT radius, TbColour colour)
{
    ULONG cv = _RC_COLVAL(colour);
    ULONG fv = (_lbCurrentDrawFlags & LB_DRAW_FLAG_FADE) ? _RC_FADECOLVAL(colour) : cv;
    UINT  flags = _lbCurrentDrawFlags;

    auto putpx = [&](SINT px, SINT py)
    {
        if (px < _lbCurrentClipRect.Left || px >= _lbCurrentClipRect.Right ||
            py < _lbCurrentClipRect.Top  || py >= _lbCurrentClipRect.Bottom)
            return;
        SINT ofs = _RC_OFF(px, py);
        if      (flags & LB_DRAW_FLAG_GLASS)        { _RC_GLASS(ofs, cv);  }
        else if (flags & LB_DRAW_FLAG_INVERT_GLASS) { _RC_IGLASS(ofs, cv); }
        else if (flags & LB_DRAW_FLAG_FADE)         { _RC_PUT(ofs, fv);    }
        else                                         { _RC_PUT(ofs, cv);    }
    };

    if (radius == 0)
    {
        putpx(xpos, ypos);
        return;
    }

    SINT cx = 0;
    SINT cy = (SINT)radius;
    SINT d  = 3 - 2 * (SINT)radius;

    while (cy >= cx)
    {
        putpx(xpos - cx, ypos - cy);
        putpx(xpos + cx, ypos - cy);
        putpx(xpos - cx, ypos + cy);
        putpx(xpos + cx, ypos + cy);
        putpx(xpos - cy, ypos - cx);
        putpx(xpos + cy, ypos - cx);
        putpx(xpos - cy, ypos + cx);
        putpx(xpos + cy, ypos + cx);

        if (d >= 0)
            d += 4 * (cx - cy--) + 10;
        else
            d += 4 * cx + 6;
        ++cx;
    }
}

//***************************************************************************
// LbDraw_CircleFilled — filled circle; each row drawn exactly once (GLASS-safe)
//***************************************************************************
void RENDERCLASS::LbDraw_CircleFilled(SINT xpos, SINT ypos, UINT radius, TbColour colour)
{
    if (radius == 0) { LbDraw_Pixel(xpos, ypos, colour); return; }

    SINT cx = 0;
    SINT cy = (SINT)radius;
    SINT d  = 3 - 2*(SINT)radius;
    SINT prevCx, prevCy;

    // Centre horizontal span
    LbDraw_HorizontalLine(xpos - cy, ypos, 2*cy + 1, colour);

    // First Bresenham step
    prevCx = cx;
    prevCy = cy;
    if (d >= 0)
    {
        LbDraw_HorizontalLine(xpos - prevCx, ypos - prevCy, 2*prevCx + 1, colour);
        LbDraw_HorizontalLine(xpos - prevCx, ypos + prevCy, 2*prevCx + 1, colour);
        d += 4*(prevCx - cy--) + 10;
    }
    else
    {
        d += 4*prevCx + 6;
    }
    prevCx = ++cx;
    prevCy = cy;

    while (cy > cx)
    {
        // Wide spans (equatorial octant)
        LbDraw_HorizontalLine(xpos - cy, ypos - cx, 2*cy + 1, colour);
        LbDraw_HorizontalLine(xpos - cy, ypos + cx, 2*cy + 1, colour);

        if (d >= 0)
        {
            // Narrow spans (polar octant) — drawn when cy decrements
            LbDraw_HorizontalLine(xpos - prevCx, ypos - prevCy, 2*prevCx + 1, colour);
            LbDraw_HorizontalLine(xpos - prevCx, ypos + prevCy, 2*prevCx + 1, colour);
            d += 4*(cx - cy--) + 10;
        }
        else
        {
            d += 4*cx + 6;
        }
        prevCx = ++cx;
        prevCy = cy;
    }

    // Diagonal (cx == cy): draw final pair
    if (cy == cx)
    {
        LbDraw_HorizontalLine(xpos - cy, ypos - cx, 2*cy + 1, colour);
        LbDraw_HorizontalLine(xpos - cy, ypos + cx, 2*cy + 1, colour);
    }
}

//***************************************************************************
// LbDraw_Line — Bresenham with Cohen-Sutherland clipping
//***************************************************************************
void RENDERCLASS::LbDraw_Line(SINT x1, SINT y1, SINT x2, SINT y2, TbColour colour)
{
    if (x2 == x1) { LbDraw_VerticalLine(x1, y1, y2 - y1, colour); return; }
    if (y2 == y1) { LbDraw_HorizontalLine(x1, y1, x2 - x1, colour); return; }

    // --- Clip X ---
    if (_lbCurrentClipRect.Left <= x1)
    {
        if (_lbCurrentClipRect.Right > x1)
        {
            if (_lbCurrentClipRect.Left <= x2)
            {
                if (_lbCurrentClipRect.Right <= x2)
                {
                    SINT clip = x2 - (_lbCurrentClipRect.Right - 1);
                    y2 -= clip * (y2 - y1) / (x2 - x1);
                    x2 = _lbCurrentClipRect.Right - 1;
                }
            }
            else
            {
                SINT clip = x2 - _lbCurrentClipRect.Left;
                y2 -= clip * (y2 - y1) / (x2 - x1);
                x2 = _lbCurrentClipRect.Left;
            }
        }
        else
        {
            if (_lbCurrentClipRect.Right <= x2) return;
            SINT clip = x1 - (_lbCurrentClipRect.Right - 1);
            y1 -= clip * (y1 - y2) / (x1 - x2);
            x1 = _lbCurrentClipRect.Right - 1;
            if (_lbCurrentClipRect.Left > x2)
            {
                SINT clip2 = x2 - _lbCurrentClipRect.Left;
                y2 -= clip2 * (y2 - y1) / (x2 - x1);
                x2 = _lbCurrentClipRect.Left;
            }
        }
    }
    else
    {
        if (_lbCurrentClipRect.Left > x2) return;
        SINT clip = x1 - _lbCurrentClipRect.Left;
        y1 -= clip * (y1 - y2) / (x1 - x2);
        x1 = _lbCurrentClipRect.Left;
        if (_lbCurrentClipRect.Right <= x2)
        {
            SINT clip2 = x2 - (_lbCurrentClipRect.Right - 1);
            y2 -= clip2 * (y2 - y1) / (x2 - x1);
            x2 = _lbCurrentClipRect.Right - 1;
        }
    }

    // --- Clip Y ---
    if (_lbCurrentClipRect.Top <= y1)
    {
        if (_lbCurrentClipRect.Bottom > y1)
        {
            if (_lbCurrentClipRect.Top <= y2)
            {
                if (_lbCurrentClipRect.Bottom <= y2)
                {
                    SINT clip = y2 - (_lbCurrentClipRect.Bottom - 1);
                    x2 -= clip * (x2 - x1) / (y2 - y1);
                    y2 = _lbCurrentClipRect.Bottom - 1;
                }
            }
            else
            {
                SINT clip = y2 - _lbCurrentClipRect.Top;
                x2 -= clip * (x2 - x1) / (y2 - y1);
                y2 = _lbCurrentClipRect.Top;
            }
        }
        else
        {
            if (_lbCurrentClipRect.Bottom <= y2) return;
            SINT clip = y1 - (_lbCurrentClipRect.Bottom - 1);
            x1 -= clip * (x1 - x2) / (y1 - y2);
            y1 = _lbCurrentClipRect.Bottom - 1;
            if (_lbCurrentClipRect.Top > y2)
            {
                SINT clip2 = y2 - _lbCurrentClipRect.Top;
                x2 -= clip2 * (x2 - x1) / (y2 - y1);
                y2 = _lbCurrentClipRect.Top;
            }
        }
    }
    else
    {
        if (_lbCurrentClipRect.Top > y2) return;
        SINT clip = y1 - _lbCurrentClipRect.Top;
        x1 -= clip * (x1 - x2) / (y1 - y2);
        y1 = _lbCurrentClipRect.Top;
        if (_lbCurrentClipRect.Bottom <= y2)
        {
            SINT clip2 = y2 - (_lbCurrentClipRect.Bottom - 1);
            x2 -= clip2 * (x2 - x1) / (y2 - y1);
            y2 = _lbCurrentClipRect.Bottom - 1;
        }
    }

    // --- Bresenham ---
    SINT dx   = x2 - x1;
    SINT dy   = y2 - y1;
    SINT adx  = 2 * abs(dx);
    SINT ady  = 2 * abs(dy);
    SINT sx   = (dx <= 0) ? -_RC_STRIDE : _RC_STRIDE;   // byte step in X
    SINT spx  = (dx <= 0) ? -1 : 1;                     // pixel step in X
    SINT sy   = (dy <= 0) ? -1 : 1;
    SINT spitch = sy * (SINT)_lbpCurrentRA->mPitch;
    SINT ofs  = _RC_OFF(x1, y1);
    SINT cx   = x1;
    SINT cy   = y1;
    ULONG cv  = _RC_COLVAL(colour);
    ULONG fv  = (_lbCurrentDrawFlags & LB_DRAW_FLAG_FADE) ? _RC_FADECOLVAL(colour) : cv;

    if (_lbCurrentDrawFlags & LB_DRAW_FLAG_GLASS)
    {
        if (ady >= adx)
        {
            for (SINT err = adx - (ady >> 1); ; err += adx)
            {
                _RC_GLASS(ofs, cv);
                if (y2 == cy) break;
                if (err >= 0) { ofs += sx; err -= ady; }
                cy += sy; ofs += spitch;
            }
        }
        else
        {
            for (SINT err = ady - (adx >> 1); ; err += ady)
            {
                _RC_GLASS(ofs, cv);
                if (x2 == cx) break;
                if (err >= 0) { ofs += spitch; err -= adx; }
                cx += spx; ofs += sx;
            }
        }
    }
    else if (_lbCurrentDrawFlags & LB_DRAW_FLAG_INVERT_GLASS)
    {
        if (ady >= adx)
        {
            for (SINT err = adx - (ady >> 1); ; err += adx)
            {
                _RC_IGLASS(ofs, cv);
                if (y2 == cy) break;
                if (err >= 0) { ofs += sx; err -= ady; }
                cy += sy; ofs += spitch;
            }
        }
        else
        {
            for (SINT err = ady - (adx >> 1); ; err += ady)
            {
                _RC_IGLASS(ofs, cv);
                if (x2 == cx) break;
                if (err >= 0) { ofs += spitch; err -= adx; }
                cx += spx; ofs += sx;
            }
        }
    }
    else
    {
        ULONG wv = (_lbCurrentDrawFlags & LB_DRAW_FLAG_FADE) ? fv : cv;
        if (ady >= adx)
        {
            for (SINT err = adx - (ady >> 1); ; err += adx)
            {
                _RC_PUT(ofs, wv);
                if (y2 == cy) break;
                if (err >= 0) { ofs += sx; err -= ady; }
                cy += sy; ofs += spitch;
            }
        }
        else
        {
            for (SINT err = ady - (adx >> 1); ; err += ady)
            {
                _RC_PUT(ofs, wv);
                if (x2 == cx) break;
                if (err >= 0) { ofs += spitch; err -= adx; }
                cx += spx; ofs += sx;
            }
        }
    }
}

//***************************************************************************
// LbDraw_HorizontalLine
//***************************************************************************
void RENDERCLASS::LbDraw_HorizontalLine(SINT x, SINT y, SINT length, TbColour colour)
{
    if (length < 0) { x += length; length = -length; }
    if (length == 0) return;

    if (x + length <= _lbCurrentClipRect.Left) return;
    if (x >= _lbCurrentClipRect.Right) return;
    if (y < _lbCurrentClipRect.Top) return;
    if (y >= _lbCurrentClipRect.Bottom) return;

    if (x < _lbCurrentClipRect.Left)
    {
        length -= _lbCurrentClipRect.Left - x;
        x = _lbCurrentClipRect.Left;
    }
    if (x + length > _lbCurrentClipRect.Right)
        length = _lbCurrentClipRect.Right - x;

    ULONG cv = _RC_COLVAL(colour);

    if (_lbCurrentDrawFlags & LB_DRAW_FLAG_GLASS)
    {
        SINT ofs = _RC_OFF(x, y);
        for (SINT i = 0; i < length; i++, ofs += _RC_STRIDE)
            _RC_GLASS(ofs, cv);
    }
    else if (_lbCurrentDrawFlags & LB_DRAW_FLAG_INVERT_GLASS)
    {
        SINT ofs = _RC_OFF(x, y);
        for (SINT i = 0; i < length; i++, ofs += _RC_STRIDE)
            _RC_IGLASS(ofs, cv);
    }
    else
    {
        if (_lbCurrentDrawFlags & LB_DRAW_FLAG_FADE)
            cv = _RC_FADECOLVAL(colour);
#if RENDER_BPP == 1
        memset(&_lbCurrentOriginPtr[_RC_OFF(x, y)], (UBYTE)cv, length);
#else
        SINT ofs = _RC_OFF(x, y);
        for (SINT i = 0; i < length; i++, ofs += _RC_STRIDE)
            _RC_PUT(ofs, cv);
#endif
    }
}

//***************************************************************************
// LbDraw_VerticalLine
//***************************************************************************
void RENDERCLASS::LbDraw_VerticalLine(SINT x, SINT y, SINT height, TbColour colour)
{
    SINT pitch = (SINT)_lbpCurrentRA->mPitch;
    if (height < 0) { y += height; height = -height; }
    if (height == 0) return;

    if (y + height <= _lbCurrentClipRect.Top) return;
    if (y >= _lbCurrentClipRect.Bottom) return;
    if (x < _lbCurrentClipRect.Left) return;
    if (x >= _lbCurrentClipRect.Right) return;

    if (y < _lbCurrentClipRect.Top)
    {
        height -= _lbCurrentClipRect.Top - y;
        y = _lbCurrentClipRect.Top;
    }
    if (y + height > _lbCurrentClipRect.Bottom)
        height = _lbCurrentClipRect.Bottom - y;

    ULONG cv = _RC_COLVAL(colour);

    if (_lbCurrentDrawFlags & LB_DRAW_FLAG_GLASS)
    {
        for (SINT ofs = _RC_OFF(x, y); height-- > 0; ofs += pitch)
            _RC_GLASS(ofs, cv);
    }
    else if (_lbCurrentDrawFlags & LB_DRAW_FLAG_INVERT_GLASS)
    {
        for (SINT ofs = _RC_OFF(x, y); height-- > 0; ofs += pitch)
            _RC_IGLASS(ofs, cv);
    }
    else
    {
        if (_lbCurrentDrawFlags & LB_DRAW_FLAG_FADE)
            cv = _RC_FADECOLVAL(colour);
        for (SINT ofs = _RC_OFF(x, y); height-- > 0; ofs += pitch)
            _RC_PUT(ofs, cv);
    }
}

//***************************************************************************
// LbDraw_Triangle — scan-line filled or outlined triangle
//***************************************************************************
void RENDERCLASS::LbDraw_Triangle(SINT x1, SINT y1, SINT x2, SINT y2, SINT x3, SINT y3, TbColour colour)
{
    if (_lbCurrentDrawFlags & LB_DRAW_FLAG_OUTLINE)
    {
        LbDraw_Line(x1, y1, x2, y2, colour);
        LbDraw_Line(x2, y2, x3, y3, colour);
        LbDraw_Line(x3, y3, x1, y1, colour);
        return;
    }

    // Sort vertices: y1 <= y2 <= y3
    if (y2 < y1) { SINT t; t=x1;x1=x2;x2=t; t=y1;y1=y2;y2=t; }
    if (y3 < y2) { SINT t; t=x2;x2=x3;x3=t; t=y2;y2=y3;y3=t; }
    if (y2 < y1) { SINT t; t=x1;x1=x2;x2=t; t=y1;y1=y2;y2=t; }

    if (y2 == y1)
    {
        if (y3 == y2) return;
        SINT leftFP, rightFP, slopeL, slopeR;
        if (x2 <= x1)
        {
            leftFP = x2 << 16; rightFP = x1 << 16;
            slopeL = ((x3-x2)<<16)/(y3-y2); slopeR = ((x3-x1)<<16)/(y3-y1);
        }
        else
        {
            leftFP = x1 << 16; rightFP = x2 << 16;
            slopeL = ((x3-x1)<<16)/(y3-y1); slopeR = ((x3-x2)<<16)/(y3-y2);
        }
        for (SINT scanY = y1; scanY < y3; scanY++, leftFP += slopeL, rightFP += slopeR)
            LbDraw_HorizontalLine(leftFP>>16, scanY, (rightFP>>16)-(leftFP>>16), colour);
        return;
    }

    SINT slopeLong  = ((x3-x1)<<16) / (y3-y1);
    SINT slopeShort = ((x2-x1)<<16) / (y2-y1);
    SINT leftFP  = x1 << 16;
    SINT rightFP = x1 << 16;
    SINT scanY   = y1;

    if (slopeShort <= slopeLong)
    {
        while (scanY < y2)
        {
            LbDraw_HorizontalLine(leftFP>>16, scanY, (rightFP>>16)-(leftFP>>16), colour);
            leftFP += slopeShort; rightFP += slopeLong; scanY++;
        }
        if (y3 != y2)
        {
            SINT slope2 = ((x3-x2)<<16)/(y3-y2);
            leftFP = x2 << 16;
            while (scanY <= y3)
            {
                LbDraw_HorizontalLine(leftFP>>16, scanY, (rightFP>>16)-(leftFP>>16), colour);
                leftFP += slope2; rightFP += slopeLong; scanY++;
            }
        }
    }
    else
    {
        while (scanY < y2)
        {
            LbDraw_HorizontalLine(rightFP>>16, scanY, (leftFP>>16)-(rightFP>>16), colour);
            leftFP += slopeShort; rightFP += slopeLong; scanY++;
        }
        if (y3 != y2)
        {
            SINT slope2 = ((x3-x2)<<16)/(y3-y2);
            SINT rightFP2 = x2 << 16;
            while (scanY <= y3)
            {
                LbDraw_HorizontalLine(rightFP>>16, scanY, (rightFP2>>16)-(rightFP>>16), colour);
                rightFP += slopeLong; rightFP2 += slope2; scanY++;
            }
        }
    }
}

//***************************************************************************
// LbDraw_ClearClipWindow
//***************************************************************************
void RENDERCLASS::LbDraw_ClearClipWindow(TbColour colour)
{
    SINT pitch = (SINT)_lbpCurrentRA->mPitch;
    SINT w = LbRect_Width(&_lbCurrentClipRect);
    SINT h = LbRect_Height(&_lbCurrentClipRect);
    if (w <= 0 || h <= 0) return;

    ULONG cv = _RC_COLVAL(colour);
#if RENDER_BPP == 1
    UBYTE *pDst = &_lbCurrentOriginPtr[_RC_OFF(_lbCurrentClipRect.Left, _lbCurrentClipRect.Top)];
    for (SINT row = 0; row < h; row++, pDst += pitch)
        memset(pDst, (UBYTE)cv, w);
#else
    for (SINT row = 0; row < h; row++)
    {
        SINT ofs = _RC_OFF(_lbCurrentClipRect.Left, _lbCurrentClipRect.Top + row);
        for (SINT col = 0; col < w; col++, ofs += _RC_STRIDE)
            _RC_PUT(ofs, cv);
    }
#endif
}

//***************************************************************************
// LbDraw_PropText — delegate to cheap text
//***************************************************************************
void RENDERCLASS::LbDraw_PropText(SINT x, SINT y, const TBCHAR *pText, TbColour colour)
{
    LbDraw_CheapText(x, y, pText, colour);
}

//***************************************************************************
// LbDraw_UnicodePropText — delegate to cheap unicode text
//***************************************************************************
void RENDERCLASS::LbDraw_UnicodePropText(SINT x, SINT y, const UNICHAR *pText, TbColour colour)
{
    LbDraw_UnicodeCheapText(x, y, pText, colour);
}
