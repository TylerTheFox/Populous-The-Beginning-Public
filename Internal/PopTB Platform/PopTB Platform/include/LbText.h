//***************************************************************************
// LbText.h : Text and font rendering
//***************************************************************************

#ifndef _LBTEXT_H
#define _LBTEXT_H


// includes
//***************************************************************************
#include <LbCoord.h>
#include <LbColour.h>

class Pop3Rect;

//***************************************************************************
// LbDraw_CheapText : Draw the built-in quick text at the given byte surface
//	void LbDraw_CheapText(SINT x, SINT y, const CHAR *pText);
//***************************************************************************
void LbDraw_CheapText(SINT x, SINT y, const TBCHAR *lpText, TbColour col);
void LbDraw_UnicodeCheapText(SINT x, SINT y, const UNICHAR *lpText, TbColour col);

//***************************************************************************
// LbDraw_SetFont : Select the typeface to use to draw
//	TbError LbDraw_SetFont(const TbTextRender *, void *);
//***************************************************************************
typedef struct TbTextRender
{
    TbError (LB_CALLBACK *OnSelect)(void **ppParam, void *pInit); // Called when _SetFont is called can be NULL
    void (LB_CALLBACK *OnDeselect)(void *pParam, const struct TbTextRender *, void *pInit); // Called on deselect with replacing params, can be NULL
    void (LB_CALLBACK *OnMeasure)(void *pParam, Pop3Size *, UINT cChar); // Must be provided
    UINT (LB_CALLBACK *OnDrawChar)(void *pParam, SINT x, SINT y, UINT cChar, TbColour col); // Must be provided
} TbTextRender;
#ifdef _LB_NDEBUG
LB_EXTERN const TbTextRender *_lbpCurrentFont;
LB_EXTERN const TbTextRender _lbSpriteRender;
LB_EXTERN const TbTextRender _lbOneColourSpriteRender;

inline const TbTextRender *LbDraw_GetTextFont()
{
    return _lbpCurrentFont;
}
inline const TbTextRender *LbDraw_GetTextSpriteRender()
{
    return (&_lbSpriteRender);
}
inline const TbTextRender *LbDraw_GetOneColourTextSpriteRender()
{
    return (&_lbOneColourSpriteRender);
}
#else
const TbTextRender * LbDraw_GetTextFont();
const TbTextRender * LbDraw_GetTextSpriteRender();
const TbTextRender * LbDraw_GetOneColourTextSpriteRender();
#endif
TbError LbDraw_SetTextFont(const TbTextRender *, void *pInit); // pInit depends upon renderer

void LbDraw_Text(SINT x, SINT y, const TBCHAR *pText, TbColour Colour);
void LbDraw_UnicodeText(SINT x, SINT y, const UNICHAR *pText, TbColour Colour);

//***************************************************************************
// Text Flags
//***************************************************************************
#define	LB_TEXT_FLAG_LEFT_JUSTIFY					(1 << 0)
#define	LB_TEXT_FLAG_RIGHT_JUSTIFY					(1 << 1)
#define	LB_TEXT_FLAG_HORIZONTAL_CENTER_JUSTIFY		(1 << 2)
#define	LB_TEXT_FLAG_HORIZONTAL_FULL_JUSTIFY		(1 << 3)

#define	LB_TEXT_FLAG_TOP_JUSTIFY					(1 << 4)
#define	LB_TEXT_FLAG_BOTTOM_JUSTIFY					(1 << 5)
#define	LB_TEXT_FLAG_VERTICAL_CENTER_JUSTIFY		(1 << 6)
#define	LB_TEXT_FLAG_VERTICAL_FULL_JUSTIFY			(1 << 7)

#define	LB_TEXT_FLAG_NORMAL							(LB_TEXT_FLAG_TOP_JUSTIFY|LB_TEXT_FLAG_LEFT_JUSTIFY)
#define	LB_TEXT_FLAG_CENTER							(LB_TEXT_FLAG_HORIZONTAL_CENTER_JUSTIFY|LB_TEXT_FLAG_VERTICAL_CENTER_JUSTIFY)

void LbDraw_FormattedText(const Pop3Rect *lpRect, const TBCHAR *pText, ULONG flags, TbColour Colour);
void LbDraw_UnicodeFormattedText(const Pop3Rect *lpRect, const UNICHAR *pText, ULONG flags, TbColour Colour);

UINT LbDraw_GetFormattedTextHeight(UINT nWidth, const TBCHAR *lpText);
UINT LbDraw_GetUnicodeFormattedTextHeight(UINT nWidth, const UNICHAR *lpText);
UINT LbDraw_GetTextSize(const TBCHAR *lpText, Pop3Size *lpSize);
UINT LbDraw_GetUnicodeTextSize(const UNICHAR *lpText, Pop3Size *lpSize);
UINT LbDraw_GetTextCharacterWidth(UINT character);
UINT LbDraw_GetTextCharacterHeight(UINT character);


#endif // ifndef _LBTEXT_H
//***************************************************************************
//				END OF FILE
//***************************************************************************
