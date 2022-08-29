#pragma once
#include "Pop3Types.h"

class Pop3Math
{
public:
    Pop3Math() = delete;
    static SLONG                    Mul16(SLONG a, SLONG b);
    static SLONG                    Div16(SLONG a, SLONG b);
    static UWORD                    ArcTan(SLONG dx, SLONG dy);
    static signed long              ROR13(signed long val);
    static double                   sqrt(double i);

    static SLONG                    SinTable[];
    static UWORD                    atantable[];
};
#define	    LB_QUADRANT_SIZE	   	512		// Number of entries in 45 degrees (PI/2)
#define		LbMaths_Sin(a)	        Pop3Math::SinTable[a]
#define		LbMaths_Cos(a)	        Pop3Math::SinTable[(a)+LB_QUADRANT_SIZE]