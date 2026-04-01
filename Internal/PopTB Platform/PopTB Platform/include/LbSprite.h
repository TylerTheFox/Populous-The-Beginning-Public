//***************************************************************************
//	LbSprite.h : Sprites
//***************************************************************************

#ifndef _LBSPRITE_H
#define _LBSPRITE_H

//***************************************************************************
#include	<LbCoord.h>
#include	<lbcolour.h>

//***************************************************************************
// <s>>TbSprite<< : The main sprite structure
//***************************************************************************
typedef struct TbSprite
{
    SBYTE		*Data;
    UWORD		Width;
    UWORD		Height;
} TbSprite;

//***************************************************************************
// LbDraw_Sprite... : Draw sprite functions
// Attributes	: CLIPPED
// Flags Tested	: GLASS, INVERT_GLASS, XFLIP, YFLIP
//***************************************************************************
void	LbDraw_Sprite(SINT x, SINT y, const TbSprite *lpSprite);
void	LbDraw_OneColourSprite(SINT x, SINT y, const TbSprite *lpSprite, TbColour colour);
void	LbDraw_RemappedSprite(SINT x, SINT y, const TbSprite *lpSprite, const UBYTE* RemapMap);
void	LbDraw_ScaledSprite(SINT x, SINT y, const TbSprite* lpSprite, UINT nDestWidth, UINT nDestHeight);

//***************************************************************************
// Functions to set the scaling info for the sprites
//***************************************************************************
void	LbDraw_SetSpriteScalingData(SINT x, SINT y, UINT nSrcWidth, UINT nSrcHeight, UINT nDestWidth, UINT nDestHeight);

extern "C" void LbDraw_SpriteUsingScalingData(SINT x, SINT y, const TbSprite *lpSprite);


#endif // ifndef _LBSPRITE_H
