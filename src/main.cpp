#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <iostream>
#include <vector>
#include <cmath>
#include <thread>
#include <algorithm>
#include <random>

#include "Globals.h"
#include "Math.h"
#include "Enemy.h"
#include "Player.h"
#include "Sound.h"
#include "Wall.h"
#include "Camera.h"

// =====================================================
// 게임 상태
// =====================================================
enum GameState
{
    PLAYING,
    PAUSED,
    WIN,
    LOSE
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

// =====================================================
// 골
// =====================================================
SDL_Rect goalNormal  = {700, 500, 40, 40};
SDL_Rect goalAnomaly = {700, 100, 40, 40};

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

void ResetWorldItems(std::vector<WorldItem>& items)
{
    items.clear();

    // 임시 배치. 벽과 겹치지 않는 위치로 둔다.
    items.push_back({ ItemType::Bottle, {150, 110, 24, 24}, false });
    items.push_back({ ItemType::Key,    {520, 110, 24, 24}, false });
    items.push_back({ ItemType::Target, {720, 460, 24, 24}, false });
}

void TryPickupNearestItem(
    const SDL_Rect& player,
    std::vector<WorldItem>& items,
    Inventory& inventory)
{
    static constexpr float PICKUP_RANGE = 48.0f;
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
        return;
    }

    if (AddToInventory(inventory, nearest->type))
    {
        nearest->picked = true;
        std::cout << "Picked up item: "
                  << GetItemLetter(nearest->type)
                  << std::endl;
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
    std::vector<SoundParticle>& soundParticles)
{
    if (!bottle.alive)
    {
        return;
    }

    // 병 깨지는 소음
    EmitSound(
        soundParticles,
        bottle.pos,
        48,
        285.0f,
        2.15f,
        1.8f);

    bottle.alive = false;
}

bool TryThrowBottle(
    const SDL_Rect& player,
    Vec2 targetWorld,
    Inventory& inventory,
    std::vector<BottleProjectile>& bottles)
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

    bottles.push_back(bottle);

    // 사용하면 깨짐
    inventory.hasBottle = false;

    return true;
}

void UpdateBottleProjectiles(
    std::vector<BottleProjectile>& bottles,
    std::vector<SoundParticle>& soundParticles,
    const std::vector<Wall>& walls,
    float dt)
{
    for (auto& bottle : bottles)
    {
        if (!bottle.alive)
        {
            continue;
        }

        bottle.flightTime += dt;

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
            BreakBottle(bottle, soundParticles);
            continue;
        }

        if (bottle.pos.y < 0.0f)
        {
            bottle.pos.y = 0.0f;
            BreakBottle(bottle, soundParticles);
            continue;
        }

        if (bottle.pos.x > WORLD_WIDTH)
        {
            bottle.pos.x = static_cast<float>(WORLD_WIDTH);
            BreakBottle(bottle, soundParticles);
            continue;
        }

        if (bottle.pos.y > WORLD_HEIGHT)
        {
            bottle.pos.y = static_cast<float>(WORLD_HEIGHT);
            BreakBottle(bottle, soundParticles);
            continue;
        }

        // 벽 충돌: 병은 튕기지 않고 깨진다.
        for (const auto& wall : walls)
        {
            if (CircleIntersectsRect(bottle.pos, bottle.radius, wall.rect))
            {
                BreakBottle(bottle, soundParticles);
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
            BreakBottle(bottle, soundParticles);
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

// =====================================================
// 벽 합성 (base + anomaly)
// =====================================================
std::vector<Wall> GetActiveWalls()
{
    std::vector<Wall> result = baseWalls;

    if (anomalyWall)
    {
        result.insert(result.end(),
                      anomalyWalls.begin(),
                      anomalyWalls.end());
    }

    return result;
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

// =====================================================
// 스테이지 초기화
// =====================================================
void ResetStage(SDL_Rect& player, std::vector<Enemy>& enemies)
{
    player = {100,100,32,32};

    enemies.clear();

    AddPatrolGuard(enemies, {516,316},
        {{650,316},{650,450},{516,450},{516,316}});

    AddSentry(enemies, {316,216});
    AddOfficer(enemies, {666,466});

    showStageText = true;
    stageTextStart = SDL_GetTicks();
    displayStage = stage;

    // reset anomaly
    anomalyActive = false;
    anomalyWall = false;
    anomalyEnemy = false;
    anomalyFOV = false;
    anomalySound = false;

    // stage 1 = safe
    if (stage == 1) return;

    // random anomaly count
    // 0~2개 이변을 랜덤으로 선택한다.
    // stage 1은 위에서 return하므로, 여기까지 오면 stage 2 이상이다.
    std::random_device rd;
    std::mt19937 rng(rd());
    
    // 0~2개 이변 허용.
    // stage 2 이상에서 반드시 이변이 나오게 하고 싶으면 (1, 2)로 바꾼다.
    std::uniform_int_distribution<int> countDist(0, 2);
    int count = countDist(rng);
    
    // 0: wall, 1: enemy, 2: fov, 3: sound
    std::vector<int> pool = { 0, 1, 2, 3 };
    
    std::shuffle(pool.begin(), pool.end(), rng);
    
    for (int i = 0; i < count && i < static_cast<int>(pool.size()); ++i)
    {
        switch (pool[i])
        {
            case 0:
                anomalyWall = true;
                break;
            case 1:
                anomalyEnemy = true;
                break;
            case 2:
                anomalyFOV = true;
                break;
            case 3:
                anomalySound = true;
                break;
        }
    }
    anomalyActive =
        anomalyWall ||
        anomalyEnemy ||
        anomalyFOV ||
        anomalySound;
    
    // 선택된 이변을 실제 게임 상태에 반영
    ApplyAnomalyEffects(enemies);
    
    std::cout
        << "[Stage " << stage << "] "
        << "anomalyActive=" << anomalyActive
        << " wall=" << anomalyWall
        << " enemy=" << anomalyEnemy
        << " fov=" << anomalyFOV
        << " sound=" << anomalySound
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

        // 이미지가 없으므로 필드 아이템은 작은 흰색 사각형으로 표시
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

void DrawInventoryHUD(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const Inventory& inventory)
{
    static constexpr int SLOT_SIZE = 44;
    static constexpr int SLOT_GAP = 8;
    static constexpr int START_X = 16;
    static constexpr int START_Y = 16;

    SDL_Rect slots[3] =
    {
        { START_X, START_Y, SLOT_SIZE, SLOT_SIZE },
        { START_X + (SLOT_SIZE + SLOT_GAP), START_Y, SLOT_SIZE, SLOT_SIZE },
        { START_X + (SLOT_SIZE + SLOT_GAP) * 2, START_Y, SLOT_SIZE, SLOT_SIZE }
    };

    const char* letters[3] = { "B", "K", "T" };
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

        if (owned[i])
        {
            DrawTextInRect(
                renderer,
                font,
                letters[i],
                slots[i],
                { 255, 255, 255, 255 });
        }
    }
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
    // 나중에 사이렌 이미지가 생기면 이 함수 내부만 SDL_RenderCopy로 교체
    static constexpr const char* ALARM_SIREN_PLACEHOLDER_TEXT = "A";

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
    std::string text = "STAGE " + std::to_string(stage);

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

// =====================================================
// MAIN
// =====================================================
int main(int argc, char* args[])
{
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    SDL_Window* window =
        SDL_CreateWindow("Stealth Game",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            SCREEN_WIDTH,
            SCREEN_HEIGHT,
            SDL_WINDOW_SHOWN);

    SDL_Renderer* renderer =
        SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    TTF_Font* font = TTF_OpenFont("unscii-16.ttf", 48);

    if (!font)
    {
        std::cout << "Font load failed\n";
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
        SDL_Quit();
        return 0;
    }

    SDL_Rect player = {100,100,32,32};

    std::vector<Enemy> enemies;
    std::vector<SoundParticle> soundParticles;

    std::vector<WorldItem> worldItems;
    Inventory inventory;
    std::vector<BottleProjectile> bottleProjectiles;

    // base map
    baseWalls =
    {
        {{200,150,100,200}},
        {{400,100,50,300}},
        {{100,400,300,50}}
    };

    // anomaly map (extra)
    anomalyWalls =
    {
        {{500,350,200,40}},
        {{250,250,40,200}}
    };

    PrepareSoundWalls(baseWalls);

    PrepareSoundWalls(anomalyWalls);
    ResetStage(player, enemies);
    ResetWorldItems(worldItems);
    inventory = Inventory{};
    bottleProjectiles.clear();

    Camera2D camera;
    camera.zoom = 1.0f;

    GameState gameState = PLAYING;
    Uint32 stateTimer = 0;
    Uint32 pauseStartTicks = 0;

    float soundTimer = 0.0f;
    float runWallSoundCooldown = 0.0f;

    int playerHP = PLAYER_MAX_HP;

    bool alarmActive = false;

    Uint64 prev = SDL_GetPerformanceCounter();
    Uint64 freq = SDL_GetPerformanceFrequency();

    showStageText = true;
    stageTextStart = SDL_GetTicks();
    displayStage = 1;
    stage = 1;

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
        bool bottleThrowPressed = false;
        int bottleThrowScreenX = 0;
        int bottleThrowScreenY = 0;
        
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
            else if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_RIGHT)
            {
                bottleThrowPressed = true;
                bottleThrowScreenX = e.button.x;
                bottleThrowScreenY = e.button.y;
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
        }

        auto walls = GetActiveWalls();

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

            MoveMode mode = GetMoveMode(key, injuredTimer);
            float speed = GetMoveSpeed(mode);

            float dx = 0, dy = 0;

            if (key[SDL_SCANCODE_W]) dy -= 1;
            if (key[SDL_SCANCODE_S]) dy += 1;
            if (key[SDL_SCANCODE_A]) dx -= 1;
            if (key[SDL_SCANCODE_D]) dx += 1;

            float len = sqrtf(dx*dx + dy*dy);
            bool moving = len > 0;

            if (moving)
            {
                dx /= len;
                dy /= len;
            }

            dx *= speed * dt;
            dy *= speed * dt;

            bool hitWall =
                MovePlayerWithCollisionResult(player, dx, dy, walls);

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
            camera.FollowImmediate(playerCenter.x, playerCenter.y);
            camera.ClampToBounds(WORLD_WIDTH, WORLD_HEIGHT);
            
            if (interactPressed)
            {
                TryPickupNearestItem(player, worldItems, inventory);
            }

            if (bottleThrowPressed)
            {
                Vec2 bottleTargetWorld = camera.ScreenToWorldPoint(bottleThrowScreenX, bottleThrowScreenY);
                TryThrowBottle(player, bottleTargetWorld, inventory, bottleProjectiles);
            }

            if (moving)
            {
                if (mode == RUN)
                {
                    if (soundTimer >= 0.04f)
                    {
                        EmitSound(soundParticles, playerCenter, 24, 245.0f, 1.05f, 1.35f);
                        soundTimer = 0.0f;
                    }
                    if (hitWall && runWallSoundCooldown <= 0.0f)
                    {
                        EmitSound(soundParticles, playerCenter, 42, 270.0f, 2.0f, 1.8f);
                        runWallSoundCooldown = 0.35f;
                    }
                }
                else if (mode == WALK)
                {
                    if (soundTimer >= 0.12f)
                    {
                        EmitSound(soundParticles, playerCenter, 8, 185.0f, 0.35f, 1.0f);
                        soundTimer = 0.0f;
                    }
                }
            }
            else
            {
                soundTimer = 0.0f;
            }

            UpdateBottleProjectiles(bottleProjectiles, soundParticles, walls, dt);

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
            std::thread enemyThread(
                UpdateEnemies,
                std::ref(enemies),
                std::cref(playerSnapshot),
                std::cref(walls),
                alarmActive,
                std::ref(alarmTriggered),
                std::ref(playerHP),
                std::ref(injuredTimer),
                dt);
            
            soundThread.join();
            enemyThread.join();
            CleanUpParticles(soundParticles, particlesNext);
            
            if (alarmTriggered)
            {
                alarmActive = true;
            }
            
            for (size_t i = 0; i < enemies.size() && i < hearingBuffer.size(); ++i)
            {
                const HearingResult& hearing = hearingBuffer[i];
                if (!hearing.heard)
                {
                    continue;
                }
                
                NotifyEnemyOfNoise(
                    enemies[i],
                    hearing.noisePos,
                    hearing.energy,
                    alarmActive);
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
                stage = 1;
                gameState = LOSE;
                stateTimer = SDL_GetTicks();
            }
            // ==============================
            // GOAL CHECK
            // ==============================
            bool hitN = SDL_HasIntersection(&player, &goalNormal);
            bool hitA = SDL_HasIntersection(&player, &goalAnomaly);

            if (hitN || hitA)
            {
                bool correct =
                    (!anomalyActive && hitN) ||
                    ( anomalyActive && hitA);

                std::cout
                << "anomalyActive: " << anomalyActive
                << " hitN: " << hitN
                << " hitA: " << hitA
                << std::endl;

                if (correct)
                {
                    stage++;
                    gameState = WIN;
                    stateTimer = SDL_GetTicks();
                }
                else
                {
                    stage = 1;
                    gameState = LOSE;
                    stateTimer = SDL_GetTicks();
                }
            }
        }
        else if (gameState == WIN || gameState == LOSE)
        {
            // =========================================
            // 스테이지 리셋
            // =========================================

            if (SDL_GetTicks() - stateTimer >= 2000)
            {
                ResetStage(
                    player,
                    enemies);

                ResetWorldItems(worldItems);
                inventory = Inventory{};
                bottleProjectiles.clear();

                soundParticles.clear();

                soundTimer = 0.0f;
                runWallSoundCooldown = 0.0f;

                playerHP = PLAYER_MAX_HP;
                alarmActive = false;

                gameState = PLAYING;
            }
        }

        // =================================================
        // RENDER
        // =================================================
        SDL_SetRenderDrawColor(renderer, 20,20,20,255);
        SDL_RenderClear(renderer);

        // =============================================
        // FOV
        // =============================================

        for (auto& enemy : enemies)
        {
            DrawFOV(
                renderer,
                enemy,
                walls,
                camera);
        }

        // walls
        SDL_SetRenderDrawColor(renderer, 120,120,120,255);

        for (auto& w : walls)
        {
            SDL_Rect r = camera.WorldToScreenRect(w.rect);
            SDL_RenderFillRect(renderer, &r);
        }

        // goals
        SDL_SetRenderDrawColor(renderer, 0,255,0,255);
        SDL_Rect gn = camera.WorldToScreenRect(goalNormal);
        SDL_RenderDrawRect(renderer, &gn);

        SDL_SetRenderDrawColor(renderer, 255,0,0,255);
        SDL_Rect ga = camera.WorldToScreenRect(goalAnomaly);
        SDL_RenderDrawRect(renderer, &ga);

        DrawWorldItems(renderer, uiFont, worldItems, camera);
        DrawBottleProjectiles(renderer, bottleProjectiles, camera);

        // player
        SDL_SetRenderDrawColor(renderer, 0,255,0,255);
        SDL_Rect ps = camera.WorldToScreenRect(player);
        SDL_RenderFillRect(renderer, &ps);

        // =============================================
        // 적
        // =============================================

        for (auto& enemy : enemies)
        {
            if (enemy.alerted)
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
            SDL_Rect enemyScreen = camera.WorldToScreenRect(enemy.rect);
            SDL_RenderFillRect(renderer, &enemyScreen);
        }

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

        DrawInventoryHUD(renderer, uiFont, inventory);

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
            DrawCenteredText(
                renderer,
                font,
                "STAGE CLEAR");
        }
        else if (gameState == LOSE)
        {
            DrawCenteredText(
                renderer,
                font,
                "GAME OVER");
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    // cleanup
    TTF_CloseFont(uiFont);
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();

    return 0;
}
