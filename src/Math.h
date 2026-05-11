#pragma once

struct Vec2
{
    float x, y;

    Vec2 operator+(const Vec2& o) const;
    Vec2 operator-(const Vec2& o) const;
    Vec2 operator*(float s) const;
};

float Length(const Vec2& v);

Vec2 Normalize(const Vec2& v);

float Dot(const Vec2& a, const Vec2& b);

float RandomFloat();