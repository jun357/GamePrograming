#include "Enemy.h"
#include "Camera.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float PI =
        3.14159265358979323846f;
    constexpr float DEG_TO_RAD =
        PI / 180.0f;
    constexpr int DEFAULT_ENEMY_WIDTH = 32;
    constexpr int DEFAULT_ENEMY_HEIGHT = 32;

    SDL_Rect MakeEnemyRectFromCenter(Vec2 center)
    {
        SDL_Rect rect;
        rect.x = (int)std::round(center.x - DEFAULT_ENEMY_WIDTH * 0.5f);
        rect.y = (int)std::round(center.y - DEFAULT_ENEMY_HEIGHT * 0.5f);
        rect.w = DEFAULT_ENEMY_WIDTH;
        rect.h = DEFAULT_ENEMY_HEIGHT;
        return rect;
    }
    float GetAngleToPoint(
        Vec2 from,
        Vec2 to,
        float fallbackAngle)
    {
        float dx = to.x - from.x;
        float dy = to.y - from.y;
        float lenSq = dx * dx + dy * dy;

        if (lenSq <= 0.0001f)
            return fallbackAngle;
        return atan2f(dy, dx);
    }
    void ResetEnemyRuntimeFields(Enemy& enemy)
    {
        enemy.state = EnemyState::Patrol;
        enemy.pos = { 0.0f, 0.0f };
        enemy.homePos = { 0.0f, 0.0f };
        enemy.homeAngle = 0.0f;
        enemy.initialized = false;
        enemy.patrolIndex = 0;
        enemy.investigateTarget = { 0.0f, 0.0f };
        enemy.lastKnownPlayerPos = { 0.0f, 0.0f };
        enemy.stateTimer = 0.0f;
        enemy.searchTimer = 0.0f;
        enemy.hearingEnergy = 0.0f;
        enemy.lastNoisePos = { 0.0f, 0.0f };
        enemy.alerted = false;
        enemy.useHeadSweep = false;
        enemy.headSweepOffset = 0.0f;
        enemy.headSweepMin = -45.0f * DEG_TO_RAD;
        enemy.headSweepMax = 45.0f * DEG_TO_RAD;
        enemy.headSweepSpeed = 0.6f;
        enemy.headSweepDirection = 1;
    }

    void ApplyPatrolGuardDefaults(Enemy& enemy)
    {
        enemy.kind = EnemyKind::PatrolGuard;
        enemy.fov = 60.0f * DEG_TO_RAD;
        enemy.viewDist = 220.0f;
        enemy.rotateSpeed = 0.01f;
        enemy.moveSpeed = 80.0f;
        enemy.searchDuration = 2.0f;
        enemy.hearingThreshold = 4.0f;
        enemy.attackCooldown = 0.0f;
        enemy.attackInterval = 2.2f;
        enemy.firstShotDelay = 0.6f;
        enemy.attackRange = 240.0f;
        enemy.attackDamage = 20;
    }
    void ApplySentryDefaults(Enemy& enemy)
    {
        enemy.kind = EnemyKind::Sentry;
        enemy.angle = 0.0f;
        enemy.fov = 90.0f * DEG_TO_RAD;
        enemy.viewDist = 250.0f;
        enemy.rotateSpeed = 0.0f;
        enemy.moveSpeed = 70.0f;
        enemy.searchDuration = 2.0f;
        enemy.hearingThreshold = 4.0f;
        enemy.useHeadSweep = true;
        enemy.headSweepOffset = 0.0f;
        enemy.headSweepMin = -45.0f * DEG_TO_RAD;
        enemy.headSweepMax = 45.0f * DEG_TO_RAD;
        enemy.headSweepSpeed = 0.6f;
        enemy.headSweepDirection = 1;
        enemy.attackCooldown = 0.0f;
        enemy.attackInterval = 2.2f;
        enemy.firstShotDelay = 0.6f;
        enemy.attackRange = 240.0f;
        enemy.attackDamage = 20;
    }
    void ApplyOfficerDefaults(Enemy& enemy)
    {
        enemy.kind = EnemyKind::Officer;
        enemy.angle = 0.0f;
        enemy.fov = 75.0f * DEG_TO_RAD;
        enemy.viewDist = 280.0f;
        enemy.rotateSpeed = 0.012f;
        enemy.moveSpeed = 90.0f;
        enemy.searchDuration = 2.5f;
        enemy.hearingThreshold = 2.5f;
        enemy.useHeadSweep = true;
        enemy.headSweepOffset = 0.0f;
        enemy.headSweepMin = -45.0f * DEG_TO_RAD;
        enemy.headSweepMax = 45.0f * DEG_TO_RAD;
        enemy.headSweepSpeed = 0.7f;
        enemy.headSweepDirection = 1;
        enemy.attackCooldown = 0.0f;
        enemy.attackInterval = 0.7f;
        enemy.firstShotDelay = 0.25f;
        enemy.attackRange = 200.0f;
        enemy.attackDamage = 10;
    }
}

void AddPatrolGuard(
    std::vector<Enemy>& enemies,
    Vec2 spawnCenter,
    const std::vector<Vec2>& patrolPoints)
{
    Enemy enemy = {};
    ResetEnemyRuntimeFields(enemy);
    enemy.rect = MakeEnemyRectFromCenter(spawnCenter);
    enemy.angle = 0.0f;
    ApplyPatrolGuardDefaults(enemy);
    enemy.patrolPoints = patrolPoints;
    if (!enemy.patrolPoints.empty())
    {
        enemy.angle = GetAngleToPoint(
            spawnCenter,
            enemy.patrolPoints[0],
            enemy.angle);
    }
    enemies.push_back(enemy);
}

void AddSentry(
    std::vector<Enemy>& enemies,
    Vec2 spawnCenter)
{
    Enemy enemy = {};
    ResetEnemyRuntimeFields(enemy);
    enemy.rect = MakeEnemyRectFromCenter(spawnCenter);
    ApplySentryDefaults(enemy);
    enemy.patrolPoints.clear();
    enemies.push_back(enemy);
}

void AddOfficer(
    std::vector<Enemy>& enemies,
    Vec2 spawnCenter)
{
    Enemy enemy = {};
    ResetEnemyRuntimeFields(enemy);
    enemy.rect = MakeEnemyRectFromCenter(spawnCenter);
    ApplyOfficerDefaults(enemy);
    enemy.patrolPoints.clear();
    enemies.push_back(enemy);
}
// =====================================================
// 선분 교차
// =====================================================

static bool LineIntersect(
    float x1, float y1,
    float x2, float y2,
    float x3, float y3,
    float x4, float y4)
{
    float denom =
        (y4 - y3) * (x2 - x1) -
        (x4 - x3) * (y2 - y1);

    if (denom == 0.0f)
        return false;

    float ua =
        ((x4 - x3) * (y1 - y3) -
        (y4 - y3) * (x1 - x3)) / denom;

    float ub =
        ((x2 - x1) * (y1 - y3) -
        (y2 - y1) * (x1 - x3)) / denom;

    return
        ua >= 0.0f && ua <= 1.0f &&
        ub >= 0.0f && ub <= 1.0f;
}

// =====================================================
// 선 vs 사각형
// =====================================================

static bool LineIntersectsRect(
    float x1, float y1,
    float x2, float y2,
    SDL_Rect rect)
{
    float rx = (float)rect.x;
    float ry = (float)rect.y;
    float rw = (float)rect.w;
    float rh = (float)rect.h;

    if (LineIntersect(
        x1, y1, x2, y2,
        rx, ry, rx + rw, ry))
        return true;

    if (LineIntersect(
        x1, y1, x2, y2,
        rx, ry, rx, ry + rh))
        return true;

    if (LineIntersect(
        x1, y1, x2, y2,
        rx + rw, ry,
        rx + rw, ry + rh))
        return true;

    if (LineIntersect(
        x1, y1, x2, y2,
        rx, ry + rh,
        rx + rw, ry + rh))
        return true;

    return false;
}

// =====================================================
// 기본 유틸리티
// =====================================================

static Vec2 GetRectCenter(const SDL_Rect& rect)
{
    return
    {
        rect.x + rect.w * 0.5f,
        rect.y + rect.h * 0.5f
    };
}

static Vec2 GetPlayerCenter(const SDL_Rect& player)
{
    return GetRectCenter(player);
}

static void SyncEnemyRectFromPos(Enemy& enemy)
{
    enemy.rect.x = (int)std::round(enemy.pos.x - enemy.rect.w * 0.5f);
    enemy.rect.y = (int)std::round(enemy.pos.y - enemy.rect.h * 0.5f);
}

static void EnsureEnemyInitialized(Enemy& enemy)
{
    if (enemy.initialized)
        return;
    enemy.pos = GetRectCenter(enemy.rect);
    enemy.homePos = enemy.pos;
    enemy.homeAngle = enemy.angle;
    enemy.initialized = true;

    SyncEnemyRectFromPos(enemy);
}

static bool RectIntersectsAnyWall(
    const SDL_Rect& rect,
    const std::vector<Wall>& walls)
{
    for (const auto& wall : walls)
    {
        if (SDL_HasIntersection(&rect, &wall.rect))
            return true;
    }

    return false;
}

static void FacePoint(
    Enemy& enemy,
    Vec2 point)
{
    Vec2 toTarget = point - enemy.pos;

    if (Length(toTarget) <= 0.001f)
        return;

    enemy.angle = atan2f(toTarget.y, toTarget.x);
}

static bool ShouldKeepFacingFixedWhileInvestigating(
    const Enemy& enemy)
{
    return enemy.kind == EnemyKind::Sentry || enemy.kind == EnemyKind::Officer;
}

static void ChangeEnemyState(
    Enemy& enemy,
    EnemyState newState)
{
    if (enemy.state == newState)
        return;

    enemy.state = newState;
    enemy.stateTimer = 0.0f;
    
    if (newState == EnemyState::Investigate &&
        ShouldKeepFacingFixedWhileInvestigating(enemy))
    {
        FacePoint(enemy, enemy.investigateTarget);
    }

    if (newState == EnemyState::Search)
    {
        enemy.searchTimer = enemy.searchDuration;
    }
}

static bool IsSuspiciousState(EnemyState state)
{
    return
        state == EnemyState::Investigate ||
        state == EnemyState::Search ||
        state == EnemyState::Return ||
        state == EnemyState::Alert;
}

static void UpdateHeadSweep(
    Enemy& enemy,
    float dt)
{
    if (!enemy.useHeadSweep)
        return;
    
    enemy.headSweepOffset +=
        enemy.headSweepDirection *
        enemy.headSweepSpeed *
        dt;

    if (enemy.headSweepOffset > enemy.headSweepMax)
    {
        enemy.headSweepOffset = enemy.headSweepMax;
        enemy.headSweepDirection = -1;
    }
    else if (enemy.headSweepOffset < enemy.headSweepMin)
    {
        enemy.headSweepOffset = enemy.headSweepMin;
        enemy.headSweepDirection = 1;
    }

    enemy.angle =
        enemy.homeAngle + enemy.headSweepOffset;
}

// =====================================================
// 적 이동
// =====================================================

static void MoveEnemyBy(
    Enemy& enemy,
    Vec2 delta,
    const std::vector<Wall>& walls)
{
    SDL_Rect next = enemy.rect;

    Vec2 nextPos = enemy.pos;
    nextPos.x += delta.x;
    next.x = (int)std::round(nextPos.x - enemy.rect.w * 0.5f);

    if (!RectIntersectsAnyWall(next, walls))
    {
        enemy.pos.x = nextPos.x;
        enemy.rect.x = next.x;
    }

    next = enemy.rect;
    nextPos = enemy.pos;
    nextPos.y += delta.y;
    next.y = (int)std::round(nextPos.y - enemy.rect.h * 0.5f);

    if (!RectIntersectsAnyWall(next, walls))
    {
        enemy.pos.y = nextPos.y;
        enemy.rect.y = next.y;
    }
}

static bool MoveEnemyToward(
    Enemy& enemy,
    Vec2 target,
    const std::vector<Wall>& walls,
    float dt,
    bool updateFacing = true)
{
    Vec2 toTarget = target - enemy.pos;
    float dist = Length(toTarget);

    const float arriveDistance = 3.0f;

    if (dist <= arriveDistance)
    {
        enemy.pos = target;
        SyncEnemyRectFromPos(enemy);
        return true;
    }

    if (enemy.moveSpeed <= 0.0f)
        return false;

    Vec2 dir = Normalize(toTarget);

    if (updateFacing)
    {
        FacePoint(enemy, target);
    }

    float step = std::min(enemy.moveSpeed * dt, dist);

    MoveEnemyBy(enemy, dir * step, walls);

    return Length(target - enemy.pos) <= arriveDistance;
}

static Vec2 GetReturnTarget(const Enemy& enemy)
{
    if (!enemy.patrolPoints.empty())
    {
        int index = enemy.patrolIndex;
        if (index < 0 || index >= (int)enemy.patrolPoints.size())
        {
            index = 0;
        }
        return enemy.patrolPoints[index];
    }
    return enemy.homePos;
}

// =====================================================
// 시야 판정
// =====================================================

static bool CanSeePlayer(
    const Enemy& enemy,
    const SDL_Rect& player,
    const std::vector<Wall>& walls)
{
    Vec2 playerCenter = GetPlayerCenter(player);
    Vec2 toPlayer =
    {
        playerCenter.x - enemy.pos.x,
        playerCenter.y - enemy.pos.y
    };

    float distSq = toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y;

    if (distSq >= enemy.viewDist * enemy.viewDist)
        return false;

    if (distSq < 0.0001f)
        return false;

    float dist = sqrtf(distSq);
    Vec2 toPlayerNorm =
    {
        toPlayer.x / dist,
        toPlayer.y / dist
    };
    Vec2 enemyDir =
    {
        cosf(enemy.angle),
        sinf(enemy.angle)
    };

    float dot = Dot(enemyDir, toPlayerNorm);
    float fovLimit = cosf(enemy.fov * 0.5f);

    if (dot <= fovLimit)
        return false;

    for (const auto& wall : walls)
    {
        if (LineIntersectsRect(
            enemy.pos.x,
            enemy.pos.y,
            playerCenter.x,
            playerCenter.y,
            wall.rect))
        {
            return false;
        }
    }
    return true;
}

static float DistanceSqToPlayer(
    const Enemy& enemy,
    const SDL_Rect& player)
{
    Vec2 playerCenter = GetPlayerCenter(player);

    float dx = playerCenter.x - enemy.pos.x;
    float dy = playerCenter.y - enemy.pos.y;

    return dx * dx + dy * dy;
}

static bool IsPlayerInAttackRange(
    const Enemy& enemy,
    const SDL_Rect& player)
{
    float distSq =
        DistanceSqToPlayer(enemy, player);

    return distSq <=
        enemy.attackRange * enemy.attackRange;
}

static float GetInitialAttackDelay(
    const Enemy& enemy,
    bool alarmActive)
{
    if (enemy.kind == EnemyKind::Officer)
    {
        return alarmActive ? 0.15f : 0.25f;
    }

    if (alarmActive)
    {
        return 0.25f;
    }

    return enemy.firstShotDelay;
}

static void ApplyEnemyGunHit(
    Enemy& enemy,
    int& playerHP)
{
    playerHP -= enemy.attackDamage;

    if (playerHP < 0)
    {
        playerHP = 0;
    }

    enemy.attackCooldown = enemy.attackInterval;
}

// =====================================================
// 상태별 업데이트
// =====================================================

static void UpdatePatrol(
    Enemy& enemy,
    const std::vector<Wall>& walls,
    float dt)
{
    if (enemy.kind == EnemyKind::PatrolGuard)
    {
        if (enemy.patrolPoints.empty())
        {
            return;
        }
        if (enemy.patrolIndex < 0 || enemy.patrolIndex >= (int)enemy.patrolPoints.size())
        {
            enemy.patrolIndex = 0;
        }
        Vec2 target = enemy.patrolPoints[enemy.patrolIndex];
        bool arrived = MoveEnemyToward(enemy, target, walls, dt);
        if (arrived)
        {
            enemy.patrolIndex =
                (enemy.patrolIndex + 1) %
                (int)enemy.patrolPoints.size();
        }
        return;
    }

    if (enemy.kind == EnemyKind::Sentry || enemy.kind == EnemyKind::Officer)
    {
        if (enemy.useHeadSweep)
        {
            UpdateHeadSweep(enemy, dt);
        }
        return;
    }
}

static void UpdateInvestigate(
    Enemy& enemy,
    const std::vector<Wall>& walls,
    float dt)
{
    bool keepFacingFixed = ShouldKeepFacingFixedWhileInvestigating(enemy);
    bool arrived = MoveEnemyToward(enemy, enemy.investigateTarget, walls, dt, !keepFacingFixed);

    if (arrived)
    {
        ChangeEnemyState(enemy, EnemyState::Search);
    }
}

static void UpdateSearch(
    Enemy& enemy,
    float dt)
{
    enemy.searchTimer -= dt;
    // 주변을 살피는 동작
    if (enemy.rotateSpeed != 0.0f)
    {
        enemy.angle += enemy.rotateSpeed * dt * 90.0f;
    }
    else
    {
        enemy.angle += 1.5f * dt;
    }
    if (enemy.searchTimer <= 0.0f)
    {
        ChangeEnemyState(enemy, EnemyState::Return);
    }
}

static void UpdateReturn(
    Enemy& enemy,
    const std::vector<Wall>& walls,
    float dt)
{
    Vec2 returnTarget = GetReturnTarget(enemy);
    bool arrived = MoveEnemyToward(enemy, returnTarget, walls, dt);

    if (arrived)
    {
        if (enemy.patrolPoints.empty())
        {
            enemy.angle = enemy.homeAngle;
        }
        ChangeEnemyState(enemy, EnemyState::Patrol);
    }
}

static void UpdateAlert(
    Enemy& enemy,
    const SDL_Rect& player,
    const std::vector<Wall>& walls,
    int& playerHP,
    float dt)
{
    Vec2 playerCenter =
        GetPlayerCenter(player);

    FacePoint(enemy, playerCenter);

    if (enemy.attackCooldown > 0.0f)
    {
        enemy.attackCooldown -= dt;
    }

    bool canSee =
        CanSeePlayer(enemy, player, walls);

    if (!canSee)
    {
        enemy.investigateTarget =
            enemy.lastKnownPlayerPos;

        ChangeEnemyState(
            enemy,
            EnemyState::Investigate);

        return;
    }

    enemy.lastKnownPlayerPos =
        playerCenter;

    if (!IsPlayerInAttackRange(enemy, player))
    {
        return;
    }

    if (enemy.attackCooldown <= 0.0f)
    {
        ApplyEnemyGunHit(
            enemy,
            playerHP);
    }
}

// =====================================================
// 적 업데이트
// =====================================================

void UpdateEnemies(
    std::vector<Enemy>& enemies,
    const SDL_Rect& player,
    const std::vector<Wall>& walls,
    bool alarmActive,
    bool& alarmTriggered,
    int& playerHP,
    float dt)
{
    for (auto& enemy : enemies)
    {
        EnsureEnemyInitialized(enemy);
        enemy.stateTimer += dt;
        // Sound.cpp가 enemy.hearingEnergy를 직접 올리고 있으므로 디버그 표시용으로만 남겨둠
        enemy.hearingEnergy *= expf(-2.0f * dt);
        if (enemy.state == EnemyState::Dead)
        {
            enemy.alerted = false;
            continue;
        }
        bool canSeePlayer = CanSeePlayer(enemy, player, walls);
        if (canSeePlayer)
        {
            enemy.lastKnownPlayerPos = GetPlayerCenter(player);
            bool wasAlreadyAlert = enemy.state == EnemyState::Alert;

            if (!wasAlreadyAlert)
            {
                ChangeEnemyState(enemy, EnemyState::Alert);
                enemy.attackCooldown = GetInitialAttackDelay(enemy, alarmActive);
            }
            alarmTriggered = true;
        }
        // 상태별 행동
        switch (enemy.state)
        {
        case EnemyState::Patrol:
            UpdatePatrol(enemy, walls, dt);
            break;
        case EnemyState::Investigate:
            UpdateInvestigate(enemy, walls, dt);
            break;
        case EnemyState::Search:
            UpdateSearch(enemy, dt);
            break;
        case EnemyState::Return:
            UpdateReturn(enemy, walls, dt);
            break;
        case EnemyState::Alert:
            UpdateAlert(enemy, player, walls, playerHP, dt);
            break;
        case EnemyState::Dead:
            break;
        }
        enemy.alerted = IsSuspiciousState(enemy.state) || enemy.hearingEnergy >= enemy.hearingThreshold;
    }
}

// =====================================================
// 소음 연결용 (임시)
// =====================================================

void RequestEnemyInvestigate(
    Enemy& enemy,
    Vec2 targetPos)
{
    if (enemy.state == EnemyState::Dead)
        return;
    if (enemy.state == EnemyState::Alert)
        return;
    enemy.investigateTarget = targetPos;
    ChangeEnemyState(enemy, EnemyState::Investigate);
}

void NotifyEnemyOfNoise(
    Enemy& enemy,
    Vec2 noisePos,
    float energy)
{
    enemy.lastNoisePos = noisePos;
    enemy.hearingEnergy += energy;

    // if (enemy.hearingEnergy >= enemy.hearingThreshold)
    // {
    //    RequestEnemyInvestigate(enemy, noisePos);
    // }
}

const char* GetEnemyStateName(EnemyState state)
{
    switch (state)
    {
    case EnemyState::Patrol:
        return "Patrol";
    case EnemyState::Investigate:
        return "Investigate";
    case EnemyState::Search:
        return "Search";
    case EnemyState::Return:
        return "Return";
    case EnemyState::Alert:
        return "Alert";
    case EnemyState::Dead:
        return "Dead";
    }
    return "Unknown";
}

// =====================================================
// FOV 렌더
// =====================================================

void DrawFOV(
    SDL_Renderer* renderer,
    Enemy& enemy,
    std::vector<Wall>& walls,
    const Camera2D& camera)
{
    int rays = 120;

    float start =
        enemy.angle -
        enemy.fov * 0.5f;

    float step =
        enemy.fov / rays;

    SDL_Point center =
    {
        enemy.rect.x +
        enemy.rect.w / 2,

        enemy.rect.y +
        enemy.rect.h / 2
    };

    SDL_SetRenderDrawColor(
        renderer,
        255, 255, 0, 80);

    for (int i = 0; i < rays; i++)
    {
        float a =
            start + step * i;

        float dx = cos(a);
        float dy = sin(a);

        float rayLength =
            enemy.viewDist;

        for (auto& w : walls)
        {
            for (float t = 0;
                t < enemy.viewDist;
                t += 2.0f)
            {
                float rx =
                    center.x + dx * t;

                float ry =
                    center.y + dy * t;

                SDL_Rect rect = w.rect;

                if (rx >= rect.x &&
                    rx <= rect.x + rect.w &&
                    ry >= rect.y &&
                    ry <= rect.y + rect.h)
                {
                    rayLength = t;
                    break;
                }
            }
        }

        SDL_Point end =
        {
            (int)(center.x + dx * rayLength),
            (int)(center.y + dy * rayLength)
        };

        Vec2 centerWorld = { (float)center.x, (float)center.y };
        Vec2 endWorld = { (float)end.x, (float)end.y };
        SDL_Point centerScreen = camera.WorldToScreenPoint(centerWorld);
        SDL_Point endScreen = camera.WorldToScreenPoint(endWorld);
        SDL_RenderDrawLine(
            renderer,
            centerScreen.x,
            centerScreen.y,
            endScreen.x,
            endScreen.y);
    }
}
