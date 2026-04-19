#include "Pop3PixelCaps.h"

void Pop3MaskInfo::Set(unsigned long mask, unsigned int msbpos)
{
    Mask = mask;
    unsigned int v4 = 31;
    if (mask)
    {
        for (unsigned long i = 0x80000000UL; (mask & i) == 0; i >>= 1)
            --v4;
    }
    if (msbpos > v4)
    {
        ShiftLeft  = 0;
        ShiftRight = (unsigned short)(msbpos - v4);
    }
    else
    {
        ShiftLeft  = (unsigned short)(v4 - msbpos);
        ShiftRight = 0;
    }
}

void Pop3PixelCaps::Set(const Pop3PixFormat& pixformat)
{
    mRed.Set(pixformat.Red,     0x17u);
    mGreen.Set(pixformat.Green, 0x0Fu);
    mBlue.Set(pixformat.Blue,   7u);
    mAlpha.Set(pixformat.Alpha, 0x1Fu);
}
