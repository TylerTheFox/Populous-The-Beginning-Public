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
#include <ctype.h>
#include <time.h>
#include "ddraw.h"
#include "palette.h"
#include "surface.h"
#include "lodepng.h"

BOOL screenshot(struct IDirectDrawSurfaceImpl *src)
{
    if (!src || !src->palette || !src->surface)
        return FALSE;

    int i;
    char title[128];
    char filename[128];
    char str_time[64];
    time_t t = time(NULL);

    strncpy(title, ddraw->title, sizeof(ddraw->title));

    for (i = 0; i<strlen(title); i++) {
        if (title[i] == ' ')
        {
            title[i] = '_';
        }
        else
        {
            title[i] = tolower(title[i]);
        }
    }

    strftime(str_time, 64, "%Y-%m-%d-%H_%M_%S", localtime(&t));
    _snprintf(filename, 128, "%s-%s.png", title, str_time);

    unsigned char* png;
    size_t pngsize;
    LodePNGState state;

    lodepng_state_init(&state);
    
    for (i = 0; i < 256; i++)
    {
        RGBQUAD *c = &src->palette->data_rgb[i];
        lodepng_palette_add(&state.info_png.color, c->rgbRed, c->rgbGreen, c->rgbBlue, 255);
        lodepng_palette_add(&state.info_raw, c->rgbRed, c->rgbGreen, c->rgbBlue, 255);
    }

    state.info_png.color.colortype = LCT_PALETTE;
    state.info_png.color.bitdepth = 8;
    state.info_raw.colortype = LCT_PALETTE;
    state.info_raw.bitdepth = 8;
    state.encoder.auto_convert = 0;

    unsigned int error = lodepng_encode(&png, &pngsize, src->surface, src->width, src->height, &state);
    if (!error) lodepng_save_file(png, pngsize, filename);

    lodepng_state_cleanup(&state);
    free(png);

    return !error;
}
