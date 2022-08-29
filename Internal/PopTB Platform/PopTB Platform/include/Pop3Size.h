#pragma once
class Pop3Size
{
public:
    unsigned int Width, Height;
    Pop3Size() = default;
    Pop3Size(unsigned int w, unsigned int h);
    void Set(unsigned int w, unsigned int h);
    Pop3Size operator=(const Pop3Size& size);
    Pop3Size operator+(const Pop3Size& a) const;
    Pop3Size operator-(const Pop3Size& a) const;
    Pop3Size operator+=(const Pop3Size& a);
    Pop3Size operator-=(const Pop3Size& a);
    Pop3Size operator*(unsigned int a) const;
    Pop3Size operator/(unsigned int a) const;
    Pop3Size operator*=(unsigned int a);
    Pop3Size operator/=(unsigned int a);
    bool operator==(const Pop3Size& a) const;
    bool operator!=(const Pop3Size& a) const;
    bool IsNull() const;
    unsigned int Area() const;
};