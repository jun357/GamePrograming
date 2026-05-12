#pragma once

#include <SDL2/SDL.h>

#include <vector>

#include "Math.h"

#include "Wall.h"

enum class EnemyKind
{
    PatrolGuard,
    Sentry,
    Officer
};

enum class EnemyState
{
    Patrol,
    Investigate,
    Search,
    Return,
    Alert,
    Dead
};

struct Enemy
{
    SDL_Rect rect;

    float angle;

    float fov;
    float viewDist;

    float rotateSpeed;

    // =====================================================
    // AI 기본 정보
    // =====================================================
    EnemyKind kind = EnemyKind::Sentry;
    EnemyState state = EnemyState::Patrol;

    Vec2 pos = { 0.0f, 0.0f };
    Vec2 homePos = { 0.0f, 0.0f };
    float homeAngle = 0.0f;
    bool initialized = false;

    // =====================================================
    // 순찰 정보
    // =====================================================
    std::vector<Vec2> patrolPoints;
    int patrolIndex = 0;
    float moveSpeed = 80.0f;

    // =====================================================
    // 조사 및 수색 정보
    // =====================================================
    Vec2 investigateTarget = { 0.0f, 0.0f };
    Vec2 lastKnownPlayerPos = { 0.0f, 0.0f };
    float stateTimer = 0.0f;
    float searchTimer = 0.0f;
    float searchDuration = 2.0f;

    // =====================================================
    // 소음 (임시)
    // =====================================================
    float hearingEnergy = 0.0f;
    float hearingThreshold = 4.0f;
    Vec2 lastNoisePos = { 0.0f, 0.0f };

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

void RequestEnemyInvestigate(
    Enemy& enemy,
    Vec2 targetPos);

void NotifyEnemyOfNoise(
    Enemy& enemy,
    Vec2 noisePos,
    float energy);

const char* GetEnemyStateName(EnemyState state);
