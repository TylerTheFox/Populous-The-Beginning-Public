#pragma once
#include "Pop3Size.h"

class Pop3Point
{
public:
    signed int X, Y;

    Pop3Point();
    Pop3Point(signed int x, signed int y);
    void Set(signed int x, signed int y);

    Pop3Point operator+(const Pop3Point& a) const;
    Pop3Point operator-(const Pop3Point& a) const;
    Pop3Point operator+=(const Pop3Point& a);
    Pop3Point operator-=(const Pop3Point& a);
    Pop3Point operator*(signed int a) const;
    Pop3Point operator/(signed int a) const;
    Pop3Point operator*=(signed int a);
    Pop3Point operator/=(signed int a);
    Pop3Point operator+(const Pop3Size& a) const;
    Pop3Point operator-(const Pop3Size& a) const;
    Pop3Point operator+=(const Pop3Size& a);
    Pop3Point operator-=(const Pop3Size& a);
    Pop3Point operator-() const;
    bool operator==(const Pop3Point& a) const;
    bool operator!=(const Pop3Point& a) const;
};
