#pragma once
//
//	_drawdef.h
//

#define _CURRENT_RENDER_AREA		_lbpCurrentRA
#define _CURRENT_RENDER_CONTEXT		_lbpRenderBase
#define _CURRENT_ORIGIN_PTR			_lbCurrentOriginPtr
#define _CURRENT_CLIP_RECT			_lbCurrentClipRect
#define _CURRENT_VIEWPORT			_lbCurrentViewPort
#define _CURRENT_DRAW_FLAGS			_lbCurrentDrawFlags
#define _CURRENT_GHOST_TABLE		_lbCurrentGhostTable
#define _CURRENT_FADE_TABLE			_lbpCurrentFadeTable
#define _CURRENT_FADE_ROW			_lbpCurrentFadeRow
#define _GET_FADE_COLOUR(col)		(_lbpCurrentFadeRow[col])

#define _CURRENT_PIXEL_CAPS			_lbCurrentPixelCaps
