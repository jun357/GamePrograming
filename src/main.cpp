#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <algorithm>
#include <random>
#include <string>

#include "Globals.h"
#include "Math.h"
#include "Enemy.h"
#include "Player.h"
#include "Sound.h"
#include "Wall.h"
#include "Camera.h"
#include "Stage.h"
#include "Tutorial.h"

// =====================================================
// 게임 상태
// =====================================================
enum GameState
{
    PLAYING,
    PAUSED,
    WIN,
    LOSE,
    CONTINUE_PROMPT
};

bool running = true;

bool showStageText = true;
Uint32 stageTextStart = 0;
int displayStage = 1;

const int PLAYER_MAX_HP = 100;
float injuredTimer = 0.0f;

const int PLAYER_HEALTH_BAR_HEIGHT = 18;
const int ENEMY_HEALTH_BAR_HEIGHT = 6;
const int ENEMY_HEALTH_BAR_GAP = 4;

// =====================================================
// 스테이지 / 이변 상태
// =====================================================
int stage = 1;
bool anomalyActive = false;

// 이변 종류 (레이어 방식)
bool anomalyWall = false;
bool anomalyEnemy = false;
bool anomalyFOV = false;
bool anomalySound = false;

bool mainStageAnomalyTriggered = false;
bool mainStageReturnHintVisible = false;
Uint32 mainStageReturnHintStart = 0;

static constexpr Uint32 MAIN_STAGE_RETURN_HINT_DURATION_MS = 5000;

std::string hudMessageText;
Uint32 hudMessageStart = 0;
Uint32 hudMessageDuration = 0;

// =====================================================
// 플레이어 시각 상태
// =====================================================

static float playerVisualFacingAngle = 1.57079632679f;
static float playerAnimTimer = 0.0f;
static int playerAnimFrame = 0;

static SDL_Scancode lastPlayerMoveFacingKey = SDL_SCANCODE_UNKNOWN;

static float playerFireVisualTimer = 0.0f;
static float playerFireVisualAngle = 0.0f;

static constexpr float PLAYER_FIRE_VISUAL_DURATION = 0.16f;
static constexpr int PLAYER_SPRITE_RENDER_SIZE = 56;
static constexpr int PLAYER_FIRE_RENDER_LONG_SIDE = 68;

// =====================================================
// 골
// =====================================================
SDL_Rect goalNormal  = {700, 500, 40, 40};
SDL_Rect goalAnomaly = {700, 100, 40, 40};

int currentMapWidth = WORLD_WIDTH;
int currentMapHeight = WORLD_HEIGHT;

// =====================================================
// 아이템 / 인벤토리
// =====================================================

enum class ItemType
{
    Bottle,
    Key,
    Target
};

struct Inventory
{
    bool hasBottle = false;
    bool hasKey = false;
    bool hasTarget = false;
};

struct WorldItem
{
    ItemType type;
    SDL_Rect rect;
    bool picked = false;
};

struct Equipment
{
    bool hasSuppressor = false;
};

static constexpr int SUPPRESSOR_PICKUP_WIDTH = 34;
static constexpr int SUPPRESSOR_PICKUP_HEIGHT = 10;
static constexpr float SUPPRESSOR_PICKUP_RANGE = 80.0f;
static constexpr float TUTORIAL_SUPPRESSOR_DROP_DISTANCE_FROM_PLAYER = 42.0f;

struct SuppressorPickup
{
    SDL_Rect rect =
    {
        260,
        118,
        SUPPRESSOR_PICKUP_WIDTH,
        SUPPRESSOR_PICKUP_HEIGHT
    };

    bool picked = false;
};

std::vector<StageEnemySpawnDef> stageEnemySpawns;
std::vector<StageItemSpawnDef> stageItemSpawns;
std::vector<StageTutorialTriggerDef> stageTutorialTriggers;
std::vector<StageTutorialBlockerDef> stageTutorialBlockers;
std::vector<StageInteractableDef> stageInteractables;
std::vector<StageEtcDef> stageEtcs;

static const char* GetItemLetter(ItemType type)
{
    switch (type)
    {
    case ItemType::Bottle:
        return "B";
    case ItemType::Key:
        return "K";
    case ItemType::Target:
        return "T";
    }

    return "?";
}

static bool InventoryHasItem(const Inventory& inventory, ItemType type)
{
    switch (type)
    {
    case ItemType::Bottle:
        return inventory.hasBottle;
    case ItemType::Key:
        return inventory.hasKey;
    case ItemType::Target:
        return inventory.hasTarget;
    }

    return false;
}

static bool AddToInventory(Inventory& inventory, ItemType type)
{
    if (InventoryHasItem(inventory, type))
    {
        return false;
    }

    switch (type)
    {
    case ItemType::Bottle:
        inventory.hasBottle = true;
        return true;

    case ItemType::Key:
        inventory.hasKey = true;
        return true;

    case ItemType::Target:
        inventory.hasTarget = true;
        return true;
    }

    return false;
}

static Vec2 GetRectCenter(const SDL_Rect& rect)
{
    return
    {
        rect.x + rect.w * 0.5f,
        rect.y + rect.h * 0.5f
    };
}

static float DistanceSqLocal(Vec2 a, Vec2 b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

static ItemType ItemTypeFromString(const std::string& text)
{
    if (text == "bottle" || text == "Bottle")
    {
        return ItemType::Bottle;
    }

    if (text == "key" || text == "Key")
    {
        return ItemType::Key;
    }

    if (text == "target" ||
        text == "document" ||
        text == "codebook" ||
        text == "Target" ||
        text == "Document" ||
        text == "Codebook")
        {
            return ItemType::Target;
        }
        return ItemType::Bottle;
}

void ResetWorldItems(std::vector<WorldItem>& items)
{
    items.clear();

    if (!stageItemSpawns.empty())
    {
        for (const auto& spawn : stageItemSpawns)
        {
            WorldItem item;
            item.type = ItemTypeFromString(spawn.itemType);
            item.rect = spawn.rect;
            item.picked = false;

            items.push_back(item);
        }

        return;
    }
}

bool TryPickupNearestItem(
    const SDL_Rect& player,
    std::vector<WorldItem>& items,
    Inventory& inventory,
    ItemType* pickedType = nullptr)
{
    static constexpr float PICKUP_RANGE = 96.0f;
    const float pickupRangeSq = PICKUP_RANGE * PICKUP_RANGE;

    Vec2 playerCenter = GetRectCenter(player);

    WorldItem* nearest = nullptr;
    float nearestDistSq = pickupRangeSq;

    for (auto& item : items)
    {
        if (item.picked)
        {
            continue;
        }

        Vec2 itemCenter = GetRectCenter(item.rect);
        float distSq = DistanceSqLocal(playerCenter, itemCenter);

        if (distSq <= nearestDistSq)
        {
            nearest = &item;
            nearestDistSq = distSq;
        }
    }

    if (!nearest)
    {
        return false;
    }

    if (!AddToInventory(inventory, nearest->type))
    {
        return false;
    }

    nearest->picked = true;

    if (pickedType)
    {
        *pickedType = nearest->type;
    }

    std::cout << "Picked up item: "
              << GetItemLetter(nearest->type)
              << std::endl;

    return true;
}

void ResetSuppressorPickup(
    SuppressorPickup& suppressor,
    const Equipment& equipment)
{
    if (stage == 1)
    {
        suppressor.rect = { -1000, -1000, 1, 1 };

        suppressor.picked = true;
        return;
    }

    for (const auto& interactable : stageInteractables)
    {
        if (interactable.id != "suppressor")
        {
            continue;
        }

        suppressor.rect = interactable.rect;
        suppressor.picked = equipment.hasSuppressor;
        return;
    }

    suppressor.rect = { -1000, -1000, 1, 1 };

    suppressor.picked = equipment.hasSuppressor;
}

static bool TryPickupSuppressor(
    const SDL_Rect& player,
    SuppressorPickup& suppressor,
    Equipment& equipment)
{
    if (equipment.hasSuppressor)
    {
        return false;
    }

    if (suppressor.picked)
    {
        return false;
    }

    if (suppressor.rect.w <= 0 || suppressor.rect.h <= 0)
    {
        return false;
    }

    bool touching = SDL_HasIntersection(
        &player,
        &suppressor.rect) == SDL_TRUE;

    Vec2 playerCenter = GetRectCenter(player);
    Vec2 suppressorCenter = GetRectCenter(suppressor.rect);

    float pickupRangeSq =
        SUPPRESSOR_PICKUP_RANGE *
        SUPPRESSOR_PICKUP_RANGE;

    bool inRange =
        DistanceSqLocal(playerCenter, suppressorCenter) <= pickupRangeSq;

    if (!touching && !inRange)
    {
        return false;
    }

    equipment.hasSuppressor = true;
    suppressor.picked = true;

    std::cout << "Suppressor equipped." << std::endl;

    return true;
}

static SDL_Rect MakeItemRectCentered(Vec2 center, int size = 24)
{
    return
    {
        static_cast<int>(center.x - size * 0.5f),
        static_cast<int>(center.y - size * 0.5f),
        size,
        size
    };
}

static SDL_Rect MakeCenteredWorldRectBySize(
    const SDL_Rect& base,
    int width,
    int height)
{
    Vec2 center =
        GetRectCenter(base);

    return
    {
        static_cast<int>(std::round(center.x - width * 0.5f)),
        static_cast<int>(std::round(center.y - height * 0.5f)),
        width,
        height
    };
}

static SDL_Rect MakeCenteredWorldRectFromSource(
    const SDL_Rect& base,
    const SDL_Rect& source,
    int longSide)
{
    if (source.w <= 0 || source.h <= 0)
    {
        return MakeCenteredWorldRectBySize(
            base,
            longSide,
            longSide);
    }

    int width = longSide;
    int height = longSide;

    if (source.w >= source.h)
    {
        width = longSide;
        height =
            static_cast<int>(
                std::round(
                    longSide *
                    static_cast<float>(source.h) /
                    static_cast<float>(source.w)));
    }
    else
    {
        height = longSide;
        width =
            static_cast<int>(
                std::round(
                    longSide *
                    static_cast<float>(source.w) /
                    static_cast<float>(source.h)));
    }

    return MakeCenteredWorldRectBySize(
        base,
        width,
        height);
}

static SDL_Rect MakeItemRectNearCorpseTowardPlayer(
    const SDL_Rect& player,
    const Enemy& corpse,
    int size = 24)
{
    static constexpr float DROP_OFFSET_FROM_CORPSE_CENTER = 52.0f;

    Vec2 playerCenter = GetRectCenter(player);
    Vec2 corpseCenter = GetRectCenter(corpse.rect);

    Vec2 playerToCorpse = corpseCenter - playerCenter;

    Vec2 dir = { 1.0f, 0.0f };
    if (LengthSq(playerToCorpse) > 0.000001f)
    {
        dir = Normalize(playerToCorpse);
    }

    Vec2 dropCenter = corpseCenter - dir * DROP_OFFSET_FROM_CORPSE_CENTER;

    return MakeItemRectCentered(dropCenter, size);
}

void ApplyOfficerDeathRewards(
    const SDL_Rect& player,
    std::vector<Enemy>& enemies,
    std::vector<WorldItem>& items,
    int& pistolAmmo,
    int magazineSize)
{
    (void)pistolAmmo;
    (void)magazineSize;

    for (auto& enemy : enemies)
    {
        if (enemy.kind != EnemyKind::Officer ||
            enemy.state != EnemyState::Dead ||
            enemy.officerRewardGiven)
        {
            continue;
        }

        if (stage != 1 && !enemy.dropKeyOnDeath)
        {
            enemy.officerRewardGiven = true;
            continue;
        }

        items.push_back(
            {
                ItemType::Key,
                MakeItemRectNearCorpseTowardPlayer(
                    player,
                    enemy,
                    24),
                    false
            });

        enemy.officerRewardGiven = true;

        std::cout << "Officer dropped key\n";
    }
}

// =====================================================
// 병 투척
// =====================================================

static constexpr float BOTTLE_RADIUS = 5.0f;
static constexpr float BOTTLE_RENDER_SIZE = 8.0f;

static constexpr float BOTTLE_MIN_THROW_DISTANCE = 70.0f;
static constexpr float BOTTLE_MAX_THROW_DISTANCE = 360.0f;
static constexpr float BOTTLE_EXPECTED_FLIGHT_TIME = 0.75f;

static constexpr float BOTTLE_INITIAL_HEIGHT = 6.0f;
static constexpr float BOTTLE_INITIAL_Z_VELOCITY = 210.0f;
static constexpr float BOTTLE_GRAVITY = 600.0f;
static constexpr float BOTTLE_LINEAR_DRAG = 0.25f;

static constexpr int BOTTLE_BREAK_SOUND_PARTICLE_COUNT = 120;
static constexpr float BOTTLE_BREAK_SOUND_SPEED = 343.0f;
static constexpr float BOTTLE_BREAK_SOUND_LOUDNESS = 3.0f;

struct BottleProjectile
{
    Vec2 pos = { 0.0f, 0.0f };
    Vec2 vel = { 0.0f, 0.0f };

    // 투척 시간이 생기도록 z축 높이를 별도로 둠
    float z = 0.0f;
    float vz = 0.0f;

    float radius = BOTTLE_RADIUS;
    float flightTime = 0.0f;
    bool alive = true;
    bool tutorialLureBottle = false;
};

static bool CircleIntersectsRect(
    Vec2 center,
    float radius,
    const SDL_Rect& rect)
{
    float closestX = ClampFloat(
        center.x,
        static_cast<float>(rect.x),
        static_cast<float>(rect.x + rect.w));

    float closestY = ClampFloat(
        center.y,
        static_cast<float>(rect.y),
        static_cast<float>(rect.y + rect.h));

    float dx = center.x - closestX;
    float dy = center.y - closestY;

    return dx * dx + dy * dy <= radius * radius;
}

static void BreakBottle(
    BottleProjectile& bottle,
    std::vector<SoundParticle>& soundParticles,
    std::vector<Vec2>* tutorialBottleBreaks = nullptr)
{
    if (!bottle.alive)
    {
        return;
    }

    EmitSound(
        soundParticles,
        bottle.pos,
        BOTTLE_BREAK_SOUND_PARTICLE_COUNT,
        BOTTLE_BREAK_SOUND_SPEED,
        BOTTLE_BREAK_SOUND_LOUDNESS,
        SoundKind::Bottle);

    if (bottle.tutorialLureBottle && tutorialBottleBreaks)
    {
        tutorialBottleBreaks->push_back(bottle.pos);
    }

    bottle.alive = false;
}

bool TryThrowBottle(
    const SDL_Rect& player,
    Vec2 targetWorld,
    Inventory& inventory,
    std::vector<BottleProjectile>& bottles,
    bool tutorialLureBottle = false)
{
    if (!inventory.hasBottle)
    {
        return false;
    }

    Vec2 playerCenter = GetRectCenter(player);
    Vec2 toTarget = targetWorld - playerCenter;

    float dist = Length(toTarget);
    if (dist < 1.0f)
    {
        return false;
    }

    Vec2 dir = Normalize(toTarget);

    float throwDistance = ClampFloat(
        dist,
        BOTTLE_MIN_THROW_DISTANCE,
        BOTTLE_MAX_THROW_DISTANCE);

    BottleProjectile bottle;
    bottle.pos = playerCenter + dir * 24.0f;
    bottle.vel = dir * (throwDistance / BOTTLE_EXPECTED_FLIGHT_TIME);
    bottle.z = BOTTLE_INITIAL_HEIGHT;
    bottle.vz = BOTTLE_INITIAL_Z_VELOCITY;
    bottle.radius = BOTTLE_RADIUS;
    bottle.flightTime = 0.0f;
    bottle.alive = true;
    bottle.tutorialLureBottle = tutorialLureBottle;

    bottles.push_back(bottle);

    // 사용하면 깨짐
    inventory.hasBottle = false;

    return true;
}

void UpdateBottleProjectiles(
    std::vector<BottleProjectile>& bottles,
    std::vector<SoundParticle>& soundParticles,
    const std::vector<Wall>& walls,
    float dt,
    std::vector<Vec2>* tutorialBottleBreaks = nullptr)
{
    for (auto& bottle : bottles)
    {
        if (!bottle.alive)
        {
            continue;
        }

        bottle.flightTime += dt;

        Vec2 previousPos = bottle.pos;

        // 수평 이동: 선형 drag
        bottle.vel += bottle.vel * (-BOTTLE_LINEAR_DRAG * dt);
        bottle.pos += bottle.vel * dt;

        // 수직 높이: 간단한 projectile motion
        bottle.vz -= BOTTLE_GRAVITY * dt;
        bottle.z += bottle.vz * dt;

        // 월드 경계 충돌
        if (bottle.pos.x < 0.0f)
        {
            bottle.pos.x = 0.0f;
            BreakBottle(bottle, soundParticles, tutorialBottleBreaks);
            continue;
        }

        if (bottle.pos.y < 0.0f)
        {
            bottle.pos.y = 0.0f;
            BreakBottle(bottle, soundParticles, tutorialBottleBreaks);
            continue;
        }

        if (bottle.pos.x > currentMapWidth)
        {
            bottle.pos.x = static_cast<float>(currentMapWidth);
            BreakBottle(bottle, soundParticles, tutorialBottleBreaks);
            continue;
        }

        if (bottle.pos.y > currentMapHeight)
        {
            bottle.pos.y = static_cast<float>(currentMapHeight);
            BreakBottle(bottle, soundParticles, tutorialBottleBreaks);
            continue;
        }

        // 벽 충돌: 병은 튕기지 않고 깨진다.
        for (const auto& wall : walls)
        {
            if (CircleIntersectsRect(bottle.pos, bottle.radius, wall.rect))
            {
                bottle.pos = previousPos;
                BreakBottle(bottle, soundParticles, tutorialBottleBreaks);
                break;
            }
        }

        if (!bottle.alive)
        {
            continue;
        }

        // 바닥에 떨어지면 깨진다.
        if (bottle.z <= 0.0f)
        {
            bottle.z = 0.0f;
            BreakBottle(bottle, soundParticles, tutorialBottleBreaks);
            continue;
        }
    }

    bottles.erase(
        std::remove_if(
            bottles.begin(),
            bottles.end(),
            [](const BottleProjectile& bottle)
            {
                return !bottle.alive;
            }),
        bottles.end());
}

void DrawBottleProjectiles(
    SDL_Renderer* renderer,
    const std::vector<BottleProjectile>& bottles,
    const Camera2D& camera)
{
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

    for (const auto& bottle : bottles)
    {
        if (!bottle.alive)
        {
            continue;
        }

        SDL_Rect worldRect =
        {
            static_cast<int>(bottle.pos.x - BOTTLE_RENDER_SIZE * 0.5f),
            static_cast<int>(bottle.pos.y - BOTTLE_RENDER_SIZE * 0.5f),
            static_cast<int>(BOTTLE_RENDER_SIZE),
            static_cast<int>(BOTTLE_RENDER_SIZE)
        };

        SDL_Rect screenRect = camera.WorldToScreenRect(worldRect);
        SDL_RenderFillRect(renderer, &screenRect);
    }
}

// =====================================================
// 벽 데이터
// =====================================================
std::vector<Wall> baseWalls;
std::vector<Wall> anomalyWalls;
std::vector<Wall> etcCollisionWalls;
std::vector<SDL_Rect> reachableFloorTiles;
TutorialController tutorial;

static void DrawTutorialBottleThrowZone(
    SDL_Renderer* renderer,
    const Camera2D& camera)
{
    SDL_Rect zoneWorld;
    if (!tutorial.GetBottleThrowZone(zoneWorld))
    {
        return;
    }

    SDL_Rect zoneScreen = camera.WorldToScreenRect(zoneWorld);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(renderer, 80, 180, 255, 70);
    SDL_RenderFillRect(renderer, &zoneScreen);

    SDL_SetRenderDrawColor(renderer, 120, 220, 255, 220);
    SDL_RenderDrawRect(renderer, &zoneScreen);
}

enum class TutorialBottleGuardScriptState
{
    Inactive,
    TurningToWall,
    TurningToDown,
    MovingDown,
    Arrived
};

static TutorialBottleGuardScriptState tutorialBottleGuardState =
    TutorialBottleGuardScriptState::Inactive;

static int tutorialBottleGuardIndex = -1;
static Vec2 tutorialBottleGuardTarget = { 0.0f, 0.0f };

static constexpr float TUTORIAL_GUARD_ANGLE_UP = -1.57079632679f;
static constexpr float TUTORIAL_GUARD_ANGLE_RIGHT = 0.0f;
static constexpr float TUTORIAL_GUARD_ANGLE_DOWN = 1.57079632679f;
static constexpr float TUTORIAL_GUARD_TWO_PI = 6.28318530718f;

static constexpr float TUTORIAL_BOTTLE_GUARD_TURN_SPEED =
    3.14159265359f;

static constexpr float TUTORIAL_BOTTLE_GUARD_MOVE_SPEED =
    72.0f;

static void ResetTutorialBottleGuardScript()
{
    tutorialBottleGuardState = TutorialBottleGuardScriptState::Inactive;
    tutorialBottleGuardIndex = -1;
    tutorialBottleGuardTarget = { 0.0f, 0.0f };
}

static float NormalizeTutorialAngle(float angle)
{
    while (angle <= -3.14159265359f)
    {
        angle += TUTORIAL_GUARD_TWO_PI;
    }

    while (angle > 3.14159265359f)
    {
        angle -= TUTORIAL_GUARD_TWO_PI;
    }

    return angle;
}

static bool StepAngleClockwiseForTutorial(
    float& angle,
    float target,
    float maxStep)
{
    float current = angle;

    while (target < current)
    {
        target += TUTORIAL_GUARD_TWO_PI;
    }

    float diff = target - current;

    if (diff <= maxStep)
    {
        angle = NormalizeTutorialAngle(target);
        return true;
    }

    angle = NormalizeTutorialAngle(current + maxStep);
    return false;
}

static void SetTutorialGuardFacing(Enemy& guard, float angle)
{
    guard.angle = angle;
    guard.homeAngle = angle;

    guard.useHeadSweep = false;
    guard.headSweepOffset = 0.0f;
    guard.headSweepDirection = 1;
}

static bool FindTutorialTriggerRectByPhase(
    const std::string& phase,
    SDL_Rect& outRect)
{
    for (const auto& trigger : stageTutorialTriggers)
    {
        if (trigger.phase == phase)
        {
            outRect = trigger.rect;
            return true;
        }
    }

    return false;
}

static int FindEnemyIndexBySpawnId(
    const std::vector<Enemy>& enemies,
    const std::string& spawnId)
{
    size_t count = std::min(stageEnemySpawns.size(), enemies.size());

    for (size_t i = 0; i < count; ++i)
    {
        if (stageEnemySpawns[i].id == spawnId)
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}

static constexpr int TUTORIAL_GUN_STOP_GAP = 32;
static constexpr int TUTORIAL_GUN_LANE_TOLERANCE = 56;

static bool IsSegmentBlockedByWallsLocal(
    Vec2 from,
    Vec2 to,
    const std::vector<Wall>& walls);

static bool CanTutorialGuardSeePlayer(
    const Enemy& guard,
    const SDL_Rect& player,
    const std::vector<Wall>& walls)
{
    Vec2 guardCenter = GetRectCenter(guard.rect);
    Vec2 playerCenter = GetRectCenter(player);

    Vec2 toPlayer = playerCenter - guardCenter;
    float distSq = LengthSq(toPlayer);

    if (distSq <= 0.000001f)
    {
        return true;
    }

    const float viewDistSq =
        guard.viewDist * guard.viewDist;

    if (distSq > viewDistSq)
    {
        return false;
    }

    Vec2 toPlayerDir = Normalize(toPlayer);
    Vec2 facing =
    {
        std::cos(guard.angle),
        std::sin(guard.angle)
    };

    float dot =
        facing.x * toPlayerDir.x +
        facing.y * toPlayerDir.y;

    const float halfFov = guard.fov * 0.5f;
    const float cosHalfFov = std::cos(halfFov);

    if (dot < cosHalfFov)
    {
        return false;
    }

    if (IsSegmentBlockedByWallsLocal(
            guardCenter,
            playerCenter,
            walls))
    {
        return false;
    }

    return true;
}

static void SetEnemyCenterForTutorial(Enemy& enemy, Vec2 center)
{
    enemy.pos = center;
    enemy.rect.x = static_cast<int>(
        std::round(center.x - enemy.rect.w * 0.5f));
    enemy.rect.y = static_cast<int>(
        std::round(center.y - enemy.rect.h * 0.5f));
}

static void StartTutorialBottleGuardLure(
    std::vector<Enemy>& enemies)
{
    if (stage != 1)
    {
        return;
    }

    if (tutorialBottleGuardState != TutorialBottleGuardScriptState::Inactive)
    {
        return;
    }

    int index = FindEnemyIndexBySpawnId(enemies, "guard_bottle");
    if (index < 0 || index >= static_cast<int>(enemies.size()))
    {
        std::cout << "guard_bottle spawn not found." << std::endl;
        return;
    }

    Enemy& guard = enemies[index];

    if (guard.state == EnemyState::Dead)
    {
        return;
    }

    SDL_Rect targetRect;
    if (FindTutorialTriggerRectByPhase("bottle_guard_target", targetRect))
    {
        Vec2 targetCenter = GetRectCenter(targetRect);

        tutorialBottleGuardTarget =
        {
            guard.pos.x,
            targetCenter.y
        };
    }
    else
    {
        tutorialBottleGuardTarget =
        {
            guard.pos.x,
            guard.pos.y + 160.0f
        };
    }

    if (tutorialBottleGuardTarget.y <= guard.pos.y + 1.0f)
    {
        tutorialBottleGuardTarget.y = guard.pos.y + 160.0f;
    }

    tutorialBottleGuardIndex = index;
    tutorialBottleGuardState = TutorialBottleGuardScriptState::TurningToWall;

    guard.state = EnemyState::Patrol;
    guard.alerted = false;
    guard.hasPendingNoise = false;
    guard.pendingNoiseEnergy = 0.0f;
    guard.hearingEnergy = 0.0f;

    SetTutorialGuardFacing(guard, TUTORIAL_GUARD_ANGLE_UP);
}

static bool tutorialCabinetAlertActive = false;
static int tutorialCabinetAlertIndex = -1;

static constexpr float TUTORIAL_CABINET_ALERT_SPEED = 110.0f;
static constexpr float TUTORIAL_CABINET_ATTACK_RANGE = 240.0f;
static constexpr int TUTORIAL_CABINET_ATTACK_DAMAGE = 20;

static void ResetTutorialCabinetAlertScript()
{
    tutorialCabinetAlertActive = false;
    tutorialCabinetAlertIndex = -1;
}

static void StartTutorialCabinetAlert(
    std::vector<Enemy>& enemies,
    const SDL_Rect& player)
{
    int index = FindEnemyIndexBySpawnId(enemies, "guard_codebook");

    if (index < 0 ||
        index >= static_cast<int>(enemies.size()) ||
        enemies[index].state == EnemyState::Dead ||
        enemies[index].bodyHidden)
    {
        // guard_cabinet이 이미 은닉된 경우를 대비한 fallback
        index = FindEnemyIndexBySpawnId(enemies, "guard_cabinet");
    }

    if (index < 0 || index >= static_cast<int>(enemies.size()))
    {
        std::cout << "No cabinet alert guard found." << std::endl;
        return;
    }

    Enemy& guard = enemies[index];

    if (guard.state == EnemyState::Dead)
    {
        return;
    }

    tutorialCabinetAlertIndex = index;
    tutorialCabinetAlertActive = true;

    guard.state = EnemyState::Alert;
    guard.alerted = true;
    guard.heightenedAlert = true;
    guard.lastKnownPlayerPos = GetRectCenter(player);
    guard.stateTimer = 0.0f;
    guard.alertLostTimer = 0.0f;
    guard.attackCooldown = 0.0f;

    std::cout << "Cabinet area alert guard started." << std::endl;
}

static void UpdateTutorialCabinetAlert(
    std::vector<Enemy>& enemies,
    const SDL_Rect& player,
    int& playerHP,
    float& injuredTimer,
    float dt)
{
    if (!tutorialCabinetAlertActive)
    {
        return;
    }

    if (tutorialCabinetAlertIndex < 0 ||
        tutorialCabinetAlertIndex >= static_cast<int>(enemies.size()))
    {
        tutorialCabinetAlertActive = false;
        return;
    }

    Enemy& guard = enemies[tutorialCabinetAlertIndex];

    if (guard.state == EnemyState::Dead || guard.bodyHidden)
    {
        tutorialCabinetAlertActive = false;
        return;
    }

    Vec2 guardCenter = GetRectCenter(guard.rect);
    Vec2 playerCenter = GetRectCenter(player);
    Vec2 toPlayer = playerCenter - guardCenter;

    float distSq = LengthSq(toPlayer);

    if (distSq > 0.000001f)
    {
        Vec2 dir = Normalize(toPlayer);
        guard.angle = std::atan2(dir.y, dir.x);
        guard.homeAngle = guard.angle;

        float distance = std::sqrt(distSq);
        float step = TUTORIAL_CABINET_ALERT_SPEED * dt;

        if (distance > TUTORIAL_CABINET_ATTACK_RANGE)
        {
            SetEnemyCenterForTutorial(
                guard,
                guardCenter + dir * step);
        }
    }

    guard.state = EnemyState::Alert;
    guard.alerted = true;
    guard.heightenedAlert = true;
    guard.lastKnownPlayerPos = playerCenter;

    if (guard.attackCooldown > 0.0f)
    {
        guard.attackCooldown -= dt;
    }

    float attackRangeSq =
        TUTORIAL_CABINET_ATTACK_RANGE *
        TUTORIAL_CABINET_ATTACK_RANGE;

    if (DistanceSqLocal(GetRectCenter(guard.rect), playerCenter) <= attackRangeSq &&
        guard.attackCooldown <= 0.0f)
    {
        playerHP -= TUTORIAL_CABINET_ATTACK_DAMAGE;
        injuredTimer = 0.4f;
        guard.attackCooldown = 0.8f;

        std::cout << "Cabinet alert guard hit player." << std::endl;
    }
}

static bool tutorialEscapeGuardMoveActive = false;

static void ResetTutorialEscapeGuardScript()
{
    tutorialEscapeGuardMoveActive = false;
}

static bool TriggerMatchesPhaseOrName(
    const StageTutorialTriggerDef& trigger,
    const std::string& key)
{
    if (trigger.phase == key)
    {
        return true;
    }

    if (trigger.id.find(key) != std::string::npos)
    {
        return true;
    }

    return false;
}

static bool HasPlayerEnteredTutorialTrigger(
    const SDL_Rect& player,
    const std::string& key)
{
    for (const auto& trigger : stageTutorialTriggers)
    {
        if (!TriggerMatchesPhaseOrName(trigger, key))
        {
            continue;
        }

        if (SDL_HasIntersection(&player, &trigger.rect) == SDL_TRUE)
        {
            return true;
        }
    }

    return false;
}

static void StartTutorialEscapeGuardMove(
    std::vector<Enemy>& enemies)
{
    if (tutorialEscapeGuardMoveActive)
    {
        return;
    }

    tutorialEscapeGuardMoveActive = true;

    int startedCount = 0;

    const size_t count =
        std::min(stageEnemySpawns.size(), enemies.size());

    for (size_t i = 0; i < count; ++i)
    {
        const std::string& id = stageEnemySpawns[i].id;

        if (id.find("guard_alert_") != 0)
        {
            continue;
        }

        Enemy& guard = enemies[i];

        if (guard.state == EnemyState::Dead)
        {
            continue;
        }

        guard.state = EnemyState::Alert;
        guard.alerted = true;
        guard.heightenedAlert = true;
        guard.angle = 0.0f;
        guard.homeAngle = 0.0f;

        ++startedCount;
    }

    std::cout
        << "Escape guard movement started: "
        << startedCount
        << " guard(s)."
        << std::endl;
}

static constexpr float ESCAPE_GUARD_CHASE_SPEED = 96.0f;
static constexpr float ESCAPE_GUARD_ATTACK_RANGE = 58.0f;
static constexpr int ESCAPE_GUARD_ATTACK_DAMAGE = 8;
static constexpr float ESCAPE_GUARD_ATTACK_COOLDOWN = 0.8f;

static bool CanTutorialEnemyOccupyCenter(
    const Enemy& enemy,
    Vec2 center,
    const std::vector<Wall>& walls)
{
    SDL_Rect rect = enemy.rect;

    rect.x = static_cast<int>(
        std::round(center.x - rect.w * 0.5f));
    rect.y = static_cast<int>(
        std::round(center.y - rect.h * 0.5f));

    if (rect.x < 0 ||
        rect.y < 0 ||
        rect.x + rect.w > currentMapWidth ||
        rect.y + rect.h > currentMapHeight)
    {
        return false;
    }

    for (const auto& wall : walls)
    {
        if (SDL_HasIntersection(&rect, &wall.rect))
        {
            return false;
        }
    }

    return true;
}

static void MoveTutorialEnemyWithCollision(
    Enemy& enemy,
    Vec2 delta,
    const std::vector<Wall>& walls)
{
    Vec2 center = GetRectCenter(enemy.rect);

    Vec2 next = center + delta;
    if (CanTutorialEnemyOccupyCenter(enemy, next, walls))
    {
        SetEnemyCenterForTutorial(enemy, next);
        return;
    }

    Vec2 nextX = { center.x + delta.x, center.y };
    if (CanTutorialEnemyOccupyCenter(enemy, nextX, walls))
    {
        SetEnemyCenterForTutorial(enemy, nextX);
        center = GetRectCenter(enemy.rect);
    }

    Vec2 nextY = { center.x, center.y + delta.y };
    if (CanTutorialEnemyOccupyCenter(enemy, nextY, walls))
    {
        SetEnemyCenterForTutorial(enemy, nextY);
    }
}

static void UpdateTutorialEscapeGuardMove(
    std::vector<Enemy>& enemies,
    const SDL_Rect& player,
    const std::vector<Wall>& walls,
    int& playerHP,
    float& injuredTimer,
    float dt)
{
    if (!tutorialEscapeGuardMoveActive)
    {
        return;
    }

    if (playerHP <= 0)
    {
        return;
    }

    const size_t count =
        std::min(stageEnemySpawns.size(), enemies.size());

    Vec2 playerCenter = GetRectCenter(player);

    for (size_t i = 0; i < count; ++i)
    {
        const std::string& id = stageEnemySpawns[i].id;

        if (id.find("guard_alert_") != 0)
        {
            continue;
        }

        Enemy& guard = enemies[i];

        if (guard.state == EnemyState::Dead ||
            guard.bodyHidden)
        {
            continue;
        }

        Vec2 guardCenter = GetRectCenter(guard.rect);
        Vec2 toPlayer = playerCenter - guardCenter;

        float distSq = LengthSq(toPlayer);

        guard.state = EnemyState::Alert;
        guard.alerted = true;
        guard.heightenedAlert = true;
        guard.lastKnownPlayerPos = playerCenter;

        if (guard.attackCooldown > 0.0f)
        {
            guard.attackCooldown -= dt;
        }

        if (distSq <= 0.000001f)
        {
            continue;
        }

        Vec2 dir = Normalize(toPlayer);

        guard.angle = std::atan2(dir.y, dir.x);
        guard.homeAngle = guard.angle;

        const float attackRangeSq =
            ESCAPE_GUARD_ATTACK_RANGE *
            ESCAPE_GUARD_ATTACK_RANGE;

        if (distSq > attackRangeSq)
        {
            Vec2 delta =
                dir * (ESCAPE_GUARD_CHASE_SPEED * dt);

            MoveTutorialEnemyWithCollision(
                guard,
                delta,
                walls);
        }
        else if (guard.attackCooldown <= 0.0f)
        {
            playerHP -= ESCAPE_GUARD_ATTACK_DAMAGE;
            injuredTimer = 0.4f;
            guard.attackCooldown = ESCAPE_GUARD_ATTACK_COOLDOWN;

            std::cout
                << "Escape guard hit player. HP="
                << playerHP
                << std::endl;
        }
    }
}

static bool UpdateTutorialBottleGuardLure(
    std::vector<Enemy>& enemies,
    float dt)
{
    if (tutorialBottleGuardState == TutorialBottleGuardScriptState::Inactive ||
        tutorialBottleGuardState == TutorialBottleGuardScriptState::Arrived)
    {
        return false;
    }

    if (tutorialBottleGuardIndex < 0 ||
        tutorialBottleGuardIndex >= static_cast<int>(enemies.size()))
    {
        tutorialBottleGuardState = TutorialBottleGuardScriptState::Inactive;
        return false;
    }

    Enemy& guard = enemies[tutorialBottleGuardIndex];

    if (guard.state == EnemyState::Dead)
    {
        tutorialBottleGuardState = TutorialBottleGuardScriptState::Inactive;
        return false;
    }

    guard.alerted = false;
    guard.hasPendingNoise = false;
    guard.pendingNoiseEnergy = 0.0f;
    guard.hearingEnergy = 0.0f;

    const float angleStep =
        TUTORIAL_BOTTLE_GUARD_TURN_SPEED * dt;

    if (tutorialBottleGuardState ==
        TutorialBottleGuardScriptState::TurningToWall)
    {
        bool reachedWallSide = StepAngleClockwiseForTutorial(
            guard.angle,
            TUTORIAL_GUARD_ANGLE_RIGHT,
            angleStep);

        SetTutorialGuardFacing(guard, guard.angle);

        if (reachedWallSide)
        {
            SetTutorialGuardFacing(guard, TUTORIAL_GUARD_ANGLE_RIGHT);
            tutorialBottleGuardState =
                TutorialBottleGuardScriptState::TurningToDown;
        }

        return false;
    }

    if (tutorialBottleGuardState ==
        TutorialBottleGuardScriptState::TurningToDown)
    {
        bool reachedDown = StepAngleClockwiseForTutorial(
            guard.angle,
            TUTORIAL_GUARD_ANGLE_DOWN,
            angleStep);

        SetTutorialGuardFacing(guard, guard.angle);

        if (reachedDown)
        {
            SetTutorialGuardFacing(guard, TUTORIAL_GUARD_ANGLE_DOWN);
            tutorialBottleGuardState =
                TutorialBottleGuardScriptState::MovingDown;
        }

        return false;
    }

    if (tutorialBottleGuardState ==
        TutorialBottleGuardScriptState::MovingDown)
    {
        SetTutorialGuardFacing(guard, TUTORIAL_GUARD_ANGLE_DOWN);

        float dy = tutorialBottleGuardTarget.y - guard.pos.y;

        if (std::fabs(dy) <= 1.0f)
        {
            SetEnemyCenterForTutorial(guard, tutorialBottleGuardTarget);
            SetTutorialGuardFacing(guard, TUTORIAL_GUARD_ANGLE_DOWN);

            tutorialBottleGuardState =
                TutorialBottleGuardScriptState::Arrived;

            return true;
        }

        float step = TUTORIAL_BOTTLE_GUARD_MOVE_SPEED * dt;

        if (step >= std::fabs(dy))
        {
            SetEnemyCenterForTutorial(guard, tutorialBottleGuardTarget);
            SetTutorialGuardFacing(guard, TUTORIAL_GUARD_ANGLE_DOWN);

            tutorialBottleGuardState =
                TutorialBottleGuardScriptState::Arrived;

            return true;
        }

        float directionY = (dy > 0.0f) ? 1.0f : -1.0f;

        Vec2 nextCenter =
        {
            guard.pos.x,
            guard.pos.y + directionY * step
        };

        SetEnemyCenterForTutorial(guard, nextCenter);
        SetTutorialGuardFacing(guard, TUTORIAL_GUARD_ANGLE_DOWN);

        return false;
    }

    return false;
}

enum class TutorialGunScriptState
{
    Inactive,
    MovingIn,
    WaitingFirstShot,
    WaitingSuppressorPickup,
    WaitingSuppressedShot,
    Done
};

static TutorialGunScriptState tutorialGunScriptState =
    TutorialGunScriptState::Inactive;

static int tutorialGunGuardIndex = -1;
static int tutorialSuppressorGuardIndex = -1;
static bool tutorialSuppressorDropped = false;
static bool tutorialKeyDropped = false;

static constexpr float TUTORIAL_GUN_GUARD_SPEED = 96.0f;
static constexpr float TUTORIAL_GUN_FACE_LEFT = 3.14159265359f;

static void ResetTutorialGunScript()
{
    tutorialGunScriptState = TutorialGunScriptState::Inactive;
    tutorialGunGuardIndex = -1;
    tutorialSuppressorGuardIndex = -1;
    tutorialSuppressorDropped = false;
    tutorialKeyDropped = false;
}

static void StartTutorialGunScript(
    std::vector<Enemy>& enemies)
{
    if (stage != 1)
    {
        return;
    }

    if (tutorialGunScriptState != TutorialGunScriptState::Inactive)
    {
        return;
    }

    tutorialGunGuardIndex =
        FindEnemyIndexBySpawnId(enemies, "guard_gun");

    tutorialSuppressorGuardIndex =
        FindEnemyIndexBySpawnId(enemies, "guard_suppressor");

    if (tutorialGunGuardIndex < 0 ||
        tutorialSuppressorGuardIndex < 0)
    {
        std::cout << "Gun tutorial guards not found." << std::endl;
        return;
    }

    enemies[tutorialGunGuardIndex].angle = TUTORIAL_GUN_FACE_LEFT;
    enemies[tutorialGunGuardIndex].homeAngle = TUTORIAL_GUN_FACE_LEFT;
    enemies[tutorialGunGuardIndex].useHeadSweep = false;
    enemies[tutorialGunGuardIndex].headSweepOffset = 0.0f;

    enemies[tutorialSuppressorGuardIndex].angle = TUTORIAL_GUN_FACE_LEFT;
    enemies[tutorialSuppressorGuardIndex].homeAngle = TUTORIAL_GUN_FACE_LEFT;
    enemies[tutorialSuppressorGuardIndex].useHeadSweep = false;
    enemies[tutorialSuppressorGuardIndex].headSweepOffset = 0.0f;

    tutorialGunScriptState = TutorialGunScriptState::MovingIn;

    std::cout << "Gun tutorial started." << std::endl;
}

static bool UpdateTutorialGunScript(
    std::vector<Enemy>& enemies,
    const SDL_Rect& player,
    const std::vector<Wall>& walls,
    float dt)
{
    if (tutorialGunScriptState != TutorialGunScriptState::MovingIn)
    {
        return false;
    }

    if (tutorialGunGuardIndex < 0 ||
        tutorialGunGuardIndex >= static_cast<int>(enemies.size()) ||
        tutorialSuppressorGuardIndex < 0 ||
        tutorialSuppressorGuardIndex >= static_cast<int>(enemies.size()))
    {
        tutorialGunScriptState = TutorialGunScriptState::Inactive;
        return false;
    }

    Enemy& gunGuard = enemies[tutorialGunGuardIndex];
    Enemy& suppressorGuard = enemies[tutorialSuppressorGuardIndex];

    if (gunGuard.state == EnemyState::Dead)
    {
        tutorialGunScriptState = TutorialGunScriptState::WaitingFirstShot;
        return false;
    }

    if (CanTutorialGuardSeePlayer(gunGuard, player, walls))
    {
        gunGuard.angle = TUTORIAL_GUN_FACE_LEFT;
        gunGuard.homeAngle = TUTORIAL_GUN_FACE_LEFT;
        
        suppressorGuard.angle = TUTORIAL_GUN_FACE_LEFT;
        suppressorGuard.homeAngle = TUTORIAL_GUN_FACE_LEFT;
        
        tutorialGunScriptState = TutorialGunScriptState::WaitingFirstShot;
        
        std::cout
            << "Gun tutorial paused."
            << std::endl;
            
        return true;
    }
    
    const float step = TUTORIAL_GUN_GUARD_SPEED * dt;

    Vec2 gunNext =
    {
        gunGuard.pos.x - step,
        gunGuard.pos.y
    };

    Vec2 suppressorNext =
    {
        suppressorGuard.pos.x - step,
        suppressorGuard.pos.y
    };

    SetEnemyCenterForTutorial(gunGuard, gunNext);
    SetEnemyCenterForTutorial(suppressorGuard, suppressorNext);

    gunGuard.angle = TUTORIAL_GUN_FACE_LEFT;
    gunGuard.homeAngle = TUTORIAL_GUN_FACE_LEFT;

    suppressorGuard.angle = TUTORIAL_GUN_FACE_LEFT;
    suppressorGuard.homeAngle = TUTORIAL_GUN_FACE_LEFT;

    return false;
}

static bool IsEnemyDeadBySpawnId(
    const std::vector<Enemy>& enemies,
    const std::string& spawnId)
{
    int index = FindEnemyIndexBySpawnId(enemies, spawnId);

    if (index < 0 || index >= static_cast<int>(enemies.size()))
    {
        return false;
    }

    return enemies[index].state == EnemyState::Dead;
}

static void DropTutorialSuppressorFromEnemy(
    const SDL_Rect& player,
    const std::vector<Enemy>& enemies,
    SuppressorPickup& suppressor,
    const std::string& spawnId)
{
    (void)player;

    if (tutorialSuppressorDropped)
    {
        return;
    }

    int index = FindEnemyIndexBySpawnId(enemies, spawnId);

    if (index < 0 || index >= static_cast<int>(enemies.size()))
    {
        return;
    }

    Vec2 corpseCenter = GetRectCenter(enemies[index].rect);

    // 시체와 완전히 겹치지 않게 살짝 왼쪽에 드롭.
    // guard_gun은 플레이어 오른쪽에서 왼쪽을 바라보므로,
    // 왼쪽 offset이 플레이어가 접근하기도 쉽고 시각적으로도 덜 가려진다.
    Vec2 dropCenter =
    {
        corpseCenter.x - 42.0f,
        corpseCenter.y
    };

    suppressor.rect =
    {
        static_cast<int>(
            dropCenter.x - SUPPRESSOR_PICKUP_WIDTH * 0.5f),
        static_cast<int>(
            dropCenter.y - SUPPRESSOR_PICKUP_HEIGHT * 0.5f),
        SUPPRESSOR_PICKUP_WIDTH,
        SUPPRESSOR_PICKUP_HEIGHT
    };

    suppressor.picked = false;
    tutorialSuppressorDropped = true;

    tutorialGunScriptState =
        TutorialGunScriptState::WaitingSuppressorPickup;

    std::cout
        << "guard_gun dropped suppressor. auto pickup target=("
        << suppressor.rect.x
        << ", "
        << suppressor.rect.y
        << ")"
        << std::endl;
}

static void DropTutorialKeyFromEnemy(
    const SDL_Rect& player,
    const std::vector<Enemy>& enemies,
    std::vector<WorldItem>& items,
    const std::string& spawnId)
{
    if (tutorialKeyDropped)
    {
        return;
    }

    int index = FindEnemyIndexBySpawnId(enemies, spawnId);

    if (index < 0 || index >= static_cast<int>(enemies.size()))
    {
        return;
    }

    Vec2 dropCenter = GetRectCenter(enemies[index].rect);

    items.push_back(
    {
        ItemType::Key,
        MakeItemRectNearCorpseTowardPlayer(player, enemies[index], 24),
        false
    });

    tutorialKeyDropped = true;
    tutorialGunScriptState = TutorialGunScriptState::Done;

    std::cout << "guard_suppressor dropped key." << std::endl;
}

static void ApplyStageMapSetupToGlobals(const StageMapSetup& setup, SDL_Rect& player)
{
    player = setup.playerStart;

    baseWalls = setup.baseWalls;
    anomalyWalls = setup.anomalyWalls;
    etcCollisionWalls = setup.etcCollisionWalls;

    goalNormal = setup.goalNormal;
    goalAnomaly = setup.goalAnomaly;

    currentMapWidth = setup.mapWidth;
    currentMapHeight = setup.mapHeight;

    stageEnemySpawns = setup.enemySpawns;
    stageItemSpawns = setup.itemSpawns;
    stageTutorialTriggers = setup.tutorialTriggers;
    stageTutorialBlockers = setup.tutorialBlockers;
    stageInteractables = setup.interactables;
    stageEtcs = setup.etcs;
}

// =====================================================
// JSON 적 생성
// =====================================================

static float AngleFromDirectionString(const std::string& dir)
{
    if (dir == "right")
    {
        return 0.0f;
    }

    if (dir == "down")
    {
        return 1.57079632679f;
    }

    if (dir == "left")
    {
        return 3.14159265359f;
    }

    if (dir == "up")
    {
        return -1.57079632679f;
    }

    return 0.0f;
}

static void AddEnemyFromStageSpawn(
    std::vector<Enemy>& enemies,
    const StageEnemySpawnDef& spawn)
{
    Vec2 center = GetRectCenter(spawn.rect);

    if (spawn.kind == "officer")
    {
        AddOfficer(enemies, center);
    }
    else if (spawn.kind == "patrol")
    {
        AddPatrolGuard(enemies, center, {});
    }
    else
    {
        AddSentry(enemies, center);
    }

    if (enemies.empty())
    {
        return;
    }

    Enemy& enemy = enemies.back();

    enemy.rect = spawn.rect;
    enemy.pos = GetRectCenter(enemy.rect);
    enemy.angle = AngleFromDirectionString(spawn.dir);
    enemy.homeAngle = enemy.angle;
    enemy.alerted = false;
    enemy.hasPendingNoise = false;
    enemy.pendingNoiseEnergy = 0.0f;
    enemy.hearingEnergy = 0.0f;
    enemy.dropKeyOnDeath =
    enemy.kind == EnemyKind::Officer && spawn.dropKey;
}

// =====================================================
// 잠긴 문
// =====================================================
struct LockedDoor
{
    Wall wall;
    bool opened = false;
    bool checkpointDoor = true;
};

std::vector<LockedDoor> lockedDoors;

static constexpr float LOCKED_DOOR_INTERACT_RANGE = 56.0f;

void PrepareLockedDoorSoundWalls(std::vector<LockedDoor>& doors)
{
    std::vector<Wall> doorWalls;
    doorWalls.reserve(doors.size());

    for (const auto& door : doors)
    {
        doorWalls.push_back(door.wall);
    }

    PrepareSoundWalls(doorWalls);

    for (size_t i = 0; i < doors.size(); ++i)
    {
        doors[i].wall = doorWalls[i];
    }
}

void ResetLockedDoors()
{
    lockedDoors.clear();

    for (const auto& interactable : stageInteractables)
    {
        if (interactable.id != "locked_door")
        {
            continue;
        }

        lockedDoors.push_back(
        {
            Wall(interactable.rect),
            false,
            true
        });
    }
    PrepareLockedDoorSoundWalls(lockedDoors);
}

bool TryOpenNearestLockedDoor(
    const SDL_Rect& player,
    std::vector<LockedDoor>& doors,
    Inventory& inventory,
    SDL_Rect* openedDoorRect = nullptr)
{
    Vec2 playerCenter = GetRectCenter(player);

    LockedDoor* nearest = nullptr;
    float nearestDistSq =
        LOCKED_DOOR_INTERACT_RANGE * LOCKED_DOOR_INTERACT_RANGE;

    for (auto& door : doors)
    {
        if (door.opened)
        {
            continue;
        }

        Vec2 doorCenter = GetRectCenter(door.wall.rect);
        float distSq = DistanceSqLocal(playerCenter, doorCenter);

        if (distSq <= nearestDistSq)
        {
            nearest = &door;
            nearestDistSq = distSq;
        }
    }

    if (!nearest)
    {
        return false;
    }

    if (!inventory.hasKey)
    {
        std::cout << "Locked door: key required\n";
        return false;
    }

    nearest->opened = true;
    
    if (openedDoorRect)
    {
        *openedDoorRect = nearest->wall.rect;
    }
    
    inventory.hasKey = false;
    std::cout << "Unlocked door\n";
    return true;
}

static bool IsTutorialCodebookDoorRect(const SDL_Rect& rect)
{
    return rect.w <= 16 &&
           rect.h >= 64 &&
           rect.x >= 5400 &&
           rect.y >= 560 &&
           rect.y <= 720;
}

// =====================================================
// 벽 합성 (base + anomaly)
// =====================================================
std::vector<Wall> GetActiveWalls()
{
    std::vector<Wall> result = baseWalls;

    if (anomalyWall)
    {
        result.insert(result.end(), anomalyWalls.begin(), anomalyWalls.end());
    }

    for (const auto& door : lockedDoors)
    {
        if (!door.opened)
        {
            result.push_back(door.wall);
        }
    }

    return result;
}

std::vector<Wall> GetActorCollisionWalls(
    const std::vector<Wall>& activeWalls)
{
    std::vector<Wall> result;
    result.reserve(activeWalls.size() + etcCollisionWalls.size());

    result.insert(
        result.end(),
        activeWalls.begin(),
        activeWalls.end());

    result.insert(
        result.end(),
        etcCollisionWalls.begin(),
        etcCollisionWalls.end());

    return result;
}

static constexpr int FLOOR_TILE_SIZE = 20;

static bool PointInsideRect(
    Vec2 point,
    const SDL_Rect& rect)
{
    return
        point.x >= rect.x &&
        point.x < rect.x + rect.w &&
        point.y >= rect.y &&
        point.y < rect.y + rect.h;
}

static bool PointInsideAnyWall(
    Vec2 point,
    const std::vector<Wall>& walls)
{
    for (const auto& wall : walls)
    {
        if (PointInsideRect(point, wall.rect))
        {
            return true;
        }
    }

    return false;
}

static void RebuildReachableFloorTiles(
    const SDL_Rect& playerStart)
{
    reachableFloorTiles.clear();

    if (currentMapWidth <= 0 || currentMapHeight <= 0)
    {
        return;
    }

    const int cols =
        (currentMapWidth + FLOOR_TILE_SIZE - 1) / FLOOR_TILE_SIZE;

    const int rows =
        (currentMapHeight + FLOOR_TILE_SIZE - 1) / FLOOR_TILE_SIZE;

    if (cols <= 0 || rows <= 0)
    {
        return;
    }

    std::vector<unsigned char> blocked(cols * rows, 0);
    std::vector<unsigned char> visited(cols * rows, 0);
    std::vector<int> queue;
    queue.reserve(cols * rows);

    auto IndexOf =
        [cols](int x, int y)
        {
            return y * cols + x;
        };

    auto TryAddCell =
        [&](int sx, int sy)
        {
            if (sx < 0 || sy < 0 || sx >= cols || sy >= rows)
            {
                return false;
            }

            int index = IndexOf(sx, sy);

            if (blocked[index] || visited[index])
            {
                return false;
            }

            visited[index] = 1;
            queue.push_back(index);

            return true;
        };
    
    auto AddSeed =
    [&](const SDL_Rect& rect)
    {
        Vec2 center = GetRectCenter(rect);
        int sx = static_cast<int>(center.x) / FLOOR_TILE_SIZE;
        int sy = static_cast<int>(center.y) / FLOOR_TILE_SIZE;
        if (TryAddCell(sx, sy))
        {
            return;
        }

        static constexpr int SEED_SEARCH_RADIUS = 4;

        for (int radius = 1; radius <= SEED_SEARCH_RADIUS; ++radius)
        {
            for (int dy = -radius; dy <= radius; ++dy)
            {
                for (int dx = -radius; dx <= radius; ++dx)
                {
                    if (std::abs(dx) != radius && std::abs(dy) != radius)
                    {
                        continue;
                    }

                    if (TryAddCell(sx + dx, sy + dy))
                    {
                        return;
                    }
                }
            }
        }
    };

    for (int y = 0; y < rows; ++y)
    {
        for (int x = 0; x < cols; ++x)
        {
            SDL_Rect tile =
            {
                x * FLOOR_TILE_SIZE,
                y * FLOOR_TILE_SIZE,
                FLOOR_TILE_SIZE,
                FLOOR_TILE_SIZE
            };

            Vec2 tileCenter =
            {
                tile.x + tile.w * 0.5f,
                tile.y + tile.h * 0.5f
            };

            if (PointInsideAnyWall(tileCenter, baseWalls))
            {
                blocked[IndexOf(x, y)] = 1;
            }
        }
    }

    AddSeed(playerStart);
    AddSeed(goalNormal);

    for (const auto& spawn : stageItemSpawns)
    {
        AddSeed(spawn.rect);
    }

    for (const auto& spawn : stageEnemySpawns)
    {
        AddSeed(spawn.rect);
    }

    size_t head = 0;

    while (head < queue.size())
    {
        int index = queue[head++];

        int x = index % cols;
        int y = index / cols;

        const int dx[4] =
        {
            1,
            -1,
            0,
            0
        };

        const int dy[4] =
        {
            0,
            0,
            1,
            -1
        };

        for (int i = 0; i < 4; ++i)
        {
            int nx = x + dx[i];
            int ny = y + dy[i];

            if (nx < 0 || ny < 0 || nx >= cols || ny >= rows)
            {
                continue;
            }

            int nextIndex = IndexOf(nx, ny);

            if (blocked[nextIndex] || visited[nextIndex])
            {
                continue;
            }

            visited[nextIndex] = 1;
            queue.push_back(nextIndex);
        }
    }

    for (int y = 0; y < rows; ++y)
    {
        for (int x = 0; x < cols; ++x)
        {
            if (!visited[IndexOf(x, y)])
            {
                continue;
            }

            SDL_Rect tile =
            {
                x * FLOOR_TILE_SIZE,
                y * FLOOR_TILE_SIZE,
                FLOOR_TILE_SIZE,
                FLOOR_TILE_SIZE
            };

            reachableFloorTiles.push_back(tile);
        }
    }
}

// =====================================================
// 이변 효과 적용
// =====================================================
void ApplyAnomalyEffects(std::vector<Enemy>& enemies)
{
    // 1. Enemy 이변:
    // 추가 보초를 배치한다.
    // 위치는 현재 맵 기준 벽과 겹치지 않고, 붉은 목표 지점 근처를 감시하는 쪽으로 둔다.
    if (anomalyEnemy)
    {
        AddSentry(enemies, {560.0f, 140.0f});

        // 새로 추가한 보초가 오른쪽, 즉 anomaly goal 쪽을 보게 함
        if (!enemies.empty())
        {
            enemies.back().angle = 0.0f;
        }
    }

    // 2. FOV 이변:
    // 적 시야가 더 넓고 길어진다.
    // FOV 렌더링에도 바로 보이므로 플레이어가 이변을 인지하기 쉽다.
    if (anomalyFOV)
    {
        for (auto& enemy : enemies)
        {
            enemy.fov *= 1.25f;
            enemy.viewDist *= 1.20f;

            enemy.headSweepMin *= 1.20f;
            enemy.headSweepMax *= 1.20f;
            enemy.headSweepSpeed *= 1.15f;
        }
    }

    // 3. Sound 이변:
    // 적이 소리에 더 민감하게 반응한다.
    // hearingThreshold가 낮아질수록 같은 소음에도 더 쉽게 Investigate로 전환된다.
    if (anomalySound)
    {
        for (auto& enemy : enemies)
        {
            enemy.hearingThreshold *= 0.55f;
            enemy.searchDuration *= 1.25f;
            enemy.investigateTimeout *= 1.15f;
        }
    }
}

static void TriggerMainStageAnomaly(std::vector<Enemy>& enemies)
{
    if (stage == 1)
    {
        return;
    }

    if (mainStageAnomalyTriggered)
    {
        return;
    }

    mainStageAnomalyTriggered = true;

    anomalyActive = false;
    anomalyWall = false;
    anomalyEnemy = false;
    anomalyFOV = false;
    anomalySound = false;

    std::random_device rd;
    std::mt19937 rng(rd());

    std::vector<int> pool =
    {
        2,
        3
    };

    std::shuffle(pool.begin(), pool.end(), rng);

    std::uniform_int_distribution<int> countDist(
        1,
        static_cast<int>(pool.size()));

    int count = countDist(rng);

    for (int i = 0; i < count; ++i)
    {
        switch (pool[i])
        {
        case 2:
            anomalyFOV = true;
            break;

        case 3:
            anomalySound = true;
            break;
        }
    }

    anomalyActive =
        anomalyFOV ||
        anomalySound;

    ApplyAnomalyEffects(enemies);

    mainStageReturnHintVisible = true;
    mainStageReturnHintStart = SDL_GetTicks();

    std::cout
        << "Codebook acquired. Return to the starting point!"
        << std::endl;

    std::cout
        << "[Main Stage] anomaly triggered after codebook pickup. "
        << "fov=" << anomalyFOV
        << " sound=" << anomalySound
        << std::endl;
}

// =====================================================
// 스테이지 초기화
// =====================================================
void ResetStage(SDL_Rect& player, std::vector<Enemy>& enemies)
{
    StageMapSetup mapSetup =
        (stage == 1)
        ? MakeTutorialStageMap()
        : MakePrototypeMainStageMap();

    ApplyStageMapSetupToGlobals(mapSetup, player);

    tutorial.Reset(mapSetup);
    ResetTutorialBottleGuardScript();
    ResetTutorialGunScript();
    ResetTutorialCabinetAlertScript();
    ResetTutorialEscapeGuardScript();
    RebuildReachableFloorTiles(player);
    
    enemies.clear();
    
    if (!stageEnemySpawns.empty())
    {
        for (const auto& spawn : stageEnemySpawns)
        {
            AddEnemyFromStageSpawn(enemies, spawn);
        }
    }

    showStageText = true;
    stageTextStart = SDL_GetTicks();
    displayStage = stage;

    anomalyActive = false;
    anomalyWall = false;
    anomalyEnemy = false;
    anomalyFOV = false;
    anomalySound = false;
    
    mainStageAnomalyTriggered = false;
    mainStageReturnHintVisible = false;
    mainStageReturnHintStart = 0;
    
    if (stage == 1)
    {
        return;
    }
    
    std::cout
      << "[Stage " << stage << "] "
      << "main stage loaded. Codebook will trigger anomaly."
      << std::endl;
}

// =====================================================
// 텍스트 출력
// =====================================================
void DrawCenteredText(SDL_Renderer* renderer, TTF_Font* font, const char* text)
{
    SDL_Color color = {255,255,255,255};

    SDL_Surface* surface =
        TTF_RenderText_Solid(font, text, color);

    SDL_Texture* texture =
        SDL_CreateTextureFromSurface(renderer, surface);

    SDL_Rect dst;
    dst.w = surface->w;
    dst.h = surface->h;
    dst.x = SCREEN_WIDTH / 2 - dst.w / 2;
    dst.y = SCREEN_HEIGHT / 2 - dst.h / 2;

    SDL_RenderCopy(renderer, texture, NULL, &dst);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void DrawTextInRect(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const char* text,
    const SDL_Rect& rect,
    SDL_Color color)
{
    if (!renderer || !font || !text)
    {
        return;
    }

    SDL_Surface* surface = TTF_RenderText_Solid(font, text, color);
    if (!surface)
    {
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture)
    {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect dst;
    dst.w = surface->w;
    dst.h = surface->h;
    dst.x = rect.x + rect.w / 2 - dst.w / 2;
    dst.y = rect.y + rect.h / 2 - dst.h / 2;

    SDL_RenderCopy(renderer, texture, NULL, &dst);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

static void ShowHudMessage(
    const std::string& text,
    Uint32 durationMs = 2500)
{
    hudMessageText = text;
    hudMessageStart = SDL_GetTicks();
    hudMessageDuration = durationMs;
}

static void DrawHudMessage(
    SDL_Renderer* renderer,
    TTF_Font* font)
{
    if (hudMessageText.empty())
    {
        return;
    }

    Uint32 now = SDL_GetTicks();

    if (now - hudMessageStart > hudMessageDuration)
    {
        hudMessageText.clear();
        hudMessageDuration = 0;
        return;
    }

    SDL_Rect box =
    {
        SCREEN_WIDTH / 2 - 280,
        82,
        560,
        42
    };

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 190);
    SDL_RenderFillRect(renderer, &box);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
    SDL_RenderDrawRect(renderer, &box);

    DrawTextInRect(
        renderer,
        font,
        hudMessageText.c_str(),
        box,
        { 255, 255, 255, 255 });
}

static bool IsScreenRectVisible(const SDL_Rect& rect);

void DrawWorldItems(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const std::vector<WorldItem>& items,
    const Camera2D& camera)
{
    for (const auto& item : items)
    {
        if (item.picked)
        {
            continue;
        }

        SDL_Rect screenRect = camera.WorldToScreenRect(item.rect);

        if (!IsScreenRectVisible(screenRect))
        {
            continue;
        }

        SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
        SDL_RenderFillRect(renderer, &screenRect);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(renderer, &screenRect);

        DrawTextInRect(
            renderer,
            font,
            GetItemLetter(item.type),
            screenRect,
            { 20, 20, 20, 255 });
    }
}

void DrawSuppressorPickup(
    SDL_Renderer* renderer,
    const SuppressorPickup& suppressor,
    const Equipment& equipment,
    const Camera2D& camera)
{
    if (equipment.hasSuppressor || suppressor.picked)
    {
        return;
    }

    SDL_Rect screenRect = camera.WorldToScreenRect(suppressor.rect);

    if (!IsScreenRectVisible(screenRect))
    {
        return;
    }

    SDL_SetRenderDrawColor(renderer, 120, 120, 120, 255);
    SDL_RenderFillRect(renderer, &screenRect);

    SDL_SetRenderDrawColor(renderer, 190, 190, 190, 255);
    SDL_RenderDrawRect(renderer, &screenRect);
}

void DrawLockedDoors(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const std::vector<LockedDoor>& doors,
    const Camera2D& camera)
{
    for (const auto& door : doors)
    {
        SDL_Rect screenRect = camera.WorldToScreenRect(door.wall.rect);

        if (!IsScreenRectVisible(screenRect))
        {
            continue;
        }


        if (door.opened)
        {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 120, 90, 40, 90);
            SDL_RenderDrawRect(renderer, &screenRect);
            continue;
        }

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 120, 70, 20, 255);
        SDL_RenderFillRect(renderer, &screenRect);

        SDL_SetRenderDrawColor(renderer, 255, 220, 120, 255);
        SDL_RenderDrawRect(renderer, &screenRect);
    }
}

void DrawUnopenableDoors(
    SDL_Renderer* renderer,
    const Camera2D& camera)
{
    for (const auto& interactable : stageInteractables)
    {
        if (interactable.id != "unopenable_door")
        {
            continue;
        }

        SDL_Rect screenRect =
            camera.WorldToScreenRect(interactable.rect);

        if (!IsScreenRectVisible(screenRect))
        {
            continue;
        }

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        SDL_SetRenderDrawColor(renderer, 95, 55, 24, 255);
        SDL_RenderFillRect(renderer, &screenRect);

        SDL_SetRenderDrawColor(renderer, 150, 95, 45, 255);
        SDL_RenderDrawRect(renderer, &screenRect);
    }
}

void DrawCabinets(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const Camera2D& camera)
{
    for (const auto& interactable : stageInteractables)
    {
        if (interactable.id != "cabinet")
        {
            continue;
        }

        SDL_Rect screenRect = camera.WorldToScreenRect(interactable.rect);

        if (!IsScreenRectVisible(screenRect))
        {
            continue;
        }

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

        SDL_SetRenderDrawColor(renderer, 70, 55, 35, 255);
        SDL_RenderFillRect(renderer, &screenRect);

        SDL_SetRenderDrawColor(renderer, 180, 145, 90, 255);
        SDL_RenderDrawRect(renderer, &screenRect);

        DrawTextInRect(
            renderer,
            font,
            "C",
            screenRect,
            { 255, 235, 180, 255 });
    }
}

struct InventoryIconTextures
{
    SDL_Texture* bottle = nullptr;
    SDL_Texture* key = nullptr;
    SDL_Texture* codebook = nullptr;
};

static SDL_Texture* LoadTexture(
    SDL_Renderer* renderer,
    const char* path)
{
    SDL_Surface* surface = IMG_Load(path);

    if (!surface)
    {
        std::cout
            << "IMG_Load failed: "
            << path
            << " / "
            << IMG_GetError()
            << std::endl;

        return nullptr;
    }

    SDL_Texture* texture =
        SDL_CreateTextureFromSurface(renderer, surface);

    SDL_FreeSurface(surface);

    if (!texture)
    {
        std::cout
            << "SDL_CreateTextureFromSurface failed: "
            << path
            << " / "
            << SDL_GetError()
            << std::endl;

        return nullptr;
    }

    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

    return texture;
}

static SDL_Rect FindOpaqueBounds(
    SDL_Surface* surface)
{
    SDL_Rect full =
    {
        0,
        0,
        surface->w,
        surface->h
    };

    int minX = surface->w;
    int minY = surface->h;
    int maxX = -1;
    int maxY = -1;

    for (int y = 0; y < surface->h; ++y)
    {
        Uint8* row =
            static_cast<Uint8*>(surface->pixels) +
            y * surface->pitch;

        for (int x = 0; x < surface->w; ++x)
        {
            Uint32 pixel =
                reinterpret_cast<Uint32*>(row)[x];

            Uint8 r = 0;
            Uint8 g = 0;
            Uint8 b = 0;
            Uint8 a = 0;

            SDL_GetRGBA(
                pixel,
                surface->format,
                &r,
                &g,
                &b,
                &a);

            if (a <= 8)
            {
                continue;
            }

            minX = std::min(minX, x);
            minY = std::min(minY, y);
            maxX = std::max(maxX, x);
            maxY = std::max(maxY, y);
        }
    }

    if (maxX < minX || maxY < minY)
    {
        return full;
    }

    return
    {
        minX,
        minY,
        maxX - minX + 1,
        maxY - minY + 1
    };
}

static bool LoadTextureWithOpaqueSource(
    SDL_Renderer* renderer,
    const char* path,
    SDL_Texture*& outTexture,
    SDL_Rect& outSource)
{
    SDL_Surface* loaded =
        IMG_Load(path);

    if (!loaded)
    {
        std::cout
            << "IMG_Load failed: "
            << path
            << " / "
            << IMG_GetError()
            << std::endl;

        outTexture = nullptr;
        outSource = { 0, 0, 1, 1 };

        return false;
    }

    SDL_Surface* surface =
        SDL_ConvertSurfaceFormat(
            loaded,
            SDL_PIXELFORMAT_RGBA32,
            0);

    SDL_FreeSurface(loaded);

    if (!surface)
    {
        std::cout
            << "SDL_ConvertSurfaceFormat failed: "
            << path
            << " / "
            << SDL_GetError()
            << std::endl;

        outTexture = nullptr;
        outSource = { 0, 0, 1, 1 };

        return false;
    }

    outSource =
        FindOpaqueBounds(surface);

    outTexture =
        SDL_CreateTextureFromSurface(
            renderer,
            surface);

    SDL_FreeSurface(surface);

    if (!outTexture)
    {
        std::cout
            << "SDL_CreateTextureFromSurface failed: "
            << path
            << " / "
            << SDL_GetError()
            << std::endl;

        outSource = { 0, 0, 1, 1 };

        return false;
    }

    SDL_SetTextureBlendMode(
        outTexture,
        SDL_BLENDMODE_BLEND);

    return true;
}

static bool LoadInventoryIconTextures(
    SDL_Renderer* renderer,
    InventoryIconTextures& icons)
{
    icons.bottle =
        LoadTexture(renderer, "assets/sprites/bottleInventory.png");

    icons.key =
        LoadTexture(renderer, "assets/sprites/keyInventory.png");

    icons.codebook =
        LoadTexture(renderer, "assets/sprites/codebookInventory.png");

    return
        icons.bottle != nullptr &&
        icons.key != nullptr &&
        icons.codebook != nullptr;
}

static void DestroyInventoryIconTextures(
    InventoryIconTextures& icons)
{
    if (icons.bottle)
    {
        SDL_DestroyTexture(icons.bottle);
        icons.bottle = nullptr;
    }

    if (icons.key)
    {
        SDL_DestroyTexture(icons.key);
        icons.key = nullptr;
    }

    if (icons.codebook)
    {
        SDL_DestroyTexture(icons.codebook);
        icons.codebook = nullptr;
    }
}

struct GameSpriteTextures
{
    SDL_Texture* floor = nullptr;

    SDL_Texture* bottleMap = nullptr;
    SDL_Texture* keyMap = nullptr;
    SDL_Texture* codebookMap = nullptr;

    SDL_Texture* bookshelf = nullptr;
    SDL_Texture* chair = nullptr;
    SDL_Texture* table = nullptr;
    SDL_Texture* typewriter = nullptr;
    SDL_Texture* bed = nullptr;

    SDL_Texture* enemy = nullptr;

    SDL_Texture* spritesheetPlayer = nullptr;
    SDL_Texture* spritesheetOfficer = nullptr;
    SDL_Texture* playerFire = nullptr;
    SDL_Rect playerFireSrc = { 0, 0, 1, 1 };
    SDL_Texture* officerFire = nullptr;
    SDL_Rect officerFireSrc = { 0, 0, 1, 1 };

    SDL_Rect enemySource =
    {
        136,
        1,
        353,
        641
    };

    SDL_Texture* cabinet = nullptr;
    SDL_Rect cabinetSrc = { 0, 0, 1, 1 };
    SDL_Texture* enemyDead = nullptr;
    SDL_Rect enemyDeadSrc = { 0, 0, 1, 1 };
    SDL_Texture* enemyDeadB = nullptr;
    SDL_Rect enemyDeadBSrc = { 0, 0, 1, 1 };
    SDL_Texture* officerDead = nullptr;
    SDL_Rect officerDeadSrc = { 0, 0, 1, 1 };
    SDL_Texture* officerDeadB = nullptr;
    SDL_Rect officerDeadBSrc = { 0, 0, 1, 1 };
    SDL_Texture* playerDead = nullptr;
    SDL_Rect playerDeadSrc = { 0, 0, 1, 1 };
};

static void DestroyTextureIfLoaded(SDL_Texture*& texture)
{
    if (texture)
    {
        SDL_DestroyTexture(texture);
        texture = nullptr;
    }
}

static bool LoadGameSpriteTextures(
    SDL_Renderer* renderer,
    GameSpriteTextures& textures)
{
    textures.floor =
        LoadTexture(renderer, "assets/sprites/floor.png");

    textures.bottleMap =
        LoadTexture(renderer, "assets/sprites/bottleMap.png");

    textures.keyMap =
        LoadTexture(renderer, "assets/sprites/keyMap.png");

    textures.codebookMap =
        LoadTexture(renderer, "assets/sprites/codebookMap.png");

    textures.bookshelf =
        LoadTexture(renderer, "assets/sprites/bookshelf.png");

    textures.chair =
        LoadTexture(renderer, "assets/sprites/chair.png");

    textures.table =
        LoadTexture(renderer, "assets/sprites/table.png");

    textures.typewriter =
        LoadTexture(renderer, "assets/sprites/typewriter.png");

    textures.bed =
        LoadTexture(renderer, "assets/sprites/bed.png");

    textures.enemy =
        LoadTexture(renderer, "assets/sprites/enemy.png");

    textures.enemyDead =
        LoadTexture(renderer, "assets/sprites/enemyDead.png");
        
    textures.enemyDeadB =
        LoadTexture(renderer, "assets/sprites/enemyDeadB.png");

    textures.officerDead =
        LoadTexture(renderer, "assets/sprites/officerDead.png");

    textures.officerDeadB =
        LoadTexture(renderer, "assets/sprites/officerDeadB.png");

    textures.playerDead =
        LoadTexture(renderer, "assets/sprites/playerDead.png");

    textures.spritesheetPlayer =
        LoadTexture(renderer, "assets/sprites/spritesheetPlayer.png");
    
    textures.spritesheetOfficer =
        LoadTexture(renderer, "assets/sprites/spritesheetOfficer.png");
        
    LoadTextureWithOpaqueSource(
        renderer,
        "assets/sprites/playerFire.png",
        textures.playerFire,
        textures.playerFireSrc);
    
    LoadTextureWithOpaqueSource(
        renderer,
        "assets/sprites/officerFire.png",
        textures.officerFire,
        textures.officerFireSrc);
        
    LoadTextureWithOpaqueSource(
        renderer,
        "assets/sprites/enemyDead.png",
        textures.enemyDead,
        textures.enemyDeadSrc);
        
    LoadTextureWithOpaqueSource(
        renderer,
        "assets/sprites/enemyDeadB.png",
        textures.enemyDeadB,
        textures.enemyDeadBSrc);
        
    LoadTextureWithOpaqueSource(
        renderer,
        "assets/sprites/officerDead.png",
        textures.officerDead,
        textures.officerDeadSrc);
        
    LoadTextureWithOpaqueSource(
        renderer,
        "assets/sprites/officerDeadB.png",
        textures.officerDeadB,
        textures.officerDeadBSrc);
        
    LoadTextureWithOpaqueSource(
        renderer,
        "assets/sprites/playerDead.png",
        textures.playerDead,
        textures.playerDeadSrc);

    LoadTextureWithOpaqueSource(
        renderer,
        "assets/sprites/cabinet.png",
        textures.cabinet,
        textures.cabinetSrc);

    return
        textures.floor &&
        textures.bottleMap &&
        textures.keyMap &&
        textures.codebookMap &&
        textures.bookshelf &&
        textures.chair &&
        textures.table &&
        textures.typewriter &&
        textures.enemy &&
        textures.cabinet &&
        textures.spritesheetPlayer &&
        textures.spritesheetOfficer &&
        textures.playerFire &&
        textures.officerFire;
}

static void DestroyGameSpriteTextures(GameSpriteTextures& textures)
{
    DestroyTextureIfLoaded(textures.floor);

    DestroyTextureIfLoaded(textures.bottleMap);
    DestroyTextureIfLoaded(textures.keyMap);
    DestroyTextureIfLoaded(textures.codebookMap);

    DestroyTextureIfLoaded(textures.bookshelf);
    DestroyTextureIfLoaded(textures.chair);
    DestroyTextureIfLoaded(textures.table);
    DestroyTextureIfLoaded(textures.typewriter);
    DestroyTextureIfLoaded(textures.bed);

    DestroyTextureIfLoaded(textures.enemy);

    DestroyTextureIfLoaded(textures.cabinet);

    DestroyTextureIfLoaded(textures.spritesheetPlayer);
    DestroyTextureIfLoaded(textures.spritesheetOfficer);

    DestroyTextureIfLoaded(textures.playerFire);
    DestroyTextureIfLoaded(textures.officerFire);
}

static SDL_Texture* GetInventoryIconTexture(
    const InventoryIconTextures& icons,
    ItemType type)
{
    switch (type)
    {
    case ItemType::Bottle:
        return icons.bottle;

    case ItemType::Key:
        return icons.key;

    case ItemType::Target:
        return icons.codebook;
    }

    return nullptr;
}

static SDL_Rect MakeAspectFitRect(
    SDL_Texture* texture,
    const SDL_Rect& bounds,
    int padding)
{
    SDL_Rect content =
    {
        bounds.x + padding,
        bounds.y + padding,
        bounds.w - padding * 2,
        bounds.h - padding * 2
    };

    int textureW = 0;
    int textureH = 0;

    if (SDL_QueryTexture(
            texture,
            nullptr,
            nullptr,
            &textureW,
            &textureH) != 0 ||
        textureW <= 0 ||
        textureH <= 0)
    {
        return content;
    }

    float scaleX =
        static_cast<float>(content.w) /
        static_cast<float>(textureW);

    float scaleY =
        static_cast<float>(content.h) /
        static_cast<float>(textureH);

    float scale = std::min(scaleX, scaleY);

    int drawW =
        static_cast<int>(std::round(textureW * scale));

    int drawH =
        static_cast<int>(std::round(textureH * scale));

    SDL_Rect dst =
    {
        content.x + content.w / 2 - drawW / 2,
        content.y + content.h / 2 - drawH / 2,
        drawW,
        drawH
    };

    return dst;
}

void DrawInventoryHUD(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const Inventory& inventory,
    const InventoryIconTextures& icons)
{
    static constexpr int SLOT_SIZE = 44;
    static constexpr int SLOT_GAP = 8;
    static constexpr int START_X = 16;
    static constexpr int START_Y = 16;
    static constexpr int ICON_PADDING = 5;

    SDL_Rect slots[3] =
    {
        { START_X, START_Y, SLOT_SIZE, SLOT_SIZE },
        { START_X + (SLOT_SIZE + SLOT_GAP), START_Y, SLOT_SIZE, SLOT_SIZE },
        { START_X + (SLOT_SIZE + SLOT_GAP) * 2, START_Y, SLOT_SIZE, SLOT_SIZE }
    };

    ItemType types[3] = {ItemType::Bottle, ItemType::Key, ItemType::Target};

    const char* fallbackLetters[3] = { "B", "K", "T" };
    bool owned[3] =
    {
        inventory.hasBottle,
        inventory.hasKey,
        inventory.hasTarget
    };

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < 3; ++i)
    {
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 180);
        SDL_RenderFillRect(renderer, &slots[i]);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
        SDL_RenderDrawRect(renderer, &slots[i]);

        if (!owned[i])
        {
            continue;
        }

        SDL_Texture* icon = GetInventoryIconTexture(icons, types[i]);
        if (icon)
        {
            SDL_Rect iconDst = MakeAspectFitRect(icon, slots[i], ICON_PADDING);
            SDL_RenderCopy(renderer, icon, nullptr, &iconDst);
        }
        else
        {
            DrawTextInRect(
                renderer,
                font,
                fallbackLetters[i],
                slots[i],
                { 255, 255, 255, 255 });
        }
    }
}

// =====================================================
// 무기
// =====================================================

static constexpr float WIRE_RANGE = 76.0f;
static constexpr float WIRE_BEHIND_DOT_THRESHOLD = -0.45f;

static constexpr int PISTOL_MAGAZINE_SIZE = 7;
static constexpr int PLAYER_GUN_DAMAGE = 100;
static constexpr float PLAYER_GUN_RANGE = 760.0f;
static constexpr float PLAYER_GUN_COOLDOWN = 0.28f;
static constexpr float BULLET_TRAIL_LIFETIME = 0.08f;

static constexpr int SUPPRESSED_GUNSHOT_PARTICLE_COUNT = 24;
static constexpr float SUPPRESSED_GUNSHOT_PARTICLE_SPEED = 230.0f;
static constexpr float SUPPRESSED_GUNSHOT_LOUDNESS = 0.9f;
static constexpr float SUPPRESSED_GUNSHOT_LIFE = 1.1f;

struct PlayerGunState
{
    int ammo = PISTOL_MAGAZINE_SIZE;
    float cooldown = 0.0f;
};

struct BulletTrail
{
    Vec2 start = { 0.0f, 0.0f };
    Vec2 end = { 0.0f, 0.0f };
    float life = BULLET_TRAIL_LIFETIME;
    float maxLife = BULLET_TRAIL_LIFETIME;
};

static constexpr float BODY_DRAG_INTERACT_RANGE = 56.0f;
static constexpr float BODY_DRAG_TARGET_DISTANCE = 34.0f;

static constexpr float BODY_DRAG_MASS = 2.5f;
static constexpr float BODY_DRAG_SPRING = 58.0f;
static constexpr float BODY_DRAG_DAMPING = 10.0f;
static constexpr float BODY_DRAG_LINEAR_DRAG = 4.0f;
static constexpr float BODY_DRAG_MAX_SPEED = 120.0f;

static bool RayIntersectsRectLocal(
    Vec2 origin,
    Vec2 dir,
    const SDL_Rect& rect,
    float& outT)
{
    const float EPSILON = 0.000001f;
    const float INF = 1.0e30f;

    float minX = static_cast<float>(rect.x);
    float maxX = static_cast<float>(rect.x + rect.w);
    float minY = static_cast<float>(rect.y);
    float maxY = static_cast<float>(rect.y + rect.h);

    float tMin = -INF;
    float tMax = INF;

    if (fabsf(dir.x) < EPSILON)
    {
        if (origin.x < minX || origin.x > maxX)
        {
            return false;
        }
    }
    else
    {
        float tx1 = (minX - origin.x) / dir.x;
        float tx2 = (maxX - origin.x) / dir.x;

        if (tx1 > tx2)
        {
            std::swap(tx1, tx2);
        }

        if (tx1 > tMin) tMin = tx1;
        if (tx2 < tMax) tMax = tx2;
    }

    if (fabsf(dir.y) < EPSILON)
    {
        if (origin.y < minY || origin.y > maxY)
        {
            return false;
        }
    }
    else
    {
        float ty1 = (minY - origin.y) / dir.y;
        float ty2 = (maxY - origin.y) / dir.y;

        if (ty1 > ty2)
        {
            std::swap(ty1, ty2);
        }

        if (ty1 > tMin) tMin = ty1;
        if (ty2 < tMax) tMax = ty2;
    }

    if (tMin > tMax || tMax < 0.0f)
    {
        return false;
    }

    outT = tMin >= 0.0f ? tMin : tMax;
    return outT >= 0.0f;
}

static bool IsSegmentBlockedByWallsLocal(
    Vec2 from,
    Vec2 to,
    const std::vector<Wall>& walls)
{
    Vec2 delta = to - from;
    float lengthSq = LengthSq(delta);

    if (lengthSq <= 0.000001f)
    {
        return false;
    }

    float length = sqrtf(lengthSq);
    Vec2 dir = delta / length;

    for (const auto& wall : walls)
    {
        float t = 0.0f;

        if (RayIntersectsRectLocal(from, dir, wall.rect, t) &&
            t <= length)
        {
            return true;
        }
    }

    return false;
}

static Vec2 ClampVec2LengthLocal(Vec2 value, float maxLength)
{
    float lenSq = LengthSq(value);
    float maxSq = maxLength * maxLength;

    if (lenSq <= maxSq || lenSq <= 0.000001f)
    {
        return value;
    }

    return Normalize(value) * maxLength;
}

static SDL_Rect MakeCorpseRectAtCenter(
    const Enemy& corpse,
    Vec2 center)
{
    SDL_Rect rect = corpse.rect;
    rect.x = static_cast<int>(center.x - rect.w * 0.5f);
    rect.y = static_cast<int>(center.y - rect.h * 0.5f);
    return rect;
}

static bool CorpseRectIntersectsAnyWall(
    const SDL_Rect& rect,
    const std::vector<Wall>& walls)
{
    for (const auto& wall : walls)
    {
        if (SDL_HasIntersection(&rect, &wall.rect))
        {
            return true;
        }
    }

    return false;
}

static bool CanCorpseOccupyCenter(
    const Enemy& corpse,
    Vec2 center,
    const std::vector<Wall>& walls)
{
    float halfW = corpse.rect.w * 0.5f;
    float halfH = corpse.rect.h * 0.5f;

    if (center.x < halfW || center.y < halfH)
    {
        return false;
    }

    if (center.x > static_cast<float>(currentMapWidth) - halfW ||
        center.y > static_cast<float>(currentMapHeight) - halfH)
    {
        return false;
    }

    SDL_Rect candidate = MakeCorpseRectAtCenter(corpse, center);
    return !CorpseRectIntersectsAnyWall(candidate, walls);
}

static void SetCorpseCenter(
    Enemy& corpse,
    Vec2 center)
{
    corpse.pos = center;
    corpse.rect = MakeCorpseRectAtCenter(corpse, center);
}

static void SnapDraggedBodyNearPlayer(
    Enemy& body,
    const SDL_Rect& player,
    const std::vector<Wall>& walls)
{
    Vec2 playerCenter = GetRectCenter(player);
    Vec2 bodyCenter = GetRectCenter(body.rect);

    Vec2 dir = bodyCenter - playerCenter;
    if (LengthSq(dir) <= 0.000001f)
    {
        dir = { -1.0f, 0.0f };
    }
    else
    {
        dir = Normalize(dir);
    }

    Vec2 target = playerCenter + dir * BODY_DRAG_TARGET_DISTANCE;

    if (CanCorpseOccupyCenter(body, target, walls))
    {
        SetCorpseCenter(body, target);
    }

    body.bodyVelocity = { 0.0f, 0.0f };
}

static void MoveCorpseWithCollision(
    Enemy& corpse,
    Vec2 delta,
    const std::vector<Wall>& walls)
{
    Vec2 center = GetRectCenter(corpse.rect);

    Vec2 tryX = { center.x + delta.x, center.y };
    if (CanCorpseOccupyCenter(corpse, tryX, walls))
    {
        center.x = tryX.x;
    }
    else
    {
        corpse.bodyVelocity.x = 0.0f;
    }

    Vec2 tryY = { center.x, center.y + delta.y };
    if (CanCorpseOccupyCenter(corpse, tryY, walls))
    {
        center.y = tryY.y;
    }
    else
    {
        corpse.bodyVelocity.y = 0.0f;
    }

    SetCorpseCenter(corpse, center);
}

static bool IsDraggingBody(
    const std::vector<Enemy>& enemies,
    int draggedBodyIndex)
{
    if (draggedBodyIndex < 0 ||
        draggedBodyIndex >= static_cast<int>(enemies.size()))
    {
        return false;
    }

    const Enemy& body = enemies[draggedBodyIndex];

    return
        body.state == EnemyState::Dead &&
        body.bodyDraggable &&
        body.bodyDragged &&
        !body.bodyHidden;
}

static int CountWireTakedownBodies(const std::vector<Enemy>& enemies)
{
    int count = 0;

    for (const auto& enemy : enemies)
    {
        if (enemy.state == EnemyState::Dead &&
            enemy.bodyDraggable &&
            !enemy.bodyHidden)
        {
            ++count;
        }
    }

    return count;
}

static int FindNearestDraggableBodyIndex(
    const SDL_Rect& player,
    const std::vector<Enemy>& enemies,
    const std::vector<Wall>& walls)
{
    Vec2 playerCenter = GetRectCenter(player);

    int bestIndex = -1;
    float bestDistSq =
        BODY_DRAG_INTERACT_RANGE * BODY_DRAG_INTERACT_RANGE;

    for (size_t i = 0; i < enemies.size(); ++i)
    {
        const Enemy& enemy = enemies[i];

        if (enemy.state != EnemyState::Dead ||
            !enemy.bodyDraggable ||
            enemy.bodyDragged ||
            enemy.bodyHidden)
        {
            continue;
        }

        Vec2 bodyCenter = GetRectCenter(enemy.rect);
        float distSq = DistanceSq(playerCenter, bodyCenter);

        if (distSq > bestDistSq)
        {
            continue;
        }

        if (IsSegmentBlockedByWallsLocal(playerCenter, bodyCenter, walls))
        {
            continue;
        }

        bestIndex = static_cast<int>(i);
        bestDistSq = distSq;
    }

    return bestIndex;
}

static void StopDraggingBody(
    std::vector<Enemy>& enemies,
    int& draggedBodyIndex)
{
    if (draggedBodyIndex >= 0 &&
        draggedBodyIndex < static_cast<int>(enemies.size()))
    {
        enemies[draggedBodyIndex].bodyDragged = false;
        enemies[draggedBodyIndex].bodyVelocity = { 0.0f, 0.0f };
    }

    draggedBodyIndex = -1;
}

static bool FindInteractableRectById(
    const std::string& id,
    SDL_Rect& outRect)
{
    for (const auto& interactable : stageInteractables)
    {
        if (interactable.id == id)
        {
            outRect = interactable.rect;
            return true;
        }
    }

    return false;
}

static bool TryFindNearestCabinetRect(
    const SDL_Rect& player,
    SDL_Rect& outRect)
{
    static constexpr float CABINET_HIDE_RANGE = 96.0f;
    const float hideRangeSq =
        CABINET_HIDE_RANGE * CABINET_HIDE_RANGE;

    Vec2 playerCenter =
        GetRectCenter(player);

    bool found = false;
    float bestDistSq = hideRangeSq;

    for (const auto& interactable : stageInteractables)
    {
        if (interactable.id != "cabinet")
        {
            continue;
        }

        Vec2 cabinetCenter =
            GetRectCenter(interactable.rect);

        float distSq =
            DistanceSqLocal(playerCenter, cabinetCenter);

        if (distSq > bestDistSq)
        {
            continue;
        }

        bestDistSq = distSq;
        outRect = interactable.rect;
        found = true;
    }

    return found;
}

static bool TryHideDraggedBodyInCabinet(
    const SDL_Rect& player,
    std::vector<Enemy>& enemies,
    int& draggedBodyIndex)
{
    if (!IsDraggingBody(enemies, draggedBodyIndex))
    {
        return false;
    }

    SDL_Rect cabinetRect;

    if (!TryFindNearestCabinetRect(player, cabinetRect))
    {
        std::cout << "No cabinet in range." << std::endl;
        return false;
    }

    Enemy& body =
        enemies[draggedBodyIndex];

    body.bodyHidden = true;
    body.bodyDragged = false;
    body.bodyDraggable = false;
    body.bodyVelocity = { 0.0f, 0.0f };

    body.rect = { -2000, -2000, 1, 1 };
    body.pos = GetRectCenter(body.rect);

    draggedBodyIndex = -1;

    std::cout << "Body hidden in cabinet." << std::endl;

    return true;
}

static bool ToggleBodyDrag(
    const SDL_Rect& player,
    std::vector<Enemy>& enemies,
    const std::vector<Wall>& walls,
    int& draggedBodyIndex)
{
    if (IsDraggingBody(enemies, draggedBodyIndex))
    {
        StopDraggingBody(enemies, draggedBodyIndex);
        std::cout << "Body dropped" << std::endl;
        return true;
    }

    draggedBodyIndex = -1;

    int targetIndex =
        FindNearestDraggableBodyIndex(player, enemies, walls);

    if (targetIndex < 0)
    {
        return false;
    }

    enemies[targetIndex].bodyDragged = true;
    enemies[targetIndex].bodyVelocity = { 0.0f, 0.0f };
    enemies[targetIndex].pos = GetRectCenter(enemies[targetIndex].rect);

    SnapDraggedBodyNearPlayer(enemies[targetIndex], player, walls);

    draggedBodyIndex = targetIndex;

    std::cout << "Body dragging started" << std::endl;
    return true;
}

static void UpdateDraggedBody(
    std::vector<Enemy>& enemies,
    int& draggedBodyIndex,
    const SDL_Rect& player,
    Vec2 playerMoveDir,
    const std::vector<Wall>& walls,
    float dt)
{
    if (!IsDraggingBody(enemies, draggedBodyIndex))
    {
        draggedBodyIndex = -1;
        return;
    }

    Enemy& body = enemies[draggedBodyIndex];

    Vec2 playerCenter = GetRectCenter(player);
    Vec2 bodyCenter = GetRectCenter(body.rect);

    Vec2 behindDir = { 0.0f, 0.0f };

    if (LengthSq(playerMoveDir) > 0.000001f)
    {
        behindDir = Normalize(playerMoveDir) * -1.0f;
    }
    else
    {
        Vec2 playerToBody = bodyCenter - playerCenter;

        if (LengthSq(playerToBody) > 0.000001f)
        {
            behindDir = Normalize(playerToBody);
        }
        else
        {
            behindDir = { -1.0f, 0.0f };
        }
    }

    Vec2 target =
        playerCenter + behindDir * BODY_DRAG_TARGET_DISTANCE;

    Vec2 displacement = target - bodyCenter;

    // F = spring * displacement - damping * velocity
    Vec2 springForce = displacement * BODY_DRAG_SPRING;
    Vec2 dampingForce = body.bodyVelocity * (-BODY_DRAG_DAMPING);
    Vec2 forceAccum = springForce + dampingForce;

    Vec2 acceleration = forceAccum * (1.0f / BODY_DRAG_MASS);

    // Euler integration: v += a * dt, p += v * dt
    body.bodyVelocity += acceleration * dt;

    // linear drag
    body.bodyVelocity +=
        body.bodyVelocity * (-BODY_DRAG_LINEAR_DRAG * dt);

    body.bodyVelocity =
        ClampVec2LengthLocal(body.bodyVelocity, BODY_DRAG_MAX_SPEED);

    MoveCorpseWithCollision(
        body,
        body.bodyVelocity * dt,
        walls);
}

static void KillEnemySilently(Enemy& enemy)
{
    enemy.hp = 0;
    enemy.state = EnemyState::Dead;
    enemy.alerted = false;
    enemy.hasPendingNoise = false;
    enemy.pendingNoiseEnergy = 0.0f;
    enemy.hearingEnergy = 0.0f;
    enemy.attackCooldown = 0.0f;
    enemy.bodyDraggable = true;
    enemy.bodyDragged = false;
    enemy.bodyHidden = false;
    enemy.bodyVelocity = { 0.0f, 0.0f };
}

static void KillEnemyByGun(Enemy& enemy)
{
    enemy.hp = 0;
    enemy.state = EnemyState::Dead;
    enemy.alerted = false;
    enemy.hasPendingNoise = false;
    enemy.pendingNoiseEnergy = 0.0f;
    enemy.hearingEnergy = 0.0f;
    enemy.attackCooldown = 0.0f;
    enemy.bodyDraggable = false;
    enemy.bodyDragged = false;
    enemy.bodyHidden = false;
    enemy.bodyVelocity = { 0.0f, 0.0f };
}

static bool IsPlayerBehindEnemy(
    const SDL_Rect& player,
    const Enemy& enemy)
{
    Vec2 playerCenter = GetRectCenter(player);
    Vec2 enemyCenter = GetRectCenter(enemy.rect);

    Vec2 enemyToPlayer = playerCenter - enemyCenter;

    if (LengthSq(enemyToPlayer) <= 0.000001f)
    {
        return false;
    }

    enemyToPlayer = Normalize(enemyToPlayer);

    Vec2 enemyForward = AngleToDir(enemy.angle);
    float dot = Dot(enemyForward, enemyToPlayer);

    return dot <= WIRE_BEHIND_DOT_THRESHOLD;
}

bool TryWireAttackEnemy(
    const SDL_Rect& player,
    std::vector<Enemy>& enemies,
    const std::vector<Wall>& walls)
{
    Vec2 playerCenter = GetRectCenter(player);

    Enemy* bestTarget = nullptr;
    float bestDistSq = WIRE_RANGE * WIRE_RANGE;

    for (auto& enemy : enemies)
    {
        if (enemy.state == EnemyState::Dead ||
            enemy.state == EnemyState::Alert)
        {
            continue;
        }

        Vec2 enemyCenter = GetRectCenter(enemy.rect);
        float distSq = DistanceSq(playerCenter, enemyCenter);

        if (distSq > bestDistSq)
        {
            continue;
        }

        if (!IsPlayerBehindEnemy(player, enemy))
        {
            continue;
        }

        if (IsSegmentBlockedByWallsLocal(playerCenter, enemyCenter, walls))
        {
            continue;
        }

        bestTarget = &enemy;
        bestDistSq = distSq;
    }

    if (!bestTarget)
    {
        return false;
    }

    KillEnemySilently(*bestTarget);
    std::cout << "Wire takedown\n";
    return true;
}

static void ApplyGunDamageToEnemy(
    Enemy& enemy,
    Vec2 shooterPos)
{
    if (enemy.state == EnemyState::Dead)
    {
        return;
    }

    enemy.hp -= PLAYER_GUN_DAMAGE;

    if (enemy.hp <= 0)
    {
        KillEnemyByGun(enemy);
        return;
    }

    enemy.state = EnemyState::Alert;
    enemy.heightenedAlert = true;
    enemy.alerted = true;
    enemy.lastKnownPlayerPos = shooterPos;
    enemy.alertLostTimer = 0.0f;
    enemy.stateTimer = 0.0f;
    enemy.hasPendingNoise = false;
    enemy.attackCooldown = 0.25f;
}

struct TutorialCheckpointSnapshot
{
    bool active = false;

    SDL_Rect player = { 0, 0, 32, 32 };

    Inventory inventory;
    Equipment equipment;
    PlayerGunState pistol;

    std::vector<WorldItem> worldItems;
    std::vector<Enemy> enemies;
    std::vector<LockedDoor> lockedDoors;

    TutorialController tutorial;
    int playerHP = PLAYER_MAX_HP;
    bool alarmActive = false;
};

static TutorialCheckpointSnapshot tutorialCheckpoint;

static void ClearTutorialCheckpoint()
{
    tutorialCheckpoint = TutorialCheckpointSnapshot{};
}

static void SaveTutorialCheckpoint(
    const SDL_Rect& player,
    const Inventory& inventory,
    const Equipment& equipment,
    const PlayerGunState& pistol,
    const std::vector<WorldItem>& worldItems,
    const std::vector<Enemy>& enemies,
    const std::vector<LockedDoor>& lockedDoors,
    const TutorialController& tutorial,
    int playerHP,
    bool alarmActive)
{
    tutorialCheckpoint.active = true;
    tutorialCheckpoint.player = player;
    tutorialCheckpoint.inventory = inventory;
    tutorialCheckpoint.equipment = equipment;
    tutorialCheckpoint.pistol = pistol;
    tutorialCheckpoint.worldItems = worldItems;
    tutorialCheckpoint.enemies = enemies;
    tutorialCheckpoint.lockedDoors = lockedDoors;
    tutorialCheckpoint.tutorial = tutorial;
    tutorialCheckpoint.playerHP = playerHP;
    tutorialCheckpoint.alarmActive = alarmActive;

    std::cout << "Tutorial checkpoint saved." << std::endl;
}

static bool RestoreTutorialCheckpoint(
    SDL_Rect& player,
    Inventory& inventory,
    Equipment& equipment,
    PlayerGunState& pistol,
    std::vector<WorldItem>& worldItems,
    std::vector<Enemy>& enemies,
    std::vector<LockedDoor>& lockedDoors,
    TutorialController& tutorial,
    int& playerHP,
    bool& alarmActive)
{
    if (!tutorialCheckpoint.active)
    {
        return false;
    }

    player = tutorialCheckpoint.player;
    inventory = tutorialCheckpoint.inventory;
    equipment = tutorialCheckpoint.equipment;
    pistol = tutorialCheckpoint.pistol;
    worldItems = tutorialCheckpoint.worldItems;
    enemies = tutorialCheckpoint.enemies;
    lockedDoors = tutorialCheckpoint.lockedDoors;
    tutorial = tutorialCheckpoint.tutorial;
    playerHP = tutorialCheckpoint.playerHP;
    alarmActive = tutorialCheckpoint.alarmActive;

    std::cout << "Tutorial checkpoint restored." << std::endl;
    return true;
}

bool TryFirePistol(
    const SDL_Rect& player,
    Vec2 targetWorld,
    std::vector<Enemy>& enemies,
    const std::vector<Wall>& walls,
    std::vector<SoundParticle>& soundParticles,
    std::vector<BulletTrail>& bulletTrails,
    PlayerGunState& pistol,
    bool hasSuppressor)
{
    if (pistol.ammo <= 0)
    {
        return false;
    }

    if (pistol.cooldown > 0.0f)
    {
        return false;
    }

    Vec2 muzzle = GetRectCenter(player);
    Vec2 toTarget = targetWorld - muzzle;

    if (LengthSq(toTarget) <= 0.000001f)
    {
        return false;
    }

    Vec2 dir = Normalize(toTarget);

    pistol.ammo--;
    pistol.cooldown = PLAYER_GUN_COOLDOWN;

    float closestWallT = PLAYER_GUN_RANGE;

    for (const auto& wall : walls)
    {
        float t = 0.0f;

        if (RayIntersectsRectLocal(muzzle, dir, wall.rect, t) &&
            t >= 0.0f &&
            t < closestWallT)
        {
            closestWallT = t;
        }
    }

    Enemy* hitEnemy = nullptr;
    float closestEnemyT = closestWallT;

    for (auto& enemy : enemies)
    {
        if (enemy.state == EnemyState::Dead)
        {
            continue;
        }

        float t = 0.0f;

        if (RayIntersectsRectLocal(muzzle, dir, enemy.rect, t) &&
            t >= 0.0f &&
            t <= closestWallT &&
            t < closestEnemyT)
        {
            hitEnemy = &enemy;
            closestEnemyT = t;
        }
    }

    float finalT = closestWallT;

    if (hitEnemy)
    {
        finalT = closestEnemyT;
        ApplyGunDamageToEnemy(*hitEnemy, muzzle);
    }

    Vec2 shotEnd = muzzle + dir * finalT;

    bulletTrails.push_back(
        {
            muzzle,
            shotEnd,
            BULLET_TRAIL_LIFETIME,
            BULLET_TRAIL_LIFETIME
        });

    EmitSound(
        soundParticles,
        muzzle,
        hasSuppressor ? SUPPRESSED_GUNSHOT_PARTICLE_COUNT : 88,
        hasSuppressor ? SUPPRESSED_GUNSHOT_PARTICLE_SPEED : 360.0f,
        hasSuppressor ? SUPPRESSED_GUNSHOT_LOUDNESS : 4.0f,
        SoundKind::Gunshot);

    std::cout
        << (hasSuppressor
            ? "Suppressed pistol fired. Ammo: "
            : "Pistol fired. Ammo: ")
        << pistol.ammo << "/"
        << PISTOL_MAGAZINE_SIZE
        << std::endl;
    
    return true;
}

static bool FindFirstPistolHitEnemyIndex(
    const SDL_Rect& player,
    Vec2 targetWorld,
    const std::vector<Enemy>& enemies,
    const std::vector<Wall>& walls,
    int& outEnemyIndex)
{
    outEnemyIndex = -1;

    Vec2 muzzle = GetRectCenter(player);
    Vec2 toTarget = targetWorld - muzzle;

    if (LengthSq(toTarget) <= 0.000001f)
    {
        return false;
    }

    Vec2 dir = Normalize(toTarget);

    float closestWallT = PLAYER_GUN_RANGE;

    for (const auto& wall : walls)
    {
        float t = 0.0f;

        if (RayIntersectsRectLocal(muzzle, dir, wall.rect, t) &&
            t >= 0.0f &&
            t < closestWallT)
        {
            closestWallT = t;
        }
    }

    float closestEnemyT = closestWallT;

    for (size_t i = 0; i < enemies.size(); ++i)
    {
        const Enemy& enemy = enemies[i];

        if (enemy.state == EnemyState::Dead)
        {
            continue;
        }

        float t = 0.0f;

        if (RayIntersectsRectLocal(muzzle, dir, enemy.rect, t) &&
            t >= 0.0f &&
            t <= closestWallT &&
            t < closestEnemyT)
        {
            outEnemyIndex = static_cast<int>(i);
            closestEnemyT = t;
        }
    }

    return outEnemyIndex >= 0;
}

static bool TryFireTutorialPistolAtEnemyId(
    const SDL_Rect& player,
    Vec2 targetWorld,
    std::vector<Enemy>& enemies,
    const std::vector<Wall>& walls,
    std::vector<SoundParticle>& soundParticles,
    std::vector<BulletTrail>& bulletTrails,
    PlayerGunState& pistol,
    bool hasSuppressor,
    const std::string& requiredEnemyId)
{
    int requiredIndex =
        FindEnemyIndexBySpawnId(enemies, requiredEnemyId);

    if (requiredIndex < 0 ||
        requiredIndex >= static_cast<int>(enemies.size()))
    {
        return false;
    }

    int hitIndex = -1;
    if (!FindFirstPistolHitEnemyIndex(
            player,
            targetWorld,
            enemies,
            walls,
            hitIndex))
    {
        std::cout << "Tutorial shot ignored: no enemy hit." << std::endl;
        return false;
    }

    if (hitIndex != requiredIndex)
    {
        std::cout << "Tutorial shot ignored: wrong target." << std::endl;
        return false;
    }

    return TryFirePistol(
        player,
        targetWorld,
        enemies,
        walls,
        soundParticles,
        bulletTrails,
        pistol,
        hasSuppressor);
}

void UpdateBulletTrails(
    std::vector<BulletTrail>& bulletTrails,
    float dt)
{
    for (auto& trail : bulletTrails)
    {
        trail.life -= dt;
    }

    bulletTrails.erase(
        std::remove_if(
            bulletTrails.begin(),
            bulletTrails.end(),
            [](const BulletTrail& trail)
            {
                return trail.life <= 0.0f;
            }),
        bulletTrails.end());
}

void DrawBulletTrails(
    SDL_Renderer* renderer,
    const std::vector<BulletTrail>& bulletTrails,
    const Camera2D& camera)
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (const auto& trail : bulletTrails)
    {
        float ratio =
            trail.maxLife > 0.0f
            ? ClampFloat(trail.life / trail.maxLife, 0.0f, 1.0f)
            : 0.0f;

        Uint8 alpha = static_cast<Uint8>(220.0f * ratio);

        SDL_SetRenderDrawColor(renderer, 255, 255, 255, alpha);

        SDL_Point start = camera.WorldToScreenPoint(trail.start);
        SDL_Point end = camera.WorldToScreenPoint(trail.end);

        SDL_RenderDrawLine(
            renderer,
            start.x,
            start.y,
            end.x,
            end.y);
    }
}

void ConsumeEnemyShotTrails(
    std::vector<Enemy>& enemies,
    std::vector<BulletTrail>& bulletTrails)
{
    for (auto& enemy : enemies)
    {
        if (!enemy.shotTrailPending)
        {
            continue;
        }

        bulletTrails.push_back(
        {
            enemy.shotTrailStart,
            enemy.shotTrailEnd,
            BULLET_TRAIL_LIFETIME,
            BULLET_TRAIL_LIFETIME
        });

        enemy.fireVisualTimer = 0.16f;
        enemy.shotTrailPending = false;
        enemy.shotTrailStart = { 0.0f, 0.0f };
        enemy.shotTrailEnd = { 0.0f, 0.0f };
    }
}

static void UpdateEnemyVisualTimers(
    std::vector<Enemy>& enemies,
    float dt)
{
    for (auto& enemy : enemies)
    {
        if (enemy.fireVisualTimer > 0.0f)
        {
            enemy.fireVisualTimer -= dt;

            if (enemy.fireVisualTimer < 0.0f)
            {
                enemy.fireVisualTimer = 0.0f;
            }
        }
    }
}

void DrawGunHUD(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const PlayerGunState& pistol,
    bool hasSuppressor)
{
    SDL_Rect slot = { 16, 68, 118, 32 };

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 180);
    SDL_RenderFillRect(renderer, &slot);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 220);
    SDL_RenderDrawRect(renderer, &slot);

    std::string text =
        "P " + std::to_string(pistol.ammo) +
        "/" + std::to_string(PISTOL_MAGAZINE_SIZE);

    if (hasSuppressor)
    {
        text += " S";
    }

    DrawTextInRect(
        renderer,
        font,
        text.c_str(),
        slot,
        { 255, 255, 255, 255 });
}

static constexpr int CHARACTER_SHEET_COLS = 4;
static constexpr int CHARACTER_SHEET_ROWS = 4;
static constexpr int CHARACTER_SHEET_FRAME_W = 512;
static constexpr int CHARACTER_SHEET_FRAME_H = 512;

static constexpr int SHEET_ROW_DOWN = 0;
static constexpr int SHEET_ROW_RIGHT = 1;
static constexpr int SHEET_ROW_UP = 2;
static constexpr int SHEET_ROW_LEFT = 3;

static SDL_Rect GetCharacterSheetFrame(
    SDL_Texture* texture,
    int row,
    int frame)
{
    if (!texture)
    {
        return { 0, 0, 1, 1 };
    }

    int texW = 0;
    int texH = 0;

    SDL_QueryTexture(
        texture,
        nullptr,
        nullptr,
        &texW,
        &texH);

    if (texW <= 0 || texH <= 0)
    {
        return { 0, 0, 1, 1 };
    }

    int frameW =
        texW / CHARACTER_SHEET_COLS;

    int frameH =
        texH / CHARACTER_SHEET_ROWS;

    frame %= CHARACTER_SHEET_COLS;

    if (frame < 0)
    {
        frame += CHARACTER_SHEET_COLS;
    }

    row = std::clamp(
        row,
        0,
        CHARACTER_SHEET_ROWS - 1);

    return
    {
        frame * frameW,
        row * frameH,
        frameW,
        frameH
    };
}

static int GetSpriteRowFromAngle(
    float angle)
{
    float x =
        std::cos(angle);

    float y =
        std::sin(angle);

    if (std::fabs(x) >= std::fabs(y))
    {
        return x >= 0.0f
            ? SHEET_ROW_LEFT
            : SHEET_ROW_RIGHT;
    }

    return y >= 0.0f
        ? SHEET_ROW_DOWN
        : SHEET_ROW_UP;
}

static bool IsPlayerMoveScancode(
    SDL_Scancode scancode)
{
    return
        scancode == SDL_SCANCODE_W ||
        scancode == SDL_SCANCODE_A ||
        scancode == SDL_SCANCODE_S ||
        scancode == SDL_SCANCODE_D;
}

static Vec2 GetVisualDirFromMoveScancode(
    SDL_Scancode scancode)
{
    switch (scancode)
    {
    case SDL_SCANCODE_W:
        return { 0.0f, -1.0f };

    case SDL_SCANCODE_S:
        return { 0.0f, 1.0f };

    case SDL_SCANCODE_A:
        return { -1.0f, 0.0f };

    case SDL_SCANCODE_D:
        return { 1.0f, 0.0f };

    default:
        return { 0.0f, 0.0f };
    }
}

static Vec2 MakePlayerVisualMoveDir(
    const Uint8* key,
    Vec2 movementDir)
{
    if (lastPlayerMoveFacingKey != SDL_SCANCODE_UNKNOWN &&
        key[lastPlayerMoveFacingKey])
    {
        return GetVisualDirFromMoveScancode(
            lastPlayerMoveFacingKey);
    }

    if (key[SDL_SCANCODE_W] && !key[SDL_SCANCODE_S])
    {
        return { 0.0f, -1.0f };
    }

    if (key[SDL_SCANCODE_S] && !key[SDL_SCANCODE_W])
    {
        return { 0.0f, 1.0f };
    }

    if (key[SDL_SCANCODE_A] && !key[SDL_SCANCODE_D])
    {
        return { -1.0f, 0.0f };
    }

    if (key[SDL_SCANCODE_D] && !key[SDL_SCANCODE_A])
    {
        return { 1.0f, 0.0f };
    }

    return movementDir;
}

static float GetPlayerAnimFrameDuration(
    MoveMode mode)
{
    if (mode == SNEAK)
    {
        return 0.22f;
    }

    if (mode == WALK)
    {
        return 0.14f;
    }

    if (mode == RUN)
    {
        return 0.08f;
    }

    if (mode == INJURED)
    {
        return 0.18f;
    }

    return 0.14f;
}

static void UpdatePlayerVisualState(
    MoveMode mode,
    bool moving,
    Vec2 moveDir,
    float dt)
{
    if (playerFireVisualTimer > 0.0f)
    {
        playerFireVisualTimer -= dt;

        if (playerFireVisualTimer < 0.0f)
        {
            playerFireVisualTimer = 0.0f;
        }
    }

    if (moving && LengthSq(moveDir) > 0.000001f)
    {
        playerVisualFacingAngle =
            std::atan2(moveDir.y, moveDir.x);
    }

    if (!moving)
    {
        playerAnimTimer = 0.0f;
        playerAnimFrame = 0;
        return;
    }

    playerAnimTimer += dt;

    float frameDuration =
        GetPlayerAnimFrameDuration(mode);

    while (playerAnimTimer >= frameDuration)
    {
        playerAnimTimer -= frameDuration;
        playerAnimFrame =
            (playerAnimFrame + 1) % CHARACTER_SHEET_COLS;
    }
}

static void StartPlayerFireVisual(
    const SDL_Rect& player,
    Vec2 targetWorld)
{
    Vec2 playerCenter =
        GetRectCenter(player);

    Vec2 toTarget =
        targetWorld - playerCenter;

    if (LengthSq(toTarget) > 0.000001f)
    {
        playerFireVisualAngle =
            std::atan2(toTarget.y, toTarget.x);

        playerVisualFacingAngle =
            playerFireVisualAngle;
    }

    playerFireVisualTimer =
        PLAYER_FIRE_VISUAL_DURATION;
}

static bool TryDrawPlayerTexture(
    SDL_Renderer* renderer,
    const SDL_Rect& player,
    const Camera2D& camera,
    const GameSpriteTextures& textures,
    int playerHP,
    GameState gameState)
{
    SDL_Rect playerScreen =
        camera.WorldToScreenRect(player);

    if (!IsScreenRectVisible(playerScreen))
    {
        return true;
    }

    if ((playerHP <= 0 || gameState == LOSE) &&
        textures.playerDead)
    {
        SDL_Rect deadWorld =
            MakeCenteredWorldRectFromSource(
                player,
                textures.playerDeadSrc,
                64);

        SDL_Rect deadScreen = camera.WorldToScreenRect(deadWorld);

        SDL_RenderCopy(
            renderer,
            textures.playerDead,
            &textures.playerDeadSrc,
            &deadScreen);

        return true;
    }

    if (playerFireVisualTimer > 0.0f &&
        textures.playerFire)
    {
        SDL_Rect fireWorld =
            MakeCenteredWorldRectFromSource(
                player,
                textures.playerFireSrc,
                PLAYER_FIRE_RENDER_LONG_SIDE);

        SDL_Rect fireScreen = camera.WorldToScreenRect(fireWorld);

        SDL_Point center =
        {
            fireScreen.w / 2,
            fireScreen.h / 2
        };

        double angleDeg =
            static_cast<double>(playerFireVisualAngle) *
            180.0 / 3.14159265358979323846;

        SDL_RenderCopyEx(
            renderer,
            textures.playerFire,
            &textures.playerFireSrc,
            &fireScreen,
            angleDeg,
            &center,
            SDL_FLIP_NONE);

        return true;
    }

    if (textures.spritesheetPlayer)
    {
        int row = GetSpriteRowFromAngle(playerVisualFacingAngle);

        SDL_Rect source =
            GetCharacterSheetFrame(
                textures.spritesheetPlayer,
                row,
                playerAnimFrame);

        SDL_Rect spriteWorld =
            MakeCenteredWorldRectBySize(
                player,
                PLAYER_SPRITE_RENDER_SIZE,
                PLAYER_SPRITE_RENDER_SIZE);

        SDL_Rect spriteScreen = camera.WorldToScreenRect(spriteWorld);

        SDL_RenderCopy(
            renderer,
            textures.spritesheetPlayer,
            &source,
            &spriteScreen);

        return true;
    }

    return false;
}

static float Clamp01(float value)
{
    if (value < 0.0f) return 0.0f;
    if (value > 1.0f) return 1.0f;
    return value;
}

void DrawHealthBar(
    SDL_Renderer* renderer,
    int x,
    int y,
    int width,
    int height,
    float hpRatio)
{
    if (width <= 2 || height <= 2)
    {
        return;
    }

    hpRatio = Clamp01(hpRatio);

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_SetRenderDrawColor(renderer, 40, 40, 40, 200);
    SDL_Rect bg = { x, y, width, height };
    SDL_RenderFillRect(renderer, &bg);

    int r = static_cast<int>(255.0f * (1.0f - hpRatio));
    int g = static_cast<int>(255.0f * hpRatio);

    SDL_SetRenderDrawColor(renderer, r, g, 0, 255);
    SDL_Rect hp =
    {
        x + 1,
        y + 1,
        static_cast<int>((width - 2) * hpRatio),
        height - 2
    };
    SDL_RenderFillRect(renderer, &hp);

    // Outline
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 180);
    SDL_RenderDrawRect(renderer, &bg);
}

void DrawAlarmSirenIndicator(
    SDL_Renderer* renderer,
    TTF_Font* font)
{
    if (!renderer || !font)
    {
        return;
    }
    static constexpr const char* ALARM_SIREN_PLACEHOLDER_TEXT = "Alert";

    static constexpr int ALARM_SIREN_MARGIN_X = 18;
    static constexpr int ALARM_SIREN_MARGIN_Y = 10;

    SDL_Color alarmColor = { 255, 40, 40, 255 };
    SDL_Surface* surface = TTF_RenderText_Solid(font, ALARM_SIREN_PLACEHOLDER_TEXT, alarmColor);

    if (!surface)
    {
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

    if (!texture)
    {
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect dst;

    dst.w = surface->w;
    dst.h = surface->h;
    dst.x = SCREEN_WIDTH - dst.w - ALARM_SIREN_MARGIN_X;
    dst.y = ALARM_SIREN_MARGIN_Y;

    SDL_RenderCopy(renderer, texture, NULL, &dst);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
}

void DrawPauseOverlay(SDL_Renderer* renderer, TTF_Font* font)
{
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 120);
    SDL_Rect overlay = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
    SDL_RenderFillRect(renderer, &overlay);
    DrawCenteredText(renderer, font, "PAUSED");
}

static bool RectsOverlap(
    const SDL_Rect& a,
    const SDL_Rect& b)
{
    if (a.x + a.w < b.x)
    {
        return false;
    }

    if (b.x + b.w < a.x)
    {
        return false;
    }

    if (a.y + a.h < b.y)
    {
        return false;
    }

    if (b.y + b.h < a.y)
    {
        return false;
    }

    return true;
}

static bool IsScreenRectVisible(const SDL_Rect& rect)
{
    SDL_Rect screen =
    {
        0,
        0,
        SCREEN_WIDTH,
        SCREEN_HEIGHT
    };

    return RectsOverlap(rect, screen);
}

static void DrawCabinetSprites(
    SDL_Renderer* renderer,
    const Camera2D& camera,
    const GameSpriteTextures& textures)
{
    for (const auto& interactable : stageInteractables)
    {
        if (interactable.id != "cabinet")
        {
            continue;
        }

        SDL_Rect screenRect =
            camera.WorldToScreenRect(interactable.rect);

        if (!IsScreenRectVisible(screenRect))
        {
            continue;
        }

        if (textures.cabinet)
        {
            SDL_RenderCopy(
                renderer,
                textures.cabinet,
                &textures.cabinetSrc,
                &screenRect);
        }
        else
        {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

            SDL_SetRenderDrawColor(renderer, 70, 55, 35, 255);
            SDL_RenderFillRect(renderer, &screenRect);

            SDL_SetRenderDrawColor(renderer, 180, 145, 90, 255);
            SDL_RenderDrawRect(renderer, &screenRect);
        }
    }
}

static constexpr int CORPSE_RENDER_LONG_SIDE = 72;

static SDL_Texture* GetCorpseTexture(
    const GameSpriteTextures& textures,
    const Enemy& enemy,
    SDL_Rect& outSource)
{
    bool killedByGun =
        !enemy.bodyDraggable;

    if (enemy.kind == EnemyKind::Officer)
    {
        if (killedByGun)
        {
            outSource = textures.officerDeadBSrc;
            return textures.officerDeadB;
        }

        outSource = textures.officerDeadSrc;
        return textures.officerDead;
    }

    if (killedByGun)
    {
        outSource = textures.enemyDeadBSrc;
        return textures.enemyDeadB;
    }

    outSource = textures.enemyDeadSrc;
    return textures.enemyDead;
}

static bool TryDrawCorpseTexture(
    SDL_Renderer* renderer,
    const Enemy& enemy,
    const Camera2D& camera,
    const GameSpriteTextures& textures)
{
    if (enemy.state != EnemyState::Dead)
    {
        return false;
    }

    if (enemy.bodyHidden)
    {
        return true;
    }

    SDL_Rect source = {0, 0, 1, 1};

    SDL_Texture* texture =
        GetCorpseTexture(
            textures,
            enemy,
            source);

    if (!texture)
    {
        return false;
    }

    SDL_Rect corpseWorld =
        MakeCenteredWorldRectFromSource(
            enemy.rect,
            source,
            CORPSE_RENDER_LONG_SIDE);

    SDL_Rect corpseScreen =
        camera.WorldToScreenRect(corpseWorld);

    if (!IsScreenRectVisible(corpseScreen))
    {
        return true;
    }

    SDL_RenderCopy(
        renderer,
        texture,
        &source,
        &corpseScreen);

    return true;
}

static SDL_Rect MakeEnemyFOVWorldBounds(const Enemy& enemy)
{
    Vec2 center = GetRectCenter(enemy.rect);

    int range =
        static_cast<int>(std::ceil(enemy.viewDist)) + 96;

    return
    {
        static_cast<int>(center.x) - range,
        static_cast<int>(center.y) - range,
        range * 2,
        range * 2
    };
}

static void CollectWallsForFOV(
    const std::vector<Wall>& walls,
    const SDL_Rect& fovBounds,
    std::vector<Wall>& outWalls)
{
    outWalls.clear();

    for (const auto& wall : walls)
    {
        if (!RectsOverlap(wall.rect, fovBounds))
        {
            continue;
        }

        outWalls.push_back(wall);
    }
}

static SDL_Rect MakeCenteredWorldRect(
    const SDL_Rect& base,
    int width,
    int height)
{
    Vec2 center = GetRectCenter(base);

    return
    {
        static_cast<int>(std::round(center.x - width * 0.5f)),
        static_cast<int>(std::round(center.y - height * 0.5f)),
        width,
        height
    };
}

static bool DrawTextureInWorldRect(
    SDL_Renderer* renderer,
    SDL_Texture* texture,
    const SDL_Rect& worldRect,
    const Camera2D& camera,
    int padding = 0)
{
    if (!texture)
    {
        return false;
    }

    SDL_Rect screenRect =
        camera.WorldToScreenRect(worldRect);

    if (!IsScreenRectVisible(screenRect))
    {
        return true;
    }

    SDL_Rect dst =
        MakeAspectFitRect(texture, screenRect, padding);

    SDL_RenderCopy(renderer, texture, nullptr, &dst);

    return true;
}

static SDL_Texture* GetWorldItemMapTexture(
    const GameSpriteTextures& textures,
    ItemType type)
{
    switch (type)
    {
    case ItemType::Bottle:
        return textures.bottleMap;

    case ItemType::Key:
        return textures.keyMap;

    case ItemType::Target:
        return textures.codebookMap;
    }

    return nullptr;
}

static SDL_Texture* GetEtcTexture(
    const GameSpriteTextures& textures,
    const StageEtcDef& etc)
{
    std::string key = etc.name;

    if (key.empty())
    {
        key = etc.type;
    }

    if (key == "bookshelf")
    {
        return textures.bookshelf;
    }

    if (key == "chair")
    {
        return textures.chair;
    }

    if (key == "table")
    {
        return textures.table;
    }

    if (key == "typewriter")
    {
        return textures.typewriter;
    }

    if (key == "bed")
    {
        return textures.bed;
    }

    return nullptr;
}

static void DrawFloor(
    SDL_Renderer* renderer,
    SDL_Texture* floorTexture,
    Camera2D& camera)
{
    for (const auto& tileWorld : reachableFloorTiles)
    {
        SDL_Rect tileScreen =
            camera.WorldToScreenRect(tileWorld);

        if (!IsScreenRectVisible(tileScreen))
        {
            continue;
        }

        if (floorTexture)
        {
            SDL_RenderCopy(
                renderer,
                floorTexture,
                nullptr,
                &tileScreen);
        }
        else
        {
            SDL_SetRenderDrawColor(renderer, 28, 28, 28, 255);
            SDL_RenderFillRect(renderer, &tileScreen);
        }
    }
}

static void DrawWorldItemSprites(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const std::vector<WorldItem>& items,
    const Camera2D& camera,
    const GameSpriteTextures& textures)
{
    for (const auto& item : items)
    {
        if (item.picked)
        {
            continue;
        }

        SDL_Rect itemWorldRect =
            MakeCenteredWorldRect(item.rect, 32, 32);

        SDL_Rect screenRect =
            camera.WorldToScreenRect(itemWorldRect);

        if (!IsScreenRectVisible(screenRect))
        {
            continue;
        }

        SDL_Texture* texture =
            GetWorldItemMapTexture(textures, item.type);

        if (texture)
        {
            SDL_Rect dst =
                MakeAspectFitRect(texture, screenRect, 0);

            SDL_RenderCopy(renderer, texture, nullptr, &dst);
        }
        else
        {
            SDL_SetRenderDrawColor(renderer, 230, 230, 230, 255);
            SDL_RenderFillRect(renderer, &screenRect);

            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &screenRect);

            DrawTextInRect(
                renderer,
                font,
                GetItemLetter(item.type),
                screenRect,
                { 20, 20, 20, 255 });
        }
    }
}

static double GetDirectionAngleDegrees(
    const std::string& dir)
{
    if (dir == "right")
    {
        return 0.0;
    }

    if (dir == "down")
    {
        return 90.0;
    }

    if (dir == "left")
    {
        return 180.0;
    }

    if (dir == "up")
    {
        return -90.0;
    }

    return 0.0;
}

static bool IsEtcNamed(
    const StageEtcDef& etc,
    const std::string& name)
{
    return etc.name == name;
}

static const StageEtcDef* FindNearestTable(
    const StageEtcDef& source)
{
    const StageEtcDef* nearest = nullptr;
    float nearestDistSq = 999999999.0f;

    Vec2 sourceCenter =
        GetRectCenter(source.rect);

    for (const auto& etc : stageEtcs)
    {
        if (!IsEtcNamed(etc, "table"))
        {
            continue;
        }

        Vec2 tableCenter =
            GetRectCenter(etc.rect);

        float distSq =
            DistanceSqLocal(sourceCenter, tableCenter);

        if (distSq < nearestDistSq)
        {
            nearest = &etc;
            nearestDistSq = distSq;
        }
    }

    return nearest;
}

static constexpr double ETC_RAD_TO_DEG =
    180.0 / 3.14159265358979323846;

static constexpr double CHAIR_TEXTURE_FORWARD_DEG = 90.0;

static constexpr double TYPEWRITER_TEXTURE_FORWARD_DEG = 90.0;

static double GetCardinalAngleToTarget(
    Vec2 from,
    Vec2 to)
{
    Vec2 delta =
        to - from;

    if (std::fabs(delta.x) >= std::fabs(delta.y))
    {
        return delta.x >= 0.0f
            ? 0.0
            : 180.0;
    }

    return delta.y >= 0.0f
        ? 90.0
        : -90.0;
}

static double GetChairRotationDegrees(
    const StageEtcDef& chair)
{
    const StageEtcDef* table =
        FindNearestTable(chair);

    if (table)
    {
        Vec2 chairCenter =
            GetRectCenter(chair.rect);

        Vec2 tableCenter =
            GetRectCenter(table->rect);

        double worldDeg =
            GetCardinalAngleToTarget(
                chairCenter,
                tableCenter);

        return worldDeg - CHAIR_TEXTURE_FORWARD_DEG;
    }

    return
        GetDirectionAngleDegrees(chair.dir) -
        CHAIR_TEXTURE_FORWARD_DEG;
}

static double GetEtcRotationDegrees(
    const StageEtcDef& etc)
{
    if (etc.name == "chair")
    {
        return GetChairRotationDegrees(etc);
    }

    if (etc.name == "typewriter")
    {
        return
            GetDirectionAngleDegrees(etc.dir) -
            TYPEWRITER_TEXTURE_FORWARD_DEG;
    }

    return 0.0;
}

static bool ShouldRotateEtc(
    const StageEtcDef& etc)
{
    return
        etc.name == "chair" ||
        etc.name == "typewriter";
}

static void DrawEtcSprites(
    SDL_Renderer* renderer,
    const Camera2D& camera,
    const GameSpriteTextures& textures)
{
    for (const auto& etc : stageEtcs)
    {
        SDL_Rect screenRect =
            camera.WorldToScreenRect(etc.rect);

        if (!IsScreenRectVisible(screenRect))
        {
            continue;
        }

        SDL_Texture* texture =
            GetEtcTexture(textures, etc);

        if (texture)
        {
            if (ShouldRotateEtc(etc))
            {
                SDL_Point center =
                {
                    screenRect.w / 2,
                    screenRect.h / 2
                };
                SDL_RenderCopyEx(
                    renderer,
                    texture,
                    nullptr,
                    &screenRect,
                    GetEtcRotationDegrees(etc),
                    &center,
                    SDL_FLIP_NONE);
            }
            else
            {
                SDL_RenderCopy(
                    renderer,
                    texture,
                    nullptr,
                    &screenRect);
            }
        }
        else
        {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 85, 65, 45, 220);
            SDL_RenderFillRect(renderer, &screenRect);

            SDL_SetRenderDrawColor(renderer, 160, 130, 90, 255);
            SDL_RenderDrawRect(renderer, &screenRect);
        }
    }
}

static constexpr double RAD_TO_DEG =
    180.0 / 3.14159265358979323846;

static constexpr double ENEMY_TEXTURE_FORWARD_OFFSET_DEG = 90.0;

static bool ShouldUseEnemyTexture(const Enemy& enemy)
{
    if (enemy.state == EnemyState::Dead)
    {
        return false;
    }

    if (enemy.kind == EnemyKind::Officer)
    {
        return false;
    }

    return true;
}

static double GetEnemyTextureAngleDegrees(const Enemy& enemy)
{
    float visualAngle =
        enemy.angle + enemy.headSweepOffset;

    return
        static_cast<double>(visualAngle) *
        RAD_TO_DEG +
        ENEMY_TEXTURE_FORWARD_OFFSET_DEG;
}

static constexpr int ENEMY_TEXTURE_RENDER_WIDTH = 40;
static constexpr int ENEMY_TEXTURE_RENDER_HEIGHT = 72;

static bool TryDrawEnemyTexture(
    SDL_Renderer* renderer,
    const Enemy& enemy,
    const Camera2D& camera,
    const GameSpriteTextures& textures)
{
    if (!ShouldUseEnemyTexture(enemy))
    {
        return false;
    }

    if (!textures.enemy)
    {
        return false;
    }

    SDL_Rect enemyVisualWorld =
        MakeCenteredWorldRectBySize(
            enemy.rect,
            ENEMY_TEXTURE_RENDER_WIDTH,
            ENEMY_TEXTURE_RENDER_HEIGHT);

    SDL_Rect enemyScreen =
        camera.WorldToScreenRect(enemyVisualWorld);

    if (!IsScreenRectVisible(enemyScreen))
    {
        return true;
    }

    SDL_Point center =
    {
        enemyScreen.w / 2,
        enemyScreen.h / 2
    };

    SDL_Rect source =
        textures.enemySource;

    double angleDeg =
        GetEnemyTextureAngleDegrees(enemy);

    SDL_RenderCopyEx(
        renderer,
        textures.enemy,
        &source,
        &enemyScreen,
        angleDeg,
        &center,
        SDL_FLIP_NONE);

    return true;
}

static constexpr int OFFICER_SPRITE_RENDER_SIZE = 56;
static constexpr int OFFICER_FIRE_RENDER_LONG_SIDE = 76;
static constexpr double OFFICER_FIRE_FORWARD_OFFSET_DEG = 0.0;

static bool TryDrawOfficerTexture(
    SDL_Renderer* renderer,
    const Enemy& enemy,
    const Camera2D& camera,
    const GameSpriteTextures& textures)
{
    if (enemy.state == EnemyState::Dead)
    {
        return false;
    }

    if (enemy.kind != EnemyKind::Officer)
    {
        return false;
    }

    float visualAngle =
        enemy.angle;

    if (enemy.fireVisualTimer > 0.0f &&
        textures.officerFire)
    {
        SDL_Rect fireWorld =
            MakeCenteredWorldRectFromSource(
                enemy.rect,
                textures.officerFireSrc,
                OFFICER_FIRE_RENDER_LONG_SIDE);

        SDL_Rect fireScreen =
            camera.WorldToScreenRect(fireWorld);

        if (!IsScreenRectVisible(fireScreen))
        {
            return true;
        }

        SDL_Point center =
        {
            fireScreen.w / 2,
            fireScreen.h / 2
        };

        double angleDeg =
            static_cast<double>(visualAngle) *
            RAD_TO_DEG +
            OFFICER_FIRE_FORWARD_OFFSET_DEG;

        SDL_RenderCopyEx(
            renderer,
            textures.officerFire,
            &textures.officerFireSrc,
            &fireScreen,
            angleDeg,
            &center,
            SDL_FLIP_NONE);

        return true;
    }

    if (textures.spritesheetOfficer)
    {
        int row =
            GetSpriteRowFromAngle(visualAngle);

        int frame = 0;

        bool shouldAnimateWalk =
            (enemy.state == EnemyState::Patrol &&
            !enemy.patrolPoints.empty()) ||
            enemy.state == EnemyState::Investigate ||
            enemy.state == EnemyState::Return;

        if (shouldAnimateWalk)
        {
            frame =
                static_cast<int>(
                    (SDL_GetTicks() / 160) %
                    CHARACTER_SHEET_COLS);
        }

        SDL_Rect source =
            GetCharacterSheetFrame(
                textures.spritesheetOfficer,
                row,
                frame);

        SDL_Rect spriteWorld =
            MakeCenteredWorldRectBySize(
                enemy.rect,
                OFFICER_SPRITE_RENDER_SIZE,
                OFFICER_SPRITE_RENDER_SIZE);

        SDL_Rect spriteScreen =
            camera.WorldToScreenRect(spriteWorld);

        if (!IsScreenRectVisible(spriteScreen))
        {
            return true;
        }

        SDL_RenderCopy(
            renderer,
            textures.spritesheetOfficer,
            &source,
            &spriteScreen);

        return true;
    }

    return false;
}

bool ShouldShowAlarmSirenIndicator(
    bool alarmActive,
    const std::vector<Enemy>& enemies)
{
    if (alarmActive)
    {
        return true;
    }

    for (const auto& enemy : enemies)
    {
        if (enemy.state == EnemyState::Alert)
        {
            return true;
        }
    }

    return false;
}

void DrawStageText(SDL_Renderer* renderer, TTF_Font* font, int stage)
{
    std::string text;

    if (stage == 1)
    {
        text = "TUTORIAL";
    }
    else if (stage == 2)
    {
        text = "MAIN MISSION";
    }
    else
    {
        text = "MISSION " + std::to_string(stage - 1);
    }

    SDL_Color color = {255,255,255,255};

    TTF_Font* bigFont = TTF_OpenFont("unscii-16.ttf", 72);

    SDL_Surface* surface =
        TTF_RenderText_Solid(bigFont, text.c_str(), color);

    SDL_Texture* texture =
        SDL_CreateTextureFromSurface(renderer, surface);

    SDL_Rect dst;
    dst.w = surface->w;
    dst.h = surface->h;
    dst.x = SCREEN_WIDTH/2 - dst.w/2;
    dst.y = SCREEN_HEIGHT/2 - dst.h/2;

    SDL_RenderCopy(renderer, texture, NULL, &dst);

    SDL_FreeSurface(surface);
    SDL_DestroyTexture(texture);
    TTF_CloseFont(bigFont);
}

static constexpr float TUTORIAL_SUPPRESSOR_AUTO_MOVE_SPEED = 540.0f;

static bool UpdateTutorialSuppressorAutoPickup(
    SDL_Rect& player,
    const std::vector<Wall>& playerCollisionWalls,
    SuppressorPickup& suppressor,
    Equipment& equipment,
    PlayerGunState& pistol,
    TutorialController& tutorial,
    float dt)
{
    if (!tutorial.IsSuppressorAutoPickupActive())
    {
        return false;
    }

    if (equipment.hasSuppressor || suppressor.picked)
    {
        tutorial.NotifySuppressorPickedUp(true);
        pistol.cooldown = 0.0f;
        return true;
    }

    Vec2 playerCenter = GetRectCenter(player);
    Vec2 targetCenter = GetRectCenter(suppressor.rect);

    Vec2 toTarget = targetCenter - playerCenter;
    float distSq = LengthSq(toTarget);

    if (distSq > 0.000001f)
    {
        float dist = std::sqrt(distSq);
        Vec2 dir = toTarget / dist;

        float step =
            TUTORIAL_SUPPRESSOR_AUTO_MOVE_SPEED * dt;

        if (step > dist)
        {
            step = dist;
        }

        MovePlayerWithCollisionResult(
            player,
            dir.x * step,
            dir.y * step,
            playerCollisionWalls);
    }

    bool picked = TryPickupSuppressor(
        player,
        suppressor,
        equipment);

    if (picked)
    {
        tutorial.NotifySuppressorPickedUp(true);
        pistol.cooldown = 0.0f;

        std::cout
            << "Suppressor auto-picked. Suppressed shot phase started."
            << std::endl;
    }

    return picked;
}

// =====================================================
// MAIN
// =====================================================

int main(int argc, char* args[])
{
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    if ((IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) != IMG_INIT_PNG)
    {
        std::cout
            << "IMG_Init failed: "
            << IMG_GetError()
            << std::endl;
    }

    SDL_Window* window =
        SDL_CreateWindow("Stealth Game",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            SCREEN_WIDTH,
            SCREEN_HEIGHT,
            SDL_WINDOW_SHOWN);

    SDL_Renderer* renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    TTF_Font* font = TTF_OpenFont("unscii-16.ttf", 48);

    if (!font)
    {
        std::cout << "Font load failed\n";
        IMG_Quit();
        TTF_Quit();
        SDL_Quit();
        return 0;
    }

    TTF_Font* uiFont = TTF_OpenFont("unscii-16.ttf", 24);
    
    if (!uiFont)
    {
        std::cout << "UI font load failed\n";
        TTF_CloseFont(font);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return 0;
    }

    InventoryIconTextures inventoryIcons;
    if (!LoadInventoryIconTextures(renderer, inventoryIcons))
    {
        std::cout
            << "Some inventory icons failed to load. "
            << "Fallback letters will be used."
            << std::endl;
    }

    GameSpriteTextures gameSprites;
    if (!LoadGameSpriteTextures(renderer, gameSprites))
    {
        std::cout
            << "Some game sprites failed to load. "
            << "Fallback rectangles will be used."
            << std::endl;
    }

    SDL_Rect player = {100,100,64,64};

    std::vector<Enemy> enemies;
    std::vector<SoundParticle> soundParticles;

    std::vector<WorldItem> worldItems;
    Inventory inventory;
    Equipment equipment;
    SuppressorPickup suppressorPickup;
    std::vector<BottleProjectile> bottleProjectiles;

    PlayerGunState pistol;
    std::vector<BulletTrail> bulletTrails;

    int draggedBodyIndex = -1;
    stage = 1;
    displayStage = 1;
    ResetStage(player, enemies);
    ResetLockedDoors();
    ResetWorldItems(worldItems);
    inventory = Inventory{};
    equipment = Equipment{};
    ResetSuppressorPickup(suppressorPickup, equipment);
    bottleProjectiles.clear();

    pistol = PlayerGunState{};
    bulletTrails.clear();

    Camera2D camera;
    camera.zoom = 1.0f;

    GameState gameState = PLAYING;
    Uint32 stateTimer = 0;
    Uint32 pauseStartTicks = 0;

    float soundTimer = 0.0f;
    float runWallSoundCooldown = 0.0f;

    int playerHP = PLAYER_MAX_HP;

    bool alarmActive = false;
    int handledBottleSoundEventId = 0;

    bool tutorialToStageJsonTransition = false;
    bool codebookRequiredMessagePrinted = false;
    bool finalMissionComplete = false;

    Uint64 prev = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();

    showStageText = true;
    stageTextStart = SDL_GetTicks();

    // =====================================================
    // GAME LOOP
    // =====================================================
    while (running)
    {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (float)(now - prev) / (float)freq;
        prev = now;

        if (dt > 0.05f) dt = 0.05f;

        SDL_Event e;
        bool interactPressed = false;
        bool bodyDragPressed = false;
        bool bottleThrowPressed = false;
        int bottleThrowScreenX = 0;
        int bottleThrowScreenY = 0;

        bool pistolFirePressed = false;
        int pistolFireScreenX = 0;
        int pistolFireScreenY = 0;

        bool continueYesPressed = false;
        bool continueNoPressed = false;

        if (e.type == SDL_KEYDOWN && !e.key.repeat)
        {
            SDL_Scancode scancode = e.key.keysym.scancode;
            if (IsPlayerMoveScancode(scancode))
            {
                lastPlayerMoveFacingKey = scancode;
            }
        }
        
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                running = false;
            }
            else if (e.type == SDL_KEYDOWN && !e.key.repeat && e.key.keysym.scancode == SDL_SCANCODE_E)
            {
                interactPressed = true;
            }
            else if (e.type == SDL_KEYDOWN && !e.key.repeat && e.key.keysym.scancode == SDL_SCANCODE_F)
            {
                bodyDragPressed = true;
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT)
            {
                bottleThrowPressed = true;
                bottleThrowScreenX = e.button.x;
                bottleThrowScreenY = e.button.y;
            }
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT)
            {
                pistolFirePressed = true;
                pistolFireScreenX = e.button.x;
                pistolFireScreenY = e.button.y;
            }
            else if (e.type == SDL_KEYDOWN && !e.key.repeat && e.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
            {
                if (gameState == PLAYING)
                {
                    gameState = PAUSED;
                    pauseStartTicks = SDL_GetTicks();
                }
                else if (gameState == PAUSED)
                {
                    gameState = PLAYING;
                    Uint32 pausedDuration = SDL_GetTicks() - pauseStartTicks;
                    // Stage 표시 시간이 pause 중에 흘러가 버리는 것을 방지
                    if (showStageText)
                    {
                        stageTextStart += pausedDuration;
                    }
                    pauseStartTicks = 0;
                }
            }
            else if (e.type == SDL_KEYDOWN && !e.key.repeat && e.key.keysym.scancode == SDL_SCANCODE_Y)
            {
                continueYesPressed = true;
            }
            else if (e.type == SDL_KEYDOWN && !e.key.repeat && e.key.keysym.scancode == SDL_SCANCODE_N)
            {
                continueNoPressed = true;
            }
        }

        auto walls = GetActiveWalls();
        auto actorCollisionWalls = GetActorCollisionWalls(walls);
        auto playerCollisionWalls = actorCollisionWalls;
        tutorial.AppendActivePlayerBlockers(playerCollisionWalls);

        if (gameState == CONTINUE_PROMPT)
        {
            if (continueYesPressed)
            {
                if (stage == 1)
                {
                    bool restored =
                        RestoreTutorialCheckpoint(
                            player,
                            inventory,
                            equipment,
                            pistol,
                            worldItems,
                            enemies,
                            lockedDoors,
                            tutorial,
                            playerHP,
                            alarmActive);
                            
                    bottleProjectiles.clear();
                    bulletTrails.clear();
                    draggedBodyIndex = -1;
                    soundParticles.clear();
                    
                    if (restored)
                    {
                        gameState = PLAYING;
                    }
                    else
                    {
                        gameState = LOSE;
                        stateTimer = SDL_GetTicks();
                    }
                }
                else
                {
                    ResetStage(player, enemies);
                    ResetLockedDoors();
                    ResetWorldItems(worldItems);

                    inventory = Inventory{};
                    equipment = Equipment{};
                    pistol = PlayerGunState{};

                    ResetSuppressorPickup(
                        suppressorPickup,
                        equipment);

                    bottleProjectiles.clear();
                    bulletTrails.clear();
                    draggedBodyIndex = -1;
                    soundParticles.clear();

                    soundTimer = 0.0f;
                    runWallSoundCooldown = 0.0f;

                    playerHP = PLAYER_MAX_HP;
                    injuredTimer = 0.0f;
                    alarmActive = false;
                    handledBottleSoundEventId = 0;
                    codebookRequiredMessagePrinted = false;
                    mainStageAnomalyTriggered = false;
                    mainStageReturnHintVisible = false;
                    finalMissionComplete = false;

                    gameState = PLAYING;
                }
            }
            else if (continueNoPressed)
            {
                gameState = LOSE;
                stateTimer = SDL_GetTicks();
            }
        }

        // =================================================
        // PLAYING
        // =================================================
        if (gameState == PLAYING)
        {
            if (showStageText)
            {
                Uint32 now = SDL_GetTicks();

                if (now - stageTextStart < 1200)
                {
                    SDL_SetRenderDrawColor(renderer, 20,20,20,255);
                    SDL_RenderClear(renderer);

                    DrawStageText(renderer, font, displayStage);

                    SDL_RenderPresent(renderer);
                    continue;
                }
                else
                {
                    showStageText = false;
                }
            }

            const Uint8* key = SDL_GetKeyboardState(NULL);

            const bool tutorialFrozen = (stage == 1 && tutorial.IsTutorialFreezeActive());
            const bool tutorialAutoMovingToSuppressor = (stage == 1 && tutorial.IsSuppressorAutoPickupActive());

            MoveMode mode = GetMoveMode(key, injuredTimer);
            if (tutorial.ShouldForceSneak() && mode != INJURED)
            {
                mode = SNEAK;
            }
            if (IsDraggingBody(enemies, draggedBodyIndex) && mode != INJURED)
            {
                mode = SNEAK;
            }
            float speed = GetMoveSpeed(mode);

            float dx = 0, dy = 0;
            
            if (!tutorialFrozen && !tutorialAutoMovingToSuppressor)
            {
                if (key[SDL_SCANCODE_W]) dy -= 1;
                if (key[SDL_SCANCODE_S]) dy += 1;
                if (key[SDL_SCANCODE_A]) dx -= 1;
                if (key[SDL_SCANCODE_D]) dx += 1;
            }

            float len = sqrtf(dx*dx + dy*dy);
            bool moving = len > 0;

            Vec2 dir = {0, 0};

            if (moving)
            {
                dir.x = dx / len;
                dir.y = dy / len;
            }

            Vec2 visualDir =
                moving
                ? MakePlayerVisualMoveDir(key, dir)
                : dir;

            dx = dir.x * speed * dt;
            dy = dir.y * speed * dt;

            bool hitWall =
                MovePlayerWithCollisionResult(player, dx, dy, playerCollisionWalls);

            tutorial.Update(player, mode, moving);

            UpdatePlayerVisualState(
                mode,
                moving,
                visualDir,
                dt);

            if (tutorialAutoMovingToSuppressor)
            {
                UpdateTutorialSuppressorAutoPickup(
                    player,
                    playerCollisionWalls,
                    suppressorPickup,
                    equipment,
                    pistol,
                    tutorial,
                    dt);
            }

            if (stage == 1 && tutorial.IsGunApproachActive())
            {
                StartTutorialGunScript(enemies);
            }

            if (tutorial.ShouldForceSneak() && mode != INJURED)
            {
                mode = SNEAK;
            }

            if (!tutorialFrozen && !tutorialAutoMovingToSuppressor && bodyDragPressed)
            {
                bool changed = ToggleBodyDrag(player, enemies, actorCollisionWalls, draggedBodyIndex);
                if (stage == 1)
                {
                    tutorial.NotifyBodyDragStarted(
                        changed && IsDraggingBody(enemies, draggedBodyIndex));
                }
            }
            if (!tutorialFrozen && !tutorialAutoMovingToSuppressor)
            {
                UpdateDraggedBody(enemies, draggedBodyIndex, player, dir, actorCollisionWalls, dt);
            }
            bool draggingBody = IsDraggingBody(enemies, draggedBodyIndex);

            if (interactPressed && draggingBody)
            {
                bool hidden = TryHideDraggedBodyInCabinet(
                    player,
                    enemies,
                    draggedBodyIndex);
                    
                if (stage == 1 && tutorial.IsBodyHideTrainingActive())
                {
                    tutorial.NotifyBodyHidden(hidden);
                }
            }

            //부상 관리

            if (injuredTimer > 0.0f)
            {
                injuredTimer -= dt;
            }

            // =========================================
            // 사운드 발생
            // =========================================

            soundTimer += dt;
            
            if (runWallSoundCooldown > 0.0f)
            {
                runWallSoundCooldown -= dt;
            }
            Vec2 playerCenter =
            {
                player.x + player.w * 0.5f,
                player.y + player.h * 0.5f
            };
            camera.zoom = 1.0f;
            camera.FollowImmediate(playerCenter.x, playerCenter.y);
            camera.ClampToBounds(currentMapWidth, currentMapHeight);
            if (pistol.cooldown > 0.0f)
            {
                pistol.cooldown -= dt;
            }
            UpdateBulletTrails(bulletTrails, dt);
            if (interactPressed && !draggingBody)
            {
                if (stage == 1 && tutorial.IsSuppressorPickupPaused())
                {
                    bool pickedSuppressor = TryPickupSuppressor(player, suppressorPickup, equipment);
                    tutorial.NotifySuppressorPickedUp(pickedSuppressor);
                    if (pickedSuppressor)
                    {
                        pistol.cooldown = 0.0f;
                        std::cout << "Suppressor equipped for tutorial shot." << std::endl;
                    }
                }
                else
                {
                    const bool wasWireTraining =
                        (stage == 1 &&
                            (tutorial.IsWireTrainingActive() ||
                            tutorial.IsCabinetWireTrainingActive()));
                    const bool wireAllowed = (stage != 1) || wasWireTraining;
                    const int wireBodyCountBefore = CountWireTakedownBodies(enemies);
                    bool usedWire = false;
                    if (wireAllowed)
                    {
                        usedWire = TryWireAttackEnemy(player, enemies, walls);
                    }
                    const bool wireTakedownConfirmed = usedWire && CountWireTakedownBodies(enemies) > wireBodyCountBefore;
                    tutorial.NotifyWireTakedown(wireTakedownConfirmed);
                    if (!usedWire && !wasWireTraining)
                    {
                        SDL_Rect openedDoorRect = { 0, 0, 0, 0 };
                        bool openedDoor =
                            TryOpenNearestLockedDoor(
                                player,
                                lockedDoors,
                                inventory,
                                &openedDoorRect);
                        if (stage == 1 && openedDoor)
                        {
                            const bool checkpointDoor =
                                IsTutorialCodebookDoorRect(openedDoorRect);
                            tutorial.NotifyLockedDoorOpened(true);
                            if (checkpointDoor)
                            {
                                alarmActive = true;
                                tutorial.UnlockPistol();
                                pistol.cooldown = 0.0f;
                                StartTutorialCabinetAlert(enemies, player);
                                SaveTutorialCheckpoint(
                                    player,
                                    inventory,
                                    equipment,
                                    pistol,
                                    worldItems,
                                    enemies,
                                    lockedDoors,
                                    tutorial,
                                    playerHP,
                                    alarmActive);
                                std::cout
                                    << "Codebook door opened: officer alert started."
                                    << std::endl;
                            }
                        }
                        if (!openedDoor)
                        {
                            bool pickedSuppressor = TryPickupSuppressor(player, suppressorPickup, equipment);
                            if (!pickedSuppressor)
                            {
                                ItemType pickedType = ItemType::Bottle;
                                bool pickedItem = TryPickupNearestItem(
                                    player,
                                    worldItems,
                                    inventory,
                                    &pickedType);
                                if (stage == 1 && pickedItem)
                                {
                                    if (pickedType == ItemType::Bottle)
                                    {
                                        tutorial.NotifyBottlePickedUp(true);
                                    }
                                    else if (pickedType == ItemType::Key)
                                    {
                                        tutorial.NotifyKeyPickedUp(true);
                                    }
                                    else if (pickedType == ItemType::Target)
                                    {
                                        tutorial.NotifyCodebookPickedUp(true);
                                    }
                                }
                                else if (stage != 1 && pickedItem && pickedType == ItemType::Target)
                                {
                                    TriggerMainStageAnomaly(enemies);
                                }
                            }
                        }
                    }
                }
            }
            if (pistolFirePressed && !draggingBody)
            {
                Vec2 pistolTargetWorld = camera.ScreenToWorldPoint(
                    pistolFireScreenX,
                    pistolFireScreenY);
                if (stage == 1 && tutorial.IsGunFirstShotPaused())
                {
                    bool fired = TryFireTutorialPistolAtEnemyId(
                        player,
                        pistolTargetWorld,
                        enemies,
                        walls,
                        soundParticles,
                        bulletTrails,
                        pistol,
                        false,
                        "guard_gun");
                    if (fired)
                    {
                        StartPlayerFireVisual(
                            player,
                            pistolTargetWorld);
                    }
                    
                    const bool killed = fired && IsEnemyDeadBySpawnId(enemies, "guard_gun");
                    if (killed)
                    {
                        DropTutorialSuppressorFromEnemy(
                            player,
                            enemies,
                            suppressorPickup,
                            "guard_gun");
                        tutorial.NotifyGunGuardKilled(true);
                    }
                }
                else if (stage == 1 && tutorial.IsGunSuppressedShotPaused())
                {
                    bool fired = TryFireTutorialPistolAtEnemyId(
                        player,
                        pistolTargetWorld,
                        enemies,
                        walls,
                        soundParticles,
                        bulletTrails,
                        pistol,
                        equipment.hasSuppressor,
                        "guard_suppressor");
                        
                    const bool killed = fired && IsEnemyDeadBySpawnId(enemies, "guard_suppressor");
                    if (killed)
                    {
                        DropTutorialKeyFromEnemy(
                            player,
                            enemies,
                            worldItems,
                            "guard_suppressor");
                            
                        tutorial.NotifySuppressorGuardKilled(true);
                    }
                }
                else
                {
                    const bool pistolAllowed = (stage != 1) || tutorial.IsPistolUnlocked();
                    if (pistolAllowed)
                    {
                        bool fired = 
                            TryFirePistol(
                                player,
                                pistolTargetWorld,
                                enemies,
                                walls,
                                soundParticles,
                                bulletTrails,
                                pistol,
                                equipment.hasSuppressor);
                        if (fired)
                        {
                            StartPlayerFireVisual(
                                player,
                                pistolTargetWorld);
                        }
                    }
                    else
                    {
                        std::cout << "Don't shoot.\n";
                    }
                }
            }
            if (bottleThrowPressed && !draggingBody && !tutorialFrozen)
            {
                Vec2 bottleTargetWorld = camera.ScreenToWorldPoint(
                    bottleThrowScreenX,
                    bottleThrowScreenY);
                const bool stageOneBottleTutorial = (stage == 1);
                const bool bottleThrowTraining = stageOneBottleTutorial && tutorial.IsBottleThrowTrainingActive();
                
                if (stageOneBottleTutorial && !bottleThrowTraining)
                {
                    std::cout << "Bottle throw is locked until the bottle tutorial.\n";
                }
                else if (bottleThrowTraining &&
                    !tutorial.CanThrowBottleAt(bottleTargetWorld))
                {
                    std::cout << "Throw ignored: target is outside highlighted corridor.\n";
                }
                else
                {
                    const bool tutorialLureBottle = bottleThrowTraining;
                    bool thrown = TryThrowBottle(
                        player,
                        bottleTargetWorld,
                        inventory,
                        bottleProjectiles,
                        tutorialLureBottle);

                    if (bottleThrowTraining)
                    {
                        tutorial.NotifyBottleThrown(thrown);
                    }
                }
            }
            if (moving && !tutorialFrozen)
            {
                if (mode == RUN)
                {
                    if (soundTimer >= 0.12f)
                    {
                        EmitSound(
                        soundParticles,
                        playerCenter,
                        40,
                        343.0f,
                        1.0f);
                        EmitSoundDirectional(
                        soundParticles,
                        playerCenter,
                        dir,
                        1.4f,
                        40,
                        343.0f,
                        1.0f);
                        soundTimer = 0.0f;
                    }
                    if (hitWall && runWallSoundCooldown <= 0.0f)
                    {
                        EmitSound(
                        soundParticles,
                        playerCenter,
                        40,
                        343.0f,
                        1.0f);
                        runWallSoundCooldown = 0.35f;
                    }
                }
                else if (mode == WALK)
                {
                    if (soundTimer >= 0.12f)
                    {
                        EmitSound(
                        soundParticles,
                        playerCenter,
                        20,
                        343.0f,
                        1.0f);
                        EmitSoundDirectional(
                        soundParticles,
                        playerCenter,
                        dir,
                        1.4f,
                        20,
                        343.0f,
                        1.0f);
                        soundTimer = 0.0f;
                    }
                }
            }
            else
            {
                soundTimer = 0.0f;
            }

            std::vector<Vec2> tutorialBottleBreaks;
            UpdateBottleProjectiles(
                bottleProjectiles,
                soundParticles,
                walls,
                dt,
                &tutorialBottleBreaks);
            for (const Vec2& breakPos : tutorialBottleBreaks)
            {
                if (tutorial.NotifyBottleBreakSound(breakPos))
                {
                    StartTutorialBottleGuardLure(enemies);
                }
            }

            if (!tutorialFrozen && UpdateTutorialGunScript(enemies, player, walls, dt))
            {
                tutorial.NotifyGunSightReached();
            }

            bool alarmTriggered = false;

            // =========================================
            // thread 출력/입력 버퍼 준비
            // =========================================
            
            // soundThread가 계산한 다음 프레임 사운드 파티클을 받을 버퍼
            std::vector<SoundParticle> particlesNext;
            
            // enemyThread가 player를 읽는 동안 main thread/player 원본과 꼬이지 않도록 snapshot 사용
            SDL_Rect playerSnapshot = player;
            
            // soundThread는 enemies 원본을 직접 읽지 않고 대신 현재 프레임의 적 위치/생존 여부만 복사한 snapshot을 읽음
            std::vector<EnemyAudioSnapshot> enemyAudioSnapshot;
            enemyAudioSnapshot.reserve(enemies.size());
            
            for (const auto& enemy : enemies)
            {
                EnemyAudioSnapshot snapshot;
                snapshot.rect = enemy.rect;
                snapshot.alive = (enemy.state != EnemyState::Dead);
                enemyAudioSnapshot.push_back(snapshot);
            }
            std::vector<HearingResult> hearingBuffer(enemyAudioSnapshot.size());

            // =========================================
            // 소음 물리 업데이트 thread
            // =========================================
            const bool tutorialStage = (stage == 1);
            std::thread soundThread(
                UpdateSoundParticles,
                std::cref(soundParticles),
                std::ref(particlesNext),
                std::cref(enemyAudioSnapshot),
                std::ref(hearingBuffer),
                std::cref(walls),
                dt);
                
            // =========================================
            // 적 FSM 업데이트 thread
            // =========================================
            std::thread enemyThread;
            if (!tutorialStage)
            {
                enemyThread = std::thread(
                    UpdateEnemies,
                    std::ref(enemies),
                    std::cref(playerSnapshot),
                    std::cref(actorCollisionWalls),
                    std::cref(walls),
                    alarmActive,
                    std::ref(alarmTriggered),
                    std::ref(playerHP),
                    std::ref(injuredTimer),
                    dt);
            }
                    
            soundThread.join();
                if (enemyThread.joinable())
                {
                    enemyThread.join();
                }

            if (UpdateTutorialBottleGuardLure(enemies, dt))
            {
                tutorial.NotifyBottleGuardArrived();
            }

            UpdateTutorialCabinetAlert(
                enemies,
                player,
                playerHP,
                injuredTimer,
                dt);
                
            if (stage == 1 &&
                tutorial.IsEscapeApproachActive() &&
                HasPlayerEnteredTutorialTrigger(player, "escape_intro"))
            {
                tutorial.NotifyEscapeStarted();
                StartTutorialEscapeGuardMove(enemies);
            }
            
            UpdateTutorialEscapeGuardMove(
                enemies,
                player,
                actorCollisionWalls,
                playerHP,
                injuredTimer,
                dt);

            UpdateEnemyVisualTimers(enemies, dt);
            ConsumeEnemyShotTrails(enemies, bulletTrails);
            ApplyOfficerDeathRewards(player, enemies, worldItems, pistol.ammo, PISTOL_MAGAZINE_SIZE);
            CleanUpParticles(soundParticles, particlesNext);
            
            if (alarmTriggered)
            {
                alarmActive = true;
            }

            if (!tutorialStage)
            {

                int bestBottleEnemyIndex = -1;
                int bestBottleEventId = 0;
                float bestBottleEnergy = 0.0f;
                Vec2 bestBottleNoisePos = { 0.0f, 0.0f };
                
                for (size_t i = 0; i < enemies.size() && i < hearingBuffer.size(); ++i)
                {
                    const HearingResult& hearing = hearingBuffer[i];
                    if (!hearing.heard)
                    {
                        continue;
                    }
                    // 병 소음은 유인용이므로 같은 병 이벤트당 가장 강하게 들은 경비 1명만 조사
                    if (hearing.kind == SoundKind::Bottle)
                    {
                        if (hearing.eventId == handledBottleSoundEventId)
                        {
                            continue;
                        }
                        if (alarmActive || enemies[i].state == EnemyState::Dead || enemies[i].state == EnemyState::Alert)
                        {
                            continue;
                        }
                        if (bestBottleEnemyIndex < 0 || hearing.energy > bestBottleEnergy)
                        {
                            bestBottleEnemyIndex = static_cast<int>(i);
                            bestBottleEventId = hearing.eventId;
                            bestBottleEnergy = hearing.energy;
                            bestBottleNoisePos = hearing.noisePos;
                        }
                        continue;
                    }
                    NotifyEnemyOfNoise(enemies[i], hearing.noisePos, hearing.energy, alarmActive);
                }
                if (bestBottleEnemyIndex >= 0)
                {
                    Enemy& bottleEnemy = enemies[bestBottleEnemyIndex];
                    NotifyEnemyOfNoise(bottleEnemy, bestBottleNoisePos, bestBottleEnergy, alarmActive);
                    if (bottleEnemy.hasPendingNoise)
                    {
                        handledBottleSoundEventId = bestBottleEventId;
                    }
                }
            }

            if (alarmTriggered)
            {
                alarmActive = true;
            }
            // ==============================
            // HP CHECK
            // ==============================
            if (playerHP <= 0)
            {
                if ((stage == 1 && tutorialCheckpoint.active) || stage != 1)
                {
                    gameState = CONTINUE_PROMPT;
                }
                else
                {
                    gameState = LOSE;
                    stateTimer = SDL_GetTicks();
                }
            }
            // ==============================
            // GOAL CHECK
            // ==============================
            bool hitGoal = SDL_HasIntersection(&player, &goalNormal) == SDL_TRUE;
            
            if (hitGoal)
            {
                if (!inventory.hasTarget)
                {
                    if (!codebookRequiredMessagePrinted)
                    {
                        ShowHudMessage("You need to obtain the codebook first.", 2500);
                        codebookRequiredMessagePrinted = true;
                    }
                }
                else if (stage == 1)
                {
                    codebookRequiredMessagePrinted = false;
                    tutorial.NotifyGoalReached();
                    tutorialToStageJsonTransition = true;
                    stage = 2;
                    gameState = WIN;
                    stateTimer = SDL_GetTicks();
                    std::cout << "Tutorial complete. Starting stage.json." << std::endl;
                }
                else
                {
                    codebookRequiredMessagePrinted = false;
                    
                    if (!mainStageAnomalyTriggered)
                    {
                        TriggerMainStageAnomaly(enemies);
                    }
                    else
                    {
                        finalMissionComplete = true;
                        gameState = WIN;
                        stateTimer = SDL_GetTicks();
                        std::cout << "Mission complete." << std::endl;
                    }
                }
            }
            else
            {
                codebookRequiredMessagePrinted = false;
            }
        }
        else if (gameState == WIN || gameState == LOSE)
        {
            if (gameState == WIN && finalMissionComplete)
            {

            }
            else if (SDL_GetTicks() - stateTimer >= 2000)
            {
                bool wasLose = (gameState == LOSE);
                bool wasTutorialToStageJsonTransition = tutorialToStageJsonTransition;
                
                ResetStage(player, enemies);
                ResetLockedDoors();
                ResetWorldItems(worldItems);
                
                if (wasLose || wasTutorialToStageJsonTransition)
                {
                    inventory = Inventory{};
                    equipment = Equipment{};
                    pistol = PlayerGunState{};
                    finalMissionComplete = false;
                }
                else
                {
                    pistol.cooldown = 0.0f;
                }
                ResetSuppressorPickup(suppressorPickup, equipment);
                
                bottleProjectiles.clear();
                bulletTrails.clear();
                draggedBodyIndex = -1;
                soundParticles.clear();
                
                soundTimer = 0.0f;
                runWallSoundCooldown = 0.0f;
                
                playerHP = PLAYER_MAX_HP;
                alarmActive = false;
                handledBottleSoundEventId = 0;
                codebookRequiredMessagePrinted = false;
                
                if (wasTutorialToStageJsonTransition)
                {
                    ClearTutorialCheckpoint();
                    tutorialToStageJsonTransition = false;
                }
                gameState = PLAYING;
            }
        }

        // =================================================
        // RENDER
        // =================================================
        SDL_SetRenderDrawColor(renderer, 20,20,20,255);
        SDL_RenderClear(renderer);

        DrawFloor(renderer, gameSprites.floor, camera);

        // =============================================
        // FOV
        // =============================================

        static std::vector<Wall> fovWalls;
        fovWalls.reserve(walls.size());
        for (auto& enemy : enemies)
        {
            if (enemy.state == EnemyState::Dead)
            {
                continue;
            }
            SDL_Rect fovWorldBounds = MakeEnemyFOVWorldBounds(enemy);
            SDL_Rect fovScreenBounds = camera.WorldToScreenRect(fovWorldBounds);
            if (!IsScreenRectVisible(fovScreenBounds))
            {
                continue;
            }

            CollectWallsForFOV(
                walls,
                fovWorldBounds,
                fovWalls);

            DrawFOV(
                renderer,
                enemy,
                fovWalls,
                camera);
        }

        // walls
        SDL_SetRenderDrawColor(renderer, 58, 58, 58, 255);

        for (auto& w : walls)
        {
            SDL_Rect r = camera.WorldToScreenRect(w.rect);
            if (!IsScreenRectVisible(r))
            {
                continue;
            }

            SDL_RenderFillRect(renderer, &r);
        }

        DrawUnopenableDoors(renderer, camera);
        DrawLockedDoors(renderer, uiFont, lockedDoors, camera);
        DrawCabinetSprites(renderer, camera, gameSprites);
        DrawEtcSprites(renderer, camera, gameSprites);

        DrawWorldItemSprites(renderer, uiFont, worldItems, camera, gameSprites);
        DrawSuppressorPickup(renderer, suppressorPickup, equipment, camera);
        DrawBottleProjectiles(renderer, bottleProjectiles, camera);
        DrawTutorialBottleThrowZone(renderer, camera);

        // player
        if (!TryDrawPlayerTexture(
            renderer,
            player,
            camera,
            gameSprites,
            playerHP,
            gameState))
        {
            SDL_Rect playerScreen = camera.WorldToScreenRect(player);
            if (IsScreenRectVisible(playerScreen))
            {
                SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
                SDL_RenderFillRect(renderer, &playerScreen);
            }
        }

        // =============================================
        // 적
        // =============================================

        for (auto& enemy : enemies)
        {
            if (TryDrawCorpseTexture(renderer, enemy, camera, gameSprites))
            {
                continue;
            }

            if (TryDrawOfficerTexture(renderer, enemy, camera, gameSprites))
            {
                continue;
            }

            if (TryDrawEnemyTexture(renderer, enemy, camera, gameSprites))
            {
                continue;
            }

            SDL_Rect enemyScreen = camera.WorldToScreenRect(enemy.rect);

            if (!IsScreenRectVisible(enemyScreen))
            {
                continue;
            }

            if (enemy.state == EnemyState::Dead)
            {
                if (enemy.bodyDragged)
                {
                    SDL_SetRenderDrawColor(
                        renderer,
                        120,
                        140,
                        210,
                        255);
                }
                else if (enemy.bodyDraggable)
                {
                    SDL_SetRenderDrawColor(
                        renderer,
                        80,
                        120,
                        170,
                        255);
                }
                else
                {
                    SDL_SetRenderDrawColor(
                        renderer,
                        105,
                        55,
                        55,
                        255);
                }
            }
            else if (enemy.kind == EnemyKind::Officer && enemy.alerted)
            {
                SDL_SetRenderDrawColor(
                    renderer,
                    210,
                    105,
                    85,
                    255);
            }
            else if (enemy.kind == EnemyKind::Officer)
            {
                SDL_SetRenderDrawColor(
                    renderer,
                    150,
                    55,
                    45,
                    255);
            }
            else if (enemy.alerted)
            {
                SDL_SetRenderDrawColor(
                    renderer,
                    255,
                    140,
                    0,
                    255);
            }
            else
            {
                SDL_SetRenderDrawColor(
                    renderer,
                    255,
                    0,
                    0,
                    255);
            }
            SDL_RenderFillRect(renderer, &enemyScreen);
        }

        DrawBulletTrails(renderer, bulletTrails, camera);

        // =============================================
        // 사운드 파티클
        // =============================================

        SDL_SetRenderDrawColor(
            renderer,
            100,
            180,
            255,
            255);

        for (auto& p : soundParticles)
        {
            SDL_Point particleScreen = camera.WorldToScreenPoint(p.pos);
            SDL_RenderDrawPoint(
                renderer,
                particleScreen.x,
                particleScreen.y);
        }
        
        // =============================================
        // 체력 바 UI
        // =============================================
        
        for (auto& enemy : enemies)
        {
            if (enemy.state == EnemyState::Dead)
            {
                continue;
            }

            SDL_Rect enemyScreen = camera.WorldToScreenRect(enemy.rect);

            if (!IsScreenRectVisible(enemyScreen))
            {
                continue;
            }

            float enemyHpRatio =
                (enemy.maxHP > 0)
                ? static_cast<float>(enemy.hp) / static_cast<float>(enemy.maxHP)
                : 0.0f;
            
            DrawHealthBar(
                renderer,
                enemyScreen.x,
                enemyScreen.y - ENEMY_HEALTH_BAR_HEIGHT - ENEMY_HEALTH_BAR_GAP,
                enemyScreen.w,
                ENEMY_HEALTH_BAR_HEIGHT,
                enemyHpRatio);
        }
        
        DrawHealthBar(
            renderer,
            0,
            SCREEN_HEIGHT - PLAYER_HEALTH_BAR_HEIGHT,
            SCREEN_WIDTH,
            PLAYER_HEALTH_BAR_HEIGHT,
            static_cast<float>(playerHP) / static_cast<float>(PLAYER_MAX_HP));

        DrawInventoryHUD(renderer, uiFont, inventory, inventoryIcons);
        if (stage != 1 || tutorial.IsPistolUnlocked())
        {
            DrawGunHUD(renderer, uiFont, pistol, equipment.hasSuppressor);
        }
        DrawHudMessage(renderer, uiFont);
        if (mainStageReturnHintVisible)
        {
            Uint32 elapsed = SDL_GetTicks() - mainStageReturnHintStart;
            if (elapsed <= MAIN_STAGE_RETURN_HINT_DURATION_MS)
            {
                SDL_Rect hintRect = { SCREEN_WIDTH / 2 - 260, 72, 520, 36 };
                DrawTextInRect(
                    renderer,
                    uiFont,
                    "Codebook acquired. Return to start!",
                    hintRect,
                    { 255, 255, 255, 255 });
            }
            else
            {
                mainStageReturnHintVisible = false;
            }
        }
        tutorial.DrawUI(renderer, uiFont);

        // =============================================
        // 경보 / 일시정지 UI
        // =============================================
        if (gameState == PAUSED)
        {
            DrawPauseOverlay(renderer, font);
        }
        if (ShouldShowAlarmSirenIndicator(alarmActive, enemies))
        {
            DrawAlarmSirenIndicator(renderer, font);
        }

        // =============================================
        // UI 텍스트
        // =============================================

        if (gameState == WIN)
        {
            if (finalMissionComplete)
            {
                DrawCenteredText(renderer, font, "MISSION COMPLETE");
            }
            else if (tutorialToStageJsonTransition || tutorial.IsTutorialComplete())
            {
                DrawCenteredText(renderer, font, "TUTORIAL COMPLETE");
            }
            else
            {
                DrawCenteredText(renderer, font, "STAGE CLEAR");
            }
        }
        else if (gameState == LOSE)
        {
            DrawCenteredText(renderer, font, "GAME OVER");
        }
        else if (gameState == CONTINUE_PROMPT)
        {
            DrawCenteredText(renderer, font, "CONTINUE?   Y: YES   N: NO");
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    // cleanup
    DestroyGameSpriteTextures(gameSprites);
    DestroyInventoryIconTextures(inventoryIcons);
    TTF_CloseFont(uiFont);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
