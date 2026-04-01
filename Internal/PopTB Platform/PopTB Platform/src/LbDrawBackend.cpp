// LbDrawBackend.cpp
// Registers all Library8 drawing/screen functions into Pop3Draw at startup.
// Call LbDraw_RegisterBackend() once before LbScreen_Initialise().
#include "Pop3Draw.h"
#include <LbDraw.h>
#include <LbScreen.h>
#include <LbSurface.h>
#include <LbSprite.h>
#include <LbText.h>
#include <LbPal.h>

// Thin wrappers where the Pop3DrawBackend signature doesn't exactly match
// the Lb* signature (e.g. different int vs TbError return, or const overloads).

static void _wrap_Rectangle(const Pop3Rect* r, TbColour c)        { LbDraw_Rectangle(r, c); }
static void _wrap_RectangleOutline(const Pop3Rect* r, TbColour c) { LbDraw_RectangleOutline(r, c); }
static void _wrap_RectangleFilled(const Pop3Rect* r, TbColour c)  { LbDraw_RectangleFilled(r, c); }

static void _wrap_SetCurrent_Surface(TbSurface* s)     { LbDraw_SetCurrent(s); }
static void _wrap_SetCurrent_RenderArea(Pop3RenderArea* r){ LbDraw_SetCurrent(r); }

static int  _wrap_Surface_Create(unsigned int w, unsigned int h, unsigned long f, TbSurface* s)
    { return (int)LbSurface_Create(w, h, f, s); }
static int  _wrap_Surface_Delete(TbSurface* s)
    { return (int)LbSurface_Delete(s); }
static int  _wrap_Surface_Lock(TbSurface* s, unsigned char** p)
    { return (int)LbSurface_Lock(s, p); }
static int  _wrap_Surface_ClearRect(const TbSurface* s, const Pop3Rect* r, TbColour c, unsigned long f)
    { return (int)LbSurface_ClearRect(s, r, c, f); }

static int  _wrap_Screen_Initialise(struct _GUID* g, int mx, void* v, void* a)
    { return (int)LbScreen_Initialise(g, (BOOL)mx, v, a); }
static int  _wrap_Screen_SetMode(unsigned int w, unsigned int h, unsigned int d, unsigned long f, const TbPalette* p)
    { return (int)LbScreen_SetMode(w, h, d, f, p); }
static int  _wrap_Screen_Swap(unsigned long f)
    { return (int)LbScreen_Swap(f); }
static int  _wrap_Screen_PartSwap(const Pop3Point& dest, const Pop3Rect& src, unsigned long f)
    { Pop3Point pt; pt.X = dest.X; pt.Y = dest.Y; return (int)LbScreen_PartSwap(pt, src, f); }
static int  _wrap_Screen_IsModeAvailable(unsigned int w, unsigned int h, unsigned int d)
    { return (int)LbScreen_IsModeAvailable(w, h, d); }
static int  _wrap_Screen_FadePalette(TbPalette* to, unsigned int steps, int type)
    { return (int)LbScreen_FadePalette(to, steps, (TbPaletteFadeType)type); }
static TbPaletteEntry* _wrap_Screen_GetPaletteEntries(TbPaletteEntry* out, unsigned int first, unsigned int count)
    { return LbScreen_GetPaletteEntries(out, first, count); }

static int  _wrap_SetTextFont(const TbTextRender* r, void* i)
    { return (int)LbDraw_SetTextFont(r, i); }

void LbDraw_RegisterBackend()
{
    Pop3DrawBackend b = {};

    // Primitives
    b.Pixel             = LbDraw_Pixel;
    b.HorizontalLine    = LbDraw_HorizontalLine;
    b.VerticalLine      = LbDraw_VerticalLine;
    b.Line              = LbDraw_Line;
    b.Rectangle         = _wrap_Rectangle;
    b.RectangleOutline  = _wrap_RectangleOutline;
    b.RectangleFilled   = _wrap_RectangleFilled;
    b.Circle            = LbDraw_Circle;
    b.CircleOutline     = LbDraw_CircleOutline;
    b.CircleFilled      = LbDraw_CircleFilled;
    b.Triangle          = LbDraw_Triangle;
    b.ClearClipRect     = LbDraw_ClearClipRect;

    // Viewport / clip
    b.SetViewPort       = LbDraw_SetViewPort;
    b.ReleaseViewPort   = LbDraw_ReleaseViewPort;
    b.SetClipRect       = LbDraw_SetClipRect;
    b.ReleaseClipRect   = LbDraw_ReleaseClipRect;
    b.GetViewPort       = LbDraw_GetViewPort;
    b.GetClipRect       = LbDraw_GetClipRect;

    // Draw state
    b.SetCurrent_Surface    = _wrap_SetCurrent_Surface;
    b.SetCurrent_RenderArea = _wrap_SetCurrent_RenderArea;
    b.SetFlags              = LbDraw_SetFlags;
    b.SetFlagsOn            = LbDraw_SetFlagsOn;
    b.SetFlagsOff           = LbDraw_SetFlagsOff;
    b.SetGhostTable         = LbDraw_SetGhostTable;
    b.SetFadeTable          = LbDraw_SetFadeTable;
    b.SetFadeStep           = LbDraw_SetFadeStep;

    // Text
    b.PropText          = LbDraw_PropText;
    b.UnicodePropText   = LbDraw_UnicodePropText;
    b.CheapText         = LbDraw_CheapText;
    b.Text              = LbDraw_Text;
    b.SetTextFont       = _wrap_SetTextFont;

    // Sprites
    b.Sprite                = LbDraw_Sprite;
    b.OneColourSprite       = LbDraw_OneColourSprite;
    b.RemappedSprite        = LbDraw_RemappedSprite;
    b.ScaledSprite          = LbDraw_ScaledSprite;
    b.SetSpriteScalingData  = LbDraw_SetSpriteScalingData;

    // Surface
    b.Surface_Create        = _wrap_Surface_Create;
    b.Surface_Delete        = _wrap_Surface_Delete;
    b.Surface_Lock          = _wrap_Surface_Lock;
    b.Surface_Unlock        = LbSurface_Unlock;
    b.Surface_ClearRect     = _wrap_Surface_ClearRect;
    b.Surface_SetGhostTable = LbSurface_SetGhostTable;
    b.Surface_SetFadeTable  = LbSurface_SetFadeTable;
    b.Surface_SetDrawFlags  = LbSurface_SetDrawFlags;

    // Screen
    b.Screen_Initialise         = _wrap_Screen_Initialise;
    b.Screen_Terminate          = LbScreen_Terminate;
    b.Screen_SetMode            = _wrap_Screen_SetMode;
    b.Screen_Swap               = _wrap_Screen_Swap;
    b.Screen_SwapClear          = LbScreen_SwapClear;
    b.Screen_PartSwap           = _wrap_Screen_PartSwap;
    b.Screen_IsModeAvailable    = _wrap_Screen_IsModeAvailable;
    b.Screen_WaitVbi            = LbScreen_WaitVbi;
    b.Screen_GetFrontSurface    = LbScreen_GetFrontSurface;
    b.Screen_GetBackSurface     = LbScreen_GetBackSurface;

    // Palette
    b.Screen_SetPaletteEntries      = LbScreen_SetPaletteEntries;
    b.Screen_GetPaletteEntries      = _wrap_Screen_GetPaletteEntries;
    b.Screen_FadePalette            = _wrap_Screen_FadePalette;
    b.Screen_StopOpenFadePalette    = LbScreen_StopOpenFadePalette;

    Pop3Draw::setBackend(b);
}
