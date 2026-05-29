#pragma once

#include <SDL2/SDL.h>

#include <vector>

#include "Math.h"

#include "Wall.h"

struct Camera2D;

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
    bool useHeadSweep = false;
    float headSweepOffset = 0.0f;
    float headSweepMin = -0.785398f;
    float headSweepMax = 0.785398f;
    float headSweepSpeed = 0.6f;
    int headSweepDirection = 1;

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

    // 소음 지점까지 직선 이동이 벽에 막힐 때 사용할 임시 우회 경로.
    // 최종 소음 지점도 배열의 마지막 point로 들어간다.
    // 동적 vector를 쓰지 않고 고정 배열을 사용해 enemyThread 성능을 유지한다.
    Vec2 investigatePath[5] = {};
    int investigatePathCount = 0;
    int investigatePathIndex = 0;
    bool needsInvestigatePathBuild = false;
    float investigateRouteTimeout = 0.0f;

    Vec2 lastKnownPlayerPos = { 0.0f, 0.0f };
    Vec2 returnTarget = { 0.0f, 0.0f };
    Vec2 resumePatrolPos = { 0.0f, 0.0f };
    int resumePatrolIndex = 0;
    float resumeAngle = 0.0f;
    bool hasResumePoint = false;
    float stateTimer = 0.0f;
    float searchTimer = 0.0f;
    float searchDuration = 2.0f;
    float searchBaseAngle = 0.0f;
    float investigateTimeout = 4.0f;
    float returnTimeout = 5.0f;
    float stuckTimer = 0.0f;

    // =====================================================
    // 소음
    // =====================================================
    float hearingEnergy = 0.0f;
    float hearingThreshold = 4.0f;
    Vec2 lastNoisePos = { 0.0f, 0.0f };
    bool hasPendingNoise = false;
    Vec2 pendingNoisePos = { 0.0f, 0.0f };
    float pendingNoiseEnergy = 0.0f;

    bool alerted = false;

    // =====================================================
    // 체력
    // =====================================================
    int maxHP = 100;
    int hp = 100;

    // =====================================================
    // 아이템 드롭
    // =====================================================
    bool officerRewardGiven = false;

    // =====================================================
    // 공격 정보
    // =====================================================
    float attackCooldown = 0.0f;   // 다음 발사까지 남은 시간
    float attackInterval = 0.8f;   // 반복 발사 간격
    float firstShotDelay = 0.5f;   // 비경보 상태에서 첫 발까지 대기 시간
    float attackRange = 250.0f;    // 총 사정거리
    int attackDamage = 20;

    // =====================================================
    // 경보
    // =====================================================
    bool heightenedAlert = false;
    float alertLostTimer = 0.0f;
    float alertSearchDuration = 4.0f;
    float alertSearchBaseAngle = 0.0f;
};

void UpdateEnemies(
    std::vector<Enemy>& enemies,
    const SDL_Rect& player,
    const std::vector<Wall>& walls,
    bool alarmActive,
    bool& alarmTriggered,
    int& playerHP,
    float& injuredTimer,
    float dt);

void DrawFOV(
    SDL_Renderer* renderer,
    Enemy& enemy,
    std::vector<Wall>& walls,
    const Camera2D& camera);

void RequestEnemyInvestigate(
    Enemy& enemy,
    Vec2 targetPos);

void NotifyEnemyOfNoise(
    Enemy& enemy,
    Vec2 noisePos,
    float energy,
    bool alarmActive);

const char* GetEnemyStateName(EnemyState state);

// 적 생성 함수

void AddPatrolGuard(
    std::vector<Enemy>& enemies,
    Vec2 spawnCenter,
    const std::vector<Vec2>& patrolPoints);

void AddSentry(
    std::vector<Enemy>& enemies,
    Vec2 spawnCenter);

void AddOfficer(
    std::vector<Enemy>& enemies,
    Vec2 spawnCenter);
