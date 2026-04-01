// LbSurf.h

#ifndef	_LBSURFACE_H
#define	_LBSURFACE_H


#ifndef __cplusplus
#error use a c++ compiler
#endif

#ifndef _LB_LIBRARY
#ifndef _LB_DEBUG
// D3D9 link is handled by Pop3Screen.cpp
#endif
#endif

// includes
//***************************************************************************
#include	"Pop3Types.h"

#include	<LbDraw.h>

#include	<LbCoord.h>
#include	<LbLists.h>

#include	<Pop3PixFmt.h>

#include	<Pop3RenderArea.h>

//***************************************************************************
//	<s>>TbSurface<< : A graphics surface
//***************************************************************************
typedef struct TbSurface
{
    LB_PUBLIC_MEMBERS
    // Constructor
    TbSurface()
    {
        lpSurface=NULL;
#ifdef _LB_DEBUG
        fLocked=FALSE;
#endif
    }

    // Get the surface renderarea
    const Pop3RenderArea& GetRenderArea()
    {
        return SurfaceArea;
    };

    // Get The surface Size
    const Pop3Size& GetSize()
    {
        return SurfaceArea.mSize;
    };

    // Get the surface pixel format
    const Pop3PixFormat& GetPixelFormat()
    {
        return SurfaceArea.mFormat;
    };

    // Get the surface pitch
    UINT GetPitch()
    {
        return SurfaceArea.mPitch;
    };



    LB_PRIVATE_MEMBERS
#ifdef _LB_DEBUG
    TbListNode	ListNode;	// For maintaining a list of created surfaces
#endif
    ULONG	DrawFlags;

    UBYTE	*GhostTable;

    UBYTE	*FadeTable;
    UBYTE	*FadeStepRow;

    TbColour	BltTransparencyColour;

    Pop3RenderArea	SurfaceArea; // Contains information on size and memory location of surface buffer.

    TbPalette	*mpPalette;		// Pointer to palette
    ULONG 	CreationFlags;		// Flags passed to LbSurfaceCreate

    BOOL			bExternal;		// is this surface owned by the library surface system

    void *lpSurface; // Legacy — kept for struct layout (always NULL)

public:

#ifdef _LB_DEBUG
    UINT	fLocked			:1;
#endif
} TbSurface;

#if defined(_LB_DEBUG) && defined(_LB_LIBRARY)
extern TbList _lbSurfaceList; // Debug surface validation
#endif

//***************************************************************************
// Register surface
//***************************************************************************
#if defined(_LB_WINDOWS)
void LbSurface_RegisterDirectDraw(void *pDirectDraw);
#endif

//***************************************************************************
// Surface flags
//***************************************************************************
#define	LB_SURFACE_SYSTEM_MEM 			0x00000800 // Use system memory for surface	
#define	LB_SURFACE_VIDEO_MEM			0x00004000 // Try to use video memory for surface 
#define LB_SURFACE_3D					0x00002000

TbError	LbSurface_Create( UINT width, UINT height, ULONG Flags, TbSurface *surface);

//***************************************************************************
// LbSurface_Delete : Delete a surface. You cannot delete the front or back surfaces
//	TbError LbSurface_Delete(TbSurface *surface);
//***************************************************************************
TbError	LbSurface_Delete(TbSurface *surface);

//***************************************************************************
// LbSurface_Lock : Lock the given surface and make it current
//***************************************************************************
TbError LbSurface_Lock(TbSurface *surface, UBYTE **lplpSurfaceMem=NULL);

//***************************************************************************
// LbSurface_Unlock : Unlock a surface
//	void LbSurface_Unlock(TbSurface *surface);
//***************************************************************************
void LbSurface_Unlock(TbSurface *surface);


//***************************************************************************
// LbSurface_Get and Set : Modification and viewing of surface attributes
//***************************************************************************
void LbSurface_SetCurrent(TbSurface *);
TbSurface * LbSurface_GetCurrent();

Pop3PixFormat *	LbSurface_GetPixelFormat(const TbSurface *surface, Pop3PixFormat &pixformat);

#ifndef _LB_DEBUG
inline TbColour	LbSurface_GetBltTransparencyColour(const TbSurface *surface)
{
    return	surface->BltTransparencyColour;
}
inline void	LbSurface_SetBltTransparencyColour(TbSurface *surface, TbColour const &colour)
{
    surface->BltTransparencyColour = colour;
}

inline UBYTE* LbSurface_GetMemoryPtr(const TbSurface *surface)
{
    return surface->SurfaceArea.mpData;
}

//surface format
inline const Pop3Size* LbSurface_GetSize(const TbSurface *surface)
{
    return &surface->SurfaceArea.mSize;
}
inline UINT LbSurface_GetDepth(const TbSurface *surface)
{
    return surface->SurfaceArea.mFormat.Depth;
}
inline UINT LbSurface_GetBytesPerPixel(const TbSurface *surface)
{
    return 	surface->SurfaceArea.mFormat.Depth/8;
}

#else
const Pop3Size *	LbSurface_GetSize(const TbSurface *surface);
UINT			LbSurface_GetDepth(const TbSurface *surface);
UINT			LbSurface_GetBytesPerPixel(const TbSurface *surface);


TbColour	LbSurface_GetBltTransparencyColour(const TbSurface *surface);
void	LbSurface_SetBltTransparencyColour(TbSurface *surface, TbColour colour);

UBYTE * LbSurface_GetMemoryPtr(const TbSurface *surface);
#endif

ULONG	LbSurface_GetDrawFlags(const TbSurface *surface);
void	LbSurface_SetDrawFlags(TbSurface *surface, ULONG fDraw);
void	LbSurface_SetDrawFlagsOn(TbSurface *surface, ULONG fDraw);
void	LbSurface_SetDrawFlagsOff(TbSurface *surface, ULONG fDraw);

UBYTE * LbSurface_GetFadeTable(const TbSurface *surface);
void	LbSurface_SetFadeTable(TbSurface *surface, UBYTE *fade_table);

ULONG	LbSurface_GetFadeStep(const TbSurface *surface);
void	LbSurface_SetFadeStep(TbSurface *surface, ULONG step);
UINT	LbSurface_GetFadeColour(TbSurface *surface, UINT colour);

UBYTE * LbSurface_GetGhostTable(const TbSurface *surface);
void	LbSurface_SetGhostTable(TbSurface *surface, UBYTE *ghost_table);
UBYTE * LbSurface_GetOriginPtr(const TbSurface *surface);

//***************************************************************************
// LbSurface_StretchBlt : Blt a rectangle from one surface to any other surface and resize
//	TbError LbSurface_StretchBlt(const TbSurface *destS, const Pop3Rect *destR, const TbSurface *srcS, const Pop3Rect *srcR, ULONG Flags);
//***************************************************************************
#define	LB_SURFACE_STRETCHBLT_NOTEARING		0x00000800 // Synchronises with vertical blank
#define	LB_SURFACE_STRETCHBLT_NO_WAIT		0x01000000 // Don't wait if the blitter is busy	(Direct draw compatible)
#define	LB_SURFACE_STRETCHBLT_TRANSPARENT	0x00008000 // Use the transparent colour
#define	LB_SURFACE_STRETCHBLT_ASYNC			0x00000200 // Do the bit asynchronously if possible (Direct draw compatible)

TbError LbSurface_StretchBlt(const TbSurface *destS, const Pop3Rect *destR, const TbSurface *srcS, const Pop3Rect *srcR, ULONG Flags);

//***************************************************************************
// LbSurface_Blt : Copy a region from one surface to another (always async if possible)
//	TbError LbSurface_Blt(TbSurface *destS, UINT x, UINT y, const TbSurface *srcS, const Pop3Rect *srcR, ULONG Flags);
//***************************************************************************

#define	LB_SURFACE_BLT_NO_WAIT			0x00000010 // Don't wait if the blitter is busy (Direct draw compatible)
#define	LB_SURFACE_BLT_TRANSPARENT		0x00000001 // Use the transparent colour		(Direct draw compatible)

TbError LbSurface_Blt(const TbSurface *destS, UINT x, UINT y, const TbSurface *srcS, const Pop3Rect *srcR, ULONG Flags);

//***************************************************************************
// LbSurface_ClearRect : Clear the rectangle on the surface to a colour using LB_SURFACE_STRETCHBLT flags below
//***************************************************************************
TbError	LbSurface_ClearRect(const TbSurface *surface, const Pop3Rect *rect, TbColour colour, ULONG flags);

//***************************************************************************
// LbSurface_GetAddressOf : Returns a pointer to the current surface's address at a point relative to origin.
//	UBYTE *LbSurface_GetAddressOf(SINT x, SINT y);
//***************************************************************************
//#ifndef	_LB_DEBUG
//	inline UBYTE *LbSurface_GetAddressOf(SINT x, SINT y)
//	{
//		return (_lbpCurrentSurface->OriginPtr+x+y*_lbpCurrentSurface->SurfaceArea.mPitch);
//	}
//#else
UBYTE * LbSurface_GetAddressOf(SINT x, SINT y);
//#endif

//***************************************************************************
// LbSurface_GetPitch : Retrieve the memory width in bytes of the currently locked surface
//	UINT LbSurface_GetPitch();
//***************************************************************************
#ifndef _LB_DEBUG
inline UINT LbSurface_GetPitch(const TbSurface *lpSurface)
{
    return (lpSurface->SurfaceArea.mPitch);
}
#else
UINT LbSurface_GetPitch(const TbSurface *lpSurface);
#endif

//***************************************************************************
//	Display a frame rate on given surface
//***************************************************************************
void LbSurface_DebugFrameRate(TbSurface *lpSurface);
#ifndef _LB_DEBUG
#define	LbSurface_DebugFrameRate	(1)?(void)0:LbSurface_DebugFrameRate
#endif

//***************************************************************************
//	LbSurface_MapToRenderArea
//***************************************************************************
Pop3RenderArea *LbSurface_MapToRenderArea(Pop3RenderArea &ra, TbSurface *surface);



#endif