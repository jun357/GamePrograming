#include "Player.h"

#include <vector>

float GetMoveSpeed(MoveMode mode)
{
    switch (mode)
    {
    case SNEAK: return 90.0f;
    case WALK: return 180.0f;
    case RUN: return 300.0f;
    }

    return 180.0f;
}

MoveMode GetMoveMode(const Uint8* keystate)
{
    if (keystate[SDL_SCANCODE_LCTRL] ||
        keystate[SDL_SCANCODE_RCTRL])
    {
        return SNEAK;
    }

    if (keystate[SDL_SCANCODE_LSHIFT] ||
        keystate[SDL_SCANCODE_RSHIFT])
    {
        return RUN;
    }

    return WALK;
}

static bool RectIntersectsAnyWall(
    const SDL_Rect& rect,
    const std::vector<Wall>& walls)
{
    for (const auto& wall : walls)
    {
        if (SDL_HasIntersection(&rect, &wall.rect))
        {
            return true;
        }
    }

    return false;
}

bool MovePlayerWithCollisionResult(
    SDL_Rect& player,
    float dx,
    float dy,
    const std::vector<Wall>& walls)
{
    bool collided = false;

    SDL_Rect next = player;
    next.x += static_cast<int>(dx);

    if (!RectIntersectsAnyWall(next, walls))
    {
        player.x = next.x;
    }
    else
    {
        collided = true;
    }

    next = player;
    next.y += static_cast<int>(dy);

    if (!RectIntersectsAnyWall(next, walls))
    {
        player.y = next.y;
    }
    else
    {
        collided = true;
    }

    return collided;
}

void MovePlayer(
    SDL_Rect& player,
    float dx,
    float dy,
    const std::vector<Wall>& walls)
{
    (void)MovePlayerWithCollisionResult(player, dx, dy, walls);
}
