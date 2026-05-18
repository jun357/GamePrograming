#pragma once

#include <vector>

#include <SDL2/SDL.h>

#include "Math.h"
#include "Enemy.h"
#include "Wall.h"

struct SoundParticle
{
    Vec2 pos;
    Vec2 vel;

    float radius = 2.0f;

    float mass = 1.0f;

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