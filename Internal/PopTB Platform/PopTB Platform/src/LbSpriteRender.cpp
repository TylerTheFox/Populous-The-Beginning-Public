/**
 * @file LbSpriteRender.cpp
 * @brief Bullfrog Library 8.1 sprite rendering engine — C++ reimplementation.
 *
 * Replaces the original x86 ASM sprite renderer from Bullfrog's library.
 * Handles all 2D sprite rendering for the game including:
 *   - Non-scaled sprite drawing (solid, transparent, one-colour)
 *   - Scaled sprite drawing (up-scaling and down-scaling) with per-pixel
 *     blending via compile-time template specialization
 *   - Horizontal/vertical flipping
 *   - Palette remapping and ghost-table (transparency) blending
 *   - Viewport clipping and coordinate transformation
 *
 * All sprites use Bullfrog's RLE encoding: signed-byte commands where
 *   positive N = N opaque pixels follow,
 *   negative N = skip |N| transparent pixels,
 *   zero     = end of scanline.
 *
 * The 16 original hand-written scaling variants have been consolidated
 * into 2 C++ template functions (LbSpriteDrawScalingUp / LbSpriteDrawScalingDown)
 * using `if constexpr` for zero-overhead compile-time specialization across
 * direction (RL/LR) and blend mode (Solid/Remap/Trans1/Trans2).
 */
#include "LbSpriteRender.h"
#include "LbDraw.h"
#include "Pop3Debug.h"

#include <assert.h>
#include <cstring>

/** @brief Enable verbose tracing inside the sprite rendering functions. */
#define SPR_VERBOSE				0

/**
 * @brief Sprite draw flags controlling flip, transparency, and blend modes.
 *
 * These bit-flags are stored in lbDisplay.DrawFlags (mirrored from _lbCurrentDrawFlags)
 * and select which rendering path is taken for both scaled and non-scaled sprites.
 *
 * | Flag                    | Bit  | Effect                                           |
 * |-------------------------|------|--------------------------------------------------|
 * | Lb_SPRITE_FLIP_HORIZ    | 0x01 | Mirror sprite left-to-right (RL direction).      |
 * | Lb_SPRITE_FLIP_VERTIC   | 0x02 | Flip sprite top-to-bottom (negative scanline).   |
 * | Lb_SPRITE_OUTLINE       | 0x04 | (Unused) Outline rendering mode.                 |
 * | Lb_SPRITE_TRANSPAR4     | 0x08 | Ghost blend: table[sprite<<8 | dest] (Trans1).   |
 * | Lb_SPRITE_TRANSPAR8     | 0x10 | Ghost blend: table[sprite | dest<<8] (Trans2).   |
 * | Lb_SPRITE_FADE          | 0x20 | Fade table rendering.                            |
 * | Lb_TEXT_UNDERLNSHADOW   | 0x22 | Palette remap mode (lbSpriteReMapPtr table).     |
 */
enum TbDrawFlags {
	Lb_SPRITE_FLIP_HORIZ = 0x00000001,
	Lb_SPRITE_FLIP_VERTIC = 0x00000002,
	Lb_SPRITE_OUTLINE = 0x00000004,
	Lb_SPRITE_TRANSPAR4 = 0x00000008,
	Lb_SPRITE_TRANSPAR8 = 0x00000010,
	Lb_SPRITE_FADE = 0x00000020,

	// Unknown or unused
	/*Lb_TEXT_HALIGN_LEFT = 0x22,
	Lb_TEXT_ONE_COLOR = 0x24,
	Lb_TEXT_HALIGN_RIGHT = 0x28,
	Lb_TEXT_HALIGN_CENTER = 0x30,
	Lb_TEXT_HALIGN_JUSTIFY = 0x32,
	Lb_TEXT_UNDERLINE = 0x34,*/
	Lb_TEXT_UNDERLNSHADOW = 0x22,
};

/** @internal
 *  Storage for temp sprite data while drawing it.
 */
struct TbSpriteDrawData {
	char			*sp;
	short			Wd;
	short			Ht;
	unsigned char	*r;
	int				nextRowDelta;
	short			startShift;
	BOOL			mirror;
};

/**
 * @brief Local display state snapshot used during sprite rendering.
 *
 * Populated by LbDisplayPrepare() at the start of each draw call to
 * capture the current viewport dimensions, clipping window, ghost table
 * pointer, and draw flags.  This avoids repeated lookups into the global
 * viewport/surface structures during the inner rendering loops.
 */
struct DisplayStruct
{
	SINT GraphicsScreenWidth;   ///< Full render surface width in pixels.
	SINT GraphicsScreenHeight;  ///< Full render surface height in pixels.

	SINT GraphicsWindowWidth;   ///< Clipping window width (viewport).
	SINT GraphicsWindowHeight;  ///< Clipping window height (viewport).

	SINT GraphicsWindowX;       ///< Clipping window left edge (viewport X offset).
	SINT GraphicsWindowY;       ///< Clipping window top edge (viewport Y offset).

	UBYTE* Spooky;              ///< Pointer to the 64 KB ghost/transparency LUT (256x256).

	UBYTE* FadeTable;           ///< Pointer to the fade palette table (unused in scaled path).
	UBYTE FadeStep;             ///< Current fade level (unused in scaled path).

	ULONG DrawFlags;            ///< Active TbDrawFlags bit-field for the current draw call.
};

/** @brief Maximum screen resolution we allocate scaling arrays for. */
#define MAX_SUPPORTED_SCREEN_WIDTH  2560
#define MAX_SUPPORTED_SCREEN_HEIGHT 1440


/** @brief Local rendering state — snapshot of viewport/surface/flags for the current draw call. */
struct DisplayStruct lbDisplay;
/** @brief Palette remap table pointer used with Lb_TEXT_UNDERLNSHADOW flag (currently unused; asserts). */
UBYTE* BullfrogLib::lbSpriteReMapPtr = nullptr;

/** @brief Set to true when dest > source dimensions; selects up-scaling vs down-scaling template. */
long scale_up;
/** @brief Same as scale_up but for the alpha channel scaling path (see alpha.h). */
long alpha_scale_up;

/** @brief Maximum number of horizontal scaling step pairs (position + delta) per sprite. */
#define SPRITE_SCALING_XSTEPS 512
/** @brief Maximum number of vertical scaling step pairs (position + delta) per sprite. */
#define SPRITE_SCALING_YSTEPS 512

/**
 * @brief Pre-computed horizontal scaling arrays.
 *
 * Stored as pairs: xsteps_array[2*i] = pixel position,  xsteps_array[2*i+1] = pixel count (delta).
 * Populated by LbSpriteSetScalingWidthClipped / LbSpriteSetScalingWidthSimple.
 */
long xsteps_array[SPRITE_SCALING_XSTEPS];
/** @brief Pre-computed vertical scaling array; same pair layout as xsteps_array but for rows. */
long ysteps_array[SPRITE_SCALING_YSTEPS];

/** @brief Alpha-channel horizontal scaling array (for alpha overlay sprites). */
long alpha_xsteps_array[SPRITE_SCALING_XSTEPS];
/** @brief Alpha-channel vertical scaling array (for alpha overlay sprites). */
long alpha_ysteps_array[SPRITE_SCALING_YSTEPS];

/**
 * @brief Snapshots the current viewport, surface, and draw-flag state into lbDisplay.
 *
 * Must be called at the top of every public draw entry point before any rendering.
 * Copies resolution, clipping window, ghost table pointer, and draw flags from
 * the global _lbCurrentViewPort / _lbpCurrentRA / _lbCurrentGhostTable / _lbCurrentDrawFlags
 * into the local DisplayStruct for fast access during rendering.
 */
void LbDisplayPrepare()
{
	memset(&lbDisplay, 0, sizeof(DisplayStruct));

	// Resolution
	auto currSurface = _lbpCurrentRA;
	lbDisplay.GraphicsScreenWidth = currSurface->GetSize().Width;
	lbDisplay.GraphicsScreenHeight = currSurface->GetSize().Height;

	// Graphics window = viewport (used for coordinate translation: view-local -> absolute).
	lbDisplay.GraphicsWindowX = _lbCurrentViewPort.Left;
	lbDisplay.GraphicsWindowY = _lbCurrentViewPort.Top;
	lbDisplay.GraphicsWindowWidth = _lbCurrentViewPort.Width();
	lbDisplay.GraphicsWindowHeight = _lbCurrentViewPort.Height();

	// Ghost Table
	lbDisplay.Spooky = _lbCurrentGhostTable;

	// Drawing flags
	lbDisplay.DrawFlags = _lbCurrentDrawFlags;
}

/**
 * @brief Clips a sprite rectangle against the graphics window boundaries.
 *
 * Given a sprite at (x, y) relative to the viewport origin, computes which
 * rows and columns of the sprite are actually visible after clipping against
 * the viewport rectangle [GraphicsWindowX .. GraphicsWindowX+Width) x
 * [GraphicsWindowY .. GraphicsWindowY+Height).  The viewport offsets are
 * added to x/y internally.
 *
 * @param x,y        Sprite position in viewport-relative coordinates.
 * @param GraphicsWindowX,Y      Viewport origin in screen coordinates.
 * @param GraphicsWindowWidth,Height  Viewport size.
 * @param spr        Sprite descriptor (Width, Height).
 * @param[out] dp    Receives the visible sub-rectangle in sprite-local coords.
 * @return LB_OK if at least one pixel is visible; LB_OK_NO_DRAW if fully clipped.
 */
TbError rangeChecking(long x, long y, SINT GraphicsWindowX, SINT GraphicsWindowY, SINT GraphicsWindowWidth, SINT GraphicsWindowHeight, const TbSprite *spr, struct DrawRetParams& dp)
{
	// Input (x,y) is viewport-relative. Convert to absolute render-area coords.
	x += GraphicsWindowX;
	y += GraphicsWindowY;
	short sprWd = spr->Width;
	short sprHt = spr->Height;

	if ((sprWd < 1) || (sprHt < 1))
	{
		Pop3Debug::trace("Zero size sprite (%d,%d)", sprWd, sprHt);
		return LB_OK_NO_DRAW;
	}

	// Clip against the current clip rect — NOT just the full render area.
	// _lbCurrentClipRect is maintained by LbDraw_SetClipRect / LbDraw_SetViewPort;
	// it defaults to (viewport ∩ render-area) after each viewport change and can
	// be widened (within the viewport) by an explicit LbDraw_SetClipRect call.
	//
	// Earlier revision of this function clipped against the full render area to
	// let tooltips extend past a narrowed viewport, but that turned out too
	// permissive: DrawRepeatingTextureRectangle and similar tile loops tile
	// slab-by-slab inside a narrow UI viewport, and the trailing slab ran past
	// the viewport edge into the adjacent maingame region. Properly honouring
	// the clip rect fixes that without breaking tooltip behaviour, because
	// tooltip paths that need to draw past a narrow viewport set a matching
	// wider viewport/clip rect up front anyway.
	//
	// _lbCurrentClipRect is stored in viewport-local coords (see LbDraw.cpp),
	// and at this point x/y are in render-area-absolute coords (we added
	// GraphicsWindowX/Y above) — so translate clip rect to absolute too.
	SINT clipLeft   = lbDisplay.GraphicsWindowX + _lbCurrentClipRect.Left;
	SINT clipTop    = lbDisplay.GraphicsWindowY + _lbCurrentClipRect.Top;
	SINT clipRight  = lbDisplay.GraphicsWindowX + _lbCurrentClipRect.Right;
	SINT clipBottom = lbDisplay.GraphicsWindowY + _lbCurrentClipRect.Bottom;

	// Safety clamp to the render area so we never write past the surface
	// regardless of what callers have configured.
	if (clipLeft   < 0) clipLeft   = 0;
	if (clipTop    < 0) clipTop    = 0;
	if (clipRight  > (SINT)lbDisplay.GraphicsScreenWidth)  clipRight  = (SINT)lbDisplay.GraphicsScreenWidth;
	if (clipBottom > (SINT)lbDisplay.GraphicsScreenHeight) clipBottom = (SINT)lbDisplay.GraphicsScreenHeight;

	// Early rejection — sprite entirely outside clip bounds
	if (x + sprWd <= clipLeft || x >= clipRight)
		return LB_OK_NO_DRAW;
	if (y + sprHt <= clipTop || y >= clipBottom)
		return LB_OK_NO_DRAW;

	// Clip sprite to visible sub-rectangle (in sprite-local coords)
	dp.left  = (x < clipLeft)            ? clipLeft - x          : 0;
	dp.right = (x + sprWd > clipRight)   ? clipRight - x         : sprWd;
	dp.top   = (y < clipTop)             ? clipTop - y           : 0;
	dp.btm   = (y + sprHt > clipBottom)  ? clipBottom - y        : sprHt;

	return LB_OK;
}

/**
 * @brief Sets up the TbSpriteDrawData structure for a non-scaled sprite draw.
 *
 * Performs the following steps:
 *   1. Calls LbDisplayPrepare() to snapshot current viewport/surface state.
 *   2. Calls rangeChecking() to clip the sprite against the viewport.
 *   3. Converts viewport-relative (x, y) to screen coordinates.
 *   4. Computes the initial output buffer pointer (spd->r) accounting for
 *      vertical flip (Lb_SPRITE_FLIP_VERTIC) and horizontal flip (Lb_SPRITE_FLIP_HORIZ).
 *   5. Sets the per-row delta (spd->nextRowDelta) — positive for top-down,
 *      negative for bottom-up.
 *   6. Walks the RLE sprite data to skip rows above the visible region (dp.top).
 *   7. If horizontally mirrored, adjusts the output pointer to the right edge
 *      and swaps left/right clipping boundaries.
 *
 * @param[out] spd  Receives the prepared draw state (sprite pointer, dimensions,
 *                  output pointer, row delta, start shift, mirror flag).
 * @param x,y       Sprite position relative to the viewport origin.
 * @param spr       The TbSprite to draw.
 * @return LB_OK on success; LB_OK_NO_DRAW if the sprite is fully clipped.
 */
TbError LbSpriteDrawPrepare(TbSpriteDrawData *spd, long x, long y, const TbSprite *spr)
{
	short sprWd = spr->Width;
	short sprHt = spr->Height;

	LbDisplayPrepare();

	struct DrawRetParams dp;

	// rangeChecking adds GraphicsWindowX/Y internally, so pass view-relative coords
	if (rangeChecking(x, y, lbDisplay.GraphicsWindowX, lbDisplay.GraphicsWindowY, lbDisplay.GraphicsWindowWidth, lbDisplay.GraphicsWindowHeight, spr, dp) != LB_OK)
		return LB_OK_NO_DRAW;

	// Convert to screen coordinates after range checking
	x += lbDisplay.GraphicsWindowX;
	y += lbDisplay.GraphicsWindowY;

	if ((lbDisplay.DrawFlags & Lb_SPRITE_FLIP_VERTIC) != 0)
	{
		spd->r = (_CURRENT_RENDER_AREA->mpData + x + (y + dp.btm - 1)* _CURRENT_RENDER_AREA->mPitch + dp.left);
		spd->nextRowDelta = -(int)_CURRENT_RENDER_AREA->mPitch;
		short tmp_btm = dp.btm;
		dp.btm = sprHt - dp.top;
		dp.top = sprHt - tmp_btm;
	}
	else
	{
		spd->r = (_CURRENT_RENDER_AREA->mpData + x + (y + dp.top)* _CURRENT_RENDER_AREA->mPitch + dp.left);
		spd->nextRowDelta = (int)_CURRENT_RENDER_AREA->mPitch;
	}
	spd->Ht = dp.btm - dp.top;
	spd->Wd = dp.right - dp.left;
	spd->sp = (char *)spr->Data;
#if SPR_VERBOSE
	Pop3Debug::trace("sprite coords X=%d...%d Y=%d...%d data=%08x", left, right, top, btm, spd->sp);
#endif
	long htIndex;
	if (dp.top)
	{
		htIndex = dp.top;
		while (1)
		{
			char chr = *(spd->sp);
			while (chr > 0)
			{
				spd->sp += chr + 1;
				chr = *(spd->sp);
			}
			spd->sp++;
			if (chr == 0)
			{
				htIndex--;
				if (htIndex <= 0) break;
			}
		}
	}
#if SPR_VERBOSE
	Pop3Debug::trace("at (%ld,%ld): drawing sprite of size (%d,%d)", x, y, (int)spd->Ht, (int)spd->Wd);
#endif
	if ((lbDisplay.DrawFlags & Lb_SPRITE_FLIP_HORIZ) != 0)
	{
		spd->r += spd->Wd - 1;
		spd->mirror = true;
		short tmpwidth = spr->Width;
		short tmpright = dp.right;
		dp.right = tmpwidth - dp.left;
		spd->startShift = tmpwidth - tmpright;
	}
	else
	{
		spd->mirror = false;
		spd->startShift = dp.left;
	}
	return LB_OK;
}

/**
 * @brief Skips RLE sprite columns that fall to the left of the visible region.
 *
 * Walks through the RLE command stream, consuming skip (negative) and pixel
 * (positive) commands until 'left' columns have been skipped.  Adjusts the
 * remaining visible width (*x1) accordingly.
 *
 * @param[in,out] sp    Pointer to the current position in the RLE stream.
 * @param[in,out] x1    Remaining visible width; decremented as columns are consumed.
 * @param left          Number of columns to skip from the left edge.
 * @return 0 if all left-side columns were fully skipped; otherwise returns the
 *         remaining partial skip count so the caller can handle the partial block.
 */
short LbSpriteDrawLineSkipLeft(const char **sp, short *x1, short left)
{
	char schr;
	// Cut the left side of the sprite, if needed
	if (left != 0)
	{
		short lpos = left;
		while (lpos > 0)
		{
			schr = *(*sp);
			// Value > 0 means count of filled characters, < 0 means skipped characters
			// Equal to 0 means EOL
			if (schr == 0)
			{
				(*x1) = 0;
				break;
			}
			if (schr < 0)
			{
				if (-schr <= lpos)
				{
					lpos += schr;
					(*sp)++;
				}
				else
					// If we have more empty spaces than we want to skip
				{
					// Return remaining part to skip, so that we can do it outside
					return lpos;
				}
			}
			else
				//if (schr > 0)
			{
				if (schr <= lpos)
					// If we have less than we want to skip
				{
					lpos -= schr;
					(*sp) += (*(*sp)) + 1;
				}
				else
					// If we have more characters than we want to skip
				{
					// Return remaining part to skip, so that we can draw it
					return lpos;
				}
			}
		}
	}
	return 0;
}
/**
 * @brief Copies sprite pixels to the output buffer with ghost-table transparency blending.
 *
 * For each pixel in buf_inp, looks up the blended result using the ghost table
 * (lbDisplay.Spooky).  The blend direction depends on the Lb_SPRITE_TRANSPAR4 flag:
 *   - TRANSPAR4 (Trans1): index = sprite_colour * 256 + destination_colour
 *   - Otherwise  (Trans2): index = destination_colour * 256 + sprite_colour
 *
 * @param[in,out] buf_out  Pointer to the output buffer cursor; advanced by buf_len.
 * @param buf_inp          Source sprite pixel data (RLE-decoded opaque run).
 * @param buf_len          Number of pixels to blend.
 * @param mirror           If true, the output pointer decrements (right-to-left).
 */
static inline void LbDrawBufferTranspr(unsigned char **buf_out, const char *buf_inp, const int buf_len, const BOOL mirror)
{
	int i;
	unsigned int val;
	if (mirror)
	{
		if ((_lbCurrentDrawFlags & Lb_SPRITE_TRANSPAR4) != 0)
		{
			for (i = 0; i < buf_len; i++)
			{
				val = *(const unsigned char *)buf_inp;
				**buf_out = lbDisplay.Spooky[256 * val + **buf_out];
				buf_inp++;
				(*buf_out)--;
			}
		}
		else
		{
			for (i = 0; i < buf_len; i++)
			{
				val = *(const unsigned char *)buf_inp;
				**buf_out = lbDisplay.Spooky[256 * **buf_out + val];
				buf_inp++;
				(*buf_out)--;
			}
		}
	}
	else
	{
		if (_lbCurrentDrawFlags & Lb_SPRITE_TRANSPAR4)
		{
			for (i = 0; i < buf_len; i++)
			{
				val = *(const unsigned char *)buf_inp;

				auto idx = 256 * val + **buf_out;
				auto max = 256 * 256;


				**buf_out = lbDisplay.Spooky[idx];
				buf_inp++;
				(*buf_out)++;
			}
		}
		else
		{
			for (i = 0; i < buf_len; i++)
			{
				val = *(const unsigned char *)buf_inp;
				**buf_out = lbDisplay.Spooky[256 * **buf_out + val];
				buf_inp++;
				(*buf_out)++;
			}
		}
	}
}
/**
 * @brief Draws one visible scanline of a sprite using transparency blending.
 *
 * Handles the partial block left over from LbSpriteDrawLineSkipLeft (if any),
 * then processes the RLE stream for the remaining visible width, calling
 * LbDrawBufferTranspr() for each opaque run and advancing the output pointer
 * for skip runs.
 *
 * @param[in,out] sp    RLE stream cursor (advanced past consumed commands).
 * @param[in,out] r     Output framebuffer cursor (advanced by drawn pixels).
 * @param[in,out] x1    Remaining visible width of this scanline.
 * @param lpos          Partial skip count returned by LbSpriteDrawLineSkipLeft.
 * @param mirror        If true, output pointer moves right-to-left.
 */
static inline void LbSpriteDrawLineTranspr(const char **sp, unsigned char **r, short *x1, short lpos, const BOOL mirror)
{
	char schr;
	unsigned char drawOut;
	// Draw any unfinished block, which should be only partially visible
	if (lpos > 0)
	{
		schr = *(*sp);
		if (schr < 0)
		{
			drawOut = -schr - lpos;
			if (drawOut > (*x1))
				drawOut = (*x1);
			if (mirror)
				(*r) -= drawOut;
			else
				(*r) += drawOut;
			(*sp)++;

		}
		else
		{
			// Draw the part of current block which exceeds value of 'lpos'
			drawOut = schr - lpos;
			if (drawOut > (*x1))
				drawOut = (*x1);
			LbDrawBufferTranspr(r, (*sp) + (lpos + 1), drawOut, mirror);
			// Update positions and break the skipping loop
			(*sp) += (*(*sp)) + 1;
		}
		(*x1) -= drawOut;
	}
	// Draw the visible part of a sprite
	while ((*x1) > 0)
	{
		schr = *(*sp);
		if (schr == 0)
		{ // EOL, breaking line loop
			break;
		}
		if (schr < 0)
		{ // Skipping some pixels
			(*x1) += schr;
			if (mirror)
				(*r) += *(*sp);
			else
				(*r) -= *(*sp);
			(*sp)++;
		}
		else
			//if ( schr > 0 )
		{ // Drawing some pixels
			drawOut = schr;
			if (drawOut >= (*x1))
				drawOut = (*x1);
			LbDrawBufferTranspr(r, (*sp) + 1, drawOut, mirror);
			(*x1) -= schr;
			(*sp) += (*(*sp)) + 1;
		}
	} //end while
}
/**
 * @brief Advances the RLE stream to the end of the current scanline.
 *
 * Called after the visible portion of a line has been drawn (or fully clipped)
 * to skip any remaining RLE commands until the end-of-line sentinel (0) is reached.
 *
 * @param[in,out] sp   RLE stream cursor.
 * @param[in,out] x1   Remaining width (used to detect if we're mid-line or past-end).
 */
void LbSpriteDrawLineSkipToEol(const char **sp, short *x1)
{
	char schr;
	if ((*x1) <= 0)
	{
		do {
			schr = *(*sp);
			while (schr > 0)
			{
				(*sp) += schr + 1;
				schr = *(*sp);
			}
			(*sp)++;
		} while (schr);
	}
	else
	{
		(*sp)++;
	}
}
/**
 * @brief Draws an entire non-scaled sprite with transparency blending.
 *
 * Iterates over all visible scanlines, calling LbSpriteDrawLineSkipLeft to
 * handle left clipping, LbSpriteDrawLineTranspr for the visible pixels,
 * and LbSpriteDrawLineSkipToEol to consume the remaining RLE data per line.
 *
 * @param sp            RLE sprite data (positioned at first visible row after prepare).
 * @param sprWd         Visible width of the sprite (after clipping).
 * @param sprHt         Visible height of the sprite (after clipping).
 * @param r             Framebuffer pointer to the first visible pixel.
 * @param nextRowDelta  Byte offset to advance to the next output row (negative if FLIP_VERTIC).
 * @param left          Number of columns to skip on the left (for clipping).
 * @param mirror        True if sprite is horizontally flipped.
 * @return LB_OK always.
 */
static inline TbError LbSpriteDrawTranspr(const char *sp, short sprWd, short sprHt, unsigned char *r, int nextRowDelta, short left, const BOOL mirror)
{
	unsigned char *nextRow;
	long htIndex;
	nextRow = &(r[nextRowDelta]);
	htIndex = sprHt;
	// For all lines of the sprite
	while (1)
	{
		short x1;
		short lpos;
		x1 = sprWd;
		// Skip the pixels left before drawing area
		lpos = LbSpriteDrawLineSkipLeft(&sp, &x1, left);
		// Do the actual drawing
		LbSpriteDrawLineTranspr(&sp, &r, &x1, lpos, mirror);
		// Go to next line
		htIndex--;
		if (htIndex == 0)
			return LB_OK;
		LbSpriteDrawLineSkipToEol(&sp, &x1);
		r = nextRow;
		nextRow += nextRowDelta;
	} //end while
	return LB_OK;
}

/**
 * @brief Copies sprite pixels to the output buffer with no blending (opaque copy).
 *
 * Simply transfers each byte from buf_inp to *buf_out, advancing the
 * output pointer forward (+1) or backward (-1) depending on the mirror flag.
 *
 * @param[in,out] buf_out  Output cursor; advanced by buf_len in the appropriate direction.
 * @param buf_inp          Source pixel data from the RLE stream.
 * @param buf_len          Number of pixels to copy.
 * @param mirror           If true, output pointer decrements (right-to-left).
 */
static inline void LbDrawBufferSolid(unsigned char **buf_out, const char *buf_inp, const int buf_len, const BOOL mirror)
{
	int i;
	if (mirror)
	{
		for (i = 0; i < buf_len; i++)
		{
			**buf_out = *(const unsigned char *)buf_inp;
			buf_inp++;
			(*buf_out)--;
		}
	}
	else
	{
		for (i = 0; i < buf_len; i++)
		{
			**buf_out = *(const unsigned char *)buf_inp;
			buf_inp++;
			(*buf_out)++;
		}
	}
}
/**
 * @brief Draws one visible scanline of a sprite as opaque solid pixels.
 *
 * Same structure as LbSpriteDrawLineTranspr but uses LbDrawBufferSolid
 * for direct pixel copy without ghost-table blending.
 *
 * @param[in,out] sp   RLE stream cursor.
 * @param[in,out] r    Framebuffer output cursor.
 * @param[in,out] x1   Remaining visible width.
 * @param lpos         Partial skip from LbSpriteDrawLineSkipLeft.
 * @param mirror       If true, output moves right-to-left.
 */
static inline void LbSpriteDrawLineSolid(const char **sp, unsigned char **r, short *x1, short lpos, const BOOL mirror)
{
	char schr;
	unsigned char drawOut;
	// Draw any unfinished block, which should be only partially visible
	if (lpos > 0)
	{
		schr = *(*sp);
		if (schr < 0)
		{
			drawOut = -schr - lpos;
			if (drawOut > (*x1))
				drawOut = (*x1);
			(*r) -= drawOut;
			(*sp)++;
		}
		else
		{
			// Draw the part of current block which exceeds value of 'lpos'
			drawOut = schr - lpos;
			if (drawOut > (*x1))
				drawOut = (*x1);
			LbDrawBufferSolid(r, (*sp) + (lpos + 1), drawOut, mirror);
			// Update positions and break the skipping loop
			(*sp) += (*(*sp)) + 1;
		}
		(*x1) -= drawOut;
	}
	// Draw the visible part of a sprite
	while ((*x1) > 0)
	{
		schr = *(*sp);
		if (schr == 0)
		{ // EOL, breaking line loop
			break;
		}
		if (schr < 0)
		{ // Skipping some pixels
			(*x1) += schr;
			(*r) += *(*sp);
			(*sp)++;
		}
		else
			//if ( schr > 0 )
		{ // Drawing some pixels
			drawOut = schr;
			if (drawOut >= (*x1))
				drawOut = (*x1);
			LbDrawBufferSolid(r, (*sp) + 1, drawOut, mirror);
			(*x1) -= schr;
			(*sp) += (*(*sp)) + 1;
		}
	} //end while
}
/**
 * @brief Draws an entire non-scaled sprite as opaque solid pixels (with mirror support).
 *
 * Used when the sprite has FLIP_HORIZ set but no transparency flags.
 * Iterates scanlines using LbSpriteDrawLineSolid for per-pixel copy.
 *
 * @param sp, sprWd, sprHt, r, nextRowDelta, left, mirror  — same as LbSpriteDrawTranspr.
 * @return LB_OK always.
 */
static inline TbError LbSpriteDrawSolid(const char *sp, short sprWd, short sprHt, unsigned char *r, int nextRowDelta, short left, const BOOL mirror)
{
	unsigned char *nextRow;
	long htIndex;
	nextRow = &(r[nextRowDelta]);
	htIndex = sprHt;
	// For all lines of the sprite
	while (1)
	{
		short x1;
		short lpos;
		x1 = sprWd;
		// Skip the pixels left before drawing area
		lpos = LbSpriteDrawLineSkipLeft(&sp, &x1, left);
		// Do the actual drawing
		LbSpriteDrawLineSolid(&sp, &r, &x1, lpos, mirror);
		// Go to next line
		htIndex--;
		if (htIndex == 0)
			return LB_OK;
		LbSpriteDrawLineSkipToEol(&sp, &x1);
		r = nextRow;
		nextRow += nextRowDelta;
	} //end while
	return LB_OK;
}

/**
 * @brief Draws one scanline using memcpy for maximum throughput (no mirror, no blend).
 *
 * Fastest non-scaled path: when no flipping or transparency is active,
 * opaque pixel runs are copied with memcpy and skip runs advance the
 * output pointer directly.
 *
 * @param[in,out] sp   RLE stream cursor.
 * @param[in,out] r    Framebuffer output cursor.
 * @param[in,out] x1   Remaining visible width.
 * @param lpos         Partial skip from LbSpriteDrawLineSkipLeft.
 */
static inline void LbSpriteDrawLineFastCpy(const char **sp, unsigned char **r, short *x1, short lpos)
{
	char schr;
	unsigned char drawOut;
	if (lpos > 0)
	{
		// Draw the part of current block which exceeds value of 'lpos'
		schr = *(*sp);
		if (schr < 0)
		{
			drawOut = -schr - lpos;
			if (drawOut > (*x1))
				drawOut = (*x1);
			(*r) += drawOut;
			(*sp)++;
		}
		else
		{
			drawOut = schr - lpos;
			if (drawOut > (*x1))
				drawOut = (*x1);
			// LbDrawBufferSolid already advances the output pointer
			LbDrawBufferSolid(r, (*sp) + (lpos + 1), drawOut, false);
			(*sp) += (*(*sp)) + 1;
		}
		(*x1) -= drawOut;
	}
	// Draw the visible part of a sprite
	while ((*x1) > 0)
	{
		schr = *(*sp);
		if (schr == 0)
		{ // EOL, breaking line loop
			break;
		}
		if (schr < 0)
		{ // Skipping some pixels
			(*x1) += schr;
			(*r) -= *(*sp);
			(*sp)++;
		}
		else
			//if ( schr > 0 )
		{ // Drawing some pixels
			drawOut = schr;
			if (drawOut >= (*x1))
				drawOut = (*x1);
			memcpy((*r), (*sp) + 1, drawOut);
			(*x1) -= schr;
			(*r) += schr;
			(*sp) += (*(*sp)) + 1;
		}
	} //end while
}
/**
 * @brief Draws an entire non-scaled sprite using the fast memcpy path.
 *
 * Selected when no flipping or transparency flags are set — the fastest
 * non-scaled rendering path.  Each scanline is processed by
 * LbSpriteDrawLineFastCpy.
 *
 * @param sp, sprWd, sprHt, r, nextRowDelta, left, mirror  — same as LbSpriteDrawTranspr.
 * @return LB_OK always.
 */
static inline TbError LbSpriteDrawFastCpy(const char *sp, short sprWd, short sprHt, unsigned char *r, int nextRowDelta, short left, const BOOL mirror)
{
	unsigned char *nextRow;
	long htIndex;
	nextRow = &(r[nextRowDelta]);
	htIndex = sprHt;
	// For all lines of the sprite
	while (1)
	{
		short x1;
		short lpos;
		x1 = sprWd;
		// Skip the pixels left before drawing area
		lpos = LbSpriteDrawLineSkipLeft(&sp, &x1, left);
		// Do the actual drawing
		LbSpriteDrawLineFastCpy(&sp, &r, &x1, lpos);
		// Go to next line
		htIndex--;
		if (htIndex == 0)
			return LB_OK;
		LbSpriteDrawLineSkipToEol(&sp, &x1);
		r = nextRow;
		nextRow += nextRowDelta;
	} //end while
	return LB_OK;
}

/**
 * @brief Fills output pixels with a single colour using ghost-table transparency.
 *
 * Like LbDrawBufferTranspr but uses a fixed colour index instead of reading
 * from sprite data.  The ghost-table lookup direction depends on TRANSPAR4.
 *
 * @param[in,out] buf_out  Output cursor; advanced by buf_len.
 * @param colour           The palette colour to render.
 * @param buf_len          Number of pixels to fill.
 * @param mirror           If true, output pointer decrements.
 */
static inline void LbDrawBufferOneColour(unsigned char **buf_out, const TbColour colour, const int buf_len, const BOOL mirror)
{
	int i;
	if (mirror)
	{
		if (_lbCurrentDrawFlags & Lb_SPRITE_TRANSPAR4)
		{
			for (i = 0; i < buf_len; i++)
			{
				**buf_out = lbDisplay.Spooky[256 * colour.Index + **buf_out];
				(*buf_out)--;
			}
		}
		else
		{
			for (i = 0; i < buf_len; i++)
			{
				**buf_out = lbDisplay.Spooky[256 * **buf_out + colour.Index];
				(*buf_out)--;
			}
		}
	}
	else
	{
		if (_lbCurrentDrawFlags & Lb_SPRITE_TRANSPAR4)
		{
			for (i = 0; i < buf_len; i++)
			{
				**buf_out = lbDisplay.Spooky[256 * colour.Index + **buf_out];
				(*buf_out)++;
			}
		}
		else
		{
			for (i = 0; i < buf_len; i++)
			{
				**buf_out = lbDisplay.Spooky[256 * **buf_out + colour.Index];
				(*buf_out)++;
			}
		}
	}
}

/**
 * @brief Draws one scanline of a single-colour sprite with transparency blending.
 *
 * Uses the sprite's RLE structure only for run lengths (opaque vs skip);
 * all opaque pixels are filled with 'colour' via LbDrawBufferOneColour.
 *
 * @param[in,out] sp   RLE stream cursor.
 * @param[in,out] r    Framebuffer output cursor.
 * @param[in,out] x1   Remaining visible width.
 * @param colour       Palette colour to fill.
 * @param lpos         Partial skip from LbSpriteDrawLineSkipLeft.
 * @param mirror       If true, output moves right-to-left.
 */
static inline void LbSpriteDrawLineTrOneColour(const char **sp, unsigned char **r, short *x1, TbColour colour, short lpos, const BOOL mirror)
{
	char schr;
	unsigned char drawOut;
	// Draw any unfinished block, which should be only partially visible
	if (lpos > 0)
	{
		schr = *(*sp);
		if (schr < 0)
		{
			drawOut = -schr - lpos;
			if (drawOut > (*x1))
				drawOut = (*x1);
			if (mirror)
				(*r) -= drawOut;
			else
				(*r) += drawOut;
			(*sp)++;

		}
		else
		{
			// Draw the part of current block which exceeds value of 'lpos'
			drawOut = schr - lpos;
			if (drawOut > (*x1))
				drawOut = (*x1);
			LbDrawBufferOneColour(r, colour, drawOut, mirror);
			// Update positions and break the skipping loop
			(*sp) += (*(*sp)) + 1;
		}
		(*x1) -= drawOut;
	}
	// Draw the visible part of a sprite
	while ((*x1) > 0)
	{
		schr = *(*sp);
		if (schr == 0)
		{ // EOL, breaking line loop
			break;
		}
		if (schr < 0)
		{ // Skipping some pixels
			(*x1) += schr;
			if (mirror)
				(*r) += *(*sp);
			else
				(*r) -= *(*sp);
			(*sp)++;
		}
		else
			//if ( schr > 0 )
		{ // Drawing some pixels
			drawOut = schr;
			if (drawOut >= (*x1))
				drawOut = (*x1);
			LbDrawBufferOneColour(r, colour, drawOut, mirror);
			(*x1) -= schr;
			(*sp) += (*(*sp)) + 1;
		}
	} //end while
}

/**
 * @brief Draws an entire single-colour sprite with transparency blending.
 *
 * Entry point for one-colour transparent sprites (e.g. selection highlights).
 * Each scanline uses LbSpriteDrawLineTrOneColour.
 *
 * @param sp, sprWd, sprHt, r, colour, nextRowDelta, left, mirror  — standard draw params.
 * @return LB_OK always.
 */
static inline TbError LbSpriteDrawTrOneColour(const char *sp, short sprWd, short sprHt, unsigned char *r, TbColour colour, int nextRowDelta, short left, const BOOL mirror)
{
	unsigned char *nextRow;
	long htIndex;
	nextRow = &(r[nextRowDelta]);
	htIndex = sprHt;
	// For all lines of the sprite
	while (1)
	{
		short x1;
		short lpos;
		x1 = sprWd;
		// Skip the pixels left before drawing area
		lpos = LbSpriteDrawLineSkipLeft(&sp, &x1, left);
		// Do the actual drawing
		LbSpriteDrawLineTrOneColour(&sp, &r, &x1, colour, lpos, mirror);
		// Go to next line
		htIndex--;
		if (htIndex == 0)
			return LB_OK;
		LbSpriteDrawLineSkipToEol(&sp, &x1);
		r = nextRow;
		nextRow += nextRowDelta;
	} //end while
	return LB_OK;
}

/**
 * @brief Fills output pixels with a single solid colour (no blending).
 *
 * @param[in,out] buf_out  Output cursor.
 * @param colour           Palette colour to write.
 * @param buf_len          Number of pixels to fill.
 * @param mirror           If true, output pointer decrements.
 */
static inline void LbDrawBufferOneColorSolid(unsigned char **buf_out, const TbColour colour, const int buf_len, const BOOL mirror)
{
	int i;
	if (mirror)
	{
		for (i = 0; i < buf_len; i++)
		{
			**buf_out = colour.Index;
			(*buf_out)--;
		}
	}
	else
	{
		for (i = 0; i < buf_len; i++)
		{
			**buf_out = colour.Index;
			(*buf_out)++;
		}
	}
}

/**
 * @brief Draws one scanline of a single-colour sprite as solid (opaque) pixels.
 *
 * Used when FLIP_HORIZ is set but no transparency flags are active.
 * All opaque runs are filled with 'colour' via LbDrawBufferOneColorSolid.
 *
 * @param[in,out] sp   RLE stream cursor.
 * @param[in,out] r    Framebuffer output cursor.
 * @param[in,out] x1   Remaining visible width.
 * @param colour       Palette colour to fill.
 * @param lpos         Partial skip from LbSpriteDrawLineSkipLeft.
 * @param mirror       If true, output moves right-to-left.
 */
static inline void LbSpriteDrawLineSlOneColour(const char **sp, unsigned char **r, short *x1, TbColour colour, short lpos, const BOOL mirror)
{
	char schr;
	unsigned char drawOut;
	// Draw any unfinished block, which should be only partially visible
	if (lpos > 0)
	{
		schr = *(*sp);
		if (schr < 0)
		{
			drawOut = -schr - lpos;
			if (drawOut > (*x1))
				drawOut = (*x1);
			(*r) -= drawOut;
			(*sp)++;
		}
		else
		{
			// Draw the part of current block which exceeds value of 'lpos'
			drawOut = schr - lpos;
			if (drawOut > (*x1))
				drawOut = (*x1);
			LbDrawBufferOneColorSolid(r, colour, drawOut, mirror);
			// Update positions and break the skipping loop
			(*sp) += (*(*sp)) + 1;
		}
		(*x1) -= drawOut;
	}
	// Draw the visible part of a sprite
	while ((*x1) > 0)
	{
		schr = *(*sp);
		if (schr == 0)
		{ // EOL, breaking line loop
			break;
		}
		if (schr < 0)
		{ // Skipping some pixels
			(*x1) += schr;
			(*r) += *(*sp);
			(*sp)++;
		}
		else
			//if ( schr > 0 )
		{ // Drawing some pixels
			drawOut = schr;
			if (drawOut >= (*x1))
				drawOut = (*x1);
			LbDrawBufferOneColorSolid(r, colour, drawOut, mirror);
			(*x1) -= schr;
			(*sp) += (*(*sp)) + 1;
		}
	} //end while
}

/**
 * @brief Draws an entire single-colour sprite as solid (opaque, mirrored) pixels.
 *
 * Used for one-colour sprites with FLIP_HORIZ but no transparency.
 * Each scanline uses LbSpriteDrawLineSlOneColour.
 *
 * @param sp, sprWd, sprHt, r, colour, nextRowDelta, left, mirror  — standard draw params.
 * @return LB_OK always.
 */
static inline TbError LbSpriteDrawSlOneColour(const char *sp, short sprWd, short sprHt, unsigned char *r, TbColour colour, int nextRowDelta, short left, const BOOL mirror)
{
	unsigned char *nextRow;
	long htIndex;
	nextRow = &(r[nextRowDelta]);
	htIndex = sprHt;
	// For all lines of the sprite
	while (1)
	{
		short x1;
		short lpos;
		x1 = sprWd;
		// Skip the pixels left before drawing area
		lpos = LbSpriteDrawLineSkipLeft(&sp, &x1, left);
		// Do the actual drawing
		LbSpriteDrawLineSlOneColour(&sp, &r, &x1, colour, lpos, mirror);
		// Go to next line
		htIndex--;
		if (htIndex == 0)
			return LB_OK;
		LbSpriteDrawLineSkipToEol(&sp, &x1);
		r = nextRow;
		nextRow += nextRowDelta;
	} //end while
	return LB_OK;
}

/**
 * @brief Draws one scanline of a single-colour sprite using memset (fastest path).
 *
 * No mirror, no blending.  Opaque runs become memset(r, colour, len) calls
 * for maximum throughput.
 *
 * @param[in,out] sp   RLE stream cursor.
 * @param[in,out] r    Framebuffer output cursor.
 * @param[in,out] x1   Remaining visible width.
 * @param colour       Palette colour to fill.
 * @param lpos         Partial skip from LbSpriteDrawLineSkipLeft.
 */
static inline void LbSpriteDrawLineFCOneColour(const char **sp, unsigned char **r, short *x1, TbColour colour, short lpos)
{
	char schr;
	unsigned char drawOut;
	if (lpos > 0)
	{
		// Draw the part of current block which exceeds value of 'lpos'
		schr = *(*sp);
		if (schr < 0)
		{
			drawOut = -schr - lpos;
			if (drawOut > (*x1))
				drawOut = (*x1);
			(*r) += drawOut;
			(*sp)++;
		}
		else
		{
			drawOut = schr - lpos;
			if (drawOut > (*x1))
				drawOut = (*x1);
			// LbDrawBufferOneColorSolid already advances the output pointer
			LbDrawBufferOneColorSolid(r, colour, drawOut, false);
			(*sp) += (*(*sp)) + 1;
		}
		(*x1) -= drawOut;
	}
	// Draw the visible part of a sprite
	while ((*x1) > 0)
	{
		schr = *(*sp);
		if (schr == 0)
		{ // EOL, breaking line loop
			break;
		}
		if (schr < 0)
		{ // Skipping some pixels
			(*x1) += schr;
			(*r) -= *(*sp);
			(*sp)++;
		}
		else
			//if ( schr > 0 )
		{ // Drawing some pixels
			drawOut = schr;
			if (drawOut >= (*x1))
				drawOut = (*x1);
			memset((*r), colour.Index, drawOut);
			(*x1) -= schr;
			(*r) += schr;
			(*sp) += (*(*sp)) + 1;
		}
	} //end while
}

/**
 * @brief Draws an entire single-colour sprite using the fast memset path.
 *
 * Fastest one-colour rendering: no flip, no transparency.
 * Each scanline uses LbSpriteDrawLineFCOneColour.
 *
 * @param sp, sprWd, sprHt, r, colour, nextRowDelta, left, mirror  — standard draw params.
 * @return LB_OK always.
 */
static inline TbError LbSpriteDrawFCOneColour(const char *sp, short sprWd, short sprHt, unsigned char *r, TbColour colour, int nextRowDelta, short left, const BOOL mirror)
{
	unsigned char *nextRow;
	long htIndex;
	nextRow = &(r[nextRowDelta]);
	htIndex = sprHt;
	// For all lines of the sprite
	while (1)
	{
		short x1;
		short lpos;
		x1 = sprWd;
		// Skip the pixels left before drawing area
		lpos = LbSpriteDrawLineSkipLeft(&sp, &x1, left);
		// Do the actual drawing
		LbSpriteDrawLineFCOneColour(&sp, &r, &x1, colour, lpos);
		// Go to next line
		htIndex--;
		if (htIndex == 0)
			return LB_OK;
		LbSpriteDrawLineSkipToEol(&sp, &x1);
		r = nextRow;
		nextRow += nextRowDelta;
	} //end while
	return LB_OK;
}

/**
 * @brief Alignment-aware DWORD-at-a-time memory copy (forward direction only).
 *
 * Used by the up-scaling template to duplicate rendered scanlines.  First aligns
 * dst to a 4-byte boundary by copying individual bytes (up to 3), then copies
 * the bulk as DWORD (4-byte) transfers, and finally handles any trailing bytes.
 *
 * This replicates the original ASM's REP MOVSD pattern for maximum throughput
 * on the original Pentium-era hardware.
 *
 * @param dst  Destination buffer (framebuffer row to fill).
 * @param src  Source buffer (previously rendered row to duplicate).
 * @param len  Number of bytes to copy.
 */
void LbPixelBlockCopyForward(UBYTE * dst, const UBYTE * src, long len)
{
	UBYTE px;
	ULONG pxquad;
	if (!((int)dst & 3) || ((px = *src, ++src, *dst = px, ++dst, --len, len)
		&& (!((int)dst & 3) || ((px = *src, ++src, *dst = px, ++dst, --len, len)
			&& (!((int)dst & 3) || (px = *src, ++src, *dst = px, ++dst, --len, len))))))
	{
		long l;
		for (l = len >> 2; l > 0; l--)
		{
			pxquad = *(ULONG *)src;
			src += sizeof(ULONG);
			*(ULONG *)dst = pxquad;
			dst += sizeof(ULONG);
		}
		if (len & 3)
		{
			*dst = *src;
			if ((len & 3) != 1)
			{
				*(dst + 1) = *(src + 1);
				if ((len & 3) != 2)
					*(dst + 2) = *(src + 2);
			}
		}
	}
}

// ============================================================================
// Scaled sprite rendering — compile-time template specialization
// ============================================================================
//
// The original Bullfrog ASM had 16 nearly identical functions covering every
// combination of:
//   - Direction:  RL (right-to-left, horizontally flipped) vs LR (left-to-right)
//   - Blend mode: Solid (direct copy), Remap (palette LUT), Trans1, Trans2
//   - Scale:      Up (pixel duplication) vs Down (pixel skipping)
//
// These are now expressed as two C++ function templates parameterised on
// SprDir and SprBlend.  `if constexpr` selects the correct code path at
// compile time, so each of the 16 instantiations compiles to the same machine
// code as the original hand-written version — zero runtime overhead.
//
// The scaling arrays (xsteps_array / ysteps_array) are laid out as pairs:
//   [2*i+0] = pixel position in the output buffer
//   [2*i+1] = number of output pixels this source pixel maps to (delta)
//
// For down-scaling, delta is 0 or 1 (pixel is either drawn or skipped).
// For up-scaling,   delta >= 1 (each source pixel fills 1 or more output pixels).
// ============================================================================

/** @brief Sprite draw direction.  RL = right-to-left (FLIP_HORIZ), LR = left-to-right (normal). */
enum class SprDir : int { RL = -1, LR = 1 };

/**
 * @brief Sprite pixel blend mode.
 *
 * | Mode   | Pixel operation                                             |
 * |--------|-------------------------------------------------------------|
 * | Solid  | Direct copy: dest = sprite_colour                          |
 * | Remap  | Palette remap: dest = table[sprite_colour]                  |
 * | Trans1 | Ghost blend:  dest = table[(sprite_colour << 8) | dest]     |
 * | Trans2 | Ghost blend:  dest = table[sprite_colour | (dest << 8)]     |
 */
enum class SprBlend { Solid, Remap, Trans1, Trans2 };

/**
 * @brief Draws a scaled-down sprite (dest <= source dimensions).
 *
 * For down-scaling, each source pixel is either drawn (delta == 1) or
 * skipped (delta == 0) based on the pre-computed scaling step arrays.
 * No pixel duplication occurs — at most one output pixel per source pixel.
 *
 * Template parameters select the direction and blend mode at compile time:
 *   - Dir:   SprDir::RL advances output left, SprDir::LR advances right.
 *   - Blend: SprBlend::Solid / Remap / Trans1 / Trans2 (see SprBlend docs).
 *
 * @param outbuf     Framebuffer pointer to the starting output position.
 * @param scanline   Bytes per row (negative if FLIP_VERTIC).
 * @param outheight  Total output surface height (for Y clipping guard).
 * @param xstep      Horizontal scaling step array (pairs: position, delta).
 * @param ystep      Vertical scaling step array (pairs: position, delta).
 * @param sprite     The RLE sprite to render.
 * @param table      Blend/remap lookup table (nullptr for Solid mode).
 * @return LB_OK always.
 */
template<SprDir Dir, SprBlend Blend>
static TbError LbSpriteDrawScalingDown(UBYTE *outbuf, int scanline, int outheight,
	long *xstep, long *ystep, const TbSprite *sprite, const UBYTE *table)
{
	constexpr int xdir = (Dir == SprDir::LR) ? 2 : -2;

	int ystep_delta = 2;
	if (scanline < 0)
		ystep_delta = -2;
	SBYTE *sprdata = sprite->Data;
	long *ycurstep = ystep;

	for (int h = sprite->Height; h > 0; h--)
	{
		if (ycurstep[1] != 0)
		{
			long *xcurstep = xstep;
			UBYTE *out_end = outbuf;
			while (1)
			{
				long pxlen = (signed char)*sprdata;
				sprdata++;
				if (pxlen == 0)
					break;
				if (pxlen < 0)
				{
					pxlen = -pxlen;
					if constexpr (Dir == SprDir::RL) {
						out_end -= xcurstep[0] + xcurstep[1];
						xcurstep -= 2 * pxlen;
						out_end += xcurstep[0] + xcurstep[1];
					} else {
						out_end -= xcurstep[0];
						xcurstep += 2 * pxlen;
						out_end += xcurstep[0];
					}
				}
				else
				{
					for (; pxlen > 0; pxlen--)
					{
						if (xcurstep[1] > 0)
						{
							if constexpr (Blend == SprBlend::Solid) {
								*out_end = (unsigned char)*sprdata;
							} else if constexpr (Blend == SprBlend::Remap) {
								*out_end = table[(unsigned char)*sprdata];
							} else if constexpr (Blend == SprBlend::Trans1) {
								unsigned int pxmap = ((unsigned char)(*sprdata) << 8);
								pxmap = (pxmap & ~0x00ff) | ((*out_end));
								*out_end = table[pxmap];
							} else { // Trans2
								unsigned int pxmap = ((unsigned char)(*sprdata));
								pxmap = (pxmap & ~0xff00) | ((*out_end) << 8);
								*out_end = table[pxmap];
							}
							if constexpr (Dir == SprDir::RL)
								out_end--;
							else
								out_end++;
						}
						sprdata++;
						xcurstep += xdir;
					}
				}
			}
			outbuf += scanline;
		}
		else
		{
			while (1)
			{
				long pxlen = (signed char)*sprdata;
				sprdata++;
				if (pxlen == 0)
					break;
				if (pxlen > 0)
					sprdata += pxlen;
			}
		}
		ycurstep += ystep_delta;
	}
	return LB_OK;
}

/**
 * @brief Draws a scaled-up sprite (dest > source dimensions).
 *
 * For up-scaling, each source pixel maps to one or more output pixels
 * (delta >= 1).  The Y-duplication strategy differs by blend mode:
 *
 *   - **Solid / Remap**: Render the first scanline normally, then duplicate
 *     it to subsequent output rows using LbPixelBlockCopyForward (fast
 *     row-copy).  This is safe because opaque/remapped pixels don't depend
 *     on the destination.
 *
 *   - **Trans1 / Trans2**: Each duplicated row must be re-rendered from the
 *     sprite data because the ghost-table blend reads the existing destination
 *     pixel, which differs per row.  A saved pointer (prevdata) rewinds the
 *     RLE stream for each Y repeat.
 *
 * @param outbuf     Framebuffer pointer to the starting output position.
 * @param scanline   Bytes per row (negative if FLIP_VERTIC).
 * @param outheight  Total output surface height (for Y clipping guard).
 * @param xstep      Horizontal scaling step array (pairs: position, delta).
 * @param ystep      Vertical scaling step array (pairs: position, delta).
 * @param sprite     The RLE sprite to render.
 * @param table      Blend/remap lookup table (nullptr for Solid mode).
 * @return LB_OK always.
 */
template<SprDir Dir, SprBlend Blend>
static TbError LbSpriteDrawScalingUp(UBYTE *outbuf, int scanline, int outheight,
	long *xstep, long *ystep, const TbSprite *sprite, const UBYTE *table)
{
	constexpr int xdir = (Dir == SprDir::LR) ? 2 : -2;
	constexpr bool useRowCopy = (Blend == SprBlend::Solid || Blend == SprBlend::Remap);

	int ystep_delta = 2;
	if (scanline < 0)
		ystep_delta = -2;
	SBYTE *sprdata = sprite->Data;
	long *ycurstep = ystep;

	for (int h = sprite->Height; h > 0; h--)
	{
		if (ycurstep[1] != 0)
		{
			int ydup = ycurstep[1];
			if (ycurstep[0] + ydup > outheight)
				ydup = outheight - ycurstep[0];

			if constexpr (useRowCopy)
			{
				// Solid/Remap: render first row, copy for remaining rows
				long *xcurstep = xstep;
				UBYTE *out_end = outbuf;
				while (1)
				{
					long pxlen = (signed char)*sprdata;
					sprdata++;
					if (pxlen == 0)
						break;
					if (pxlen < 0)
					{
						pxlen = -pxlen;
						if constexpr (Dir == SprDir::RL) {
							out_end -= xcurstep[0] + xcurstep[1];
							xcurstep -= 2 * pxlen;
							out_end += xcurstep[0] + xcurstep[1];
						} else {
							out_end -= xcurstep[0];
							xcurstep += 2 * pxlen;
							out_end += xcurstep[0];
						}
					}
					else
					{
						UBYTE *out_start = out_end;
						for (; pxlen > 0; pxlen--)
						{
							int xdup = xcurstep[1];
							if (xcurstep[0] + xdup > abs(scanline))
								xdup = abs(scanline) - xcurstep[0];
							if (xdup > 0)
							{
								unsigned char pxval;
								if constexpr (Blend == SprBlend::Solid)
									pxval = (unsigned char)*sprdata;
								else
									pxval = table[(unsigned char)*sprdata];
								for (; xdup > 0; xdup--)
								{
									*out_end = pxval;
									if constexpr (Dir == SprDir::RL)
										out_end--;
									else
										out_end++;
								}
							}
							sprdata++;
							xcurstep += xdir;
						}
						int ycur = ydup - 1;
						if (ycur > 0)
						{
							int solid_len;
							if constexpr (Dir == SprDir::RL) {
								solid_len = (int)(out_start - out_end);
								out_start = out_end;
								solid_len++;
							} else {
								solid_len = (int)(out_end - out_start);
							}
							UBYTE *out_line = out_start + scanline;
							for (; ycur > 0; ycur--)
							{
								if (solid_len > 0)
									LbPixelBlockCopyForward(out_line, out_start, solid_len);
								out_line += scanline;
							}
						}
					}
				}
				outbuf += scanline;
				int ycur = ydup - 1;
				for (; ycur > 0; ycur--)
					outbuf += scanline;
			}
			else
			{
				// Trans1/Trans2: re-render from sprite data for each duplicated row
				SBYTE *prevdata = sprdata;
				while (ydup > 0)
				{
					sprdata = prevdata;
					long *xcurstep = xstep;
					UBYTE *out_end = outbuf;
					while (1)
					{
						long pxlen = (signed char)*sprdata;
						sprdata++;
						if (pxlen == 0)
							break;
						if (pxlen < 0)
						{
							pxlen = -pxlen;
							if constexpr (Dir == SprDir::RL) {
								out_end -= xcurstep[0] + xcurstep[1];
								xcurstep -= 2 * pxlen;
								out_end += xcurstep[0] + xcurstep[1];
							} else {
								out_end -= xcurstep[0];
								xcurstep += 2 * pxlen;
								out_end += xcurstep[0];
							}
						}
						else
						{
							for (; pxlen > 0; pxlen--)
							{
								int xdup = xcurstep[1];
								if (xcurstep[0] + xdup > abs(scanline))
									xdup = abs(scanline) - xcurstep[0];
								if (xdup > 0)
								{
									unsigned int pxmap;
									if constexpr (Blend == SprBlend::Trans1)
										pxmap = ((unsigned char)(*sprdata) << 8);
									else
										pxmap = ((unsigned char)(*sprdata));
									for (; xdup > 0; xdup--)
									{
										if constexpr (Blend == SprBlend::Trans1)
											pxmap = (pxmap & ~0x00ff) | ((*out_end));
										else
											pxmap = (pxmap & ~0xff00) | ((*out_end) << 8);
										*out_end = table[pxmap];
										if constexpr (Dir == SprDir::RL)
											out_end--;
										else
											out_end++;
									}
								}
								sprdata++;
								xcurstep += xdir;
							}
						}
					}
					outbuf += scanline;
					ydup--;
				}
			}
		}
		else
		{
			while (1)
			{
				long pxlen = (signed char)*sprdata;
				sprdata++;
				if (pxlen == 0)
					break;
				if (pxlen > 0)
					sprdata += pxlen;
			}
		}
		ycurstep += ystep_delta;
	}
	return LB_OK;
}

// ============================================================================
// Scaling step array setup functions
// ============================================================================
//
// These functions populate the xsteps_array / ysteps_array pair arrays that
// drive the scaled rendering templates.  Each array element is a pair:
//   [2*i+0] = accumulated pixel position in the output (destination) buffer
//   [2*i+1] = delta (number of output pixels this source pixel maps to)
//
// Two variants exist for each axis (width / height):
//   - "Clipped": output positions are clamped to [0, gwidth/gheight) so that
//     pixels outside the viewport produce delta == 0 and are skipped.
//   - "Simple":  no clamping — used when the sprite is known to be fully
//     inside the viewport.  Slightly faster due to fewer branches.
//
// The scaling factor is computed as a 16.16 fixed-point ratio:
//   factor = (dest_size << 16) / source_size
// and the initial half-step (factor >> 1) + (offset << 16) centres the first
// output pixel correctly.
// ============================================================================

/**
 * @brief Zeros out a horizontal scaling step array.
 *
 * Sets every pair to (0, 0), meaning "no pixel, zero width".
 * Called when swidth or dwidth is zero/negative.
 *
 * @param xsteps_arr  The step array to clear.
 * @param swidth      Source sprite width (number of pairs to clear).
 */
void LbSpriteClearScalingWidthArray(long* xsteps_arr, long swidth)
{
	int i;
	long* pwidth;
	pwidth = xsteps_arr;
	for (i = 0; i < swidth; i++)
	{
		pwidth[0] = 0;
		pwidth[1] = 0;
		pwidth += 2;
	}
}

/** @brief Clears the primary horizontal scaling array. */
void LbSpriteClearScalingWidth(void)
{
	LbSpriteClearScalingWidthArray(xsteps_array, SPRITE_SCALING_XSTEPS);
}

/** @brief Clears the alpha-channel horizontal scaling array. */
void LbSpriteClearAlphaScalingWidth(void)
{
	LbSpriteClearScalingWidthArray(alpha_xsteps_array, SPRITE_SCALING_XSTEPS);
}

/**
 * @brief Populates a horizontal scaling step array with viewport clipping.
 *
 * Computes the destination pixel position and delta for each source column,
 * clamping positions to [0, gwidth) so that columns falling outside the
 * viewport get delta == 0 and are skipped during rendering.
 *
 * @param xsteps_arr  Output step array (must hold at least swidth pairs).
 * @param x           Destination X offset (may be negative if sprite extends left of viewport).
 * @param swidth      Source sprite width in pixels.
 * @param dwidth      Desired destination width in pixels.
 * @param gwidth      Viewport width (clipping boundary).
 */
void LbSpriteSetScalingWidthClippedArray(long* xsteps_arr, long x, long swidth, long dwidth, long gwidth)
{
	long* pwidth;
	long pxpos;
	pwidth = xsteps_arr;
	long factor = (dwidth << 16) / swidth;
	long tmp = (factor >> 1) + (x << 16);
	pxpos = tmp >> 16;
	if (pxpos < 0)
		pxpos = 0;
	if (pxpos >= gwidth)
		pxpos = gwidth;
	long w = swidth;
	do {
		pwidth[0] = pxpos;
		tmp += factor;
		long pxend = tmp >> 16;
		if (pxend < 0)
			pxend = 0;
		if (pxend >= gwidth)
			pxend = gwidth;
		long delta = pxend - pxpos;
		pwidth[1] = delta;
		pxpos += delta;
		w--;
		pwidth += 2;
	} while (w > 0);
}

/** @brief Convenience wrapper: populates the primary xsteps_array with clipping. */
void LbSpriteSetScalingWidthClipped(long x, long swidth, long dwidth, long gwidth)
{
#if SPR_VERBOSE
	Pop3Debug::trace("starting %d -> %d at %d", (int)swidth, (int)dwidth, (int)x);
#endif
	if (swidth > SPRITE_SCALING_XSTEPS)
		swidth = SPRITE_SCALING_XSTEPS;
	LbSpriteSetScalingWidthClippedArray(xsteps_array, x, swidth, dwidth, gwidth);
}

/**
 * @brief Populates a horizontal scaling step array without viewport clipping.
 *
 * Faster than the clipped variant — no boundary checks.  Used when the
 * sprite is known to be fully within the viewport.
 *
 * Internally unrolls 8 pairs at a time for improved throughput.
 *
 * @param xsteps_arr  Output step array.
 * @param x           Destination X offset.
 * @param swidth      Source sprite width.
 * @param dwidth      Desired destination width.
 */
void LbSpriteSetScalingWidthSimpleArray(long* xsteps_arr, long x, long swidth, long dwidth)
{
	long* pwidth;
	long cwidth;
	pwidth = xsteps_arr;
	long factor = (dwidth << 16) / swidth;
	long tmp = (factor >> 1) + (x << 16);
	cwidth = tmp >> 16;
	long w = swidth;
	do {
		int i;
		for (i = 0; i < 16; i += 2)
		{
			pwidth[i] = cwidth;
			tmp += factor;
			pwidth[i + 1] = (tmp >> 16) - cwidth;
			cwidth = (tmp >> 16);
			w--;
			if (w <= 0)
				break;
		}
		pwidth += 16;
	} while (w > 0);
}

/** @brief Convenience wrapper: populates the primary xsteps_array without clipping. */
void LbSpriteSetScalingWidthSimple(long x, long swidth, long dwidth)
{
#if SPR_VERBOSE
	Pop3Debug::trace("starting %d -> %d at %d", (int)swidth, (int)dwidth, (int)x);
#endif
	if (swidth > SPRITE_SCALING_XSTEPS)
		swidth = SPRITE_SCALING_XSTEPS;
	LbSpriteSetScalingWidthSimpleArray(xsteps_array, x, swidth, dwidth);
}

/**
 * @brief Zeros out a vertical scaling step array.
 *
 * Sets every pair to (0, 0).  Called when sheight or dheight is zero/negative.
 *
 * @param ysteps_arr  The step array to clear.
 * @param sheight     Source sprite height (number of pairs to clear).
 */
void LbSpriteClearScalingHeightArray(long* ysteps_arr, long sheight)
{
	int i;
	long* pheight;
	pheight = ysteps_arr;
	for (i = 0; i < sheight; i++)
	{
		pheight[0] = 0;
		pheight[1] = 0;
		pheight += 2;
	}
}

/** @brief Clears the primary vertical scaling array. */
void LbSpriteClearScalingHeight(void)
{
	LbSpriteClearScalingHeightArray(ysteps_array, SPRITE_SCALING_YSTEPS);
}

/**
 * @brief Populates a vertical scaling step array with viewport clipping.
 *
 * Same algorithm as LbSpriteSetScalingWidthClippedArray but for the Y axis.
 * Positions are clamped to [0, gheight) so rows outside the viewport are skipped.
 *
 * @param ysteps_arr  Output step array.
 * @param y           Destination Y offset.
 * @param sheight     Source sprite height.
 * @param dheight     Desired destination height.
 * @param gheight     Viewport height (clipping boundary).
 */
void LbSpriteSetScalingHeightClippedArray(long* ysteps_arr, long y, long sheight, long dheight, long gheight)
{
	long* pheight;
	long lnpos;
	pheight = ysteps_arr;
	long factor = (dheight << 16) / sheight;
	long tmp = (factor >> 1) + (y << 16);
	lnpos = tmp >> 16;
	if (lnpos < 0)
		lnpos = 0;
	if (lnpos >= gheight)
		lnpos = gheight;
	long h = sheight;
	do {
		pheight[0] = lnpos;
		tmp += factor;
		long lnend = tmp >> 16;
		if (lnend < 0)
			lnend = 0;
		if (lnend >= gheight)
			lnend = gheight;
		long delta = lnend - lnpos;
		pheight[1] = delta;
		lnpos += delta;
		h--;
		pheight += 2;
	} while (h > 0);
}

/** @brief Convenience wrapper: populates the primary ysteps_array with clipping. */
void LbSpriteSetScalingHeightClipped(long y, long sheight, long dheight, long gheight)
{
#if SPR_VERBOSE
	Pop3Debug::trace("starting %d -> %d at %d", (int)sheight, (int)dheight, (int)y);
#endif
	if (sheight > SPRITE_SCALING_YSTEPS)
		sheight = SPRITE_SCALING_YSTEPS;
	LbSpriteSetScalingHeightClippedArray(ysteps_array, y, sheight, dheight, gheight);
}

/**
 * @brief Populates a vertical scaling step array without viewport clipping.
 *
 * Faster than the clipped variant.  Unrolls 8 pairs at a time.
 *
 * @param ysteps_arr  Output step array.
 * @param y           Destination Y offset.
 * @param sheight     Source sprite height.
 * @param dheight     Desired destination height.
 */
void LbSpriteSetScalingHeightSimpleArray(long* ysteps_arr, long y, long sheight, long dheight)
{
	long* pheight;
	long cheight;
	pheight = ysteps_arr;
	long factor = (dheight << 16) / sheight;
	long tmp = (factor >> 1) + (y << 16);
	cheight = tmp >> 16;
	long h = sheight;
	do {
		int i = 0;
		for (i = 0; i < 16; i += 2)
		{
			pheight[i] = cheight;
			tmp += factor;
			pheight[i + 1] = (tmp >> 16) - cheight;
			cheight = (tmp >> 16);
			h--;
			if (h <= 0)
				break;
		}
		pheight += 16;
	} while (h > 0);
}

/** @brief Convenience wrapper: populates the primary ysteps_array without clipping. */
void LbSpriteSetScalingHeightSimple(long y, long sheight, long dheight)
{
#if SPR_VERBOSE
	Pop3Debug::trace("starting %d -> %d at %d", (int)sheight, (int)dheight, (int)y);
#endif
	if (sheight > SPRITE_SCALING_YSTEPS)
		sheight = SPRITE_SCALING_YSTEPS;
	LbSpriteSetScalingHeightSimpleArray(ysteps_array, y, sheight, dheight);
}


/**
 * @brief Computes an absolute framebuffer address from (x, y) screen coordinates.
 *
 * @param x  Horizontal position in pixels.
 * @param y  Vertical position in pixels.
 * @return   Pointer into the current render area's pixel buffer at (x, y).
 */
inline UBYTE *LbDraw_GetAddress(SINT x, SINT y)
{
	return (_CURRENT_RENDER_AREA->mpData + x + y * _CURRENT_RENDER_AREA->mPitch);
}

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////// ABOVE IS IMPL //////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

// ============================================================================
// Public entry points
// ============================================================================
//
// These are the functions called by the rest of the engine to draw sprites.
// Each entry point:
//   1. Calls LbSpriteDrawPrepare() to clip the sprite to the viewport.
//   2. Inspects _lbCurrentDrawFlags to select the appropriate rendering path
//      (transparent / solid / fast-copy / single-colour / scaled).
//   3. Delegates to the corresponding internal drawing function or template.
//
// The function-pointer table (BfDraw_Sprite, BfDraw_OneColourSprite, etc.)
// initially points at these functions and can be hot-swapped to the original
// ASM wrappers via the ImGui debug window.
// ============================================================================

/**
 * @brief Draws a non-scaled sprite at screen coordinates (x, y).
 *
 * Rendering path selection based on draw flags:
 * - Transparency flags set  ->  LbSpriteDrawTranspr  (ghost-table blended)
 * - Horizontal flip set     ->  LbSpriteDrawSolid     (reversed scanline walk)
 * - Neither                 ->  LbSpriteDrawFastCpy   (memcpy-optimised)
 *
 * @param x    Screen X position (left edge of sprite).
 * @param y    Screen Y position (top edge of sprite).
 * @param spr  Pointer to the RLE-encoded TbSprite.
 * @return LB_OK on success, or an error code from LbSpriteDrawPrepare.
 */
TbError BullfrogLib::LbSpriteDraw(long x, long y, const TbSprite * spr)
{
	static TbSpriteDrawData spd{};
	TbError ret;

#if SPR_VERBOSE
	Pop3Debug::trace("%s:%d LbSpriteDraw at (%ld,%ld)", __FILE__, __LINE__, x, y);
#endif

	ret = LbSpriteDrawPrepare(&spd, x, y, spr);

	if (ret != LB_OK)
		return ret;
	else if (!spd.sp)
		return LB_OK;

	if ((_lbCurrentDrawFlags & (Lb_SPRITE_TRANSPAR4 | Lb_SPRITE_TRANSPAR8)) != 0)
		return LbSpriteDrawTranspr(spd.sp, spd.Wd, spd.Ht, spd.r, spd.nextRowDelta, spd.startShift, spd.mirror);
	else
		if ((_lbCurrentDrawFlags & Lb_SPRITE_FLIP_HORIZ) != 0)
			return LbSpriteDrawSolid(spd.sp, spd.Wd, spd.Ht, spd.r, spd.nextRowDelta, spd.startShift, spd.mirror);
		else
			return LbSpriteDrawFastCpy(spd.sp, spd.Wd, spd.Ht, spd.r, spd.nextRowDelta, spd.startShift, spd.mirror);
}

/**
 * @brief Draws a non-scaled single-colour sprite at screen coordinates (x, y).
 *
 * Every opaque pixel in the sprite is replaced with @p colour.  The shape
 * (transparent vs opaque) is preserved from the RLE data.
 *
 * Rendering path selection:
 * - Transparency flags set  ->  LbSpriteDrawTrOneColour  (ghost-table blended)
 * - Horizontal flip set     ->  LbSpriteDrawSlOneColour  (reversed scanline)
 * - Neither                 ->  LbSpriteDrawFCOneColour  (memset-optimised)
 *
 * @param x       Screen X position.
 * @param y       Screen Y position.
 * @param spr     Pointer to the RLE-encoded TbSprite.
 * @param colour  Palette index used for every opaque pixel.
 * @return LB_OK on success, or an error code from LbSpriteDrawPrepare.
 */
TbError BullfrogLib::LbSpriteDrawOneColour(long x, long y, const TbSprite * spr, const TbColour colour)
{
	TbSpriteDrawData spd;
	TbError ret;

	ret = LbSpriteDrawPrepare(&spd, x, y, spr);
	if (ret != LB_OK)
		return ret;
	if ((_lbCurrentDrawFlags & (Lb_SPRITE_TRANSPAR4 | Lb_SPRITE_TRANSPAR8)) != 0) {
		return LbSpriteDrawTrOneColour(spd.sp, spd.Wd, spd.Ht, spd.r, colour, spd.nextRowDelta, spd.startShift, spd.mirror);
	}
	else
		if ((_lbCurrentDrawFlags & Lb_SPRITE_FLIP_HORIZ) != 0) {
			return LbSpriteDrawSlOneColour(spd.sp, spd.Wd, spd.Ht, spd.r, colour, spd.nextRowDelta, spd.startShift, spd.mirror);
		}
		else {
			return LbSpriteDrawFCOneColour(spd.sp, spd.Wd, spd.Ht, spd.r, colour, spd.nextRowDelta, spd.startShift, spd.mirror);
		}
}

/**
 * @brief Draws a sprite using pre-computed scaling data (xsteps / ysteps arrays).
 *
 * This is the main scaled-sprite renderer.  The caller must have already
 * populated xsteps_array and ysteps_array (via LbSpriteSetScalingData or
 * manually) before calling this function.
 *
 * Steps performed:
 *   1. Snapshot display state via LbDisplayPrepare().
 *   2. Compute output buffer pointer and scanline direction from flip flags.
 *   3. Select one of 16 template instantiations based on:
 *      - Blend mode: Remap / Trans1 / Trans2 / Solid
 *      - Direction:  LR or RL (horizontal flip)
 *      - Scale mode: Up or Down (set by LbSpriteSetScalingData)
 *
 * @param posx    Logical X position (index into xsteps_array).
 * @param posy    Logical Y position (index into ysteps_array).
 * @param sprite  Pointer to the RLE-encoded TbSprite.
 * @return LB_OK on success, LB_ERROR if the sprite data is malformed.
 */
TbError BullfrogLib::LbSpriteDrawUsingScalingData(long posx, long posy, const TbSprite* sprite)
{
	long* xstep;
	long* ystep;
	int scanline;

	LbDisplayPrepare();

#if SPR_VERBOSE
	Pop3Debug::trace("at (%ld,%ld): drawing", posx, posy);
#endif

	{
		long sposx, sposy;
		sposx = posx;
		sposy = posy;

		scanline = (int)_CURRENT_RENDER_AREA->mPitch;
		if ((lbDisplay.DrawFlags & Lb_SPRITE_FLIP_HORIZ) != 0) {
			sposx = sprite->Width + posx - 1;
		}
		if ((lbDisplay.DrawFlags & Lb_SPRITE_FLIP_VERTIC) != 0) {
			sposy = sprite->Height + posy - 1;
			scanline = -(int)_CURRENT_RENDER_AREA->mPitch;
		}

		xstep = &xsteps_array[2 * sposx];
		ystep = &ysteps_array[2 * sposy];
	}

	UBYTE* outbuf;
	int outheight;
	{
		int gspos_x, gspos_y;
		gspos_y = ystep[0];
		if ((lbDisplay.DrawFlags & Lb_SPRITE_FLIP_VERTIC) != 0)
			gspos_y += ystep[1] - 1;
		gspos_x = xstep[0];
		if ((lbDisplay.DrawFlags & Lb_SPRITE_FLIP_HORIZ) != 0)
			gspos_x += xstep[1] - 1;

		gspos_x += lbDisplay.GraphicsWindowX;
		gspos_y += lbDisplay.GraphicsWindowY;

		outbuf = LbDraw_GetAddress(gspos_x, gspos_y);
		outheight = lbDisplay.GraphicsScreenHeight;
	}
	const bool xflip = (lbDisplay.DrawFlags & Lb_SPRITE_FLIP_HORIZ) != 0;

	if ((lbDisplay.DrawFlags & Lb_TEXT_UNDERLNSHADOW) != 0)
	{
		ASSERT(false); // BullfrogLib::lbSpriteReMapPtr not implemented.
		if (scale_up)
			return xflip ? LbSpriteDrawScalingUp<SprDir::RL, SprBlend::Remap>(outbuf, scanline, outheight, xstep, ystep, sprite, BullfrogLib::lbSpriteReMapPtr)
			             : LbSpriteDrawScalingUp<SprDir::LR, SprBlend::Remap>(outbuf, scanline, outheight, xstep, ystep, sprite, BullfrogLib::lbSpriteReMapPtr);
		else
			return xflip ? LbSpriteDrawScalingDown<SprDir::RL, SprBlend::Remap>(outbuf, scanline, outheight, xstep, ystep, sprite, BullfrogLib::lbSpriteReMapPtr)
			             : LbSpriteDrawScalingDown<SprDir::LR, SprBlend::Remap>(outbuf, scanline, outheight, xstep, ystep, sprite, BullfrogLib::lbSpriteReMapPtr);
	}
	else if ((lbDisplay.DrawFlags & Lb_SPRITE_TRANSPAR4) != 0)
	{
		ASSERTMSG(lbDisplay.Spooky != NULL, "lbDisplay.Spooky is null");
		if (scale_up)
			return xflip ? LbSpriteDrawScalingUp<SprDir::RL, SprBlend::Trans1>(outbuf, scanline, outheight, xstep, ystep, sprite, lbDisplay.Spooky)
			             : LbSpriteDrawScalingUp<SprDir::LR, SprBlend::Trans1>(outbuf, scanline, outheight, xstep, ystep, sprite, lbDisplay.Spooky);
		else
			return xflip ? LbSpriteDrawScalingDown<SprDir::RL, SprBlend::Trans1>(outbuf, scanline, outheight, xstep, ystep, sprite, lbDisplay.Spooky)
			             : LbSpriteDrawScalingDown<SprDir::LR, SprBlend::Trans1>(outbuf, scanline, outheight, xstep, ystep, sprite, lbDisplay.Spooky);
	}
	else if ((lbDisplay.DrawFlags & Lb_SPRITE_TRANSPAR8) != 0)
	{
		ASSERTMSG(lbDisplay.Spooky != NULL, "lbDisplay.Spooky is null");
		if (scale_up)
			return xflip ? LbSpriteDrawScalingUp<SprDir::RL, SprBlend::Trans2>(outbuf, scanline, outheight, xstep, ystep, sprite, lbDisplay.Spooky)
			             : LbSpriteDrawScalingUp<SprDir::LR, SprBlend::Trans2>(outbuf, scanline, outheight, xstep, ystep, sprite, lbDisplay.Spooky);
		else
			return xflip ? LbSpriteDrawScalingDown<SprDir::RL, SprBlend::Trans2>(outbuf, scanline, outheight, xstep, ystep, sprite, lbDisplay.Spooky)
			             : LbSpriteDrawScalingDown<SprDir::LR, SprBlend::Trans2>(outbuf, scanline, outheight, xstep, ystep, sprite, lbDisplay.Spooky);
	}
	else
	{
		if (scale_up)
			return xflip ? LbSpriteDrawScalingUp<SprDir::RL, SprBlend::Solid>(outbuf, scanline, outheight, xstep, ystep, sprite, nullptr)
			             : LbSpriteDrawScalingUp<SprDir::LR, SprBlend::Solid>(outbuf, scanline, outheight, xstep, ystep, sprite, nullptr);
		else
			return xflip ? LbSpriteDrawScalingDown<SprDir::RL, SprBlend::Solid>(outbuf, scanline, outheight, xstep, ystep, sprite, nullptr)
			             : LbSpriteDrawScalingDown<SprDir::LR, SprBlend::Solid>(outbuf, scanline, outheight, xstep, ystep, sprite, nullptr);
	}
}

/**
 * @brief Populates the global xsteps / ysteps scaling arrays.
 *
 * Decides whether to use the "simple" (no clipping) or "clipped" variant
 * for each axis based on the sprite's position relative to the viewport.
 *
 * Also sets the global @c scale_up flag:
 * - @c true  if dwidth > swidth OR dheight > sheight  (use ScalingUp template)
 * - @c false if dwidth <= swidth AND dheight <= sheight (use ScalingDown template)
 *
 * @param x        Destination X offset within the viewport.
 * @param y        Destination Y offset within the viewport.
 * @param swidth   Source sprite width.
 * @param sheight  Source sprite height.
 * @param dwidth   Desired destination width.
 * @param dheight  Desired destination height.
 */
void BullfrogLib::LbSpriteSetScalingData(long x, long y, long swidth, long sheight, long dwidth, long dheight)
{
	long gwidth = lbDisplay.GraphicsWindowWidth;
	long gheight = lbDisplay.GraphicsWindowHeight;
	scale_up = true;
	if ((dwidth <= swidth) && (dheight <= sheight))
		scale_up = false;
	// Checking whether to select simple scaling creation,
	// or more comprehensive one - with clipping
	if ((swidth <= 0) || (dwidth <= 0)) {
#if SPR_VERBOSE
		Pop3Debug::trace("tried scaling width %ld -> %ld", swidth, dwidth);
#endif
		LbSpriteClearScalingWidth();
	}
	else
		// Normally it would be enough to check if ((dwidth+y) >= gwidth),
		// but due to rounding we need to add swidth
		if ((x < 0) || ((dwidth + swidth + x) >= gwidth))
		{
			LbSpriteSetScalingWidthClipped(x, swidth, dwidth, gwidth);
		}
		else {
			LbSpriteSetScalingWidthSimple(x, swidth, dwidth);
		}
	if ((sheight <= 0) || (dheight <= 0)) {
#if SPR_VERBOSE
		Pop3Debug::trace("tried scaling height %ld -> %ld", sheight, dheight);
#endif
		LbSpriteClearScalingHeight();
	}
	else
		// Normally it would be enough to check if ((dheight+y) >= gheight),
		// but our simple rounding may enlarge the image
		if ((y < 0) || ((dheight + sheight + y) >= gheight))
		{
			LbSpriteSetScalingHeightClipped(y, sheight, dheight, gheight);
		}
		else {
			LbSpriteSetScalingHeightSimple(y, sheight, dheight);
		}
}

/**
 * @brief Convenience entry point: scales and draws a sprite in one call.
 *
 * Equivalent to calling LbSpriteSetScalingData() followed by
 * LbSpriteDrawUsingScalingData(0, 0, sprite).
 *
 * @param xpos        Screen X position.
 * @param ypos        Screen Y position.
 * @param sprite      Pointer to the RLE-encoded TbSprite.
 * @param dest_width  Desired width in pixels on screen.
 * @param dest_height Desired height in pixels on screen.
 * @return LB_OK on success, LB_ERROR on failure.
 */
TbError BullfrogLib::LbSpriteDrawScaled(long xpos, long ypos, const TbSprite * sprite, long dest_width, long dest_height)
{
#if SPR_VERBOSE
	Pop3Debug::trace("at (%ld,%ld) size (%ld,%ld)", xpos, ypos, dest_width, dest_height);
#endif
	static TbSpriteDrawData spd{};
	auto ret = LbSpriteDrawPrepare(&spd, xpos, ypos, sprite);

	if (ret != LB_OK)
		return ret;
	else if (!spd.sp)
		return LB_OK;

	if ((dest_width <= 0) || (dest_height <= 0))
		return LB_ERROR;

	LbSpriteSetScalingData(xpos, ypos, sprite->Width, sprite->Height, dest_width, dest_height);
	ret = LbSpriteDrawUsingScalingData(0, 0, sprite);

	return ret;
}
