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
#include "palette.h"
#include "surface.h"

IDirectDrawPaletteImpl *LastFreedPalette; // Dungeon Keeper hack

HRESULT __stdcall ddraw_palette_GetEntries(IDirectDrawPaletteImpl *This, DWORD dwFlags, DWORD dwBase, DWORD dwNumEntries, LPPALETTEENTRY lpEntries)
{
    int i, x;

    printf("DirectDrawPalette::GetEntries(This=%p, dwFlags=%08X, dwBase=%d, dwNumEntries=%d, lpEntries=%p)\n", This, (int)dwFlags, (int)dwBase, (int)dwNumEntries, lpEntries);

    for (i = dwBase, x = 0; i < dwBase + dwNumEntries; i++, x++)
    {
        if (This->data_rgb)
        {
            lpEntries[x].peRed = This->data_rgb[i].rgbRed;
            lpEntries[x].peGreen = This->data_rgb[i].rgbGreen;
            lpEntries[x].peBlue = This->data_rgb[i].rgbBlue;
        }
    }

    return DD_OK;
}

HRESULT __stdcall ddraw_palette_SetEntries(IDirectDrawPaletteImpl *This, DWORD dwFlags, DWORD dwStartingEntry, DWORD dwCount, LPPALETTEENTRY lpEntries)
{
    int i, x;

#if _DEBUG_X
    printf("DirectDrawPalette::SetEntries(This=%p, dwFlags=%08X, dwStartingEntry=%d, dwCount=%d, lpEntries=%p)\n", This, (int)dwFlags, (int)dwStartingEntry, (int)dwCount, lpEntries);
#endif

    for (i = dwStartingEntry, x = 0; i < dwStartingEntry + dwCount; i++, x++)
    {
        This->data_bgr[i] = (lpEntries[x].peBlue << 16) | (lpEntries[x].peGreen << 8) | lpEntries[x].peRed;

        if (This->data_rgb)
        {
            This->data_rgb[i].rgbRed = lpEntries[x].peRed;
            This->data_rgb[i].rgbGreen = lpEntries[x].peGreen;
            This->data_rgb[i].rgbBlue = lpEntries[x].peBlue;
            This->data_rgb[i].rgbReserved = 0;
        }
    }

    if (ddraw->primary && ddraw->primary->palette == This && ddraw->render.run)
    {
        InterlockedExchange(&ddraw->render.paletteUpdated, TRUE);
        ReleaseSemaphore(ddraw->render.sem, 1, NULL);
    }

    return DD_OK;
}

HRESULT __stdcall ddraw_palette_QueryInterface(IDirectDrawPaletteImpl *This, REFIID riid, void **obj)
{
    printf("??? DirectDrawPalette::QueryInterface(This=%p, riid=%08X, obj=%p)\n", This, (unsigned int)riid, obj);
    return S_OK;
}

ULONG __stdcall ddraw_palette_AddRef(IDirectDrawPaletteImpl *This)
{
    printf("DirectDrawPalette::AddRef(This=%p)\n", This);

    This->Ref++;

    return This->Ref;
}

ULONG __stdcall ddraw_palette_Release(IDirectDrawPaletteImpl *This)
{
    printf("DirectDrawPalette::Release(This=%p)\n", This);

    This->Ref--;

    if(This->Ref == 0)
    {
        printf("    Released (%p)\n", This);

        LastFreedPalette = This;
        HeapFree(GetProcessHeap(), 0, This);
        return 0;
    }

    return This->Ref;
}

HRESULT __stdcall ddraw_palette_GetCaps(IDirectDrawPaletteImpl *This, LPDWORD caps)
{
    printf("??? DirectDrawPalette::GetCaps(This=%p, caps=%p)\n", This, caps);
    return DD_OK;
}

HRESULT __stdcall ddraw_palette_Initialize(IDirectDrawPaletteImpl *This, LPDIRECTDRAW lpDD, DWORD dw, LPPALETTEENTRY paent)
{
    printf("??? DirectDrawPalette::Initialize(This=%p, ...)\n", This);
    return DD_OK;
}

struct IDirectDrawPaletteImplVtbl piface =
{
    /* IUnknown */
    ddraw_palette_QueryInterface,
    ddraw_palette_AddRef,
    ddraw_palette_Release,
    /* IDirectDrawPalette */
    ddraw_palette_GetCaps,
    ddraw_palette_GetEntries,
    ddraw_palette_Initialize,
    ddraw_palette_SetEntries
};

HRESULT __stdcall ddraw_CreatePalette(IDirectDrawImpl *This, DWORD dwFlags, LPPALETTEENTRY lpDDColorArray, LPDIRECTDRAWPALETTE FAR * lpDDPalette, IUnknown FAR * unkOuter)
{
    printf("DirectDraw::CreatePalette(This=%p, dwFlags=%08X, DDColorArray=%p, DDPalette=%p, unkOuter=%p)\n", This, (int)dwFlags, lpDDColorArray, lpDDPalette, unkOuter);

    IDirectDrawPaletteImpl *Palette = (IDirectDrawPaletteImpl *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(IDirectDrawPaletteImpl));
    Palette->lpVtbl = &piface;
    printf(" Palette = %p\n", Palette);
    *lpDDPalette = (LPDIRECTDRAWPALETTE)Palette;

    ddraw_palette_SetEntries(Palette, dwFlags, 0, 256, lpDDColorArray);

    ddraw_palette_AddRef(Palette);

    return DD_OK;
}

