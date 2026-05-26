#include "Enemy.h"
#include "Camera.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float PI = 3.14159265358979323846f;
    constexpr float DEG_TO_RAD = PI / 180.0f;
    constexpr int DEFAULT_ENEMY_WIDTH = 32;
    constexpr int DEFAULT_ENEMY_HEIGHT = 32;
    constexpr float ENEMY_MOVE_TURN_SPEED = 160.0f * DEG_TO_RAD;
    constexpr float ENEMY_INVESTIGATE_TURN_SPEED = 95.0f * DEG_TO_RAD;
    constexpr float ENEMY_SEARCH_TURN_SPEED = 75.0f * DEG_TO_RAD;
    constexpr float ENEMY_ALERT_AIM_TURN_SPEED = 180.0f * DEG_TO_RAD;
    constexpr float ENEMY_ALERT_SEARCH_TURN_SPEED = 90.0f * DEG_TO_RAD;
    constexpr float NOISE_RETRIGGER_DISTANCE = 80.0f;
    constexpr float NOISE_RETRIGGER_DISTANCE_SQ = NOISE_RETRIGGER_DISTANCE * NOISE_RETRIGGER_DISTANCE;
    constexpr float ENEMY_ARRIVE_DISTANCE = 3.0f;
    constexpr float ENEMY_ARRIVE_DISTANCE_SQ = ENEMY_ARRIVE_DISTANCE * ENEMY_ARRIVE_DISTANCE;
    constexpr float ENEMY_VISUAL_SNAP_DISTANCE = 0.5f;
    constexpr float ENEMY_VISUAL_SNAP_DISTANCE_SQ = ENEMY_VISUAL_SNAP_DISTANCE * ENEMY_VISUAL_SNAP_DISTANCE;

    // Alert 상태에서 마지막 목격 위치로 이동하다가
    // 벽에 막히거나 너무 오래 걸리면 현재 위치에서 수색으로 전환한다.
    constexpr float ENEMY_ALERT_CHASE_STUCK_TIME = 0.85f;
    constexpr float ENEMY_ALERT_CHASE_TIMEOUT = 3.0f;

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
        for (int i = 0; i < 5; ++i)
        {
            enemy.investigatePath[i] = { 0.0f, 0.0f };
        }
        enemy.investigatePathCount = 0;
        enemy.investigatePathIndex = 0;
        enemy.needsInvestigatePathBuild = false;
        enemy.investigateRouteTimeout = 0.0f;
        
        enemy.lastKnownPlayerPos = { 0.0f, 0.0f };
        enemy.returnTarget = { 0.0f, 0.0f };
        enemy.resumePatrolPos = { 0.0f, 0.0f };
        enemy.resumePatrolIndex = 0;
        enemy.resumeAngle = 0.0f;
        enemy.hasResumePoint = false;
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
        enemy.maxHP = 100;
        enemy.hp = enemy.maxHP;
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
        enemy.maxHP = 100;
        enemy.hp = enemy.maxHP;
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
        enemy.maxHP = 200;
        enemy.hp = enemy.maxHP;
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

static SDL_Rect MakeEnemyCollisionRectAtCenter(
    const Enemy& enemy,
    Vec2 center)
{
    SDL_Rect rect = enemy.rect;

    rect.x = static_cast<int>(
        std::round(center.x - rect.w * 0.5f));

    rect.y = static_cast<int>(
        std::round(center.y - rect.h * 0.5f));

    return rect;
}

static bool CanEnemyOccupyCenter(
    const Enemy& enemy,
    Vec2 center,
    const std::vector<Wall>& walls)
{
    SDL_Rect rect =
        MakeEnemyCollisionRectAtCenter(enemy, center);

    return !RectIntersectsAnyWall(rect, walls);
}

// =====================================================
// 소음 조사용 간단 우회 경로 생성
// =====================================================

static constexpr int INVESTIGATE_PATH_CAPACITY = 5;
static constexpr float INVESTIGATE_PATH_CLEARANCE = 48.0f;
static constexpr float INVESTIGATE_PATH_TIMEOUT_MARGIN = 1.5f;

static bool IsSegmentBlockedByWalls(
    Vec2 from,
    Vec2 to,
    const std::vector<Wall>& walls)
{
    for (const auto& wall : walls)
    {
        if (LineIntersectsRect(
            from.x,
            from.y,
            to.x,
            to.y,
            wall.rect))
        {
            return true;
        }
    }

    return false;
}

static const Wall* FindFirstBlockingWall(
    Vec2 from,
    Vec2 to,
    const std::vector<Wall>& walls)
{
    for (const auto& wall : walls)
    {
        if (LineIntersectsRect(
            from.x,
            from.y,
            to.x,
            to.y,
            wall.rect))
        {
            return &wall;
        }
    }

    return nullptr;
}

static void MakeBypassCandidatesForWall(
    const SDL_Rect& wallRect,
    Vec2 candidates[4])
{
    float left =
        static_cast<float>(wallRect.x) - INVESTIGATE_PATH_CLEARANCE;

    float right =
        static_cast<float>(wallRect.x + wallRect.w) +
        INVESTIGATE_PATH_CLEARANCE;

    float top =
        static_cast<float>(wallRect.y) - INVESTIGATE_PATH_CLEARANCE;

    float bottom =
        static_cast<float>(wallRect.y + wallRect.h) +
        INVESTIGATE_PATH_CLEARANCE;

    // waypoint
    candidates[0] = { left,  top };
    candidates[1] = { right, top };
    candidates[2] = { left,  bottom };
    candidates[3] = { right, bottom };
}

static bool IsValidInvestigationWaypoint(
    const Enemy& enemy,
    Vec2 waypoint,
    const std::vector<Wall>& walls)
{
    return CanEnemyOccupyCenter(enemy, waypoint, walls);
}

static float CalculateRouteLength(
    Vec2 start,
    const Vec2* points,
    int pointCount)
{
    float total = 0.0f;
    Vec2 prev = start;

    for (int i = 0; i < pointCount; ++i)
    {
        total += Distance(prev, points[i]);
        prev = points[i];
    }

    return total;
}

static void StoreInvestigatePath(
    Enemy& enemy,
    const Vec2* points,
    int pointCount)
{
    if (pointCount <= 0)
    {
        enemy.investigatePathCount = 0;
        enemy.investigatePathIndex = 0;
        enemy.investigateRouteTimeout = enemy.investigateTimeout;
        return;
    }

    if (pointCount > INVESTIGATE_PATH_CAPACITY)
    {
        pointCount = INVESTIGATE_PATH_CAPACITY;
    }

    for (int i = 0; i < pointCount; ++i)
    {
        enemy.investigatePath[i] = points[i];
    }

    enemy.investigatePathCount = pointCount;
    enemy.investigatePathIndex = 0;

    float routeLength =
        CalculateRouteLength(enemy.pos, enemy.investigatePath, pointCount);

    float speed = enemy.moveSpeed;
    if (speed < 1.0f)
    {
        speed = 1.0f;
    }

    float routeTimeout =
        routeLength / speed + INVESTIGATE_PATH_TIMEOUT_MARGIN;

    enemy.investigateRouteTimeout =
        routeTimeout > enemy.investigateTimeout
        ? routeTimeout
        : enemy.investigateTimeout;
}

static void BuildInvestigatePath(
    Enemy& enemy,
    const std::vector<Wall>& walls)
{
    enemy.needsInvestigatePathBuild = false;
    enemy.investigatePathCount = 0;
    enemy.investigatePathIndex = 0;
    enemy.investigateRouteTimeout = enemy.investigateTimeout;

    Vec2 target = enemy.investigateTarget;
    if (!IsSegmentBlockedByWalls(enemy.pos, target, walls))
    {
        Vec2 directPath[1] = { target };
        StoreInvestigatePath(enemy, directPath, 1);
        return;
    }

    const Wall* blockingWall =
        FindFirstBlockingWall(enemy.pos, target, walls);

    if (!blockingWall)
    {
        Vec2 directPath[1] = { target };
        StoreInvestigatePath(enemy, directPath, 1);
        return;
    }

    Vec2 candidates[4];
    MakeBypassCandidatesForWall(blockingWall->rect, candidates);

    bool foundPath = false;
    float bestLength = 0.0f;
    Vec2 bestPath[3];
    int bestPathCount = 0;

    // 1차: waypoint 1개로 돌아갈 수 있는 경로를 찾음
    for (int i = 0; i < 4; ++i)
    {
        Vec2 waypoint = candidates[i];

        if (!IsValidInvestigationWaypoint(enemy, waypoint, walls))
        {
            continue;
        }

        if (IsSegmentBlockedByWalls(enemy.pos, waypoint, walls))
        {
            continue;
        }

        if (IsSegmentBlockedByWalls(waypoint, target, walls))
        {
            continue;
        }

        Vec2 path[2] = { waypoint, target };
        float length = CalculateRouteLength(enemy.pos, path, 2);

        if (!foundPath || length < bestLength)
        {
            foundPath = true;
            bestLength = length;
            bestPath[0] = waypoint;
            bestPath[1] = target;
            bestPathCount = 2;
        }
    }

    // 2차: waypoint 2개로 벽의 위/아래를 더 확실히 돌아가는 경로를 찾음
    for (int i = 0; i < 4; ++i)
    {
        Vec2 first = candidates[i];

        if (!IsValidInvestigationWaypoint(enemy, first, walls))
        {
            continue;
        }

        if (IsSegmentBlockedByWalls(enemy.pos, first, walls))
        {
            continue;
        }

        for (int j = 0; j < 4; ++j)
        {
            if (i == j)
            {
                continue;
            }

            Vec2 second = candidates[j];

            if (!IsValidInvestigationWaypoint(enemy, second, walls))
            {
                continue;
            }

            if (IsSegmentBlockedByWalls(first, second, walls))
            {
                continue;
            }

            if (IsSegmentBlockedByWalls(second, target, walls))
            {
                continue;
            }

            Vec2 path[3] = { first, second, target };
            float length = CalculateRouteLength(enemy.pos, path, 3);

            if (!foundPath || length < bestLength)
            {
                foundPath = true;
                bestLength = length;
                bestPath[0] = first;
                bestPath[1] = second;
                bestPath[2] = target;
                bestPathCount = 3;
            }
        }
    }

    if (foundPath)
    {
        StoreInvestigatePath(enemy, bestPath, bestPathCount);
        return;
    }

    // fallback: 경로 후보를 못 찾으면 기존처럼 직접 이동
    Vec2 directPath[1] = { target };
    StoreInvestigatePath(enemy, directPath, 1);
}

static void SetEnemyCenter(Enemy& enemy, Vec2 center)
{
    enemy.pos = center;
    enemy.rect = MakeEnemyCollisionRectAtCenter(enemy, center);
}

static Vec2 ProjectVec2(Vec2 v, Vec2 onto)
{
    float denom = Dot(onto, onto);

    if (denom <= 0.000001f)
    {
        return { 0.0f, 0.0f };
    }

    return onto * (Dot(v, onto) / denom);
}

static Vec2 MakeFullSpeedSlideDelta(Vec2 delta, Vec2 tangent)
{
    Vec2 projected = ProjectVec2(delta, tangent);

    if (LengthSq(projected) <= 0.000001f)
    {
        return { 0.0f, 0.0f };
    }

    return Normalize(projected) * Length(delta);
}

static void ConsiderEnemyMoveCandidate(
    const Enemy& enemy,
    Vec2 startPos,
    Vec2 target,
    Vec2 candidateDelta,
    const std::vector<Wall>& walls,
    Vec2& bestPos,
    float& bestScore,
    bool& foundCandidate)
{
    if (LengthSq(candidateDelta) <= 0.000001f)
    {
        return;
    }

    Vec2 candidatePos = startPos + candidateDelta;

    if (!CanEnemyOccupyCenter(enemy, candidatePos, walls))
    {
        return;
    }

    float startDistSq = DistanceSq(startPos, target);
    float candidateDistSq = DistanceSq(candidatePos, target);

    float score =
        (startDistSq - candidateDistSq) +
        LengthSq(candidateDelta) * 0.001f;

    if (score < -0.25f)
    {
        return;
    }

    if (!foundCandidate || score > bestScore)
    {
        bestScore = score;
        bestPos = candidatePos;
        foundCandidate = true;
    }
}

static void FacePoint(Enemy& enemy, Vec2 point)
{
    Vec2 toTarget = point - enemy.pos;
    if (LengthSq(toTarget) <= 0.0001f)
    {
        return;
    }

    enemy.angle = WrapAngle(atan2f(toTarget.y, toTarget.x));
}

static void RotateTowardAngle(
    Enemy& enemy,
    float targetAngle,
    float maxRadiansPerSecond,
    float dt)
{
    float delta = WrapAngle(targetAngle - enemy.angle);
    float maxStep = maxRadiansPerSecond * dt;

    if (maxStep <= 0.0f)
    {
        return;
    }

    if (fabsf(delta) <= maxStep)
    {
        enemy.angle = WrapAngle(targetAngle);
        return;
    }

    enemy.angle = WrapAngle(
        enemy.angle + (delta > 0.0f ? maxStep : -maxStep));
}

static void FacePointSmooth(
    Enemy& enemy,
    Vec2 point,
    float maxRadiansPerSecond,
    float dt)
{
    Vec2 toTarget = point - enemy.pos;

    if (LengthSq(toTarget) <= 0.0001f)
    {
        return;
    }

    float targetAngle = atan2f(toTarget.y, toTarget.x);

    RotateTowardAngle(
        enemy,
        targetAngle,
        maxRadiansPerSecond,
        dt);
}

static bool ShouldKeepFacingFixedWhileInvestigating(const Enemy& enemy)
{
    return false;
}

static void SaveReturnPointForInvestigation(
    Enemy& enemy,
    EnemyState oldState)
{
    if (enemy.hasResumePoint)
    {
        return;
    }

    if (oldState == EnemyState::Dead ||
        oldState == EnemyState::Alert)
    {
        return;
    }

    if (!enemy.patrolPoints.empty())
    {
        enemy.resumePatrolPos = enemy.pos;
        enemy.resumePatrolIndex = enemy.patrolIndex;
        enemy.resumeAngle = enemy.angle;
    }
    else
    {
        enemy.resumePatrolPos = enemy.homePos;
        enemy.resumePatrolIndex = enemy.patrolIndex;
        enemy.resumeAngle = enemy.homeAngle;
    }

    enemy.hasResumePoint = true;
}

static void SnapEnemyToPointIfVeryClose(Enemy& enemy, Vec2 point)
{
    if (DistanceSq(enemy.pos, point) <= ENEMY_VISUAL_SNAP_DISTANCE_SQ)
    {
        enemy.pos = point;
        SyncEnemyRectFromPos(enemy);
    }
}

static void RestoreReturnResumeData(Enemy& enemy)
{
    if (!enemy.hasResumePoint)
    {
        if (enemy.patrolPoints.empty())
        {
            SnapEnemyToPointIfVeryClose(enemy, enemy.homePos);

            if (DistanceSq(enemy.pos, enemy.homePos) <=
                ENEMY_ARRIVE_DISTANCE_SQ)
            {
                enemy.angle = enemy.homeAngle;
                enemy.headSweepOffset = 0.0f;
            }
        }

        enemy.stuckTimer = 0.0f;
        return;
    }

    if (!enemy.patrolPoints.empty())
    {
        if (enemy.resumePatrolIndex >= 0 &&
            enemy.resumePatrolIndex <
            static_cast<int>(enemy.patrolPoints.size()))
        {
            enemy.patrolIndex = enemy.resumePatrolIndex;
        }

        SnapEnemyToPointIfVeryClose(enemy, enemy.resumePatrolPos);
    }
    else
    {
        SnapEnemyToPointIfVeryClose(enemy, enemy.homePos);

        if (DistanceSq(enemy.pos, enemy.homePos) <=
            ENEMY_ARRIVE_DISTANCE_SQ)
        {
            enemy.angle = enemy.homeAngle;
            enemy.headSweepOffset = 0.0f;
        }
    }

    enemy.hasResumePoint = false;
    enemy.stuckTimer = 0.0f;
}

static Vec2 GetReturnTarget(const Enemy& enemy)
{
    if (enemy.hasResumePoint)
    {
        return enemy.resumePatrolPos;
    }

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

static void ChangeEnemyState(Enemy& enemy, EnemyState newState)
{
    EnemyState oldState = enemy.state;

    if (oldState == newState)
    {
        return;
    }

    enemy.state = newState;
    enemy.stateTimer = 0.0f;
    enemy.stuckTimer = 0.0f;
    enemy.angle = WrapAngle(enemy.angle);

    switch (newState)
    {
    case EnemyState::Patrol:
        enemy.hasPendingNoise = false;
        enemy.pendingNoiseEnergy = 0.0f;
        enemy.hearingEnergy = 0.0f;
        enemy.hasResumePoint = false;

        enemy.investigatePathCount = 0;
        enemy.investigatePathIndex = 0;
        enemy.needsInvestigatePathBuild = false;
        enemy.investigateRouteTimeout = 0.0f;

        break;

    case EnemyState::Investigate:
        SaveReturnPointForInvestigation(enemy, oldState);
        enemy.investigatePathCount = 0;
        enemy.investigatePathIndex = 0;
        enemy.needsInvestigatePathBuild = true;
        enemy.investigateRouteTimeout = 0.0f;
        break;

    case EnemyState::Search:
        enemy.searchTimer = enemy.heightenedAlert
            ? enemy.searchDuration * 1.5f
            : enemy.searchDuration;
        enemy.searchBaseAngle = enemy.angle;
        break;

    case EnemyState::Return:
        enemy.returnTarget = GetReturnTarget(enemy);
        break;

    case EnemyState::Alert:
        SaveReturnPointForInvestigation(enemy, oldState);
        enemy.heightenedAlert = true;
        enemy.alertLostTimer = 0.0f;
        enemy.alertSearchBaseAngle = enemy.angle;
        enemy.hasPendingNoise = false;
        break;

    case EnemyState::Dead:
        enemy.alerted = false;
        break;
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
        WrapAngle(enemy.homeAngle + enemy.headSweepOffset);
}

// =====================================================
// 적 이동
// =====================================================

static void MoveEnemyBy(
    Enemy& enemy,
    Vec2 delta,
    const std::vector<Wall>& walls,
    Vec2 target)
{
    if (LengthSq(delta) <= 0.000001f)
    {
        return;
    }

    Vec2 startPos = enemy.pos;
    Vec2 bestPos = startPos;

    float bestScore = -1.0e30f;
    bool foundCandidate = false;
    // 1. 원래 의도한 이동
    ConsiderEnemyMoveCandidate(enemy, startPos, target, delta, walls, bestPos, bestScore, foundCandidate);
    // 2. 기존 방식과 유사한 축 분리 이동 후보
    ConsiderEnemyMoveCandidate(enemy, startPos, target, { delta.x, 0.0f }, walls, bestPos, bestScore, foundCandidate);
    ConsiderEnemyMoveCandidate(enemy, startPos, target, { 0.0f, delta.y }, walls, bestPos, bestScore, foundCandidate);
    // 3. full-speed wall slide 후보
    Vec2 slideX = MakeFullSpeedSlideDelta(delta, { 1.0f, 0.0f });
    Vec2 slideY = MakeFullSpeedSlideDelta(delta, { 0.0f, 1.0f });
    ConsiderEnemyMoveCandidate(enemy, startPos, target, slideX, walls, bestPos, bestScore, foundCandidate);
    ConsiderEnemyMoveCandidate(enemy, startPos, target, slideY, walls, bestPos, bestScore, foundCandidate);
    // 4. 벽 모서리에서 살짝 틀어 지나가는 후보
    float stepLen = Length(delta);
    Vec2 dir = Normalize(delta);
    const float INV_SQRT2 = 0.70710678118f;

    Vec2 left45 =
    {
        (dir.x - dir.y) * INV_SQRT2,
        (dir.x + dir.y) * INV_SQRT2
    };

    Vec2 right45 =
    {
        (dir.x + dir.y) * INV_SQRT2,
        (-dir.x + dir.y) * INV_SQRT2
    };

    ConsiderEnemyMoveCandidate(enemy, startPos, target, left45 * stepLen, walls, bestPos, bestScore, foundCandidate);

    ConsiderEnemyMoveCandidate(enemy, startPos, target, right45 * stepLen, walls, bestPos, bestScore, foundCandidate);

    if (foundCandidate)
    {
        SetEnemyCenter(enemy, bestPos);
    }
}
static bool MoveEnemyToward(
    Enemy& enemy,
    Vec2 target,
    const std::vector<Wall>& walls,
    float dt,
    bool updateFacing = true,
    float turnSpeed = ENEMY_MOVE_TURN_SPEED)
{
    Vec2 toTarget = target - enemy.pos;
    float distSq = LengthSq(toTarget);

    if (distSq <= ENEMY_VISUAL_SNAP_DISTANCE_SQ)
    {
        enemy.pos = target;
        SyncEnemyRectFromPos(enemy);
        enemy.stuckTimer = 0.0f;
        return true;
    }

    if (enemy.moveSpeed <= 0.0f)
    {
        enemy.stuckTimer += dt;
        return false;
    }

    float dist = sqrtf(distSq);

    Vec2 before = enemy.pos;
    Vec2 dir = Normalize(toTarget);

    if (updateFacing)
    {
        FacePointSmooth(enemy, target, turnSpeed, dt);
    }

    float step = std::min(enemy.moveSpeed * dt, dist);

    MoveEnemyBy(enemy, dir * step, walls, target);

    if (DistanceSq(before, enemy.pos) <= 0.01f)
    {
        enemy.stuckTimer += dt;
    }
    else
    {
        enemy.stuckTimer = 0.0f;
    }

    float remainingSq = DistanceSq(target, enemy.pos);

    if (remainingSq <= ENEMY_VISUAL_SNAP_DISTANCE_SQ)
    {
        enemy.pos = target;
        SyncEnemyRectFromPos(enemy);
        enemy.stuckTimer = 0.0f;
        return true;
    }
    return remainingSq <= ENEMY_ARRIVE_DISTANCE_SQ;
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
    int& playerHP,
    float& injuredTimer)
{
    playerHP -= enemy.attackDamage;
    injuredTimer = 0.6f;

    if (playerHP < 0)
    {
        playerHP = 0;
    }

    enemy.attackCooldown = enemy.attackInterval;
}

static void BeginAlertLocalSearchAtCurrentPosition(Enemy& enemy)
{
    // 마지막 목격 위치가 벽 너머라서 도달 불가능한 경우, 현재 위치를 수색 기준점으로 바꾼다.
    enemy.lastKnownPlayerPos = enemy.pos;

    // 이제부터는 "이 위치에서 얼마나 수색했는가"를 재기 위해 초기화한다.
    enemy.alertLostTimer = 0.0f;
    enemy.alertSearchBaseAngle = enemy.angle;

    // 막힘 상태도 초기화한다.
    enemy.stuckTimer = 0.0f;
}

static bool IsNoiseTaskState(EnemyState state)
{
    return
        state == EnemyState::Investigate ||
        state == EnemyState::Search ||
        state == EnemyState::Return;
}

static bool IsSameNoiseAreaForCurrentTask(
    const Enemy& enemy,
    Vec2 noisePos)
{
    Vec2 reference = enemy.lastNoisePos;

    if (enemy.state == EnemyState::Investigate)
    {
        reference = enemy.investigateTarget;
    }
    else if (enemy.hasPendingNoise)
    {
        reference = enemy.pendingNoisePos;
    }

    return DistanceSq(noisePos, reference) <=
        NOISE_RETRIGGER_DISTANCE_SQ;
}

static void ConsumePendingNoise(Enemy& enemy, bool alarmActive)
{
    if (!enemy.hasPendingNoise)
    {
        return;
    }

    if (alarmActive ||
        enemy.state == EnemyState::Dead ||
        enemy.state == EnemyState::Alert)
    {
        enemy.hasPendingNoise = false;
        enemy.pendingNoiseEnergy = 0.0f;
        return;
    }

    enemy.investigateTarget = enemy.pendingNoisePos;
    enemy.lastNoisePos = enemy.pendingNoisePos;
    enemy.hearingEnergy = 0.0f;
    enemy.pendingNoiseEnergy = 0.0f;
    enemy.hasPendingNoise = false;

    enemy.investigatePathCount = 0;
    enemy.investigatePathIndex = 0;
    enemy.needsInvestigatePathBuild = true;
    enemy.investigateRouteTimeout = 0.0f;

    ChangeEnemyState(enemy, EnemyState::Investigate);
}

// =====================================================
// 상태별 업데이트
// =====================================================

static void UpdatePatrol(Enemy& enemy, const std::vector<Wall>& walls, float dt)
{
    const float ARRIVE_DISTANCE = 3.0f;

    if (enemy.kind == EnemyKind::PatrolGuard)
    {
        if (enemy.patrolPoints.empty())
        {
            return;
        }

        if (enemy.patrolIndex < 0 ||
            enemy.patrolIndex >= static_cast<int>(enemy.patrolPoints.size()))
        {
            enemy.patrolIndex = 0;
        }

        Vec2 target = enemy.patrolPoints[enemy.patrolIndex];
        bool arrived = MoveEnemyToward(enemy, target, walls, dt, true);

        if (arrived)
        {
            enemy.patrolIndex =
                (enemy.patrolIndex + 1) %
                static_cast<int>(enemy.patrolPoints.size());
        }

        return;
    }

    // 보초/간부가 Patrol 상태인데 원래 위치가 아니면 우선 home으로 복귀시킨다.
    if (DistanceSq(enemy.pos, enemy.homePos) >
        ARRIVE_DISTANCE * ARRIVE_DISTANCE)
    {
        bool arrived = MoveEnemyToward(enemy, enemy.homePos, walls, dt, true);
        if (arrived)
        {
            enemy.angle = enemy.homeAngle;
            enemy.headSweepOffset = 0.0f;
            enemy.stuckTimer = 0.0f;
        }
        return;
    }

    UpdateHeadSweep(enemy, dt);
}

static void UpdateInvestigate(
    Enemy& enemy,
    const std::vector<Wall>& walls,
    float dt)
{
    if (enemy.needsInvestigatePathBuild ||
        enemy.investigatePathCount <= 0)
    {
        BuildInvestigatePath(enemy, walls);
    }

    if (enemy.investigatePathCount <= 0)
    {
        ChangeEnemyState(enemy, EnemyState::Search);
        return;
    }

    if (enemy.investigatePathIndex < 0)
    {
        enemy.investigatePathIndex = 0;
    }

    if (enemy.investigatePathIndex >= enemy.investigatePathCount)
    {
        ChangeEnemyState(enemy, EnemyState::Search);
        return;
    }

    Vec2 currentTarget =
        enemy.investigatePath[enemy.investigatePathIndex];

    bool arrived = MoveEnemyToward(
        enemy,
        currentTarget,
        walls,
        dt,
        true,
        ENEMY_INVESTIGATE_TURN_SPEED);

    if (arrived)
    {
        if (enemy.investigatePathIndex + 1 <
            enemy.investigatePathCount)
        {
            enemy.investigatePathIndex++;
            enemy.stuckTimer = 0.0f;
            return;
        }

        ChangeEnemyState(enemy, EnemyState::Search);
        return;
    }

    float timeout =
        enemy.investigateRouteTimeout > 0.0f
        ? enemy.investigateRouteTimeout
        : enemy.investigateTimeout;

    if (enemy.stateTimer >= timeout)
    {
        ChangeEnemyState(enemy, EnemyState::Search);
    }
}

static void UpdateSearch(Enemy& enemy, float dt)
{
    enemy.searchTimer -= dt;

    const float sweepRange = enemy.heightenedAlert
        ? 90.0f * DEG_TO_RAD
        : 45.0f * DEG_TO_RAD;

    const float sweepSpeed = enemy.heightenedAlert ? 1.4f : 1.1f;

    float sweep = sinf(enemy.stateTimer * sweepSpeed) * sweepRange;
    float desiredAngle = WrapAngle(enemy.searchBaseAngle + sweep);

    float turnSpeed = enemy.heightenedAlert
        ? ENEMY_ALERT_SEARCH_TURN_SPEED
        : ENEMY_SEARCH_TURN_SPEED;

    RotateTowardAngle(
        enemy,
        desiredAngle,
        turnSpeed,
        dt);

    if (enemy.searchTimer <= 0.0f)
    {
        ChangeEnemyState(enemy, EnemyState::Return);
    }
}

static void SnapToReturnTargetAndPatrol(Enemy& enemy)
{
    RestoreReturnResumeData(enemy);

    ChangeEnemyState(enemy, EnemyState::Patrol);
}

static void UpdateReturn(Enemy& enemy, const std::vector<Wall>& walls, float dt)
{
    bool arrived =
        MoveEnemyToward(enemy, enemy.returnTarget, walls, dt, true);

    if (arrived)
    {
        RestoreReturnResumeData(enemy);

        ChangeEnemyState(enemy, EnemyState::Patrol);
        return;
    }
    if (enemy.stuckTimer >= 1.25f)
    {
        SnapToReturnTargetAndPatrol(enemy);
    }
}

static void UpdateAlert(
    Enemy& enemy,
    const SDL_Rect& player,
    const std::vector<Wall>& walls,
    bool alarmActive,
    int& playerHP,
    float& injuredTimer,
    float dt)
{
    const float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;
    const float ARRIVE_DISTANCE = 3.0f;

    if (enemy.attackCooldown > 0.0f)
    {
        enemy.attackCooldown -= dt;
    }

    bool canSee = CanSeePlayer(enemy, player, walls);
    Vec2 playerCenter = GetPlayerCenter(player);

    if (canSee)
    {
        enemy.alertLostTimer = 0.0f;
        enemy.lastKnownPlayerPos = playerCenter;
        enemy.alertSearchBaseAngle = enemy.angle;

        FacePointSmooth(enemy, playerCenter, ENEMY_ALERT_AIM_TURN_SPEED, dt);

        if (IsPlayerInAttackRange(enemy, player) &&
            enemy.attackCooldown <= 0.0f)
        {
            ApplyEnemyGunHit(enemy, playerHP, injuredTimer);
        }

        return;
    }

    enemy.alertLostTimer += dt;

    if (DistanceSq(enemy.pos, enemy.lastKnownPlayerPos) >
        ARRIVE_DISTANCE * ARRIVE_DISTANCE)
    {
        bool reachedLastKnown =
            MoveEnemyToward(
                enemy,
                enemy.lastKnownPlayerPos,
                walls,
                dt,
                true,
                ENEMY_ALERT_AIM_TURN_SPEED);

        // 아직 정상적으로 추적 중이면 계속 마지막 목격 위치로 이동한다.
        if (!reachedLastKnown &&
            enemy.stuckTimer < ENEMY_ALERT_CHASE_STUCK_TIME &&
            enemy.alertLostTimer < ENEMY_ALERT_CHASE_TIMEOUT)
        {
            return;
        }

        // 도착했거나, 벽에 막혔거나, 너무 오래 추적했다면 현재 위치에서 강한 수색을 시작한다.
        BeginAlertLocalSearchAtCurrentPosition(enemy);
    }

    // 마지막 목격 지점에서 강한 수색
    const float sweepRange = 80.0f * DEG_TO_RAD;
    const float sweepSpeed = 1.2f;
    float sweep = sinf(enemy.alertLostTimer * sweepSpeed) * sweepRange;
    float desiredAngle = WrapAngle(enemy.alertSearchBaseAngle + sweep);
    RotateTowardAngle(enemy, desiredAngle, ENEMY_ALERT_SEARCH_TURN_SPEED, dt);

    if (enemy.alertLostTimer >= enemy.alertSearchDuration)
    {
        ChangeEnemyState(enemy, EnemyState::Search);
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
    float& injuredTimer,
    float dt)
{
    for (auto& enemy : enemies)
    {
        EnsureEnemyInitialized(enemy);
        enemy.stateTimer += dt;
        enemy.angle = WrapAngle(enemy.angle);

        if (enemy.state == EnemyState::Dead)
        {
            enemy.alerted = false;
            continue;
        }

        if (alarmActive)
        {
            enemy.heightenedAlert = true;
        }

        enemy.hearingEnergy *= expf(-2.0f * dt);

        bool canSeePlayerNow = CanSeePlayer(enemy, player, walls);

        if (canSeePlayerNow)
        {
            enemy.lastKnownPlayerPos = GetPlayerCenter(player);

            if (enemy.state != EnemyState::Alert)
            {
                ChangeEnemyState(enemy, EnemyState::Alert);
                enemy.attackCooldown = GetInitialAttackDelay(enemy, alarmActive);
            }

            alarmTriggered = true;
        }
        else
        {
            ConsumePendingNoise(enemy, alarmActive);
        }

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
            UpdateAlert(enemy, player, walls, alarmActive, playerHP, injuredTimer, dt);
            break;

        case EnemyState::Dead:
            break;
        }

        enemy.angle = WrapAngle(enemy.angle);

        enemy.alerted =
            enemy.heightenedAlert ||
            IsSuspiciousState(enemy.state) ||
            enemy.hearingEnergy >= enemy.hearingThreshold;
    }
}

// =====================================================
// 소음 연결
// =====================================================

void RequestEnemyInvestigate(Enemy& enemy, Vec2 targetPos)
{
    if (enemy.state == EnemyState::Dead || enemy.state == EnemyState::Alert)
    {
        return;
    }

    enemy.investigateTarget = targetPos;
    enemy.lastNoisePos = targetPos;
    enemy.hasPendingNoise = false;
    enemy.hearingEnergy = 0.0f;

    enemy.investigatePathCount = 0;
    enemy.investigatePathIndex = 0;
    enemy.needsInvestigatePathBuild = true;
    enemy.investigateRouteTimeout = 0.0f;

    ChangeEnemyState(enemy, EnemyState::Investigate);
}

void NotifyEnemyOfNoise(
    Enemy& enemy,
    Vec2 noisePos,
    float energy,
    bool alarmActive)
{
    if (enemy.state == EnemyState::Dead)
    {
        return;
    }

    if (alarmActive || enemy.state == EnemyState::Alert)
    {
        enemy.hasPendingNoise = false;
        return;
    }

    if (IsNoiseTaskState(enemy.state) &&
        IsSameNoiseAreaForCurrentTask(enemy, noisePos))
    {
        return;
    }

    enemy.lastNoisePos = noisePos;
    enemy.hearingEnergy += energy;

    if (enemy.hearingEnergy >= enemy.hearingThreshold)
    {
        enemy.pendingNoisePos = noisePos;
        enemy.pendingNoiseEnergy = enemy.hearingEnergy;
        enemy.hasPendingNoise = true;
    }
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
