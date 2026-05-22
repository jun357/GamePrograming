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

Vec2 Vec2::operator/(float s) const
{
    return
    {
        x / s,
        y / s
    };
}

// 추가된 복합 대입 연산자들
Vec2& Vec2::operator+=(const Vec2& o)
{
    x += o.x;
    y += o.y;
    return *this;
}

Vec2& Vec2::operator-=(const Vec2& o)
{
    x -= o.x;
    y -= o.y;
    return *this;
}

Vec2& Vec2::operator*=(float s)
{
    x *= s;
    y *= s;
    return *this;
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

Vec2 Reflect(
    const Vec2& v,
    const Vec2& n)
{
    return
        v - n * (2.0f * Dot(v, n));
}

float LengthSq(const Vec2& v)
{
    return v.x * v.x + v.y * v.y;
}

float DistanceSq(const Vec2& a, const Vec2& b)
{
    return LengthSq(b - a);
}

float Distance(const Vec2& a, const Vec2& b)
{
    return sqrtf(DistanceSq(a, b));
}

float Cross(const Vec2& a, const Vec2& b)
{
    return a.x * b.y - a.y * b.x;
}

float ClampFloat(float value, float lo, float hi)
{
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

Vec2 AngleToDir(float angle)
{
    return { cosf(angle), sinf(angle) };
}

float DirToAngle(Vec2 dir)
{
    return atan2f(dir.y, dir.x);
}

float WrapAngle(float angle)
{
    constexpr float PI = 3.14159265358979323846f;
    constexpr float TWO_PI = PI * 2.0f;

    angle = fmodf(angle + PI, TWO_PI);
    if (angle < 0.0f)
    {
        angle += TWO_PI;
    }

    return angle - PI;
}

float LerpAngle(float from, float to, float t)
{
    float diff = atan2f(sinf(to - from), cosf(to - from));
    return WrapAngle(from + diff * t);
}
