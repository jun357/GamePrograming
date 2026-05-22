#pragma once

#include <vector>

#include <SDL2/SDL.h>

#include "Math.h"
#include "Enemy.h"
#include "Wall.h"

struct SoundParticle
{
    Vec2 pos = { 0.0f, 0.0f };
    Vec2 vel = { 0.0f, 0.0f };

    // 실제 소음 발생 지점
    Vec2 source = { 0.0f, 0.0f };

    float radius = 2.0f;

    float mass = 1.0f;

    float loudness = 1.0f;

    float life = 1.4f;

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
    float strongestEnergy = 0.0f;
    Vec2 noisePos = { 0.0f, 0.0f };
    bool heard = false;
};

void EmitSound(
    std::vector<SoundParticle>& particles,
    Vec2 origin,
    int count,
    float speed,
    float loudness = 1.0f,
    float life = 1.4f);

void PrepareSoundWalls(std::vector<Wall>& walls);

void UpdateSoundParticles(
    const std::vector<SoundParticle>& read,
    std::vector<SoundParticle>& write,
    const std::vector<EnemyAudioSnapshot>& enemies,
    std::vector<HearingResult>& hearingBuffer,
    const std::vector<Wall>& walls,
    float dt);

void CleanUpParticles(std::vector<SoundParticle>& read, std::vector<SoundParticle>& write);
