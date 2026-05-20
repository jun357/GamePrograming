#pragma once

#include <SDL2/SDL.h>
#include <vector>

struct WallCell
{
    bool solid;

    float restitution = 1.0f;

    float roughness = 0.0f;
};

struct Wall
{
    SDL_Rect rect;

    int cellSize = 8;

    int gridWidth;
    int gridHeight;

    std::vector<WallCell> cells;

    bool generated = false;

    Wall(SDL_Rect r, int cs = 8)
    {
        rect = r;
        cellSize = cs;

        gridWidth =
            (rect.w + cellSize - 1) / cellSize;

        gridHeight =
            (rect.h + cellSize - 1) / cellSize;
    }
};
inline WallCell& GetCell(
    Wall& w,
    int x,
    int y)
{
    return w.cells[y * w.gridWidth + x];
}

inline const WallCell& GetCell(
    const Wall& w,
    int x,
    int y)
{
    return w.cells[y * w.gridWidth + x];
}
