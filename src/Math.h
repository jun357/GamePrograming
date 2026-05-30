#pragma once

struct Vec2
{
    float x, y;

    Vec2 operator+(const Vec2& o) const;
    Vec2 operator-(const Vec2& o) const;
    Vec2 operator*(float s) const;
    Vec2 operator/(float s) const;
    Vec2& operator+=(const Vec2& o);
    Vec2& operator-=(const Vec2& o);
    Vec2& operator*=(float s);
    float LengthSq() const;
};

float Length(const Vec2& v);

Vec2 Normalize(const Vec2& v);

float Dot(const Vec2& a, const Vec2& b);

float RandomFloat();

// 추가
Vec2 Reflect(
    const Vec2& v,
    const Vec2& n);

float LengthSq(const Vec2& v);
float DistanceSq(const Vec2& a, const Vec2& b);
float Distance(const Vec2& a, const Vec2& b);

float Cross(const Vec2& a, const Vec2& b);
float ClampFloat(float value, float lo, float hi);

Vec2 AngleToDir(float angle);
float DirToAngle(Vec2 dir);
float WrapAngle(float angle);
float LerpAngle(float from, float to, float t);
