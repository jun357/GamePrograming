#include "Math.h"

#include <cmath>
#include <cstdlib>

Vec2 Vec2::operator+(const Vec2& o) const
{
    return { x + o.x, y + o.y };
}

Vec2 Vec2::operator-(const Vec2& o) const
{
    return { x - o.x, y - o.y };
}

Vec2 Vec2::operator*(float s) const
{
    return { x * s, y * s };
}

float Length(const Vec2& v)
{
    return sqrtf(v.x * v.x + v.y * v.y);
}

Vec2 Normalize(const Vec2& v)
{
    float l = Length(v);

    if (l <= 0.0001f)
        return { 0,0 };

    return { v.x / l, v.y / l };
}

float Dot(const Vec2& a, const Vec2& b)
{
    return a.x * b.x + a.y * b.y;
}

float RandomFloat()
{
    return (float)rand() / (float)RAND_MAX;
}