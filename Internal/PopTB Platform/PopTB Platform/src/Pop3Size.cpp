#include "../include/Pop3Size.h"

Pop3Size::Pop3Size(unsigned w, unsigned h)
{
    Set(w, h);
}

void Pop3Size::Set(unsigned int w, unsigned int h)
{
    Width = w;
    Height = h;
}

Pop3Size Pop3Size::operator=(const Pop3Size& size)
{
    Width = size.Width;
    Height = size.Height;
    return *this;
}

Pop3Size Pop3Size::operator+(const Pop3Size& a) const
{
    return Pop3Size(Width + a.Width, Height + a.Height);
}

Pop3Size Pop3Size::operator-(const Pop3Size& a) const
{
    return Pop3Size(Width - a.Width, Height - a.Height);
}

Pop3Size Pop3Size::operator+=(const Pop3Size& a)
{
    Width += a.Width;
    Height += a.Height;
    return *this;
}

Pop3Size Pop3Size::operator-=(const Pop3Size& a)
{
    Width -= a.Width;
    Height -= a.Height;
    return *this;
}

Pop3Size Pop3Size::operator*(unsigned a) const
{
    return Pop3Size(Width * a, Height * a);
}

Pop3Size Pop3Size::operator/(unsigned a) const
{
    return Pop3Size(Width / a, Height / a);
}

Pop3Size Pop3Size::operator*=(unsigned a)
{
    Width *= a;
    Height *= a;
    return *this;
}

Pop3Size Pop3Size::operator/=(unsigned a)
{
    Width /= a;
    Height /= a;
    return *this;
}

bool Pop3Size::operator==(const Pop3Size& a) const
{
    return ((Width == a.Width) && (Height == a.Height));
}

bool Pop3Size::operator!=(const Pop3Size& a) const
{
    return ((Width != a.Width) || (Height != a.Height));
}

bool Pop3Size::IsNull() const
{
    return ((Width == 0) || (Height == 0));
}

unsigned Pop3Size::Area() const
{
    return Width * Height;
}
