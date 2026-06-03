#pragma once

#include <SDL2/SDL.h>
#include <vector>

#include "Wall.h"

struct StageMapSetup
{
    SDL_Rect playerStart = { 100, 100, 32, 32 };

    std::vector<Wall> baseWalls;
    std::vector<Wall> anomalyWalls;

    SDL_Rect goalNormal = { 700, 500, 40, 40 };
    SDL_Rect goalAnomaly = { 700, 100, 40, 40 };
};

StageMapSetup MakeTutorialMovementStageMap();

StageMapSetup MakePrototypeMainStageMap();
