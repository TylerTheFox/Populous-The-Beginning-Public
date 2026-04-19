#include "Pop3Rect.h"

void Pop3Rect::Set(signed int l, signed int t, signed int r, signed int b)
{
    Left = l;
    Top = t;
    Right = r;
    Bottom = b;
}

void Pop3Rect::Set(const Pop3Point& topleft, const Pop3Size& size)
{
    SetPosition(topleft);
    SetSize(size);
}

void Pop3Rect::Set(const Pop3Point& topleft, const Pop3Point& botright)
{
    SetPosition(topleft);
    SetBottomRight(botright);
}

Pop3Rect Pop3Rect::operator=(const Pop3Point& rhs)
{
    Left = rhs.X;
    Right = rhs.X;
    Top = rhs.Y;
    Bottom = rhs.Y;
    return *this;
}

Pop3Rect Pop3Rect::operator=(const Pop3Size& rhs)
{
    Left = 0;
    Right = rhs.Width;
    Top = 0;
    Bottom = rhs.Height;
    return *this;
}

bool Pop3Rect::operator==(const Pop3Rect& a) const
{
    return (Left == a.Left) && (Right == a.Right) && (Top == a.Top) && (Bottom == a.Bottom);
}

bool Pop3Rect::operator!=(const Pop3Rect& a) const
{
    return !(*this == a);
}

Pop3Point Pop3Rect::GetPosition() const
{
    return Pop3Point(Left, Top);
}

Pop3Size Pop3Rect::GetSize() const
{
    return Pop3Size((unsigned int)(Right - Left), (unsigned int)(Bottom - Top));
}

Pop3Point Pop3Rect::GetBottomRight() const
{
    return Pop3Point(Right, Bottom);
}

Pop3Point Pop3Rect::GetTopRight() const
{
    return Pop3Point(Right, Top);
}

Pop3Point Pop3Rect::GetBottomLeft() const
{
    return Pop3Point(Left, Bottom);
}

void Pop3Rect::Grow(signed int dx, signed int dy)
{
    Left -= dx;
    Right += dx;
    Top -= dy;
    Bottom += dy;
}

void Pop3Rect::Shrink(signed int dx, signed int dy)
{
    Left += dx;
    Right -= dx;
    Top += dy;
    Bottom -= dy;
}

Pop3Rect Pop3Rect::Intersection(const Pop3Rect& a) const
{
    Pop3Rect result;
    result.Left = (a.Left > Left) ? a.Left : Left;
    result.Top = (a.Top > Top) ? a.Top : Top;
    result.Right = (a.Right < Right) ? a.Right : Right;
    result.Bottom = (a.Bottom < Bottom) ? a.Bottom : Bottom;

    if (result.Right < result.Left)
        result.Right = result.Left;
    if (result.Top > result.Bottom)
        result.Bottom = result.Top;

    return result;
}

bool Pop3Rect::Intersects(const Pop3Rect& a) const
{
    return (a.Right >= Left && a.Left <= Right &&
            Top <= a.Bottom && a.Top <= Bottom);
}

Pop3Rect Pop3Rect::Bounding(const Pop3Rect& a) const
{
    Pop3Rect result;
    result.Left = (Left < a.Left) ? Left : a.Left;
    result.Top = (Top < a.Top) ? Top : a.Top;
    result.Right = (Right > a.Right) ? Right : a.Right;
    result.Bottom = (Bottom > a.Bottom) ? Bottom : a.Bottom;
    return result;
}

bool Pop3Rect::Contains(const Pop3Point& a) const
{
    return (a.X >= Left) && (a.X < Right) && (a.Y >= Top) && (a.Y < Bottom);
}

bool Pop3Rect::Contains(const Pop3Rect& a) const
{
    return (Left <= a.Left) && (Top <= a.Top) && (Right >= a.Right) && (Bottom >= a.Bottom);
}

Pop3Rect Pop3Rect::operator+(const Pop3Point& a) const
{
    return Pop3Rect(Left + a.X, Top + a.Y, Right + a.X, Bottom + a.Y);
}

Pop3Rect Pop3Rect::operator-(const Pop3Point& a) const
{
    return Pop3Rect(Left - a.X, Top - a.Y, Right - a.X, Bottom - a.Y);
}

Pop3Rect Pop3Rect::operator+=(const Pop3Point& a)
{
    Left += a.X;
    Top += a.Y;
    Right += a.X;
    Bottom += a.Y;
    return *this;
}

Pop3Rect Pop3Rect::operator-=(const Pop3Point& a)
{
    Left -= a.X;
    Top -= a.Y;
    Right -= a.X;
    Bottom -= a.Y;
    return *this;
}

void Pop3Rect::Empty()
{
    Right = Left;
    Bottom = Top;
}

void Pop3Rect::Offset(Pop3Point& a)
{
    Left += a.X;
    Top += a.Y;
    Right += a.X;
    Bottom += a.Y;
}

void Pop3Rect::SetPosition(const Pop3Point& a)
{
    signed int dx = a.X - Left;
    signed int dy = a.Y - Top;
    Left = a.X;
    Top = a.Y;
    Right += dx;
    Bottom += dy;
}

void Pop3Rect::SetPosition(signed int x, signed int y)
{
    signed int dx = x - Left;
    signed int dy = y - Top;
    Left = x;
    Top = y;
    Right += dx;
    Bottom += dy;
}

void Pop3Rect::SetBottomRight(const Pop3Point& a)
{
    Right = a.X;
    Bottom = a.Y;
}

void Pop3Rect::SetBottomRight(signed int x, signed int y)
{
    Right = x;
    Bottom = y;
}

void Pop3Rect::SetSize(const Pop3Size& a)
{
    Right = Left + a.Width;
    Bottom = Top + a.Height;
}

bool Pop3Rect::IsNull() const
{
    return (Left == Right) || (Top == Bottom);
}

bool Pop3Rect::IsEmpty() const
{
    return (Left == Right) || (Top == Bottom);
}

bool Pop3Rect::IsNormal() const
{
    return (Right >= Left) && (Top <= Bottom);
}

signed int Pop3Rect::Width() const
{
    return Right - Left;
}

signed int Pop3Rect::Height() const
{
    return Bottom - Top;
}

void Pop3Rect::Normalise()
{
    if (Right < Left)
    {
        Left ^= Right;
        Right ^= Left;
        Left ^= Right;
    }
    if (Top > Bottom)
    {
        Top ^= Bottom;
        Bottom ^= Top;
        Top ^= Bottom;
    }
}
