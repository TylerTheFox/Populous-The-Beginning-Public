#pragma once
#include    "Pop3Types.h"

//***************************************************************************
// Utility macros and functions
//***************************************************************************
// Changed for template versions 25/5/98 AMM
#define LB_MIN(a, b) 	((a) < (b) ? (a) : (b))
#define LB_MAX(a, b)	((a) > (b) ? (a) : (b))

/*template <class T>
inline const T& LB_MIN( const T& a, const T& b )
{
return (a < b ? a : b);
};

template <class T>
inline const T& LB_MAX( const T& a, const T& b )
{
return (a > b ? a : b);
};*/

//***************************************************************************
//	LB_SWAP_LONG_BYTES
//***************************************************************************
inline long LB_SWAP_LONG_BYTES(long l)
{
    __asm
    {
        mov eax, l
        bswap eax
        mov l, eax
    }
    return l;
}

//***************************************************************************
//	LB_SWAP_WORD_BYTES
//***************************************************************************
#define LB_SWAP_WORD_BYTES(w) ((((w)&(0xff))<<8)|(((UWORD)w)>>8))

//***************************************************************************
//	LB_SWAP
//***************************************************************************
#define	LB_SWAP(a,b)	((a)^=(b),(b)^=(a),(a)^=(b))

//***************************************************************************
//	LB_HIGHER_POWER2
//***************************************************************************
#if defined(__MSC__) && defined(__cplusplus)
inline unsigned long LB_HIGHER_POWER2(unsigned long value)
{
    unsigned int return_value;
    __asm
    {
        mov eax, value
        bsr ecx, eax
        inc ecx
        mov eax, 1
        shl eax, cl
        mov return_value, eax
    }
    return return_value;
}
#else
#error LB_HIGHER_POWER2 not defined in non MSC compiler
#endif


//***************************************************************************
// Maximum Filename Length : Always use this when buffers hold a filename
//***************************************************************************
#ifdef _LB_WINDOWS
const UINT LB_MAX_PATH = 260; // Same as MAX_PATH
#else
const UINT LB_MAX_PATH = _MAX_PATH;
#endif


//***************************************************************************
// LB_FTOI :  Uses the fistp instruction to convert a float to an int
//***************************************************************************
#define LB_FTOI(_i,_f)	{								\
							float ff = (_f);			\
							__asm fld	ff				\
							__asm fistp	dword ptr _i	\
						}


//***************************************************************************
// LB_ROTL :  Rotate a number left by a certain amount
//***************************************************************************
inline ULONG LB_ROTL(ULONG value, ULONG amount)
{
    __asm
    {
        mov cl, byte ptr amount
        rol value, cl
    }
    return value;
}


//***************************************************************************
// LB_ROTR :  Rotate a number right by a certain amount
//***************************************************************************
inline ULONG LB_ROTR(ULONG value, ULONG amount)
{
    __asm
    {
        mov cl, byte ptr amount
        ror value, cl
    }
    return value;
}
