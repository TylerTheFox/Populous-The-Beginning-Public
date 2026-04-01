#ifndef _LBRECT_H
#define _LBRECT_H
#include "Pop3Types.h"
#include <Pop3Rect.h>

void LbRect_Offset(Pop3Rect* pRect, const SINT dx, const SINT dy);
SINT LbRect_Width(const Pop3Rect* pRect);
SINT LbRect_Height(const Pop3Rect* pRect);
void LbRect_Bounding(Pop3Rect* pDestRect, const Pop3Rect* pSrcRect);
BOOL LbRect_Intersection(Pop3Rect* pDestRect, const Pop3Rect* pSrcRect);
BOOL LbRect_Contains(const Pop3Rect* pRect1, const Pop3Rect* pRect2);
BOOL LbRect_IsEmpty(const Pop3Rect* pRect);
void LbRect_Empty(Pop3Rect* pRect);
void LbRect_Normalise(Pop3Rect* pRect);
void LbPoint_Offset(Pop3Point* pPoint, SINT dx, SINT dy);
void LbRect_Set(Pop3Rect* pRect, const SINT l, const SINT t, const SINT r, const SINT b);
void LbRect_SetFromSize(Pop3Rect* pRect, const Pop3Size* pSize);
void LbRect_SetWindow(Pop3Rect* pRect, const SINT l, const SINT t, const SINT Width, const SINT Height);
void LbRect_Copy(Pop3Rect* pDestRect, const Pop3Rect* pSrcRect);
BOOL LbRect_IsNormal(const Pop3Rect* pRect);
BOOL LbRect_Intersects(const Pop3Rect* pRect1, const Pop3Rect* pRect2);

#endif