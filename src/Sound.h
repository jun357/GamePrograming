#pragma once

#include <vector>

#include <SDL2/SDL.h>

#include "Math.h"
#include "Enemy.h"
#include "Wall.h"

struct SoundParticle
{
    Vec2 pos;
    Vec2 dir;

    float energy;

    int bounces = 0;

    bool alive = true;
};

void EmitSound(
    std::vector<SoundParticle>& particles,
    Vec2 origin,
    int count,
    float energy);

void UpdateSoundParticles(
    std::vector<SoundParticle>& particles,
    std::vector<Enemy>& enemies,
    std::vector<Wall>& walls,
    float dt);