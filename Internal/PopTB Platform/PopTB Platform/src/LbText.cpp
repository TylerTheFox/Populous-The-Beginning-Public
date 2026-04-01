//***************************************************************************
// LbText.cpp : Text and font rendering
//***************************************************************************
#include <LbText.h>
#include <LbDraw.h>
#include <LbSprite.h>

//***************************************************************************
// Static callback functions for TbTextRender
//***************************************************************************
static void __cdecl SpriteRender_OnMeasure(void *pParam, Pop3Size *lpSize, UINT cChar)
{
    const TbSprite *spr = &((const TbSprite*)pParam)[cChar - 32];
    lpSize->Width = spr->Width;
    lpSize->Height = spr->Height;
}

static UINT __cdecl SpriteRender_OnDrawChar(void *pParam, SINT x, SINT y, UINT cChar, TbColour col)
{
    const TbSprite *spr = &((const TbSprite*)pParam)[cChar - 32];
    BfDraw_Sprite(x, y, spr);
    return spr->Width;
}

static void __cdecl OneColourSpriteRender_OnMeasure(void *pParam, Pop3Size *lpSize, UINT cChar)
{
    const TbSprite *spr = &((const TbSprite*)pParam)[cChar - 32];
    lpSize->Width = spr->Width;
    lpSize->Height = spr->Height;
}

static UINT __cdecl OneColourSpriteRender_OnDrawChar(void *pParam, SINT x, SINT y, UINT cChar, TbColour col)
{
    const TbSprite *spr = &((const TbSprite*)pParam)[cChar - 32];
    BfDraw_OneColourSprite(x, y, spr, col);
    return spr->Width;
}

//***************************************************************************
// Globals
//***************************************************************************
const TbTextRender *_lbpCurrentFont;
static void *_lbFontData;

extern const TbTextRender _lbSpriteRender = {
    NULL,                              // OnSelect
    NULL,                              // OnDeselect
    SpriteRender_OnMeasure,            // OnMeasure
    SpriteRender_OnDrawChar            // OnDrawChar
};

extern const TbTextRender _lbOneColourSpriteRender = {
    NULL,                              // OnSelect
    NULL,                              // OnDeselect
    OneColourSpriteRender_OnMeasure,   // OnMeasure
    OneColourSpriteRender_OnDrawChar   // OnDrawChar
};

//***************************************************************************
// Cheap text bitmap font data (3-pixel wide, 6-pixel tall built-in font)
//***************************************************************************
static const UBYTE charData[] = {
    0,0,0,0,0,0, // 32 ' '
    2,2,2,0,2,0, // 33 '!'
    5,5,0,0,0,0, // 34 '"'
    5,7,5,7,5,0, // 35 '#'
    6,3,2,6,3,0, // 36 '$'
    5,4,2,1,5,0, // 37 '%'
    2,5,2,5,6,0, // 38 '&'
    4,2,0,0,0,0, // 39 '''
    2,1,1,1,2,0, // 40 '('
    2,4,4,4,2,0, // 41 ')'
    0,5,2,5,0,0, // 42 '*'
    0,2,7,2,0,0, // 43 '+'
    0,0,0,0,2,1, // 44 ','
    0,0,7,0,0,0, // 45 '-'
    0,0,0,0,2,0, // 46 '.'
    0,0,4,2,1,0, // 47 '/'
    7,5,5,5,7,0, // 48 '0'
    2,3,2,2,7,0, // 49 '1'
    3,4,2,1,7,0, // 50 '2'
    3,4,2,4,3,0, // 51 '3'
    1,1,5,7,4,0, // 52 '4'
    7,1,3,4,3,0, // 53 '5'
    1,1,7,5,7,0, // 54 '6'
    7,4,6,4,4,0, // 55 '7'
    7,5,2,5,7,0, // 56 '8'
    7,5,7,4,4,0, // 57 '9'
    0,0,2,0,2,0, // 58 ':'
    0,0,2,0,2,1, // 59 ';'
    4,2,1,2,4,0, // 60 '<'
    0,7,0,7,0,0, // 61 '='
    1,2,4,2,1,0, // 62 '>'
    2,5,4,2,0,2, // 63 '?'
    0,6,5,5,1,6, // 64 '@'
    2,5,7,5,5,0, // 65 'A'
    3,5,3,5,3,0, // 66 'B'
    6,1,1,1,6,0, // 67 'C'
    3,5,5,5,3,0, // 68 'D'
    7,1,3,1,7,0, // 69 'E'
    7,1,3,1,1,0, // 70 'F'
    6,1,1,5,6,0, // 71 'G'
    5,5,7,5,5,0, // 72 'H'
    7,2,2,2,7,0, // 73 'I'
    4,4,4,4,3,0, // 74 'J'
    5,5,3,5,5,0, // 75 'K'
    1,1,1,1,7,0, // 76 'L'
    5,7,7,5,5,0, // 77 'M'
    5,7,7,7,5,0, // 78 'N'
    2,5,5,5,2,0, // 79 'O'
    3,5,3,1,1,0, // 80 'P'
    2,5,5,7,6,0, // 81 'Q'
    3,5,3,5,5,0, // 82 'R'
    6,1,2,4,3,0, // 83 'S'
    7,2,2,2,2,0, // 84 'T'
    5,5,5,5,7,0, // 85 'U'
    5,5,5,5,2,0, // 86 'V'
    5,5,7,7,5,0, // 87 'W'
    5,5,2,5,5,0, // 88 'X'
    5,5,2,2,2,0, // 89 'Y'
    7,4,2,1,7,0, // 90 'Z'
    3,1,1,1,3,0, // 91 '['
    0,0,1,2,4,0, // 92 '\'
    6,4,4,4,6,0, // 93 ']'
    2,5,0,0,0,0, // 94 '^'
    0,0,0,0,7,0, // 95 '_'
    0,0,1,2,4,0, // 96 '`'
    3,4,6,5,6,0, // 97 'a'
    1,3,5,5,3,0, // 98 'b'
    0,6,1,1,6,0, // 99 'c'
    4,6,5,5,6,0, // 100 'd'
    0,6,7,1,6,0, // 101 'e'
    0,2,1,3,1,1, // 102 'f'
    0,2,5,6,4,3, // 103 'g'
    1,1,3,5,5,0, // 104 'h'
    2,0,3,2,7,0, // 105 'i'
    2,0,2,2,2,1, // 106 'j'
    1,1,5,3,5,0, // 107 'k'
    3,2,2,2,7,0, // 108 'l'
    0,7,7,5,5,0, // 109 'm'
    0,3,5,5,5,0, // 110 'n'
    0,2,5,5,2,0, // 111 'o'
    0,3,5,3,1,1, // 112 'p'
    0,6,5,6,4,4, // 113 'q'
    0,5,3,1,1,0, // 114 'r'
    0,6,3,6,3,0, // 115 's'
    2,7,2,2,4,0, // 116 't'
    0,5,5,5,7,0, // 117 'u'
    0,5,5,5,2,0, // 118 'v'
    0,5,5,7,7,0, // 119 'w'
    0,5,2,2,5,0, // 120 'x'
    0,5,5,6,4,3, // 121 'y'
    0,7,2,1,7,0, // 122 'z'
    6,2,1,2,6,0, // 123 '{'
    2,2,2,2,2,0, // 124 '|'
    3,2,4,2,3,0, // 125 '}'
    0,0,0,0,7,0, // 126 '~'
    7,7,7,7,7,7, // 127
    0,0,0,0,0,0, // 128
    5,0,5,5,7,0, // 129
    0,0,0,0,0,0, // 130
    0,0,0,0,0,0, // 131
    5,0,7,7,5,0, // 132
    0,0,0,0,0,0, // 133
    0,0,0,0,0,0, // 134
    0,0,0,0,0,0, // 135
    0,0,0,0,0,0, // 136
    0,0,0,0,0,0, // 137
    0,0,0,0,0,0, // 138
    0,0,0,0,0,0, // 139
    0,0,0,0,0,0, // 140
    0,0,0,0,0,0, // 141
    5,0,7,5,7,0, // 142
    0,0,0,0,0,0, // 143
    0,0,0,0,0,0, // 144
    0,0,0,0,0,0, // 145
    0,0,0,0,0,0, // 146
    0,0,0,0,0,0, // 147
    5,2,5,7,5,0, // 148
    5,0,7,5,7,0, // 149
    0,0,0,0,0,0, // 150
    0,0,0,0,0,0, // 151
    0,0,0,0,0,0, // 152
    5,0,7,5,7,0, // 153
    5,0,5,5,7,0, // 154
    0,0,0,0,0,0, // 155
};

// Bit-to-byte expansion masks for 3-pixel wide cheap text at 8bpp
static const ULONG masks[] = {
    0x00000000, // 0
    0x000000FF, // 1
    0x0000FF00, // 2
    0x0000FFFF, // 3
    0x00FF0000, // 4
    0x00FF00FF, // 5
    0x00FFFF00, // 6
    0x00FFFFFF, // 7
};

//***************************************************************************
// Draw_CheapText : Internal cheap text rendering (templated for char/unicode)
//***************************************************************************
template<typename CHARTYPE>
static void Draw_CheapText(SINT x, SINT y, const CHARTYPE *pText, TbColour col)
{
    static UBYTE lastCol = 0;
    static ULONG lastFill = 0;

    if (_lbpCurrentRA->mFormat.Depth != 8)
        return;
    if (_lbCurrentClipRect.Bottom < y + 6)
        return;
    if (_lbCurrentClipRect.Top > y)
        return;

    UBYTE *pDst = LbDraw_GetAddressOf(x, y);
    SINT pitch = _lbpCurrentRA->mPitch;
    UBYTE colIdx = col.Index;

    if (lastCol != colIdx)
    {
        lastCol = colIdx;
        lastFill = colIdx | (colIdx << 8) | (colIdx << 16) | (colIdx << 24);
    }
    ULONG fill = lastFill;

    while (*pText)
    {
        UBYTE ch = (UBYTE)*pText++;
        if (_lbCurrentClipRect.Left > x)
        {
            pDst += 4;
            x += 4;
            continue;
        }
        if (_lbCurrentClipRect.Right < x + 4)
            return;

        const UBYTE *pChar = &charData[6 * (ch - 32)];
        ULONG *pRow = (ULONG*)pDst;
        for (int i = 6; i; --i)
        {
            ULONG mask = masks[*pChar++];
            *pRow = (mask & fill) | (*pRow & ~mask);
            pRow = (ULONG*)((UBYTE*)pRow + pitch);
        }
        pDst += 4;
        x += 4;
    }
}

//***************************************************************************
// Draw_Text : Internal text rendering using current font callbacks
//***************************************************************************
template<typename CHARTYPE>
static void Draw_Text(SINT x, SINT y, const CHARTYPE *pText, TbColour col)
{
    SINT startX = x;
    while (*pText)
    {
        if (*pText == 10)
        {
            y += LbDraw_GetTextCharacterHeight(0x20);
            x = startX;
            pText++;
        }
        else
        {
            UINT ch = (UBYTE)*pText++;
            x += _lbpCurrentFont->OnDrawChar(_lbFontData, x, y, ch, col);
        }
    }
}

//***************************************************************************
// Text_Measure : Internal text measurement
//***************************************************************************
template<typename CHARTYPE>
static UINT Text_Measure(const CHARTYPE *pText, Pop3Size *lpSize)
{
    Pop3Size localSize;
    UINT lineWidth = 0;

    if (!lpSize)
        lpSize = &localSize;

    lpSize->Width = 0;
    lpSize->Height = LbDraw_GetTextCharacterHeight(0x20);

    while (*pText)
    {
        if (*pText == 10)
        {
            lpSize->Height += LbDraw_GetTextCharacterHeight(0x20);
            if (lpSize->Width < lineWidth)
                lpSize->Width = lineWidth;
            lineWidth = 0;
        }
        else
        {
            lineWidth += LbDraw_GetTextCharacterWidth(*pText);
        }
        pText++;
    }
    if (lpSize->Width < lineWidth)
        lpSize->Width = lineWidth;
    return lpSize->Width;
}

//***************************************************************************
// Public API
//***************************************************************************

void LbDraw_CheapText(SINT x, SINT y, const TBCHAR *lpText, TbColour col)
{
    Draw_CheapText(x, y, lpText, col);
}

void LbDraw_UnicodeCheapText(SINT x, SINT y, const UNICHAR *lpText, TbColour col)
{
    Draw_CheapText(x, y, lpText, col);
}

TbError LbDraw_SetTextFont(const TbTextRender *pRender, void *pInit)
{
    if (_lbpCurrentFont)
    {
        if (_lbpCurrentFont->OnDeselect)
            _lbpCurrentFont->OnDeselect(_lbFontData, pRender, pInit);
    }
    _lbpCurrentFont = pRender;
    if (pRender->OnSelect)
    {
        if (pRender->OnSelect(&_lbFontData, pInit))
        {
            _lbpCurrentFont = NULL;
            return LB_ERROR;
        }
    }
    else
    {
        _lbFontData = pInit;
    }
    return LB_OK;
}

void LbDraw_Text(SINT x, SINT y, const TBCHAR *pText, TbColour Colour)
{
    Draw_Text(x, y, pText, Colour);
}

void LbDraw_UnicodeText(SINT x, SINT y, const UNICHAR *pText, TbColour Colour)
{
    Draw_Text(x, y, pText, Colour);
}

UINT LbDraw_GetUnicodeTextSize(const UNICHAR *lpText, Pop3Size *lpSize)
{
    return Text_Measure(lpText, lpSize);
}

UINT LbDraw_GetTextCharacterWidth(UINT character)
{
    Pop3Size size;
    _lbpCurrentFont->OnMeasure(_lbFontData, &size, character);
    return size.Width;
}

UINT LbDraw_GetTextCharacterHeight(UINT character)
{
    Pop3Size size;
    _lbpCurrentFont->OnMeasure(_lbFontData, &size, character);
    return size.Height;
}
