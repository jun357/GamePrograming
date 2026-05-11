#pragma once

#include <SDL2/SDL.h>

struct Wall
{
    SDL_Rect rect;

    float absorption;
    float reflection;
    float transmission;
};