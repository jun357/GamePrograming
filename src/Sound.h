#pragma once

#include <vector>

#include <SDL2/SDL.h>

#include "Math.h"
#include "Enemy.h"
#include "Wall.h"

enum class SoundKind
{
    Generic,
    Footstep,
    Bottle,
    Gunshot
};

struct SoundParticle
{
    Vec2 pos = { 0.0f, 0.0f };
    Vec2 vel = { 0.0f, 0.0f };

    // 실제 소음 발생 지점
    Vec2 source = { 0.0f, 0.0f };
    // 같은 EmitSound()에서 나온 파티클들을 하나의 소음 이벤트로 묶기 위한 값
    int eventId = 0;
    SoundKind kind = SoundKind::Generic;
    // 소음 이벤트가 생성된 뒤 지난 시간
    float age = 0.0f;

    float radius = 2.0f;

    float mass = 1.0f;

    float loudness = 1.0f;

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

    int eventId = 0;
    SoundKind kind = SoundKind::Generic;

    bool heard = false;
};

void EmitSound(
    std::vector<SoundParticle>& particles,
    Vec2 origin,
    int count,
    float speed,
    float loudness = 1.0f,
    SoundKind kind = SoundKind::Generic);

void EmitSoundDirectional(
    std::vector<SoundParticle>& particles,
    Vec2 origin,
    Vec2 direction,
    float spreadRad,
    int count,
    float speed,
    float loudness = 1.0f,
    SoundKind kind = SoundKind::Generic);

void PrepareSoundWalls(std::vector<Wall>& walls);

void UpdateSoundParticles(
    const std::vector<SoundParticle>& read,
    std::vector<SoundParticle>& write,
    const std::vector<EnemyAudioSnapshot>& enemies,
    std::vector<HearingResult>& hearingBuffer,
    const std::vector<Wall>& walls,
    float dt);

void CleanUpParticles(std::vector<SoundParticle>& read, std::vector<SoundParticle>& write);
