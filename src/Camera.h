#pragma once

#include <SDL2/SDL.h>

#include "Globals.h"
#include "Math.h"

struct Camera2D
{
    float x = 0.0f;
    float y = 0.0f;

    int screenW = SCREEN_WIDTH;
    int screenH = SCREEN_HEIGHT;

    float zoom = 1.0f;

    void FollowImmediate(float targetX, float targetY)
    {
        x = targetX - (screenW * 0.5f) / zoom;
        y = targetY - (screenH * 0.5f) / zoom;
    }

    void FollowSmooth(float targetX, float targetY, float smoothing, float dt)
    {
        float desiredX = targetX - (screenW * 0.5f) / zoom;
        float desiredY = targetY - (screenH * 0.5f) / zoom;

        x += (desiredX - x) * smoothing * dt;
        y += (desiredY - y) * smoothing * dt;
    }

    void ClampToBounds(float worldW, float worldH)
    {
        float viewW = screenW / zoom;
        float viewH = screenH / zoom;

        if (worldW <= viewW)
        {
            x = (worldW - viewW) * 0.5f;
        }
        else
        {
            if (x < 0.0f) x = 0.0f;
            if (x > worldW - viewW) x = worldW - viewW;
        }

        if (worldH <= viewH)
        {
            y = (worldH - viewH) * 0.5f;
        }
        else
        {
            if (y < 0.0f) y = 0.0f;
            if (y > worldH - viewH) y = worldH - viewH;
        }
    }

    SDL_Rect WorldToScreenRect(const SDL_Rect& r) const
    {
        return {
            (int)((r.x - x) * zoom),
            (int)((r.y - y) * zoom),
            (int)(r.w * zoom),
            (int)(r.h * zoom)
        };
    }

    SDL_Point WorldToScreenPoint(Vec2 p) const
    {
        return {
            (int)((p.x - x) * zoom),
            (int)((p.y - y) * zoom)
        };
    }

    Vec2 ScreenToWorldPoint(int sx, int sy) const
    {
        return {
            sx / zoom + x,
            sy / zoom + y
        };
    }
};
