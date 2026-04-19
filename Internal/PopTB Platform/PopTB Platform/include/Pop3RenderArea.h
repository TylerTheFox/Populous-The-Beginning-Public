#pragma once
#include <Pop3Point.h>
#include <Pop3Rect.h>
#include <Pop3PixFmt.h>
#include <LbError.h>
#include <Pop3Debug.h>

struct TbSurface;
struct TbPalette;

class Pop3RenderArea
{
public:
    Pop3RenderArea();
    Pop3RenderArea(TbSurface* surface);

    Pop3RenderArea(Pop3Size size, Pop3PixFormat& format, unsigned char* data = nullptr, unsigned int memwidth = 0);

    TbError SetupClientRA(Pop3RenderArea& ra, Pop3Point& origin, Pop3Size& size = Pop3Size(0xffffff, 0xffffff)) const;
    TbError SetupClientRA(Pop3RenderArea& ra, Pop3Rect& rect) const;

    unsigned int GetMemSize() const;
    TbError AllocMemory();
    void FreeMemory();

    unsigned char* GetPtr() const { return mpData; }
    Pop3Size GetSize() const { return mSize; }
    unsigned int GetPitch() const { return mPitch; }
    Pop3PixFormat GetFormat() const { return mFormat; }

    unsigned char*  mpData;
    Pop3Size        mSize;
    unsigned int    mPitch;
    Pop3PixFormat   mFormat;
};

inline unsigned int Pop3RenderArea::GetMemSize() const
{
    ASSERTMSG(mPitch, "RenderArea not set up properly. Invalid Pitch");
    return mPitch * mSize.Height;
}
