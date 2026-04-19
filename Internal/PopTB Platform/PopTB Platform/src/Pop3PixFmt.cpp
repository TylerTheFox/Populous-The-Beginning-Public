#include "Pop3PixFmt.h"

//                                           R          G       B         A       Depth  Palette
Pop3PixFormat Pop3PixFormat8  (          0,        0,      0,        0,     8,    1);
Pop3PixFormat Pop3PixFormat16 (     0x7C00,    0x3E0,   0x1F,        0,    16,    0);
Pop3PixFormat Pop3PixFormat24 ( 0x00FF0000,   0xFF00,   0xFF,        0,    24,    0);
Pop3PixFormat Pop3PixFormat32 ( 0x00FF0000,   0xFF00,   0xFF, 0xFF000000,  32,    0);
