#pragma once
#include <Pop3Point.h>

class Pop3Rect
{
public:
    signed int Left, Top, Right, Bottom;

    Pop3Rect() : Left(0), Top(0), Right(0), Bottom(0) {}

    Pop3Rect(signed int l, signed int t, signed int r, signed int b)
        : Left(l), Top(t), Right(r), Bottom(b) {}

    Pop3Rect(const Pop3Point& point, const Pop3Size& size)
        : Left(point.X), Top(point.Y),
          Right(point.X + size.Width), Bottom(point.Y + size.Height) {}

    Pop3Rect(const Pop3Size& size)
        : Left(0), Top(0), Right(size.Width), Bottom(size.Height) {}

    Pop3Rect(const Pop3Point& topLeft, const Pop3Point& botRight)
        : Left(topLeft.X), Top(topLeft.Y), Right(botRight.X), Bottom(botRight.Y) {}

    void Set(signed int l, signed int t, signed int r, signed int b);
    void Set(const Pop3Point& topleft, const Pop3Size& size);
    void Set(const Pop3Point& topleft, const Pop3Point& botright);

    // Assignment from Pop3Point/Pop3Size
    Pop3Rect operator=(const Pop3Point& rhs);
    Pop3Rect operator=(const Pop3Size& rhs);

    // Comparison
    bool operator==(const Pop3Rect& a) const;
    bool operator!=(const Pop3Rect& a) const;

    // Decomposition
    Pop3Point GetPosition() const;
    Pop3Size GetSize() const;
    Pop3Point GetBottomRight() const;
    Pop3Point GetTopRight() const;
    Pop3Point GetBottomLeft() const;

    // Sizing
    void Grow(signed int dx, signed int dy);
    void Shrink(signed int dx, signed int dy);

    // Set operations
    Pop3Rect Intersection(const Pop3Rect& a) const;
    bool Intersects(const Pop3Rect& a) const;
    Pop3Rect Bounding(const Pop3Rect& a) const;

    // Containment
    bool Contains(const Pop3Point& a) const;
    bool Contains(const Pop3Rect& a) const;

    // Offsetting
    Pop3Rect operator+(const Pop3Point& a) const;
    Pop3Rect operator-(const Pop3Point& a) const;
    Pop3Rect operator+=(const Pop3Point& a);
    Pop3Rect operator-=(const Pop3Point& a);

    void Empty();
    void Offset(Pop3Point& a);
    void SetPosition(const Pop3Point& a);
    void SetPosition(signed int x, signed int y);
    void SetBottomRight(const Pop3Point& a);
    void SetBottomRight(signed int x, signed int y);
    void SetSize(const Pop3Size& a);

    bool IsNull() const;
    bool IsEmpty() const;
    bool IsNormal() const;

    signed int Width() const;
    signed int Height() const;

    void Normalise();
};
