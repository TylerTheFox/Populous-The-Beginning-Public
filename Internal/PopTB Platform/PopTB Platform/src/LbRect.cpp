#include <LbRect.h>
#include <Pop3Debug.h>

void LbRect_Set(Pop3Rect *pRect, const SINT l, const SINT t, const SINT r, const SINT b)
{
    pRect->Left   = l;
    pRect->Top    = t;
    pRect->Right  = r;
    pRect->Bottom = b;
}

void LbRect_SetFromSize(Pop3Rect *pRect, const Pop3Size *pSize)
{
    pRect->Left   = 0;
    pRect->Top    = 0;
    pRect->Right  = pSize->Width;
    pRect->Bottom = pSize->Height;
}

void LbRect_SetWindow(Pop3Rect *pRect, const SINT l, const SINT t, const SINT Width, const SINT Height)
{
    pRect->Left   = l;
    pRect->Top    = t;
    pRect->Right  = l + Width;
    pRect->Bottom = t + Height;
}

void LbRect_Copy(Pop3Rect *pDestRect, const Pop3Rect *pSrcRect)
{
    *pDestRect = *pSrcRect;
}

void LbRect_Offset(Pop3Rect *pRect, const SINT dx, const SINT dy)
{
    pRect->Left   += dx;
    pRect->Right  += dx;
    pRect->Top    += dy;
    pRect->Bottom += dy;
}

SINT LbRect_Width(const Pop3Rect *pRect)
{
    return pRect->Right - pRect->Left;
}

SINT LbRect_Height(const Pop3Rect *pRect)
{
    return pRect->Bottom - pRect->Top;
}

void LbRect_Bounding(Pop3Rect *pDestRect, const Pop3Rect *pSrcRect)
{
    if (pSrcRect->Left   < pDestRect->Left)   pDestRect->Left   = pSrcRect->Left;
    if (pSrcRect->Top    < pDestRect->Top)     pDestRect->Top    = pSrcRect->Top;
    if (pSrcRect->Right  > pDestRect->Right)  pDestRect->Right  = pSrcRect->Right;
    if (pSrcRect->Bottom > pDestRect->Bottom) pDestRect->Bottom = pSrcRect->Bottom;
}

BOOL LbRect_Intersection(Pop3Rect *pDestRect, const Pop3Rect *pSrcRect)
{
    if (pSrcRect->Left   > pDestRect->Left)   pDestRect->Left   = pSrcRect->Left;
    if (pSrcRect->Top    > pDestRect->Top)     pDestRect->Top    = pSrcRect->Top;
    if (pSrcRect->Right  < pDestRect->Right)  pDestRect->Right  = pSrcRect->Right;
    if (pSrcRect->Bottom < pDestRect->Bottom) pDestRect->Bottom = pSrcRect->Bottom;
    return pDestRect->Left < pDestRect->Right && pDestRect->Top < pDestRect->Bottom;
}

BOOL LbRect_Contains(const Pop3Rect *pRect1, const Pop3Rect *pRect2)
{
    return pRect2->Left   >= pRect1->Left
        && pRect2->Top    >= pRect1->Top
        && pRect2->Right  <= pRect1->Right
        && pRect2->Bottom <= pRect1->Bottom;
}

BOOL LbRect_IsEmpty(const Pop3Rect *pRect)
{
    return pRect->Right == pRect->Left || pRect->Top == pRect->Bottom;
}

void LbRect_Empty(Pop3Rect *pRect)
{
    pRect->Right  = pRect->Left;
    pRect->Bottom = pRect->Top;
}

void LbRect_Normalise(Pop3Rect *pRect)
{
    if (pRect->Right < pRect->Left)
    {
        pRect->Left  ^= pRect->Right;
        pRect->Right ^= pRect->Left;
        pRect->Left  ^= pRect->Right;
    }
    if (pRect->Top > pRect->Bottom)
    {
        pRect->Top    ^= pRect->Bottom;
        pRect->Bottom ^= pRect->Top;
        pRect->Top    ^= pRect->Bottom;
    }
}

BOOL LbRect_IsNormal(const Pop3Rect *pRect)
{
    return pRect->Right >= pRect->Left && pRect->Top <= pRect->Bottom;
}

BOOL LbRect_Intersects(const Pop3Rect *pRect1, const Pop3Rect *pRect2)
{
    ASSERTMSG(LbRect_IsNormal(pRect1), _LBSTR("LbRect_Intersects: pRect1 is not normalised"));
    ASSERTMSG(LbRect_IsNormal(pRect2), _LBSTR("LbRect_Intersects: pRect2 is not normalised"));
    return pRect1->Left   <= pRect2->Right
        && pRect1->Right  >= pRect2->Left
        && pRect1->Top    <= pRect2->Bottom
        && pRect2->Top    <= pRect1->Bottom;
}
