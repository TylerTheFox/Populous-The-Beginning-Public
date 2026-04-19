#pragma once
// TbPixFormat is now just an alias for Pop3PixFormat.
// This header exists for backward compatibility.
#include <Pop3PixFmt.h>
typedef Pop3PixFormat TbPixFormat;
#define TbPixFormat8  Pop3PixFormat8
#define TbPixFormat16 Pop3PixFormat16
#define TbPixFormat24 Pop3PixFormat24
#define TbPixFormat32 Pop3PixFormat32
