/*
 * Copyright (c) 2010 Toni Spets <toni.spets@iki.fi>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <windows.h>
#include <stdio.h>

#include "main.h"
#include "hook.h"
#include "surface.h"
#include "mouse.h"
#include "scale_pattern.h"

// enables redraw via blt/unlock if there wasn't any flip for X ms
#define FLIP_REDRAW_TIMEOUT 1000 / 10

void dump_ddbltflags(DWORD dwFlags);
void dump_ddscaps(DWORD dwCaps);
void dump_ddsd(DWORD dwFlags);
DWORD WINAPI render_soft_main(void);
void *pvBmpBits;

HRESULT __stdcall ddraw_surface_QueryInterface(IDirectDrawSurfaceImpl *This, REFIID riid, void **obj)
{
    printf("??? DirectDrawSurface::QueryInterface(This=%p, riid=%08X, obj=%p)\n", This, (unsigned int)riid, obj);

    if (riid && !IsEqualGUID(&IID_IDirectDrawSurface, riid))
    {
        printf("  GUID = %08X\n", ((GUID *)riid)->Data1);

        IDirectDrawSurface_AddRef(This);
    }

    *obj = This;

    return S_OK;
}

ULONG __stdcall ddraw_surface_AddRef(IDirectDrawSurfaceImpl *This)
{
    printf("DirectDrawSurface::AddRef(This=%p)\n", This);
    This->Ref++;
    return This->Ref;
}

ULONG __stdcall ddraw_surface_Release(IDirectDrawSurfaceImpl *This)
{
    printf("DirectDrawSurface::Release(This=%p)\n", This);

    This->Ref--;

    if(This->Ref == 0)
    {
        printf("    Released (%p)\n", This);

        if(This->caps & DDSCAPS_PRIMARYSURFACE)
        {
            EnterCriticalSection(&ddraw->cs);
            ddraw->primary = NULL;
            LeaveCriticalSection(&ddraw->cs);
        }

        if (This->bitmap)
            DeleteObject(This->bitmap);
        else if (This->surface)
            HeapFree(GetProcessHeap(), 0, This->surface);

        if (This->hDC)
            DeleteDC(This->hDC);

        if (This->bmi)
            HeapFree(GetProcessHeap(), 0, This->bmi);

        if(This->palette && This->palette != LastFreedPalette)
        {
            IDirectDrawPalette_Release(This->palette);
        }
        HeapFree(GetProcessHeap(), 0, This);
        return 0;
    }
    return This->Ref;
}

HRESULT __stdcall ddraw_surface_AddAttachedSurface(IDirectDrawSurfaceImpl *This, LPDIRECTDRAWSURFACE lpDDSurface)
{
    printf("??? DirectDrawSurface::AddAttachedSurface(This=%p, lpDDSurface=%p)\n", This, lpDDSurface);
    IDirectDrawSurface_AddRef(lpDDSurface);
    return DD_OK;
}

HRESULT __stdcall ddraw_surface_AddOverlayDirtyRect(IDirectDrawSurfaceImpl *This, LPRECT a)
{
    printf("??? DirectDrawSurface::AddOverlayDirtyRect(This=%p, ...)\n", This);
    return DD_OK;
}

HRESULT __stdcall ddraw_surface_Blt(IDirectDrawSurfaceImpl *This, LPRECT lpDestRect, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx)
{
    IDirectDrawSurfaceImpl *Source = (IDirectDrawSurfaceImpl *)lpDDSrcSurface;

#if _DEBUG_X
    printf("DirectDrawSurface::Blt(This=%p, lpDestRect=%p, lpDDSrcSurface=%p, lpSrcRect=%p, dwFlags=%08X, lpDDBltFx=%p)\n", This, lpDestRect, lpDDSrcSurface, lpSrcRect, (int)dwFlags, lpDDBltFx);

    dump_ddbltflags(dwFlags);
    
    if(lpDestRect)
    {
        printf(" dest: l: %d t: %d r: %d b: %d\n", (int)lpDestRect->left, (int)lpDestRect->top, (int)lpDestRect->right, (int)lpDestRect->bottom);
    }
    if(lpSrcRect)
    {
        printf("  src: l: %d t: %d r: %d b: %d\n", (int)lpSrcRect->left, (int)lpSrcRect->top, (int)lpSrcRect->right, (int)lpSrcRect->bottom);
    }
#endif

    RECT srcRect = { 0, 0, Source ? Source->width : 0, Source ? Source->height : 0 };
    RECT dstRect = { 0, 0, This->width, This->height };

    if (lpSrcRect && Source)
        memcpy(&srcRect, lpSrcRect, sizeof(srcRect));

    if (lpDestRect)
        memcpy(&dstRect, lpDestRect, sizeof(dstRect));

    // stretch or clip?
    BOOL isStretchBlt = 
        ((srcRect.right - srcRect.left) != (dstRect.right - dstRect.left)) ||
        ((srcRect.bottom - srcRect.top) != (dstRect.bottom - dstRect.top));

    if (Source)
    {
        if (srcRect.right > Source->width)
            srcRect.right = Source->width;

        if (srcRect.bottom > Source->height)
            srcRect.bottom = Source->height;
    }

    if (dstRect.right > This->width)
        dstRect.right = This->width;

    if (dstRect.bottom > This->height)
        dstRect.bottom = This->height;

    int src_w = srcRect.right - srcRect.left;
    int src_h = srcRect.bottom - srcRect.top;
    int src_x = srcRect.left;
    int src_y = srcRect.top;

    int dst_w = dstRect.right - dstRect.left;
    int dst_h = dstRect.bottom - dstRect.top;
    int dst_x = dstRect.left;
    int dst_y = dstRect.top;


    if (This->surface && (dwFlags & DDBLT_COLORFILL))
    {
        unsigned char *dst = (unsigned char *)This->surface + (dst_x * This->lXPitch) + (This->lPitch * dst_y);
        unsigned char *firstRow = dst;
        unsigned int dst_pitch = dst_w * This->lXPitch;
        int x, i;

        if (This->bpp == 8)
        {
            unsigned char color = (unsigned char)lpDDBltFx->dwFillColor;

            for (i = 0; i < dst_h; i++)
            {
                memset(dst, color, dst_pitch);
                dst += This->lPitch;
            }
        }
        else if (This->bpp == 16)
        {
            unsigned short *row1 = (unsigned short *)dst;
            unsigned short color = (unsigned short)lpDDBltFx->dwFillColor;

            if ((color & 0xFF) == (color >> 8))
            {
                unsigned char c8 = (unsigned char)(color & 0xFF);

                for (i = 0; i < dst_h; i++)
                {
                    memset(dst, c8, dst_pitch);
                    dst += This->lPitch;
                }
            }
            else
            {
                for (x = 0; x < dst_w; x++)
                    row1[x] = color;

                for (i = 1; i < dst_h; i++)
                {
                    dst += This->lPitch;
                    memcpy(dst, firstRow, dst_pitch);
                }
            }
        }
    }

    if (Source)
    {
        if ((dwFlags & DDBLT_KEYSRC) || (dwFlags & DDBLT_KEYSRCOVERRIDE))
        {
            DDCOLORKEY colorKey;

            colorKey.dwColorSpaceLowValue =
                (dwFlags & DDBLT_KEYSRCOVERRIDE) ? 
                    lpDDBltFx->ddckSrcColorkey.dwColorSpaceLowValue : Source->colorKey.dwColorSpaceLowValue;

            colorKey.dwColorSpaceHighValue =
                (dwFlags & DDBLT_KEYSRCOVERRIDE) ?
                    lpDDBltFx->ddckSrcColorkey.dwColorSpaceHighValue : Source->colorKey.dwColorSpaceHighValue;
            
            if (!isStretchBlt)
            {
                int width = dst_w > src_w ? src_w : dst_w;
                int height = dst_h > src_h ? src_h : dst_h;

                if (This->bpp == 8)
                {
                    int y1, x1;
                    for (y1 = 0; y1 < height; y1++)
                    {
                        int ydst = This->width * (y1 + dst_y);
                        int ysrc = Source->width * (y1 + src_y);

                        for (x1 = 0; x1 < width; x1++)
                        {
                            unsigned char c = ((unsigned char *)Source->surface)[x1 + src_x + ysrc];

                            if (c < colorKey.dwColorSpaceLowValue || c > colorKey.dwColorSpaceHighValue)
                            {
                                ((unsigned char *)This->surface)[x1 + dst_x + ydst] = c;
                            }
                        }
                    }
                }
                else if (This->bpp == 16)
                {
                    int y1, x1;
                    for (y1 = 0; y1 < height; y1++)
                    {
                        int ydst = This->width * (y1 + dst_y);
                        int ysrc = Source->width * (y1 + src_y);

                        for (x1 = 0; x1 < width; x1++)
                        {
                            unsigned short c = ((unsigned short *)Source->surface)[x1 + src_x + ysrc];

                            if (c < colorKey.dwColorSpaceLowValue || c > colorKey.dwColorSpaceHighValue)
                            {
                                ((unsigned short *)This->surface)[x1 + dst_x + ydst] = c;
                            }
                        }
                    }
                }
            }
            else
            {
                printf("   DDBLT_KEYSRC / DDBLT_KEYSRCOVERRIDE does not support stretching");
            }
        }
        else
        {
            if (!isStretchBlt)
            {
                int width = dst_w > src_w ? src_w : dst_w;
                int height = dst_h > src_h ? src_h : dst_h;

                unsigned char *src = 
                    (unsigned char *)Source->surface + (src_x * Source->lXPitch) + (Source->lPitch * src_y);

                unsigned char *dst = 
                    (unsigned char *)This->surface + (dst_x * This->lXPitch) + (This->lPitch * dst_y);

                unsigned int widthInBytes = width * This->lXPitch;

                int i;
                for (i = 0; i < height; i++)
                {
                    memcpy(dst, src, widthInBytes);

                    src += Source->lPitch;
                    dst += This->lPitch;
                }
            }
            else
            {
                /* Linear scaling using integer math
                * Since the scaling pattern for x will aways be the same, the pattern itself gets pre-calculated
                * and stored in an array.
                * Y scaling pattern gets calculated during the blit loop
                */
                unsigned int x_ratio = (unsigned int)((src_w << 16) / dst_w) + 1;
                unsigned int y_ratio = (unsigned int)((src_h << 16) / dst_h) + 1;

                unsigned int s_src_x, s_src_y;
                unsigned int dest_base, source_base;

                scale_pattern *pattern = malloc((dst_w + 1) * (sizeof(scale_pattern)));
                int pattern_idx = 0;
                unsigned int last_src_x = 0;

                if (pattern != NULL)
                {
                    pattern[pattern_idx] = (scale_pattern) { ONCE, 0, 0, 1 };

                    /* Build the pattern! */
                    int x;
                    for (x = 1; x < dst_w; x++) {
                        s_src_x = (x * x_ratio) >> 16;
                        if (s_src_x == last_src_x)
                        {
                            if (pattern[pattern_idx].type == REPEAT || pattern[pattern_idx].type == ONCE)
                            {
                                pattern[pattern_idx].type = REPEAT;
                                pattern[pattern_idx].count++;
                            }
                            else if (pattern[pattern_idx].type == SEQUENCE)
                            {
                                pattern_idx++;
                                pattern[pattern_idx] = (scale_pattern) { REPEAT, x, s_src_x, 1 };
                            }
                        }
                        else if (s_src_x == last_src_x + 1)
                        {
                            if (pattern[pattern_idx].type == SEQUENCE || pattern[pattern_idx].type == ONCE)
                            {
                                pattern[pattern_idx].type = SEQUENCE;
                                pattern[pattern_idx].count++;
                            }
                            else if (pattern[pattern_idx].type == REPEAT)
                            {
                                pattern_idx++;
                                pattern[pattern_idx] = (scale_pattern) { ONCE, x, s_src_x, 1 };
                            }
                        }
                        else
                        {
                            pattern_idx++;
                            pattern[pattern_idx] = (scale_pattern) { ONCE, x, s_src_x, 1 };
                        }
                        last_src_x = s_src_x;
                    }
                    pattern[pattern_idx + 1] = (scale_pattern) { END, 0, 0, 0 };


                    /* Do the actual blitting */
                    int count = 0;
                    int y;

                    for (y = 0; y < dst_h; y++) {

                        dest_base = dst_x + This->width * (y + dst_y);

                        s_src_y = (y * y_ratio) >> 16;

                        source_base = src_x + Source->width * (s_src_y + src_y);

                        pattern_idx = 0;
                        scale_pattern *current = &pattern[pattern_idx];

                        if (This->bpp == 8)
                        {
                            unsigned char *d, *s, v;
                            unsigned char *src = (unsigned char *)Source->surface;
                            unsigned char *dst = (unsigned char *)This->surface;

                            do {
                                switch (current->type)
                                {
                                case ONCE:
                                    dst[dest_base + current->dst_index] =
                                        src[source_base + current->src_index];
                                    break;

                                case REPEAT:
                                    d = (dst + dest_base + current->dst_index);
                                    v = src[source_base + current->src_index];

                                    count = current->count;
                                    while (count-- > 0)
                                        *d++ = v;

                                    break;

                                case SEQUENCE:
                                    d = dst + dest_base + current->dst_index;
                                    s = src + source_base + current->src_index;

                                    memcpy((void *)d, (void *)s, current->count * This->lXPitch);
                                    break;

                                case END:
                                default:
                                    break;
                                }

                                current = &pattern[++pattern_idx];
                            } while (current->type != END);
                        }
                        else if (This->bpp == 16)
                        {
                            unsigned short *d, *s, v;
                            unsigned short *src = (unsigned short *)Source->surface;
                            unsigned short *dst = (unsigned short *)This->surface;

                            do {
                                switch (current->type)
                                {
                                case ONCE:
                                    dst[dest_base + current->dst_index] =
                                        src[source_base + current->src_index];
                                    break;

                                case REPEAT:
                                    d = (dst + dest_base + current->dst_index);
                                    v = src[source_base + current->src_index];

                                    count = current->count;
                                    while (count-- > 0)
                                        *d++ = v;

                                    break;

                                case SEQUENCE:
                                    d = dst + dest_base + current->dst_index;
                                    s = src + source_base + current->src_index;

                                    memcpy((void *)d, (void *)s, current->count * This->lXPitch);
                                    break;

                                case END:
                                default:
                                    break;
                                }

                                current = &pattern[++pattern_idx];
                            } while (current->type != END);
                        }
                    }
                    free(pattern);
                }
            }

        }
    }

    if(This->caps & DDSCAPS_PRIMARYSURFACE && 
        ddraw->render.run &&
        (!(This->flags & DDSD_BACKBUFFERCOUNT) || This->lastFlipTick + FLIP_REDRAW_TIMEOUT < timeGetTime()))
    {
        InterlockedExchange(&ddraw->render.surfaceUpdated, TRUE);
        ReleaseSemaphore(ddraw->render.sem, 1, NULL);
        if (ddraw->renderer == render_soft_main)
        {
            WaitForSingleObject(ddraw->render.ev, INFINITE);
            ResetEvent(ddraw->render.ev);
        }
        else
        {
            SwitchToThread();
        }

        if (ddraw->ticksLimiter.ticklength > 0)
        {
            ddraw->ticksLimiter.useBltOrFlip = TRUE;
            LimitGameTicks();
        }
    }

    return DD_OK;
}

HRESULT __stdcall ddraw_surface_BltBatch(IDirectDrawSurfaceImpl *This, LPDDBLTBATCH a, DWORD b, DWORD c)
{
    printf("??? IDirectDrawSurface::BltBatch(This=%p, ...)\n", This);
    return DD_OK;
}

HRESULT __stdcall ddraw_surface_BltFast(IDirectDrawSurfaceImpl *This, DWORD dst_x, DWORD dst_y, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DWORD flags)
{
    IDirectDrawSurfaceImpl *Source = (IDirectDrawSurfaceImpl *)lpDDSrcSurface;

#if _DEBUG_X
    printf("IDirectDrawSurface::BltFast(This=%p, x=%d, y=%d, lpDDSrcSurface=%p, lpSrcRect=%p, flags=%08X)\n", This, dst_x, dst_y, lpDDSrcSurface, lpSrcRect, flags);

    if (flags & DDBLTFAST_NOCOLORKEY)
    {
        printf("  DDBLTFAST_NOCOLORKEY\n");
    }

    if (flags & DDBLTFAST_SRCCOLORKEY)
    {
        printf("  DDBLTFAST_SRCCOLORKEY\n");
    }

    if (flags & DDBLTFAST_DESTCOLORKEY)
    {
        printf("  DDBLTFAST_DESTCOLORKEY\n");
    }
#endif

    RECT srcRect = { 0, 0, Source ? Source->width : 0, Source ? Source->height : 0 };

    if (lpSrcRect && Source)
    {
        memcpy(&srcRect, lpSrcRect, sizeof(srcRect));

        if (srcRect.right > Source->width)
            srcRect.right = Source->width;

        if (srcRect.bottom > Source->height)
            srcRect.bottom = Source->height;
    }

    int src_x = srcRect.left;
    int src_y = srcRect.top;

    RECT dstRect = { dst_x, dst_y, (srcRect.right - srcRect.left) + dst_x, (srcRect.bottom - srcRect.top) + dst_y };

    if (dstRect.right > This->width)
        dstRect.right = This->width;

    if (dstRect.bottom > This->height)
        dstRect.bottom = This->height;

    int dst_w = dstRect.right - dstRect.left;
    int dst_h = dstRect.bottom - dstRect.top;

    if (Source)
    {
        if (flags & DDBLTFAST_SRCCOLORKEY)
        {
            if (This->bpp == 8)
            {
                int y1, x1;
                for (y1 = 0; y1 < dst_h; y1++)
                {
                    int ydst = This->width * (y1 + dst_y);
                    int ysrc = Source->width * (y1 + src_y);

                    for (x1 = 0; x1 < dst_w; x1++)
                    {
                        unsigned char c = ((unsigned char *)Source->surface)[x1 + src_x + ysrc];

                        if (c < Source->colorKey.dwColorSpaceLowValue || c > Source->colorKey.dwColorSpaceHighValue)
                        {
                            ((unsigned char *)This->surface)[x1 + dst_x + ydst] = c;
                        }
                    }
                }
            }
            else if (This->bpp == 16)
            {
                int y1, x1;
                for (y1 = 0; y1 < dst_h; y1++)
                {
                    int ydst = This->width * (y1 + dst_y);
                    int ysrc = Source->width * (y1 + src_y);

                    for (x1 = 0; x1 < dst_w; x1++)
                    {
                        unsigned short c = ((unsigned short *)Source->surface)[x1 + src_x + ysrc];
                        
                        if (c < Source->colorKey.dwColorSpaceLowValue || c > Source->colorKey.dwColorSpaceHighValue)
                        {
                            ((unsigned short *)This->surface)[x1 + dst_x + ydst] = c;
                        }
                    }
                }
            }
        }
        else
        {
            unsigned char *src =
                (unsigned char *)Source->surface + (src_x * Source->lXPitch) + (Source->lPitch * src_y);

            unsigned char *dst =
                (unsigned char *)This->surface + (dst_x * This->lXPitch) + (This->lPitch * dst_y);

            unsigned int widthInBytes = dst_w * This->lXPitch;

            int i;
            for (i = 0; i < dst_h; i++)
            {
                memcpy(dst, src, widthInBytes);

                src += Source->lPitch;
                dst += This->lPitch;
            }
        }
    }

    if (This->caps & DDSCAPS_PRIMARYSURFACE &&
        ddraw->render.run &&
        (!(This->flags & DDSD_BACKBUFFERCOUNT) || This->lastFlipTick + FLIP_REDRAW_TIMEOUT < timeGetTime()))
    {
        InterlockedExchange(&ddraw->render.surfaceUpdated, TRUE);
        ReleaseSemaphore(ddraw->render.sem, 1, NULL);
    }

    return DD_OK;
}

HRESULT __stdcall ddraw_surface_DeleteAttachedSurface(IDirectDrawSurfaceImpl *This, DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDSurface)
{
    printf("IDirectDrawSurface::DeleteAttachedSurface(This=%p, dwFlags=%08X, lpDDSurface=%p)\n", This, (int)dwFlags, lpDDSurface);
    IDirectDrawSurface_Release(lpDDSurface);
    return DD_OK;
}

HRESULT __stdcall ddraw_surface_GetSurfaceDesc(IDirectDrawSurfaceImpl *This, LPDDSURFACEDESC lpDDSurfaceDesc)
{
#if _DEBUG_X
    printf("IDirectDrawSurface::GetSurfaceDesc(This=%p, lpDDSurfaceDesc=%p)\n", This, lpDDSurfaceDesc);
#endif

    lpDDSurfaceDesc->dwSize = sizeof(DDSURFACEDESC);
    lpDDSurfaceDesc->dwFlags = DDSD_WIDTH|DDSD_HEIGHT|DDSD_PITCH|DDSD_PIXELFORMAT|DDSD_LPSURFACE;
    lpDDSurfaceDesc->dwWidth = This->width;
    lpDDSurfaceDesc->dwHeight = This->height;
    lpDDSurfaceDesc->lPitch = This->lPitch;
    lpDDSurfaceDesc->lpSurface = This->surface;
    lpDDSurfaceDesc->ddpfPixelFormat.dwSize = sizeof(DDPIXELFORMAT);
    lpDDSurfaceDesc->ddpfPixelFormat.dwFlags = DDPF_RGB;
    lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount = This->bpp;
    lpDDSurfaceDesc->ddsCaps.dwCaps = This->caps | DDSCAPS_VIDEOMEMORY;

    if (This->bpp == 8)
    {
        lpDDSurfaceDesc->ddpfPixelFormat.dwFlags |= DDPF_PALETTEINDEXED8;
    }
    else if (This->bpp == 16)
    {
        lpDDSurfaceDesc->ddpfPixelFormat.dwRBitMask = 0xF800;
        lpDDSurfaceDesc->ddpfPixelFormat.dwGBitMask = 0x07E0;
        lpDDSurfaceDesc->ddpfPixelFormat.dwBBitMask = 0x001F;
    }

    return DD_OK;
}

HRESULT __stdcall ddraw_surface_EnumAttachedSurfaces(IDirectDrawSurfaceImpl *This, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback)
{
    printf("IDirectDrawSurface::EnumAttachedSurfaces(This=%p, lpContext=%p, lpEnumSurfacesCallback=%p)\n", This, lpContext, lpEnumSurfacesCallback);

    /* this is not actually complete, but Carmageddon seems to call EnumAttachedSurfaces instead of GetSurfaceDesc to get the main surface description */
    static DDSURFACEDESC ddSurfaceDesc;
    memset(&ddSurfaceDesc, 0, sizeof(DDSURFACEDESC));

    ddraw_surface_GetSurfaceDesc(This, &ddSurfaceDesc);
    This->caps |= DDSCAPS_BACKBUFFER; // Nox hack
    lpEnumSurfacesCallback((LPDIRECTDRAWSURFACE)This, &ddSurfaceDesc, lpContext);

    if ((This->caps & DDSCAPS_PRIMARYSURFACE) && (This->caps & DDSCAPS_FLIP) && !(This->caps & DDSCAPS_BACKBUFFER))
        ddraw_surface_AddRef(This);

    This->caps &= ~DDSCAPS_BACKBUFFER;

    return DD_OK;
}

HRESULT __stdcall ddraw_surface_EnumOverlayZOrders(IDirectDrawSurfaceImpl *This, DWORD a, LPVOID b, LPDDENUMSURFACESCALLBACK c)
{
    printf("??? IDirectDrawSurface::EnumOverlayZOrders(This=%p, ...)\n", This);
    return DD_OK;
}

HRESULT __stdcall ddraw_surface_Flip(IDirectDrawSurfaceImpl *This, LPDIRECTDRAWSURFACE surface, DWORD flags)
{
#if _DEBUG_X
    printf("IDirectDrawSurface::Flip(This=%p, surface=%p, flags=%08X)\n", This, surface, flags);
#endif

    if(This->caps & DDSCAPS_PRIMARYSURFACE && ddraw->render.run)
    {
        FILETIME lastFlipFT = { 0 };
        if (ddraw->flipLimiter.hTimer)
            GetSystemTimeAsFileTime(&lastFlipFT);

        This->lastFlipTick = timeGetTime();

        InterlockedExchange(&ddraw->render.surfaceUpdated, TRUE);
        
        if (ddraw->renderer == render_soft_main)
        {
            ResetEvent(ddraw->render.ev);
            ReleaseSemaphore(ddraw->render.sem, 1, NULL);
            WaitForSingleObject(ddraw->render.ev, INFINITE);
        }
        else
        {
            ReleaseSemaphore(ddraw->render.sem, 1, NULL);
            SwitchToThread();
        }

        if (flags & DDFLIP_WAIT)
        {
            if (ddraw->flipLimiter.hTimer)
            {
                if (!ddraw->flipLimiter.dueTime.QuadPart)
                {
                    memcpy(&ddraw->flipLimiter.dueTime, &lastFlipFT, sizeof(LARGE_INTEGER));
                }
                else
                {
                    while (CompareFileTime((FILETIME *)&ddraw->flipLimiter.dueTime, &lastFlipFT) == -1)
                        ddraw->flipLimiter.dueTime.QuadPart += ddraw->flipLimiter.tickLengthNs;

                    SetWaitableTimer(ddraw->flipLimiter.hTimer, &ddraw->flipLimiter.dueTime, 0, NULL, NULL, FALSE);
                    WaitForSingleObject(ddraw->flipLimiter.hTimer, ddraw->flipLimiter.ticklength * 2);
                }
            }
            else
            {
                DWORD tick = This->lastFlipTick;
                while (tick % ddraw->flipLimiter.ticklength) tick++;
                int sleepTime = tick - This->lastFlipTick;

                int renderTime = timeGetTime() - This->lastFlipTick;
                if (renderTime > 0)
                    sleepTime -= renderTime;

                if (sleepTime > 0 && sleepTime <= ddraw->flipLimiter.ticklength)
                    Sleep(sleepTime);
            }
        }

        if (ddraw->ticksLimiter.ticklength > 0)
        {
            ddraw->ticksLimiter.useBltOrFlip = TRUE;
            LimitGameTicks();
        }
    }

    return DD_OK;
}

HRESULT __stdcall ddraw_surface_GetAttachedSurface(IDirectDrawSurfaceImpl *This, LPDDSCAPS lpDdsCaps, LPDIRECTDRAWSURFACE FAR *surface)
{
    printf("??? IDirectDrawSurface::GetAttachedSurface(This=%p, dwCaps=%08X, surface=%p)\n", This, lpDdsCaps->dwCaps, surface);
    
    if ((This->caps & DDSCAPS_PRIMARYSURFACE) && (This->caps & DDSCAPS_FLIP) && (lpDdsCaps->dwCaps & DDSCAPS_BACKBUFFER))
    {
        ddraw_surface_AddRef(This);
        *surface = (LPDIRECTDRAWSURFACE)This;
    }

    return DD_OK;
}

HRESULT __stdcall ddraw_surface_GetBltStatus(IDirectDrawSurfaceImpl *This, DWORD a)
{
#if _DEBUG_X
    printf("??? IDirectDrawSurface::GetBltStatus(This=%p, ...)\n", This);
#endif
    return DD_OK;
}

HRESULT __stdcall ddraw_surface_GetCaps(IDirectDrawSurfaceImpl *This, LPDDSCAPS lpDDSCaps)
{
    printf("DirectDrawSurface::GetCaps(This=%p, lpDDSCaps=%p)\n", This, lpDDSCaps);
    lpDDSCaps->dwCaps = This->caps;
    return DD_OK;
}

HRESULT __stdcall ddraw_surface_GetClipper(IDirectDrawSurfaceImpl *This, LPDIRECTDRAWCLIPPER FAR *a)
{
#if _DEBUG_X
    printf("??? IDirectDrawSurface::GetClipper(This=%p, ...)\n", This);
#endif
    return DD_OK;
}

HRESULT __stdcall ddraw_surface_GetColorKey(IDirectDrawSurfaceImpl *This, DWORD flags, LPDDCOLORKEY colorKey)
{
#if _DEBUG_X
    printf("??? DirectDrawSurface::GetColorKey(This=%p, flags=0x%08X, colorKey=%p)\n", This, flags, colorKey);
#endif

    if (colorKey)
    {
        colorKey->dwColorSpaceHighValue = This->colorKey.dwColorSpaceHighValue;
        colorKey->dwColorSpaceLowValue = This->colorKey.dwColorSpaceLowValue;
    }

    return DD_OK;
}

HRESULT __stdcall ddraw_surface_GetDC(IDirectDrawSurfaceImpl *This, HDC FAR *a)
{
#if _DEBUG_X
    printf("IDirectDrawSurface::GetDC(This=%p, ...)\n", This);
#endif
    if (This->width % 4)
    {
        printf("   GetDC: width=%d height=%d\n", This->width, This->height);
    }

    RGBQUAD *data = 
        This->palette && This->palette->data_rgb ? This->palette->data_rgb :
        ddraw->primary && ddraw->primary->palette ? ddraw->primary->palette->data_rgb :
        NULL;

    if (data)
        SetDIBColorTable(This->hDC, 0, 256, data);

    *a = This->hDC;
    return DD_OK;
}

HRESULT __stdcall ddraw_surface_GetFlipStatus(IDirectDrawSurfaceImpl *This, DWORD a)
{
#if _DEBUG_X
    printf("??? IDirectDrawSurface::GetFlipStatus(This=%p, ...)\n", This);
#endif
    return DD_OK;
}

HRESULT __stdcall ddraw_surface_GetOverlayPosition(IDirectDrawSurfaceImpl *This, LPLONG a, LPLONG b)
{
    printf("??? IDirectDrawSurface::GetOverlayPosition(This=%p, ...)\n", This);
    return DD_OK;
}

HRESULT __stdcall ddraw_surface_GetPalette(IDirectDrawSurfaceImpl *This, LPDIRECTDRAWPALETTE FAR *lplpDDPalette)
{
    printf("DirectDrawSurface::GetPalette(This=%p, lplpDDPalette=%p)\n", This, lplpDDPalette);
    *lplpDDPalette = (LPDIRECTDRAWPALETTE)This->palette;
    return DD_OK;
}

HRESULT __stdcall ddraw_surface_GetPixelFormat(IDirectDrawSurfaceImpl *This, LPDDPIXELFORMAT ddpfPixelFormat)
{
    printf("IDirectDrawSurface::GetPixelFormat(This=%p, ...)\n", This);

    DWORD size = ddpfPixelFormat->dwSize;

    if (size == sizeof(DDPIXELFORMAT))
    {
        memset(ddpfPixelFormat, 0, sizeof(DDPIXELFORMAT));

        ddpfPixelFormat->dwSize = size;
        ddpfPixelFormat->dwFlags = DDPF_RGB;
        ddpfPixelFormat->dwRGBBitCount = This->bpp;

        if (This->bpp == 8)
        {
            ddpfPixelFormat->dwFlags |= DDPF_PALETTEINDEXED8;
        }
        else if (This->bpp == 16)
        {
            ddpfPixelFormat->dwRBitMask = 0xF800;
            ddpfPixelFormat->dwGBitMask = 0x07E0;
            ddpfPixelFormat->dwBBitMask = 0x001F;
        }

        return DD_OK;
    }

    return DDERR_INVALIDPARAMS;
}

HRESULT __stdcall ddraw_surface_Initialize(IDirectDrawSurfaceImpl *This, LPDIRECTDRAW a, LPDDSURFACEDESC b)
{
    printf("??? IDirectDrawSurface::Initialize(This=%p, ...)\n", This);
    return DD_OK;
}

HRESULT __stdcall ddraw_surface_IsLost(IDirectDrawSurfaceImpl *This)
{
#if _DEBUG_X
    printf("IDirectDrawSurface::IsLost(This=%p)\n", This);
#endif
    return DD_OK;
}

HRESULT __stdcall ddraw_surface_Lock(IDirectDrawSurfaceImpl *This, LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent)
{
#if _DEBUG_X
    printf("DirectDrawSurface::Lock(This=%p, lpDestRect=%p, lpDDSurfaceDesc=%p, dwFlags=%08X, hEvent=%p)\n", This, lpDestRect, lpDDSurfaceDesc, (int)dwFlags, hEvent);

    if(dwFlags & DDLOCK_SURFACEMEMORYPTR)
    {
        printf(" dwFlags: DDLOCK_SURFACEMEMORYPTR\n");
    }
    if(dwFlags & DDLOCK_WAIT)
    {
        printf(" dwFlags: DDLOCK_WAIT\n");
    }
    if(dwFlags & DDLOCK_EVENT)
    {
        printf(" dwFlags: DDLOCK_EVENT\n");
    }
    if(dwFlags & DDLOCK_READONLY)
    {
        printf(" dwFlags: DDLOCK_READONLY\n");
    }
    if(dwFlags & DDLOCK_WRITEONLY)
    {
        printf(" dwFlags: DDLOCK_WRITEONLY\n");
    }
#endif

    return ddraw_surface_GetSurfaceDesc(This, lpDDSurfaceDesc);
}

HRESULT __stdcall ddraw_surface_ReleaseDC(IDirectDrawSurfaceImpl *This, HDC a)
{
#if _DEBUG_X
    printf("DirectDrawSurface::ReleaseDC(This=%p, ...)\n", This);
#endif
    return DD_OK;
}

HRESULT __stdcall ddraw_surface_Restore(IDirectDrawSurfaceImpl *This)
{
#if _DEBUG_X
    printf("??? DirectDrawSurface::Restore(This=%p)\n", This);
#endif
    return DD_OK;
}

HRESULT __stdcall ddraw_surface_SetClipper(IDirectDrawSurfaceImpl *This, LPDIRECTDRAWCLIPPER a)
{
    printf("??? DirectDrawSurface::SetClipper(This=%p, ...)\n", This);
    return DD_OK;
}

HRESULT __stdcall ddraw_surface_SetColorKey(IDirectDrawSurfaceImpl *This, DWORD flags, LPDDCOLORKEY colorKey)
{
#if _DEBUG_X
    printf("??? DirectDrawSurface::SetColorKey(This=%p, flags=0x%08X, colorKey=%p)\n", This, flags, colorKey);

    if (colorKey)
    {
        printf("  dwColorSpaceHighValue=%d\n", colorKey->dwColorSpaceHighValue);
        printf("  dwColorSpaceLowValue=%d\n", colorKey->dwColorSpaceLowValue);
    }
#endif

    if (colorKey)
    {
        This->colorKey.dwColorSpaceHighValue = colorKey->dwColorSpaceHighValue;
        This->colorKey.dwColorSpaceLowValue = colorKey->dwColorSpaceLowValue;
    }

    return DD_OK;
}

HRESULT __stdcall ddraw_surface_SetOverlayPosition(IDirectDrawSurfaceImpl *This, LONG a, LONG b)
{
    printf("??? DirectDrawSurface::SetOverlayPosition(This=%p, ...)\n", This);
    return DD_OK;
}

HRESULT __stdcall ddraw_surface_SetPalette(IDirectDrawSurfaceImpl *This, LPDIRECTDRAWPALETTE lpDDPalette)
{
    printf("DirectDrawSurface::SetPalette(This=%p, lpDDPalette=%p)\n", This, lpDDPalette);

    IDirectDrawPalette_AddRef(lpDDPalette);

    if(This->palette)
    {
        IDirectDrawPalette_Release(This->palette);
    }

    EnterCriticalSection(&ddraw->cs);

    This->palette = (IDirectDrawPaletteImpl *)lpDDPalette;
    This->palette->data_rgb = &This->bmi->bmiColors[0];

    int i;
    for (i = 0; i < 256; i++)
    {
        This->palette->data_rgb[i].rgbRed = This->palette->data_bgr[i] & 0xFF;
        This->palette->data_rgb[i].rgbGreen = (This->palette->data_bgr[i] >> 8) & 0xFF;
        This->palette->data_rgb[i].rgbBlue = (This->palette->data_bgr[i] >> 16) & 0xFF;
        This->palette->data_rgb[i].rgbReserved = 0;
    }

    LeaveCriticalSection(&ddraw->cs);

    return DD_OK;
}

HRESULT __stdcall ddraw_surface_Unlock(IDirectDrawSurfaceImpl *This, LPVOID lpRect)
{
#if _DEBUG_X
    printf("DirectDrawSurface::Unlock(This=%p, lpRect=%p)\n", This, lpRect);
#endif
    
    HWND hWnd = ddraw->bnetActive ? FindWindowEx(HWND_DESKTOP, NULL, "SDlgDialog", NULL) : NULL;
    if (hWnd && (This->caps & DDSCAPS_PRIMARYSURFACE))
    {
        if (ddraw->primary->palette && ddraw->primary->palette->data_rgb)
            SetDIBColorTable(ddraw->primary->hDC, 0, 256, ddraw->primary->palette->data_rgb);

        //GdiTransparentBlt idea taken from Aqrit's war2 ddraw

        RGBQUAD quad;
        GetDIBColorTable(ddraw->primary->hDC, 0xFE, 1, &quad);
        COLORREF color = RGB(quad.rgbRed, quad.rgbGreen, quad.rgbBlue);
        BOOL erase = FALSE;

        do
        {
            RECT rc;
            if (fake_GetWindowRect(hWnd, &rc))
            {
                if (rc.bottom - rc.top == 479)
                    erase = TRUE;

                HDC hDC = GetDCEx(hWnd, NULL, DCX_PARENTCLIP | DCX_CACHE);

                GdiTransparentBlt(
                    hDC,
                    0,
                    0,
                    rc.right - rc.left,
                    rc.bottom - rc.top,
                    ddraw->primary->hDC,
                    rc.left,
                    rc.top,
                    rc.right - rc.left,
                    rc.bottom - rc.top,
                    color
                );

                ReleaseDC(hWnd, hDC);
            }

        } while ((hWnd = FindWindowEx(HWND_DESKTOP, hWnd, "SDlgDialog", NULL)));

        if (erase)
        {
            BOOL x = ddraw->ticksLimiter.useBltOrFlip;

            DDBLTFX fx = { .dwFillColor = 0xFE };
            IDirectDrawSurface_Blt(This, NULL, NULL, NULL, DDBLT_COLORFILL, &fx);

            ddraw->ticksLimiter.useBltOrFlip = x;
        }
    }

    if (This->caps & DDSCAPS_PRIMARYSURFACE &&
        ddraw->render.run &&
        (!(This->flags & DDSD_BACKBUFFERCOUNT) || This->lastFlipTick + FLIP_REDRAW_TIMEOUT < timeGetTime()))
    {
        InterlockedExchange(&ddraw->render.surfaceUpdated, TRUE);
        ReleaseSemaphore(ddraw->render.sem, 1, NULL);

        if (ddraw->ticksLimiter.ticklength > 0 && !ddraw->ticksLimiter.useBltOrFlip)
            LimitGameTicks();
    }

    return DD_OK;
}

HRESULT __stdcall ddraw_surface_UpdateOverlay(IDirectDrawSurfaceImpl *This, LPRECT a, LPDIRECTDRAWSURFACE b, LPRECT c, DWORD d, LPDDOVERLAYFX e)
{
    printf("??? DirectDrawSurface::UpdateOverlay(This=%p, ...)\n", This);
    return DD_OK;
}

HRESULT __stdcall ddraw_surface_UpdateOverlayDisplay(IDirectDrawSurfaceImpl *This, DWORD a)
{
    printf("??? DirectDrawSurface::UpdateOverlayDisplay(This=%p, ...)\n", This);
    return DD_OK;
}

HRESULT __stdcall ddraw_surface_UpdateOverlayZOrder(IDirectDrawSurfaceImpl *This, DWORD a, LPDIRECTDRAWSURFACE b)
{
    printf("??? DirectDrawSurface::UpdateOverlayZOrder(This=%p, ...)\n", This);
    return DD_OK;
}

struct IDirectDrawSurfaceImplVtbl siface =
{
    /* IUnknown */
    ddraw_surface_QueryInterface,
    ddraw_surface_AddRef,
    ddraw_surface_Release,
    /* IDirectDrawSurface */
    ddraw_surface_AddAttachedSurface,
    ddraw_surface_AddOverlayDirtyRect,
    ddraw_surface_Blt,
    ddraw_surface_BltBatch,
    ddraw_surface_BltFast,
    ddraw_surface_DeleteAttachedSurface,
    ddraw_surface_EnumAttachedSurfaces,
    ddraw_surface_EnumOverlayZOrders,
    ddraw_surface_Flip,
    ddraw_surface_GetAttachedSurface,
    ddraw_surface_GetBltStatus,
    ddraw_surface_GetCaps,
    ddraw_surface_GetClipper,
    ddraw_surface_GetColorKey,
    ddraw_surface_GetDC,
    ddraw_surface_GetFlipStatus,
    ddraw_surface_GetOverlayPosition,
    ddraw_surface_GetPalette,
    ddraw_surface_GetPixelFormat,
    ddraw_surface_GetSurfaceDesc,
    ddraw_surface_Initialize,
    ddraw_surface_IsLost,
    ddraw_surface_Lock,
    ddraw_surface_ReleaseDC,
    ddraw_surface_Restore,
    ddraw_surface_SetClipper,
    ddraw_surface_SetColorKey,
    ddraw_surface_SetOverlayPosition,
    ddraw_surface_SetPalette,
    ddraw_surface_Unlock,
    ddraw_surface_UpdateOverlay,
    ddraw_surface_UpdateOverlayDisplay,
    ddraw_surface_UpdateOverlayZOrder
};

HRESULT __stdcall ddraw_CreateSurface(IDirectDrawImpl *This, LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE FAR *lpDDSurface, IUnknown FAR * unkOuter)
{
    printf("DirectDraw::CreateSurface(This=%p, lpDDSurfaceDesc=%p, lpDDSurface=%p, unkOuter=%p)\n", This, lpDDSurfaceDesc, lpDDSurface, unkOuter);

    dump_ddsd(lpDDSurfaceDesc->dwFlags);

    IDirectDrawSurfaceImpl *Surface = (IDirectDrawSurfaceImpl *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirectDrawSurfaceImpl));

    Surface->lpVtbl = &siface;

    lpDDSurfaceDesc->dwFlags |= DDSD_CAPS;

    /* private stuff */
    Surface->bpp = This->bpp;
    Surface->flags = lpDDSurfaceDesc->dwFlags;

    if(lpDDSurfaceDesc->dwFlags & DDSD_CAPS)
    {
        if(lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
        {
            ddraw->primary = Surface;

            Surface->width = This->width;
            Surface->height = This->height;
        }

        dump_ddscaps(lpDDSurfaceDesc->ddsCaps.dwCaps);
        Surface->caps = lpDDSurfaceDesc->ddsCaps.dwCaps;
    }

    if( !(lpDDSurfaceDesc->dwFlags & DDSD_CAPS) || !(lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) )
    {
        Surface->width = lpDDSurfaceDesc->dwWidth;
        Surface->height = lpDDSurfaceDesc->dwHeight;
    }

    if(Surface->width && Surface->height)
    {
        if (Surface->width == 622 && Surface->height == 51) Surface->width = 624; //AoE2
        if (Surface->width == 71 && Surface->height == 24) Surface->width = 72; //Commandos

        Surface->bmi = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(BITMAPINFOHEADER) + sizeof(RGBQUAD) * 256);
        Surface->bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        Surface->bmi->bmiHeader.biWidth = Surface->width;
        Surface->bmi->bmiHeader.biHeight = -(Surface->height + 200);
        Surface->bmi->bmiHeader.biPlanes = 1;
        Surface->bmi->bmiHeader.biBitCount = Surface->bpp;
        Surface->bmi->bmiHeader.biCompression = Surface->bpp == 16 ? BI_BITFIELDS : BI_RGB;

        WORD cClrBits = (WORD)(Surface->bmi->bmiHeader.biPlanes * Surface->bmi->bmiHeader.biBitCount);
        if (cClrBits < 24)
            Surface->bmi->bmiHeader.biClrUsed = (1 << cClrBits);

        Surface->bmi->bmiHeader.biSizeImage = ((Surface->width * cClrBits + 31) & ~31) / 8 * Surface->height;

        if (Surface->bpp == 8)
        {
            int i;
            for (i = 0; i < 256; i++)
            {
                Surface->bmi->bmiColors[i].rgbRed = i;
                Surface->bmi->bmiColors[i].rgbGreen = i;
                Surface->bmi->bmiColors[i].rgbBlue = i;
                Surface->bmi->bmiColors[i].rgbReserved = 0;
            }
        }
        else if (Surface->bpp == 16)
        {
            ((DWORD *)Surface->bmi->bmiColors)[0] = 0xF800;
            ((DWORD *)Surface->bmi->bmiColors)[1] = 0x07E0;
            ((DWORD *)Surface->bmi->bmiColors)[2] = 0x001F;
        }

        Surface->lXPitch = Surface->bpp / 8;
        Surface->lPitch = Surface->width * Surface->lXPitch;

        Surface->hDC = CreateCompatibleDC(ddraw->render.hDC);
        Surface->bitmap = CreateDIBSection(Surface->hDC, Surface->bmi, DIB_RGB_COLORS, (void **)&Surface->surface, NULL, 0);
        Surface->bmi->bmiHeader.biHeight = -Surface->height;

        if (!Surface->bitmap)
            Surface->surface = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, Surface->lPitch * (Surface->height + 200) * Surface->lXPitch);

        if (lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
            pvBmpBits = Surface->surface;

        SelectObject(Surface->hDC, Surface->bitmap);
    }

    if (lpDDSurfaceDesc->dwFlags & DDSD_BACKBUFFERCOUNT)
    {
        printf("  dwBackBufferCount=%d\n", lpDDSurfaceDesc->dwBackBufferCount);
    }

    printf(" Surface = %p (%dx%d@%d)\n", Surface, (int)Surface->width, (int)Surface->height, (int)Surface->bpp);

    *lpDDSurface = (LPDIRECTDRAWSURFACE)Surface;

    Surface->Ref = 0;
    ddraw_surface_AddRef(Surface);
    
    return DD_OK;
}

void dump_ddbltflags(DWORD dwFlags)
{
    if (dwFlags & DDBLT_ALPHADEST) {
        printf("  DDBLT_ALPHADEST\n");
    }
    if (dwFlags & DDBLT_ALPHADESTCONSTOVERRIDE) {
        printf("  DDBLT_ALPHADESTCONSTOVERRIDE\n");
    }
    if (dwFlags & DDBLT_ALPHADESTNEG) {
        printf("  DDBLT_ALPHADESTNEG\n");
    }
    if (dwFlags & DDBLT_ALPHADESTSURFACEOVERRIDE) {
        printf("  DDBLT_ALPHADESTSURFACEOVERRIDE\n");
    }
    if (dwFlags & DDBLT_ALPHAEDGEBLEND) {
        printf("  DDBLT_ALPHAEDGEBLEND\n");
    }
    if (dwFlags & DDBLT_ALPHASRC) {
        printf("  DDBLT_ALPHASRC\n");
    }
    if (dwFlags & DDBLT_ALPHASRCCONSTOVERRIDE) {
        printf("  DDBLT_ALPHASRCCONSTOVERRIDE\n");
    }
    if (dwFlags & DDBLT_ALPHASRCNEG) {
        printf("  DDBLT_ALPHASRCNEG\n");
    }
    if (dwFlags & DDBLT_ALPHASRCSURFACEOVERRIDE) {
        printf("  DDBLT_ALPHASRCSURFACEOVERRIDE\n");
    }
    if (dwFlags & DDBLT_ASYNC) {
        printf("  DDBLT_ASYNC\n");
    }
    if (dwFlags & DDBLT_COLORFILL) {
        printf("  DDBLT_COLORFILL\n");
    }
    if (dwFlags & DDBLT_DDFX) {
        printf("  DDBLT_DDFX\n");
    }
    if (dwFlags & DDBLT_DDROPS) {
        printf("  DDBLT_DDROPS\n");
    }
    if (dwFlags & DDBLT_KEYDEST) {
        printf("  DDBLT_KEYDEST\n");
    }
    if (dwFlags & DDBLT_KEYDESTOVERRIDE) {
        printf("  DDBLT_KEYDESTOVERRIDE\n");
    }
    if (dwFlags & DDBLT_KEYSRC) {
        printf("  DDBLT_KEYSRC\n");
    }
    if (dwFlags & DDBLT_KEYSRCOVERRIDE) {
        printf("  DDBLT_KEYSRCOVERRIDE\n");
    }
    if (dwFlags & DDBLT_ROP) {
        printf("  DDBLT_ROP\n");
    }
    if (dwFlags & DDBLT_ROTATIONANGLE) {
        printf("  DDBLT_ROTATIONANGLE\n");
    }
    if (dwFlags & DDBLT_ZBUFFER) {
        printf("  DDBLT_ZBUFFER\n");
    }
    if (dwFlags & DDBLT_ZBUFFERDESTCONSTOVERRIDE) {
        printf("  DDBLT_ZBUFFERDESTCONSTOVERRIDE\n");
    }
    if (dwFlags & DDBLT_ZBUFFERDESTOVERRIDE) {
        printf("  DDBLT_ZBUFFERDESTOVERRIDE\n");
    }
    if (dwFlags & DDBLT_ZBUFFERSRCCONSTOVERRIDE) {
        printf("  DDBLT_ZBUFFERSRCCONSTOVERRIDE\n");
    }
    if (dwFlags & DDBLT_ZBUFFERSRCOVERRIDE) {
        printf("  DDBLT_ZBUFFERSRCOVERRIDE\n");
    }
    if (dwFlags & DDBLT_WAIT) {
        printf("  DDBLT_WAIT\n");
    }
    if (dwFlags & DDBLT_DEPTHFILL) {
        printf("  DDBLT_DEPTHFILL\n");
    }
}

void dump_ddscaps(DWORD dwCaps)
{
    if (dwCaps & DDSCAPS_ALPHA)
    {
        printf("    DDSCAPS_ALPHA\n");
    }
    if (dwCaps & DDSCAPS_BACKBUFFER)
    {
        printf("    DDSCAPS_BACKBUFFER\n");
    }
    if (dwCaps & DDSCAPS_FLIP)
    {
        printf("    DDSCAPS_FLIP\n");
    }
    if (dwCaps & DDSCAPS_FRONTBUFFER)
    {
        printf("    DDSCAPS_FRONTBUFFER\n");
    }
    if (dwCaps & DDSCAPS_PALETTE)
    {
        printf("    DDSCAPS_PALETTE\n");
    }
    if (dwCaps & DDSCAPS_TEXTURE)
    {
        printf("    DDSCAPS_TEXTURE\n");
    }
    if(dwCaps & DDSCAPS_PRIMARYSURFACE)
    {
        printf("    DDSCAPS_PRIMARYSURFACE\n");
    }
    if(dwCaps & DDSCAPS_OFFSCREENPLAIN)
    {
        printf("    DDSCAPS_OFFSCREENPLAIN\n");
    }
    if(dwCaps & DDSCAPS_VIDEOMEMORY)
    {
        printf("    DDSCAPS_VIDEOMEMORY\n");
    }
    if(dwCaps & DDSCAPS_LOCALVIDMEM)
    {
        printf("    DDSCAPS_LOCALVIDMEM\n");
    }
}

void dump_ddsd(DWORD dwFlags)
{
    if(dwFlags & DDSD_CAPS)
    {
        printf("    DDSD_CAPS\n");
    }
    if(dwFlags & DDSD_HEIGHT)
    {
        printf("    DDSD_HEIGHT\n");
    }
    if(dwFlags & DDSD_WIDTH)
    {
        printf("    DDSD_WIDTH\n");
    }
    if(dwFlags & DDSD_PITCH)
    {
        printf("    DDSD_PITCH\n");
    }
    if(dwFlags & DDSD_BACKBUFFERCOUNT)
    {
        printf("    DDSD_BACKBUFFERCOUNT\n");
    }
    if(dwFlags & DDSD_ZBUFFERBITDEPTH)
    {
        printf("    DDSD_ZBUFFERBITDEPTH\n");
    }
    if(dwFlags & DDSD_ALPHABITDEPTH)
    {
        printf("    DDSD_ALPHABITDEPTH\n");
    }
    if(dwFlags & DDSD_LPSURFACE)
    {
        printf("    DDSD_LPSURFACE\n");
    }
    if(dwFlags & DDSD_PIXELFORMAT)
    {
        printf("    DDSD_PIXELFORMAT\n");
    }
    if(dwFlags & DDSD_CKDESTOVERLAY)
    {
        printf("    DDSD_CKDESTOVERLAY\n");
    }
    if(dwFlags & DDSD_CKDESTBLT)
    {
        printf("    DDSD_CKDESTBLT\n");
    }
    if(dwFlags & DDSD_CKSRCOVERLAY)
    {
        printf("    DDSD_CKSRCOVERLAY\n");
    }
    if(dwFlags & DDSD_CKSRCBLT)
    {
        printf("    DDSD_CKSRCBLT\n");
    }
    if(dwFlags & DDSD_MIPMAPCOUNT)
    {
        printf("    DDSD_MIPMAPCOUNT\n");
    }
    if(dwFlags & DDSD_REFRESHRATE)
    {
        printf("    DDSD_REFRESHRATE\n");
    }
    if(dwFlags & DDSD_LINEARSIZE)
    {
        printf("    DDSD_LINEARSIZE\n");
    }
    if(dwFlags & DDSD_ALL)
    {
        printf("    DDSD_ALL\n");
    }
}
