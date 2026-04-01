//***************************************************************************
// LbPal.h : Palette manipulation and conversion
//***************************************************************************
#ifndef _LBPAL_H
#define _LBPAL_H

#define _LB_PALETTE_SIZE	256


#include <LbError.h>
#include    "Pop3Types.h"


//***************************************************************************
//	<s>>TbPaletteEntry<<
//***************************************************************************



class TbPaletteEntry
{
    LB_PUBLIC_MEMBERS
    union
    {
        struct
        {
            UBYTE Red;
            UBYTE Green;
            UBYTE Blue;
            UBYTE Reserved;
        };
        ULONG	Packed;
    };

    BOOL operator==(const TbPaletteEntry &rhs);

};



inline BOOL TbPaletteEntry::operator==(const TbPaletteEntry &rhs)
{
    return ((Packed & 0xffffff) == (rhs.Packed & 0xffffff));
};

inline TbPaletteEntry LB_PALETTE_RGB( UBYTE r, UBYTE g, UBYTE b)
{
    TbPaletteEntry entry;

    entry.Packed =(		(((ULONG)((r)&0xFF))<<0) 	|
                        (((ULONG)((g)&0xFF))<<8)	|
                        (((ULONG)((b)&0xFF))<<16)	);

    return entry;
}


//***************************************************************************
//	<s>>TbPalette<<
//***************************************************************************
typedef struct TbPalette
{
    TbPaletteEntry Entry[256];
} TbPalette;

//***************************************************************************
// LbPalette_FromCompact : Convert a compact palette to a TbPalette
//	void LbPalette_FromCompact(TbPalette *palette, const UBYTE *pPalBytes);
//***************************************************************************
void LbPalette_FromCompact(TbPalette *palette, const UBYTE *pPalBytes, UINT num);
void LbPalette_ToCompact(UBYTE *pTriples, const TbPalette *palette, UINT num);

//***************************************************************************
// LbPalette_FindColour : Locate the closest colour match in the palette supplied.
//	UBYTE LbPalette_FindColour(const TbPalette *palette, UBYTE r, UBYTE g, UBYTE b);
//***************************************************************************
UBYTE LbPalette_FindColour(const TbPalette *palette, UBYTE r, UBYTE g, UBYTE b);

//***************************************************************************
// LbPalette_GenerateGhostTable :
//***************************************************************************
TbError LbPalette_GenerateGhostTable(UBYTE **ppTable, const TbPalette *palette, UINT percent, const TBCHAR *lpFilename);

//***************************************************************************
// LbPalette_Fade: Start/Perform a fade.
//***************************************************************************
typedef enum TbPaletteFadeType
{
    LB_PALETTE_FADE_OPEN,
    LB_PALETTE_FADE_CLOSED
} TbPaletteFadeFlag;



#endif // ifndef _LBPAL_H
//***************************************************************************
//				END OF FILE
//***************************************************************************
