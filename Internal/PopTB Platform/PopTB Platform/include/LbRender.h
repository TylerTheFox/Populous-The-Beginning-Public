#ifndef _LBRENDER_H
#define	_LBRENDER_H

#include "Pop3Types.h"
#include <LbColour.h>
#include <TbRect.h>
#include "LbSpriteRender.h"
class RenderBase
{
public:
    virtual void LbDraw_Pixel(SINT x, SINT y, TbColour colour) = 0;

    virtual void LbDraw_Rectangle(const TbRect &rect, TbColour col) = 0;
    virtual void LbDraw_RectangleOutline(const TbRect &rect, TbColour col) = 0;
    virtual void LbDraw_RectangleFilled(const TbRect &rect, TbColour colour) = 0;

    virtual void LbDraw_Circle(SINT x, SINT y, UINT radius, TbColour col) = 0;
    virtual void LbDraw_CircleOutline(SINT xpos, SINT ypos, UINT radius, TbColour colour) = 0;
    virtual void LbDraw_CircleFilled(SINT xpos, SINT ypos, UINT radius, TbColour colour) = 0;

    virtual void LbDraw_Line(SINT x1, SINT y1, SINT x2, SINT y2, TbColour colour) = 0;
    virtual void LbDraw_HorizontalLine(SINT x, SINT y, SINT length, TbColour colour) = 0;
    virtual void LbDraw_VerticalLine(SINT x, SINT y, SINT height, TbColour colour) = 0;

    virtual void LbDraw_Triangle(SINT x1, SINT y1, SINT x2, SINT y2, SINT x3, SINT y3, TbColour colour) = 0;

    virtual void LbDraw_ClearClipWindow(TbColour colour) = 0;

    virtual void LbDraw_PropText(SINT x, SINT y, const TBCHAR *pText, TbColour colour) = 0;
    virtual void LbDraw_UnicodePropText(SINT x, SINT y, const UNICHAR *pText, TbColour colour) = 0;

    //virtual	void BfDraw_Sprite(SINT x, SINT y, const TbSprite *lpSprite) = 0;
    //virtual	void BfDraw_OneColourSprite(SINT x, SINT y, const TbSprite* lpSprite, TbColour colour) = 0;


};



// 8 bit renderer
#define RENDERCLASS		RenderContext8
#include "LbRendef.h"
#undef RENDERCLASS

// 16 bit (555) renderer
#define RENDERCLASS		RenderContext16
#include "LbRendef.h"
#undef RENDERCLASS

// 24 bit renderer
#define RENDERCLASS		RenderContext24
#include "LbRendef.h"
#undef RENDERCLASS

// 32 bit renderer
#define RENDERCLASS		RenderContext32
#include "LbRendef.h"
#undef RENDERCLASS

// There will only be one instance of these objects and they are defined in LbDraw.cpp
extern RenderContext8	_lbRenderContext8;
extern RenderContext16	_lbRenderContext16;
extern RenderContext24	_lbRenderContext24;
extern RenderContext32	_lbRenderContext32;


#endif