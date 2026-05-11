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

void MovePlayer(
    SDL_Rect& player,
    float dx,
    float dy,
    const std::vector<Wall>& walls)
{
    SDL_Rect next = player;

    next.x += (int)dx;

    for (auto& w : walls)
    {
        if (SDL_HasIntersection(&next, &w.rect))
        {
            dx = 0;
            break;
        }
    }

    player.x += (int)dx;

    next = player;

    next.y += (int)dy;

    for (auto& w : walls)
    {
        if (SDL_HasIntersection(&next, &w.rect))
        {
            dy = 0;
            break;
        }
    }

    player.y += (int)dy;
}