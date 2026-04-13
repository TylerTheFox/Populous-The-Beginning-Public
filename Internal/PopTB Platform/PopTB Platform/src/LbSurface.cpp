#include "Pop3Debug.h"
#include "Pop3Platform_Win32.h"
#include <ddraw.h>
#include <LbSurface.h>

// Global list of registered surfaces (debug mode only, zeroed in release)
#if defined(_LB_DEBUG) && defined(_LB_LIBRARY)
TbList _lbSurfaceList;
#endif

// -----------------------------------------------------------------------
// LbSurface_RegisterDirectDraw : Register DirectDraw device with the library
// Called from LbScreen during initialisation.
// -----------------------------------------------------------------------
void LbSurface_RegisterDirectDraw(IDirectDraw *pDirectDraw)
{
    // No-op in this implementation: DirectDraw is managed by LbScreen
}

// -----------------------------------------------------------------------
// LbSurface_Create : Create a new software surface backed by system memory.
// For DevelopSW/ReleaseSW configurations the surface has no DirectDraw
// backing; we simply allocate the pixel buffer and fill in SurfaceArea.
// -----------------------------------------------------------------------
TbError LbSurface_Create(UINT width, UINT height, ULONG Flags, TbSurface *surface)
{
    if (!surface)
        return LB_ERROR;

    Pop3PixFormat fmt;
    fmt.Depth = 8;

    UINT pitch = width; // 8bpp: 1 byte per pixel
    UBYTE *buf = (UBYTE *)malloc(pitch * height);
    if (!buf)
        return LB_ERROR;

    surface->SurfaceArea.mpData       = buf;
    surface->SurfaceArea.mSize.Width  = width;
    surface->SurfaceArea.mSize.Height = height;
    surface->SurfaceArea.mPitch       = pitch;
    surface->SurfaceArea.mFormat      = fmt;
    surface->lpSurface           = NULL;
    surface->DrawFlags           = 0;
    surface->GhostTable          = NULL;
    surface->FadeTable           = NULL;
    surface->FadeStepRow         = NULL;
    surface->CreationFlags       = Flags;
    surface->bExternal           = FALSE;
    surface->mpPalette           = NULL;

    return LB_OK;
}

// -----------------------------------------------------------------------
// LbSurface_Delete : Free a surface created with LbSurface_Create.
// -----------------------------------------------------------------------
TbError LbSurface_Delete(TbSurface *surface)
{
    if (!surface)
        return LB_ERROR;
    if (!surface->bExternal && surface->SurfaceArea.mpData)
    {
        free(surface->SurfaceArea.mpData);
        surface->SurfaceArea.mpData = NULL;
    }
    surface->lpSurface = NULL;
    return LB_OK;
}

// -----------------------------------------------------------------------
// LbSurface_Lock : Lock a surface so its pixel data can be accessed.
// For DirectDraw surfaces, calls IDirectDrawSurface::Lock to get the
// pixel buffer pointer and pitch.
// -----------------------------------------------------------------------
TbError LbSurface_Lock(TbSurface *surface, UBYTE **lplpSurfaceMem)
{
    if (!surface)
        return LB_ERROR;

    if (surface->lpSurface)
    {
        DDSURFACEDESC ddsd;
        memset(&ddsd, 0, sizeof(ddsd));
        ddsd.dwSize = sizeof(ddsd);
        HRESULT hr = surface->lpSurface->Lock(NULL, &ddsd, DDLOCK_WAIT, NULL);
        if (hr != DD_OK)
            return LB_ERROR;
        surface->SurfaceArea.mpData  = (UBYTE*)ddsd.lpSurface;
        surface->SurfaceArea.mPitch  = (UINT)ddsd.lPitch;
    }

    LbDraw_SetCurrent(surface);
    if (lplpSurfaceMem)
        *lplpSurfaceMem = surface->SurfaceArea.mpData;
    return LB_OK;
}

// -----------------------------------------------------------------------
// LbSurface_Unlock : Release a lock acquired with LbSurface_Lock.
// -----------------------------------------------------------------------
void LbSurface_Unlock(TbSurface *surface)
{
    if (surface && surface->lpSurface && surface->SurfaceArea.mpData)
        surface->lpSurface->Unlock(surface->SurfaceArea.mpData);
    // mpData is intentionally left set — callers may read it after Unlock (e.g. LbSurface_ClearRect)
}

// -----------------------------------------------------------------------
// LbSurface_SetCurrent : Make a surface the active draw target.
// Deprecated — callers should use LbDraw_SetCurrent directly.
// -----------------------------------------------------------------------
void LbSurface_SetCurrent(TbSurface *surface)
{
    LbDraw_SetCurrent(surface);
}

// -----------------------------------------------------------------------
// LbSurface_GetCurrent : Deprecated — always returns NULL.
// Use LbDraw_GetCurrent() to obtain the current Pop3RenderArea instead.
// -----------------------------------------------------------------------
TbSurface * LbSurface_GetCurrent(void)
{
    return NULL;
}

// -----------------------------------------------------------------------
// LbSurface_GetPixelFormat
// -----------------------------------------------------------------------
Pop3PixFormat * LbSurface_GetPixelFormat(const TbSurface *surface, Pop3PixFormat &pixformat)
{
    pixformat = surface->SurfaceArea.mFormat;
    return &pixformat;
}

// -----------------------------------------------------------------------
// LbSurface_GetDrawFlags
// -----------------------------------------------------------------------
ULONG LbSurface_GetDrawFlags(const TbSurface *surface)
{
    return surface->DrawFlags;
}

// -----------------------------------------------------------------------
// LbSurface_SetDrawFlags
// -----------------------------------------------------------------------
void LbSurface_SetDrawFlags(TbSurface *surface, ULONG fDraw)
{
    surface->DrawFlags = fDraw;
    if (&surface->SurfaceArea == _lbpCurrentRA)
        LbDraw_SetFlags(fDraw);
}

// -----------------------------------------------------------------------
// LbSurface_SetDrawFlagsOn
// -----------------------------------------------------------------------
void LbSurface_SetDrawFlagsOn(TbSurface *surface, ULONG fDraw)
{
    surface->DrawFlags |= fDraw;
    if (&surface->SurfaceArea == _lbpCurrentRA)
        LbDraw_SetFlagsOn(fDraw);
}

// -----------------------------------------------------------------------
// LbSurface_SetDrawFlagsOff
// -----------------------------------------------------------------------
void LbSurface_SetDrawFlagsOff(TbSurface *surface, ULONG fDraw)
{
    surface->DrawFlags &= ~fDraw;
    if (&surface->SurfaceArea == _lbpCurrentRA)
        LbDraw_SetFlagsOff(fDraw);
}

// -----------------------------------------------------------------------
// LbSurface_GetFadeTable
// -----------------------------------------------------------------------
UBYTE * LbSurface_GetFadeTable(const TbSurface *surface)
{
    return surface->FadeTable;
}

// -----------------------------------------------------------------------
// LbSurface_SetFadeTable : Set the fade table for a surface.
// Also updates the current draw context if this surface is active.
// -----------------------------------------------------------------------
void LbSurface_SetFadeTable(TbSurface *surface, UBYTE *fade_table)
{
    surface->FadeTable    = fade_table;
    surface->FadeStepRow  = NULL;
    if (&surface->SurfaceArea == _lbpCurrentRA)
        LbDraw_SetFadeTable(fade_table);
}

// -----------------------------------------------------------------------
// LbSurface_GetFadeStep
// -----------------------------------------------------------------------
ULONG LbSurface_GetFadeStep(const TbSurface *surface)
{
    if (!surface->FadeTable || !surface->FadeStepRow)
        return 0;
    return (ULONG)((surface->FadeStepRow - surface->FadeTable) >> 8);
}

// -----------------------------------------------------------------------
// LbSurface_SetFadeStep
// -----------------------------------------------------------------------
void LbSurface_SetFadeStep(TbSurface *surface, ULONG step)
{
    if (!surface->FadeTable)
        return;
    surface->FadeStepRow = surface->FadeTable + (step << 8);
    if (&surface->SurfaceArea == _lbpCurrentRA)
        LbDraw_SetFadeStep(step);
}

// -----------------------------------------------------------------------
// LbSurface_GetFadeColour : Look up the faded colour for a surface.
// -----------------------------------------------------------------------
UINT LbSurface_GetFadeColour(TbSurface *surface, UINT colour)
{
    if (!surface->FadeStepRow || colour >= 256)
        return colour;
    return surface->FadeStepRow[colour];
}

// -----------------------------------------------------------------------
// LbSurface_GetGhostTable
// -----------------------------------------------------------------------
UBYTE * LbSurface_GetGhostTable(const TbSurface *surface)
{
    return surface->GhostTable;
}

// -----------------------------------------------------------------------
// LbSurface_SetGhostTable : Set the ghost (transparency) table for a surface.
// Also updates the current draw context if this surface is active.
// -----------------------------------------------------------------------
void LbSurface_SetGhostTable(TbSurface *surface, UBYTE *ghost_table)
{
    surface->GhostTable = ghost_table;
    if (&surface->SurfaceArea == _lbpCurrentRA)
        LbDraw_SetGhostTable(ghost_table);
}

// -----------------------------------------------------------------------
// LbSurface_ClearRect : Fill a rectangle on a surface with a solid colour.
// If rect is NULL, the entire surface is cleared.
// -----------------------------------------------------------------------
TbError LbSurface_ClearRect(const TbSurface *surface, const Pop3Rect *rect, TbColour colour, ULONG flags)
{
    if (!surface)
        return LB_ERROR;

    UBYTE *data  = surface->SurfaceArea.mpData;
    UINT pitch   = surface->SurfaceArea.mPitch;
    UINT width   = surface->SurfaceArea.mSize.Width;
    UINT height  = surface->SurfaceArea.mSize.Height;

    bool locked = false;
    if (!data && surface->lpSurface)
    {
        DDSURFACEDESC ddsd;
        memset(&ddsd, 0, sizeof(ddsd));
        ddsd.dwSize = sizeof(ddsd);
        if (surface->lpSurface->Lock(NULL, &ddsd, DDLOCK_WAIT, NULL) != DD_OK)
            return LB_ERROR;
        data   = (UBYTE*)ddsd.lpSurface;
        pitch  = (UINT)ddsd.lPitch;
        locked = true;
    }

    if (!data)
        return LB_ERROR;

    UINT x0 = 0, y0 = 0, x1 = width, y1 = height;
    if (rect)
    {
        x0 = (UINT)rect->Left;
        y0 = (UINT)rect->Top;
        x1 = (UINT)rect->Right;
        y1 = (UINT)rect->Bottom;
        if (x1 > width)  x1 = width;
        if (y1 > height) y1 = height;
    }

    UBYTE idx = colour.Index;
    for (UINT y = y0; y < y1; ++y)
        memset(data + y * pitch + x0, idx, x1 - x0);

    if (locked)
        surface->lpSurface->Unlock(data);

    return LB_OK;
}

// -----------------------------------------------------------------------
// LbSurface_GetAddressOf : Return pointer to pixel (x,y) in current surface.
// -----------------------------------------------------------------------
UBYTE * LbSurface_GetAddressOf(SINT x, SINT y)
{
    if (!_lbpCurrentRA)
        return NULL;
    return _lbCurrentOriginPtr + x + y * (SINT)_lbpCurrentRA->mPitch;
}

// -----------------------------------------------------------------------
// LbSurface_MapToRenderArea : Copy surface's Pop3RenderArea into ra.
// -----------------------------------------------------------------------
Pop3RenderArea * LbSurface_MapToRenderArea(Pop3RenderArea &ra, TbSurface *surface)
{
    ra = surface->SurfaceArea;
    return &ra;
}

// -----------------------------------------------------------------------
// LbSurface_GetOriginPtr : Deprecated — returns NULL.
// -----------------------------------------------------------------------
UBYTE * LbSurface_GetOriginPtr(const TbSurface *surface)
{
    return NULL;
}

// -----------------------------------------------------------------------
// LbSurface_StretchBlt : Not implemented for software surfaces.
// -----------------------------------------------------------------------
TbError LbSurface_StretchBlt(const TbSurface *destS, const Pop3Rect *destR, const TbSurface *srcS, const Pop3Rect *srcR, ULONG Flags)
{
    return LB_ERROR;
}

// -----------------------------------------------------------------------
// LbSurface_Blt : Not implemented for software surfaces.
// -----------------------------------------------------------------------
TbError LbSurface_Blt(const TbSurface *destS, UINT x, UINT y, const TbSurface *srcS, const Pop3Rect *srcR, ULONG Flags)
{
    return LB_ERROR;
}

// -----------------------------------------------------------------------
// LbSurface_GetDDSurface : Returns the underlying IDirectDrawSurface pointer.
// -----------------------------------------------------------------------
IDirectDrawSurface * LbSurface_GetDDSurface(TbSurface *pSurface)
{
    if (!pSurface)
        return NULL;
    return pSurface->lpSurface;
}
