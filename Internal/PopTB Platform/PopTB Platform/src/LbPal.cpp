//***************************************************************************
// LbPal.cpp : Palette utilities
//***************************************************************************
#include <lbPal.h>
#include <lbtypes.h>
#include <LbException.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>

//***************************************************************************
// LbPalette_FromCompact : Expand packed RGB triples into TbPalette
//***************************************************************************
void LbPalette_FromCompact(TbPalette *palette, const UBYTE *pPalBytes, UINT num)
{
    for (UINT i = 0; i < num; i++)
    {
        palette->Entry[i].Red      = pPalBytes[3*i + 0];
        palette->Entry[i].Green    = pPalBytes[3*i + 1];
        palette->Entry[i].Blue     = pPalBytes[3*i + 2];
        palette->Entry[i].Reserved = 0;
    }
}

//***************************************************************************
// LbPalette_ToCompact : Pack TbPalette into RGB triples
//***************************************************************************
void LbPalette_ToCompact(UBYTE *pTriples, const TbPalette *palette, UINT num)
{
    for (UINT i = 0; i < num; i++)
    {
        pTriples[3*i + 0] = palette->Entry[i].Red;
        pTriples[3*i + 1] = palette->Entry[i].Green;
        pTriples[3*i + 2] = palette->Entry[i].Blue;
    }
}

//***************************************************************************
// LbPalette_FindColour : Closest palette match (3-phase: Euclidean, Manhattan, brightness)
//***************************************************************************
UBYTE LbPalette_FindColour(const TbPalette *palette, UBYTE r, UBYTE g, UBYTE b)
{
    // Phase 1: minimum squared Euclidean distance
    int bestDist = 0x1000000;
    for (int i = 0; i < 256; i++)
    {
        int dr = (int)r - (int)palette->Entry[i].Red;
        int dg = (int)g - (int)palette->Entry[i].Green;
        int db = (int)b - (int)palette->Entry[i].Blue;
        int dist = dr*dr + dg*dg + db*db;
        if (dist < bestDist)
        {
            bestDist = dist;
            if (!dist)
                return (UBYTE)i;
        }
    }

    // Phase 2: collect all indices with bestDist
    UBYTE candidates1[256];
    UINT count1 = 0;
    for (int i = 0; i < 256; i++)
    {
        int dr = (int)r - (int)palette->Entry[i].Red;
        int dg = (int)g - (int)palette->Entry[i].Green;
        int db = (int)b - (int)palette->Entry[i].Blue;
        if (dr*dr + dg*dg + db*db == bestDist)
            candidates1[count1++] = (UBYTE)i;
    }
    if (count1 == 1)
        return candidates1[0];

    // Phase 3: minimum Manhattan distance among candidates
    int bestMan = 0x1000000;
    for (UINT i = 0; i < count1; i++)
    {
        UINT idx = candidates1[i];
        int dist = abs((int)r - (int)palette->Entry[idx].Red)
                 + abs((int)g - (int)palette->Entry[idx].Green)
                 + abs((int)b - (int)palette->Entry[idx].Blue);
        if (dist < bestMan)
            bestMan = dist;
    }
    UBYTE candidates2[256];
    UINT count2 = 0;
    for (UINT i = 0; i < count1; i++)
    {
        UINT idx = candidates1[i];
        int dist = abs((int)r - (int)palette->Entry[idx].Red)
                 + abs((int)g - (int)palette->Entry[idx].Green)
                 + abs((int)b - (int)palette->Entry[idx].Blue);
        if (dist == bestMan)
            candidates2[count2++] = candidates1[i];
    }
    if (count2 == 1)
        return candidates2[0];

    // Phase 4: minimum weighted brightness (B^2 + 2*G^2 + 2*R^2) among remaining
    int bestBright = 0x1000000;
    UBYTE result = candidates2[0];
    for (UINT i = 0; i < count2; i++)
    {
        UINT idx = candidates2[i];
        int B = (int)palette->Entry[idx].Blue;
        int G = (int)palette->Entry[idx].Green;
        int R = (int)palette->Entry[idx].Red;
        int bright = B*B + 2*G*G + 2*R*R;
        if (bright < bestBright)
        {
            bestBright = bright;
            result = candidates2[i];
        }
    }
    return result;
}

//***************************************************************************
// LbPalette_GenerateGhostTable : Build 256x256 colour-blend lookup table
//   table[256*src + dst] = closest palette index to blend(src, dst, percent%)
//   Loads from lpFilename if present; saves to lpFilename after generation.
//***************************************************************************
TbError LbPalette_GenerateGhostTable(UBYTE **ppTable, const TbPalette *palette, UINT percent, const TBCHAR *lpFilename)
{
    // Try to load cached table from file
    if (lpFilename && lpFilename[0])
    {
        FILE *f = fopen(lpFilename, "rb");
        if (f)
        {
            UBYTE *table = (UBYTE*)malloc(256 * 256);
            if (!table) { fclose(f); return LB_ERROR; }
            if (fread(table, 1, 256 * 256, f) == 256 * 256)
            {
                fclose(f);
                *ppTable = table;
                return LB_OK;
            }
            free(table);
            fclose(f);
        }
    }

    UBYTE *table = (UBYTE*)malloc(256 * 256);
    if (!table)
        return LB_ERROR;

    for (int src = 0; src < 256; src++)
    {
        for (int dst = 0; dst < 256; dst++)
        {
            int r = ((int)palette->Entry[src].Red   * (int)percent + (int)palette->Entry[dst].Red   * (int)(100 - percent)) / 100;
            int g = ((int)palette->Entry[src].Green * (int)percent + (int)palette->Entry[dst].Green * (int)(100 - percent)) / 100;
            int b = ((int)palette->Entry[src].Blue  * (int)percent + (int)palette->Entry[dst].Blue  * (int)(100 - percent)) / 100;
            table[256 * src + dst] = LbPalette_FindColour(palette, (UBYTE)r, (UBYTE)g, (UBYTE)b);
        }
    }

    // Save to cache file if provided
    if (lpFilename && lpFilename[0])
    {
        FILE *f = fopen(lpFilename, "wb");
        if (f)
        {
            fwrite(table, 1, 256 * 256, f);
            fclose(f);
        }
    }

    *ppTable = table;
    return LB_OK;
}
