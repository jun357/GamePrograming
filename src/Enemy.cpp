#include "Enemy.h"

#include <cmath>

// =====================================================
// 선분 교차
// =====================================================

static bool LineIntersect(
    float x1, float y1,
    float x2, float y2,
    float x3, float y3,
    float x4, float y4)
{
    float denom =
        (y4 - y3) * (x2 - x1) -
        (x4 - x3) * (y2 - y1);

    if (denom == 0.0f)
        return false;

    float ua =
        ((x4 - x3) * (y1 - y3) -
        (y4 - y3) * (x1 - x3)) / denom;

    float ub =
        ((x2 - x1) * (y1 - y3) -
        (y2 - y1) * (x1 - x3)) / denom;

    return
        ua >= 0 && ua <= 1 &&
        ub >= 0 && ub <= 1;
}

// =====================================================
// 선 vs 사각형
// =====================================================

static bool LineIntersectsRect(
    float x1, float y1,
    float x2, float y2,
    SDL_Rect rect)
{
    float rx = rect.x;
    float ry = rect.y;
    float rw = rect.w;
    float rh = rect.h;

    if (LineIntersect(
        x1, y1, x2, y2,
        rx, ry, rx + rw, ry))
        return true;

    if (LineIntersect(
        x1, y1, x2, y2,
        rx, ry, rx, ry + rh))
        return true;

    if (LineIntersect(
        x1, y1, x2, y2,
        rx + rw, ry,
        rx + rw, ry + rh))
        return true;

    if (LineIntersect(
        x1, y1, x2, y2,
        rx, ry + rh,
        rx + rw, ry + rh))
        return true;

    return false;
}

// =====================================================
// 적 업데이트
// =====================================================

void UpdateEnemies(
    std::vector<Enemy>& enemies,
    SDL_Rect& player,
    std::vector<Wall>& walls,
    bool& playerDetected,
    float dt)
{
    for (auto& enemy : enemies)
    {
        enemy.angle +=
            enemy.rotateSpeed * dt * 60.0f;

        enemy.hearingEnergy *= 0.92f;

        enemy.alerted =
            enemy.hearingEnergy > 4.0f;

        float ex =
            enemy.rect.x +
            enemy.rect.w * 0.5f;

        float ey =
            enemy.rect.y +
            enemy.rect.h * 0.5f;

        float px =
            player.x +
            player.w * 0.5f;

        float py =
            player.y +
            player.h * 0.5f;

        float dirX = cos(enemy.angle);
        float dirY = sin(enemy.angle);

        float toPX = px - ex;
        float toPY = py - ey;

        float distSq =
            toPX * toPX +
            toPY * toPY;

        if (distSq <
            enemy.viewDist *
            enemy.viewDist)
        {
            if (distSq < 0.0001f)
                continue;

            float dist = sqrt(distSq);

            float normX = toPX / dist;
            float normY = toPY / dist;

            float dot =
                dirX * normX +
                dirY * normY;

            float fovLimit =
                cos(enemy.fov * 0.5f);

            if (dot > fovLimit)
            {
                bool blocked = false;

                for (auto& w : walls)
                {
                    if (LineIntersectsRect(
                        ex, ey,
                        px, py,
                        w.rect))
                    {
                        blocked = true;
                        break;
                    }
                }

                if (!blocked)
                {
                    playerDetected = true;
                }
            }
        }
    }
}

// =====================================================
// FOV 렌더
// =====================================================

void DrawFOV(
    SDL_Renderer* renderer,
    Enemy& enemy,
    std::vector<Wall>& walls)
{
    int rays = 120;

    float start =
        enemy.angle -
        enemy.fov * 0.5f;

    float step =
        enemy.fov / rays;

    SDL_Point center =
    {
        enemy.rect.x +
        enemy.rect.w / 2,

        enemy.rect.y +
        enemy.rect.h / 2
    };

    SDL_SetRenderDrawColor(
        renderer,
        255, 255, 0, 80);

    for (int i = 0; i < rays; i++)
    {
        float a =
            start + step * i;

        float dx = cos(a);
        float dy = sin(a);

        float rayLength =
            enemy.viewDist;

        for (auto& w : walls)
        {
            for (float t = 0;
                t < enemy.viewDist;
                t += 2.0f)
            {
                float rx =
                    center.x + dx * t;

                float ry =
                    center.y + dy * t;

                SDL_Rect rect = w.rect;

                if (rx >= rect.x &&
                    rx <= rect.x + rect.w &&
                    ry >= rect.y &&
                    ry <= rect.y + rect.h)
                {
                    rayLength = t;
                    break;
                }
            }
        }

        SDL_Point end =
        {
            (int)(center.x + dx * rayLength),
            (int)(center.y + dy * rayLength)
        };

        SDL_RenderDrawLine(
            renderer,
            center.x,
            center.y,
            end.x,
            end.y);
    }
}