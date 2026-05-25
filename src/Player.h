#pragma once

#include <vector>
#include <SDL2/SDL.h>

#include "Wall.h"

enum MoveMode
{
    SNEAK,
    WALK,
    RUN,
    INJURED
};

float GetMoveSpeed(MoveMode mode);

MoveMode GetMoveMode(const Uint8* keystate, float injuredTimer);

void MovePlayer(
    SDL_Rect& player,
    float dx,
    float dy,
    const std::vector<Wall>& walls);

bool MovePlayerWithCollisionResult(
    SDL_Rect& player,
    float dx,
    float dy,
    const std::vector<Wall>& walls);
