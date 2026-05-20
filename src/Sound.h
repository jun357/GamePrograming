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

struct EnemyAudioSnapshot
{
    SDL_Rect rect;
    bool alive = true;
};

struct HearingResult
{
    float energy = 0.0f;
    Vec2 noisePos = {0.0f, 0.0f};
    bool heard = false;
};

void EmitSound(
    std::vector<SoundParticle>& particles,
    Vec2 origin,
    int count,
    float energy);

void PrepareSoundWalls(std::vector<Wall>& walls);

void UpdateSoundParticles(
    const std::vector<SoundParticle>& read,
    std::vector<SoundParticle>& write,
    const std::vector<EnemyAudioSnapshot>& enemies,
    std::vector<HearingResult>& hearingBuffer,
    const std::vector<Wall>& walls,
    float dt);

void CleanUpParticles(std::vector<SoundParticle>& read, std::vector<SoundParticle>& write);
