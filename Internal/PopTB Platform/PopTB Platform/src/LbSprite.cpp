#include <LbSprite.h>
#include <LbException.h>
#include "LbSpriteRender.h"

// -----------------------------------------------------------------------
// LbDraw_Sprite : Draw a sprite at (x,y) using the active renderer.
// Delegates to the BfDraw_Sprite function pointer, which is hot-swappable
// between the C++ reimplementation and the legacy safety wrapper.
// -----------------------------------------------------------------------
void LbDraw_Sprite(SINT x, SINT y, const TbSprite *lpSprite)
{
    BfDraw_Sprite(x, y, lpSprite);
}

// -----------------------------------------------------------------------
// LbDraw_OneColourSprite : Draw a sprite in a single tint colour.
// -----------------------------------------------------------------------
void LbDraw_OneColourSprite(SINT x, SINT y, const TbSprite *lpSprite, TbColour colour)
{
    BfDraw_OneColourSprite(x, y, lpSprite, colour);
}

// -----------------------------------------------------------------------
// LbDraw_RemappedSprite : Draw a sprite with palette remapping.
// LbSpriteDrawRemap is not yet implemented; fall back to plain draw.
// -----------------------------------------------------------------------
void LbDraw_RemappedSprite(SINT x, SINT y, const TbSprite *lpSprite, const UBYTE *RemapMap)
{
    BfDraw_Sprite(x, y, lpSprite);
}

// -----------------------------------------------------------------------
// LbDraw_ScaledSprite : Draw a sprite scaled to (nDestWidth x nDestHeight).
// -----------------------------------------------------------------------
void LbDraw_ScaledSprite(SINT x, SINT y, const TbSprite *lpSprite, UINT nDestWidth, UINT nDestHeight)
{
    BfDraw_ScaledSprite(x, y, lpSprite, (long)nDestWidth, (long)nDestHeight);
}

// -----------------------------------------------------------------------
// LbDraw_SetSpriteScalingData : Set up pre-computed scaling tables.
// -----------------------------------------------------------------------
void LbDraw_SetSpriteScalingData(SINT x, SINT y, UINT nSrcWidth, UINT nSrcHeight, UINT nDestWidth, UINT nDestHeight)
{
    BullfrogLib::LbSpriteSetScalingData(x, y, (long)nSrcWidth, (long)nSrcHeight, (long)nDestWidth, (long)nDestHeight);
}

// -----------------------------------------------------------------------
// LbDraw_SpriteUsingScalingData : Draw using pre-set scaling data.
// -----------------------------------------------------------------------
extern "C" void LbDraw_SpriteUsingScalingData(SINT x, SINT y, const TbSprite *lpSprite)
{
    BfDraw_SpriteUsingScalingData(x, y, lpSprite);
}
