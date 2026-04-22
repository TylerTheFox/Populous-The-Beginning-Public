#pragma once
#include    "Pop3Types.h"
#include    <intrin.h>
#include    <emmintrin.h>
#include    <stdlib.h>

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
// Formerly used inline __asm { bswap eax }. _byteswap_ulong is a MSVC
// intrinsic that emits the same bswap instruction on x86/x64 and lets the
// compiler optimize across the call site (inline __asm is an optimization
// barrier).
inline long LB_SWAP_LONG_BYTES(long l)
{
    return (long)_byteswap_ulong((unsigned long)l);
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
// Formerly:  bsr ecx, value; inc ecx; mov eax, 1; shl eax, cl
// Semantics: return 1 << (index_of_highest_set_bit(value) + 1) — the next
// power-of-two strictly greater than value. Uses _BitScanReverse, which
// emits the same bsr instruction. Note: _BitScanReverse's result is
// undefined when value == 0 (same as bsr); callers have always been
// responsible for that case.
inline unsigned long LB_HIGHER_POWER2(unsigned long value)
{
    unsigned long index;
    _BitScanReverse(&index, value);
    return 1UL << (index + 1);
}


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
// Formerly:  fld ff; fistp dword ptr _i
// Semantics: rounds to int using the FPU's current rounding mode (default:
// round-to-nearest-even). A plain `(int)f` truncates and would produce
// different results for non-integer inputs — so we use _mm_cvtss_si32,
// which rounds via MXCSR (also defaulting to round-to-nearest-even),
// matching the original rounding behavior.
#define LB_FTOI(_i,_f)  do {                                         \
                            (_i) = _mm_cvtss_si32(_mm_set_ss((_f))); \
                        } while (0)


//***************************************************************************
// LB_ROTL :  Rotate a number left by a certain amount
//***************************************************************************
// _rotl is a MSVC intrinsic that lowers to the single rol instruction.
inline ULONG LB_ROTL(ULONG value, ULONG amount)
{
    return (ULONG)_rotl((unsigned int)value, (int)amount);
}


//***************************************************************************
// LB_ROTR :  Rotate a number right by a certain amount
//***************************************************************************
// _rotr is a MSVC intrinsic that lowers to the single ror instruction.
inline ULONG LB_ROTR(ULONG value, ULONG amount)
{
    return (ULONG)_rotr((unsigned int)value, (int)amount);
}
