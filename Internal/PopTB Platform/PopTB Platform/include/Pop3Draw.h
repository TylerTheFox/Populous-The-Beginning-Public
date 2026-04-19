#pragma once
#include "Pop3Types.h"
#include "Pop3Size.h"
#include "Pop3Point.h"

// Forward declarations so this header stays self-contained
struct TbSurface;
class  Pop3RenderArea;
class  Pop3Rect;
class  TbColour;
struct TbSprite;
struct TbPalette;
class  TbPaletteEntry;
struct TbTextRender;
struct _GUID;

// ============================================================
//  Pop3DrawBackend
//
//  A flat table of function pointers covering every drawing,
//  surface and screen service that Library8 provides.
//
//  The game never calls Library8 directly — it always goes
//  through Pop3Draw::backend(). Library8 (or any future
//  renderer) registers itself once at startup via
//  Pop3Draw::setBackend().
// ============================================================

struct Pop3DrawBackend
{
    // ----------------------------------------------------------
    // Primitives  (LbDraw_*)
    // ----------------------------------------------------------
    void    (*Pixel)            (int x, int y, TbColour col);
    void    (*HorizontalLine)   (int x, int y, int length, TbColour col);
    void    (*VerticalLine)     (int x, int y, int height, TbColour col);
    void    (*Line)             (int x1, int y1, int x2, int y2, TbColour col);
    void    (*Rectangle)        (const Pop3Rect* rect, TbColour col);
    void    (*RectangleOutline) (const Pop3Rect* rect, TbColour col);
    void    (*RectangleFilled)  (const Pop3Rect* rect, TbColour col);
    void    (*Circle)           (int x, int y, unsigned int r, TbColour col);
    void    (*CircleOutline)    (int x, int y, unsigned int r, TbColour col);
    void    (*CircleFilled)     (int x, int y, unsigned int r, TbColour col);
    void    (*Triangle)         (int x1, int y1, int x2, int y2, int x3, int y3, TbColour col);
    void    (*ClearClipRect)    (TbColour col);

    // ----------------------------------------------------------
    // Viewport / clip
    // ----------------------------------------------------------
    void    (*SetViewPort)      (const Pop3Rect* vp);
    void    (*ReleaseViewPort)  ();
    void    (*SetClipRect)      (const Pop3Rect* clip);
    void    (*ReleaseClipRect)  ();
    Pop3Rect* (*GetViewPort)      ();
    Pop3Rect* (*GetClipRect)      ();

    // ----------------------------------------------------------
    // Draw state
    // ----------------------------------------------------------
    void    (*SetCurrent_Surface)    (TbSurface* surface);
    void    (*SetCurrent_RenderArea) (Pop3RenderArea* ra);
    void    (*SetFlags)              (unsigned long flags);
    void    (*SetFlagsOn)            (unsigned long flags);
    void    (*SetFlagsOff)           (unsigned long flags);
    void    (*SetGhostTable)         (unsigned char* table);
    void    (*SetFadeTable)          (unsigned char* table);
    void    (*SetFadeStep)           (unsigned long step);

    // ----------------------------------------------------------
    // Text
    // ----------------------------------------------------------
    void    (*PropText)         (int x, int y, const char* text, TbColour col);
    void    (*UnicodePropText)  (int x, int y, const unsigned short* text, TbColour col);
    void    (*CheapText)        (int x, int y, const char* text, TbColour col);
    void    (*Text)             (int x, int y, const char* text, TbColour col);
    int     (*SetTextFont)      (const TbTextRender* render, void* init);

    // ----------------------------------------------------------
    // Sprites
    // ----------------------------------------------------------
    void    (*Sprite)           (int x, int y, const TbSprite* spr);
    void    (*OneColourSprite)  (int x, int y, const TbSprite* spr, TbColour col);
    void    (*RemappedSprite)   (int x, int y, const TbSprite* spr, const unsigned char* remap);
    void    (*ScaledSprite)     (int x, int y, const TbSprite* spr, unsigned int w, unsigned int h);
    void    (*SetSpriteScalingData)(int x, int y, unsigned int sw, unsigned int sh, unsigned int dw, unsigned int dh);

    // ----------------------------------------------------------
    // Surface management
    // ----------------------------------------------------------
    int     (*Surface_Create)   (unsigned int w, unsigned int h, unsigned long flags, TbSurface* out);
    int     (*Surface_Delete)   (TbSurface* surf);
    int     (*Surface_Lock)     (TbSurface* surf, unsigned char** ppMem);
    void    (*Surface_Unlock)   (TbSurface* surf);
    int     (*Surface_ClearRect)(const TbSurface* surf, const Pop3Rect* rect, TbColour col, unsigned long flags);
    void    (*Surface_SetGhostTable)(TbSurface* surf, unsigned char* table);
    void    (*Surface_SetFadeTable) (TbSurface* surf, unsigned char* table);
    void    (*Surface_SetDrawFlags)  (TbSurface* surf, unsigned long flags);

    // ----------------------------------------------------------
    // Screen / mode
    // ----------------------------------------------------------
    int     (*Screen_Initialise)        (struct _GUID* guid, int modex, void* hwndView, void* hwndApp);
    void    (*Screen_Terminate)         ();
    int     (*Screen_SetMode)           (unsigned int w, unsigned int h, unsigned int depth, unsigned long flags, const TbPalette* pal);
    int     (*Screen_Swap)              (unsigned long flags);
    void    (*Screen_SwapClear)         (TbColour col);
    int     (*Screen_PartSwap)          (const Pop3Point& dest, const Pop3Rect& src, unsigned long flags);
    int     (*Screen_IsModeAvailable)   (unsigned int w, unsigned int h, unsigned int depth);
    void    (*Screen_WaitVbi)           ();
    TbSurface* (*Screen_GetFrontSurface)();
    TbSurface* (*Screen_GetBackSurface) ();

    // ----------------------------------------------------------
    // Palette
    // ----------------------------------------------------------
    void    (*Screen_SetPaletteEntries) (const TbPaletteEntry* entries, unsigned int first, unsigned int count);
    TbPaletteEntry* (*Screen_GetPaletteEntries)(TbPaletteEntry* out, unsigned int first, unsigned int count);
    int     (*Screen_FadePalette)       (TbPalette* to, unsigned int steps, int fadeType);
    void    (*Screen_StopOpenFadePalette)();
};

// ============================================================
//  Pop3Draw
//
//  Thin static wrapper. Call Pop3Draw::setBackend() once at
//  startup (Library8 does this in its init path). Then use
//  Pop3Draw::backend() to call any drawing function.
//
//  Example:
//      Pop3Draw::backend().Pixel(x, y, col);
//      Pop3Draw::backend().Screen_Swap(0);
// ============================================================
class Pop3Draw
{
public:
    Pop3Draw() = delete;

    static void setBackend(const Pop3DrawBackend& b) { _backend = b; _registered = true; }
    static bool isRegistered()                        { return _registered; }

    static Pop3DrawBackend& backend()                 { return _backend; }

private:
    static Pop3DrawBackend  _backend;
    static bool             _registered;
};
