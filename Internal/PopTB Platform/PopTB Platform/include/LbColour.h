#pragma once
//***************************************************************************
// Colour
//***************************************************************************

#include <LbPal.h>
#include <Pop3PixelCaps.h>

#include "Pop3Debug.h"

// the global palette, used by the colour class
extern TbPalette _lbGlobalPalette;


/////////////////////////////////////////////////
// <t>>TbColour<<
/////////////////////////////////////////////////
class TbColour
{
public:
    TbColour()
    {
        Index = Red = Green = Blue = Alpha = 0;
    }

    // Constructors
    TbColour(UBYTE Ind)
    {
        // Palette lookup
        Index = Ind;
        Red = _lbGlobalPalette.Entry[Index].Red;
        Green = _lbGlobalPalette.Entry[Index].Green;
        Blue = _lbGlobalPalette.Entry[Index].Blue;
        Alpha = 0xff;
    };

    TbColour(int Ind)
    {
        ASSERT(Ind >= 0);
        ASSERT(Ind<256);

        // Palette lookup
        Index = (UBYTE)Ind;
        Red = _lbGlobalPalette.Entry[Index].Red;
        Green = _lbGlobalPalette.Entry[Index].Green;
        Blue = _lbGlobalPalette.Entry[Index].Blue;
        Alpha = 0xff;
    };

    TbColour(UBYTE r, UBYTE g, UBYTE b)
    {
        Set(r, g, b);
    };

    TbColour(UBYTE r, UBYTE g, UBYTE b, UBYTE Pal) : Red(r), Green(g), Blue(b), Index(Pal), Alpha(0xff)
    {
    };


    // Set the colour to an rgb value
    inline void Set(UBYTE r, UBYTE g, UBYTE b, UBYTE a = 0xff);

    // Function to get the colour as a packed 32bit value
    inline ULONG Get32bitValue() const;

    // Function to get the colour as a formatted  - packed to the specifications in the pixel format mask
    inline ULONG GetFormattedValue(const Pop3PixelCaps& pixcaps) const;
    inline ULONG GetFormattedValue(const Pop3PixFormat& pixformat) const;

    // public data
    union
    {
        ULONG Packed;

        struct
        {
            UBYTE Blue;
            UBYTE Green;
            UBYTE Red;
            UBYTE Alpha;
        };
    };

    UBYTE Index;
};


// Set to an rgb value then do a palette lookup
inline void TbColour::Set(UBYTE r, UBYTE g, UBYTE b, UBYTE a)
{
    Red = r;
    Green = g;
    Blue = b;
    Alpha = a;

    // Find colour for index
    Index = LbPalette_FindColour(&_lbGlobalPalette, r, g, b);
};

// Gets a 32 bit value.
inline ULONG TbColour::Get32bitValue() const
{
    ULONG RetVal = (Red << 16) | (Green << 8) | (Blue) | (Alpha << 24);
    return RetVal;
};

// Gets a formatted long according to the pixformat specified.
inline ULONG TbColour::GetFormattedValue(const Pop3PixelCaps& pixcaps) const
{
    return ((((Packed) >> pixcaps.mRed.ShiftRight) << pixcaps.mRed.ShiftLeft) & pixcaps.mRed.Mask)
        | ((((Packed) >> pixcaps.mGreen.ShiftRight) << pixcaps.mGreen.ShiftLeft) & pixcaps.mGreen.Mask)
        | ((((Packed) >> pixcaps.mBlue.ShiftRight) << pixcaps.mBlue.ShiftLeft) & pixcaps.mBlue.Mask)
        | ((((Packed) >> pixcaps.mAlpha.ShiftRight) << pixcaps.mAlpha.ShiftLeft) & pixcaps.mAlpha.Mask);
}

// Gets a formatted long according to the pixformat specified.
inline ULONG TbColour::GetFormattedValue(const Pop3PixFormat& pixformat) const
{
    Pop3PixelCaps pixcaps;
    pixcaps.Set(pixformat);
    return GetFormattedValue(pixcaps);
}



