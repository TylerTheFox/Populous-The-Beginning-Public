#pragma once
#include "Pop3Types.h"

class Pop3PixFormat
{
public:
    unsigned long Red;
    unsigned long Green;
    unsigned long Blue;
    unsigned long Alpha;
    unsigned int Depth;
    int bPalette;

    Pop3PixFormat()
        : Red(0), Green(0), Blue(0), Alpha(0), Depth(0), bPalette(0) {}

    Pop3PixFormat(unsigned long rmask, unsigned long gmask, unsigned long bmask,
                  unsigned long amask, unsigned int depth, int ispalette = 0)
        : Red(rmask), Green(gmask), Blue(bmask), Alpha(amask),
          Depth(depth), bPalette(ispalette) {}

    unsigned int GetBitDepth() const { return Depth; }
    unsigned int GetByteDepth() const { return (Depth + 1) >> 3; }
    int IsPalette() const { return bPalette; }

    void Set(unsigned long rmask, unsigned long gmask, unsigned long bmask,
             unsigned long amask, unsigned int depth, int ispalette)
    {
        Red = rmask;
        Green = gmask;
        Blue = bmask;
        Alpha = amask;
        Depth = depth;
        bPalette = ispalette;
    }

    bool operator==(const Pop3PixFormat& rhs) const
    {
        return (Red == rhs.Red && Green == rhs.Green && Blue == rhs.Blue &&
                Alpha == rhs.Alpha && Depth == rhs.Depth && (!bPalette == !rhs.bPalette));
    }

    bool operator!=(const Pop3PixFormat& rhs) const
    {
        return !(*this == rhs);
    }
};

extern Pop3PixFormat Pop3PixFormat8;
extern Pop3PixFormat Pop3PixFormat16;
extern Pop3PixFormat Pop3PixFormat24;
extern Pop3PixFormat Pop3PixFormat32;
