#pragma once
#include "Pop3Types.h"
#include <Pop3PixFmt.h>

class Pop3MaskInfo
{
public:
    Pop3MaskInfo()
        : Mask(0), ShiftLeft(0), ShiftRight(0) {}

    Pop3MaskInfo(unsigned long mask, unsigned int msbpos)
    {
        Set(mask, msbpos);
    }

    void Set(unsigned long mask, unsigned int msbpos);

    unsigned long Mask;
    unsigned short ShiftLeft;
    unsigned short ShiftRight;
};

class Pop3PixelCaps
{
public:
    Pop3PixelCaps() = default;

    Pop3PixelCaps(const Pop3PixFormat& pixformat)
    {
        Set(pixformat);
    }

    void Set(const Pop3PixFormat& pixformat);

    Pop3MaskInfo mRed;
    Pop3MaskInfo mGreen;
    Pop3MaskInfo mBlue;
    Pop3MaskInfo mAlpha;
};
