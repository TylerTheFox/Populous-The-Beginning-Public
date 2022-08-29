#include "Pop3Point.h"
Pop3Point::Pop3Point()
{
    X = 0;
    Y = 0;
}

Pop3Point::Pop3Point(int x, int y)
{
    Set(x, y);
}

void Pop3Point::Set(signed int x, signed int y)
{
    X = x;
    Y = y;
}

Pop3Point Pop3Point::operator+(const Pop3Point& a) const
{
    return Pop3Point(X + a.X, Y + a.Y);
}

Pop3Point Pop3Point::operator-(const Pop3Point& a) const
{
    return Pop3Point(X - a.X, Y - a.Y);
}

Pop3Point Pop3Point::operator+=(const Pop3Point& a)
{
    X += a.X;
    Y += a.Y;
    return *this;
}

Pop3Point Pop3Point::operator-=(const Pop3Point& a)
{
    X -= a.X;
    Y -= a.Y;
    return *this;
}

Pop3Point Pop3Point::operator*(int a) const
{
    return Pop3Point(X * a, Y * a);
}

Pop3Point Pop3Point::operator/(int a) const
{
    return Pop3Point(X / a, Y / a);
}

Pop3Point Pop3Point::operator*=(int a)
{
    X *= a;
    Y *= a;
    return *this;
}

Pop3Point Pop3Point::operator/=(int a)
{
    X /= a;
    Y /= a;
    return *this;
}

Pop3Point Pop3Point::operator+(const Pop3Size& a) const
{
    return Pop3Point(X + a.Width, Y + a.Height);
}

Pop3Point Pop3Point::operator-(const Pop3Size& a) const
{
    return Pop3Point(X - a.Width, Y - a.Height);
}

Pop3Point Pop3Point::operator-=(const Pop3Size & a)
{
    X -= a.Width;
    Y -= a.Height;
    return *this;
}

Pop3Point Pop3Point::operator-() const
{
    return Pop3Point(-X, -Y);
}

bool Pop3Point::operator==(const Pop3Point& a) const
{
    return ((X == a.X) && (Y == a.Y));
}

bool Pop3Point::operator!=(const Pop3Point& a) const
{
    return ((X != a.X) || (Y != a.Y));
}

Pop3Point Pop3Point::operator+=(const Pop3Size& a)
{
    X += a.Width;
    Y += a.Height;
    return *this;
}
