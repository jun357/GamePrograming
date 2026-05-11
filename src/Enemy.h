#pragma once

#include <SDL2/SDL.h>

#include <vector>

#include "Wall.h"

struct Enemy
{
    SDL_Rect rect;

    float angle;

    float fov;
    float viewDist;

    float rotateSpeed;

    float hearingEnergy = 0.0f;

    bool alerted = false;
};

void UpdateEnemies(
    std::vector<Enemy>& enemies,
    SDL_Rect& player,
    std::vector<Wall>& walls,
    bool& playerDetected,
    float dt);

void DrawFOV(
    SDL_Renderer* renderer,
    Enemy& enemy,
    std::vector<Wall>& walls);