#include "Sound.h"

#include <algorithm>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cmath>

#include "Math.h"

static void AddHearingEnergy(
    HearingResult& result,
    Vec2 noisePos,
    float energy,
    SoundKind kind,
    int eventId)
{
    if (energy <= 0.0f)
    {
        return;
    }

    result.energy += energy;
    result.heard = true;

    if (energy > result.strongestEnergy)
    {
        result.strongestEnergy = energy;
        result.noisePos = noisePos;
        result.kind = kind;
        result.eventId = eventId;
    }
}

// =====================================================
// 사운드 생성
// =====================================================

void EmitSound(
    std::vector<SoundParticle>& particles,
    Vec2 origin,
    int count,
    float speed,
    float loudness,
    SoundKind kind)
{
    static int nextSoundEventId = 1;

    int eventId = nextSoundEventId++;

    if (nextSoundEventId <= 0)
    {
        nextSoundEventId = 1;
    }

    for (int i = 0; i < count; ++i)
    {
        float angle = RandomFloat() * 6.28318530718f;
        Vec2 dir = { cosf(angle), sinf(angle) };

        SoundParticle p;
        p.pos = origin;
        p.source = origin;

        p.eventId = eventId;
        p.kind = kind;
        p.age = 0.0f;

        p.vel = dir * speed;
        p.radius = 2.0f;
        p.mass = 1.0f;
        p.loudness = loudness;
        p.alive = true;

        particles.push_back(p);
    }
}

// =====================================================
// 방향성 사운드 생성
// direction : 기준 방향 벡터 (정규화 권장)
// spreadRad : 퍼지는 각도(라디안)
// =====================================================

void EmitSoundDirectional(
    std::vector<SoundParticle>& particles,
    Vec2 origin,
    Vec2 direction,
    float spreadRad,
    int count,
    float speed,
    float loudness,
    SoundKind kind)
{
    static int nextSoundEventId = 1;

    int eventId = nextSoundEventId++;

    if (nextSoundEventId <= 0)
    {
        nextSoundEventId = 1;
    }

    // 방향 정규화
    float len = sqrtf(direction.x * direction.x +
                      direction.y * direction.y);

    if (len <= 0.0001f)
    {
        direction = { 1.0f, 0.0f };
    }
    else
    {
        direction.x /= len;
        direction.y /= len;
    }

    // 기준 각도
    float baseAngle = atan2f(direction.y, direction.x);

    for (int i = 0; i < count; ++i)
    {
        // spread 범위 내 랜덤 각도
        float offset =
            (RandomFloat() - 0.5f) * spreadRad;

        float angle = baseAngle + offset;

        Vec2 dir =
        {
            cosf(angle),
            sinf(angle)
        };

        SoundParticle p;

        p.pos = origin;
        p.source = origin;

        p.eventId = eventId;
        p.kind = kind;
        p.age = 0.0f;

        p.vel = dir * speed;

        p.radius = 2.0f;
        p.mass = 1.0f;

        p.loudness = loudness;

        p.alive = true;

        particles.push_back(p);
    }
}

void GeneratePorousWall(Wall& w, float solidProbability) { w.cells.resize( w.gridWidth * w.gridHeight); for (int y = 0; y < w.gridHeight; y++) { for (int x = 0; x < w.gridWidth; x++) { auto& cell = GetCell(w, x, y); cell.solid = RandomFloat() < solidProbability; } } }
void EnsureWallGenerated(Wall& w) { if (!w.generated) { GeneratePorousWall(w, 0.25f); w.generated = true; } } 
void PrepareSoundWalls(std::vector<Wall>& walls)
{
    for (auto& w : walls)
    {
        EnsureWallGenerated(w);
    }
}

// =====================================================
// 셀 충돌
// =====================================================

static void ResolveCellCollision(
    const SoundParticle& in,
    SoundParticle& out,
    const Wall& w)
{
    SDL_Rect rect = w.rect;
    Vec2 pos = out.pos;

    // =============================================
    // 벽 영역 밖
    // =============================================

    if (pos.x < rect.x ||
        pos.y < rect.y ||
        pos.x >= rect.x + rect.w ||
        pos.y >= rect.y + rect.h)
    {
        return;
    }

    if (!w.generated || w.cells.empty() || w.cellSize <= 0)
    {
        return;
    }

    // =============================================
    // 셀 좌표
    // =============================================

    int gx = (int)(pos.x - rect.x) / w.cellSize;
    int gy = (int)(pos.y - rect.y) / w.cellSize;

    if (gx < 0 || gy < 0 ||
        gx >= w.gridWidth ||
        gy >= w.gridHeight)
        return;

    const WallCell& cell = GetCell(w, gx, gy);

    // =============================================
    // 빈 셀은 통과
    // =============================================

    if (!cell.solid)
        return;

    // =============================================
    // 셀 중심
    // =============================================

    float cx = rect.x + gx * w.cellSize + w.cellSize * 0.5f;
    float cy = rect.y + gy * w.cellSize + w.cellSize * 0.5f;

    Vec2 delta = { pos.x - cx, pos.y - cy };

    float half = w.cellSize * 0.5f;

    // =============================================
    // penetration
    // =============================================

    float px = half - fabsf(delta.x);
    float py = half - fabsf(delta.y);

    Vec2 normal;

    SoundParticle temp = out;

    // =============================================
    // 최소 penetration 축 선택
    // =============================================

    if (px < py)
    {
        normal = { delta.x > 0 ? 1.f : -1.f, 0 };
        temp.pos.x += normal.x * px;
    }
    else
    {
        normal = { 0, delta.y > 0 ? 1.f : -1.f };
        temp.pos.y += normal.y * py;
    }

    // =============================================
    // 반발계수를 적용한 반사
    // =============================================

    float vn = temp.vel.x * normal.x + temp.vel.y * normal.y;
    if (vn < 0.0f)
    {
        float e = cell.restitution;
        temp.vel.x -= (1.0f + e) * vn * normal.x;
        temp.vel.y -= (1.0f + e) * vn * normal.y;
    }
    
    // =============================================
    // roughness scattering
    // =============================================


    temp.vel.x += (RandomFloat() - 0.5f) * cell.roughness;
    temp.vel.y += (RandomFloat() - 0.5f) * cell.roughness;

    out = temp;
}

// =====================================================
// 입자 충돌
// =====================================================

static void ResolveParticleCollision(
    const SoundParticle& aIn,
    const SoundParticle& bIn,
    SoundParticle& aOut,
    SoundParticle& bOut)
{
    Vec2 delta = bIn.pos - aIn.pos;

    float dist = Length(delta);
    float minDist = aIn.radius + bIn.radius;

    if (dist <= 0.0001f || dist >= minDist)
        return;

    Vec2 normal = Normalize(delta);

    // =============================================
    // penetration correction
    // =============================================

    float penetration = minDist - dist;

    Vec2 correction = normal * (penetration * 0.5f);

    aOut.pos -= correction;
    bOut.pos += correction;

    // =============================================
    // relative velocity
    // =============================================

    Vec2 rv = bIn.vel - aIn.vel;

    float velAlongNormal = Dot(rv, normal);

    if (velAlongNormal > 0)
        return;

    // =============================================
    // 완전 탄성 충돌
    // =============================================

    float restitution = 1.0f;

    float j =
        -(1.0f + restitution) * velAlongNormal;

    j /= (1.0f / aIn.mass) + (1.0f / bIn.mass);

    Vec2 impulse = normal * j;

    aOut.vel -= impulse / aIn.mass;
    bOut.vel += impulse / bIn.mass;
}

void CleanUpParticles(
    std::vector<SoundParticle>& read,
    std::vector<SoundParticle>& write)
{
    std::swap(read, write);

    read.erase(
        std::remove_if(
            read.begin(),
            read.end(),
            [](const SoundParticle& p)
            {
                return !p.alive;
            }),
        read.end());
}

constexpr int CELL_SIZE = 64;

struct Cell
{
    int x, y;

    bool operator==(const Cell& other) const
    {
        return x == other.x && y == other.y;
    }
};

struct CellHash
{
    size_t operator()(const Cell& c) const
    {
        return std::hash<int>()(c.x) ^ (std::hash<int>()(c.y) << 1);
    }
};

using ParticleGrid = std::unordered_map<Cell, std::vector<int>, CellHash>;

inline Cell ToCell(const Vec2& p)
{
    return Cell{
        (int)std::floor(p.x / CELL_SIZE),
        (int)std::floor(p.y / CELL_SIZE)
    };
}

void BuildGrid(
    const std::vector<SoundParticle>& particles,
    ParticleGrid& grid)
{
    grid.clear();

    for (int i = 0; i < (int)particles.size(); ++i)
    {
        if (!particles[i].alive)
            continue;

        Cell c = ToCell(particles[i].pos);
        grid[c].push_back(i);
    }
}

void StepParticles(
    std::vector<SoundParticle>& particles,
    const std::vector<Wall>& walls,
    float stepDt)
{
    for (auto& p : particles)
    {
        if (!p.alive) continue;

        p.pos += p.vel * stepDt;
        p.age += stepDt;

        for (const auto& w : walls)
        {
            SoundParticle temp = p;
            ResolveCellCollision(p, temp, w);
            p = temp;
        }

        const float deathRate = 0.2f;

        if (RandomFloat() < deathRate * stepDt)
        {
            p.alive = false;
        }
    }
}

float ComputeLocalDensityGrid(
    int index,
    const std::vector<SoundParticle>& particles,
    const ParticleGrid& grid)
{
    const auto& p = particles[index];

    Cell base = ToCell(p.pos);

    float density = 0.0f;

    for (int dx = -1; dx <= 1; ++dx)
    for (int dy = -1; dy <= 1; ++dy)
    {
        Cell c{ base.x + dx, base.y + dy };

        auto it = grid.find(c);
        if (it == grid.end())
            continue;

        for (int j : it->second)
        {
            if (j == index) continue;

            const auto& q = particles[j];
            if (!q.alive) continue;

            // 간단 density 모델
            float dist2 =
                (q.pos - p.pos).LengthSq();

            density += 1.0f / (1.0f + dist2);
        }
    }

    return density;
}

std::vector<float> ComputeDensityFieldGrid(
    const std::vector<SoundParticle>& particles,
    const ParticleGrid& grid)
{
    std::vector<float> density(particles.size(), 0.0f);

    for (int i = 0; i < (int)particles.size(); ++i)
    {
        if (!particles[i].alive)
            continue;

        density[i] = ComputeLocalDensityGrid(i, particles, grid);
    }

    return density;
}

void ApplyDensitySurvival(
    std::vector<SoundParticle>& particles,
    const std::vector<float>& density)
{
    const float dieThreshold = 0.015f;

    for (int i = 0; i < (int)particles.size(); ++i)
    {
        if (!particles[i].alive)
            continue;

        if (density[i] < dieThreshold)
            particles[i].alive = false;
    }
}

void ComputeHearingGrid(
    const std::vector<SoundParticle>& particles,
    const std::vector<float>& density,
    const ParticleGrid& grid,
    const std::vector<EnemyAudioSnapshot>& enemies,
    std::vector<HearingResult>& hearingBuffer)
{
    for (size_t i = 0; i < enemies.size() && i < hearingBuffer.size(); ++i)
    {
        const auto& enemy = enemies[i];
        if (!enemy.alive)
            continue;

        auto& result = hearingBuffer[i];

        Vec2 enemyCenter =
        {
            enemy.rect.x + enemy.rect.w * 0.5f,
            enemy.rect.y + enemy.rect.h * 0.5f
        };

        Cell base = ToCell(enemyCenter);

        for (int dx = -2; dx <= 2; ++dx)
        for (int dy = -2; dy <= 2; ++dy)
        {
            Cell c{ base.x + dx, base.y + dy };

            auto it = grid.find(c);
            if (it == grid.end())
                continue;

            for (int j : it->second)
            {
                const auto& p = particles[j];
                if (!p.alive) continue;

                // ==============================
                // enemy 기준 거리 계산
                // ==============================
                Vec2 diff = p.pos - enemyCenter;
                float dist2 = diff.LengthSq();

                // ==============================
                // 거리 감쇠 (핵심 추가)
                // ==============================
                float attenuation = 1.0f / (1.0f + dist2);

                // ==============================
                // density는 "증폭 factor"
                // ==============================
                float energy = density[j] * attenuation * 20.0f * p.loudness;

                AddHearingEnergy(
                    result,
                    p.source,
                    energy,
                    p.kind,
                    p.eventId);
            }
        }
    }
}

void UpdateSoundParticles(
    const std::vector<SoundParticle>& read,
    std::vector<SoundParticle>& write,
    const std::vector<EnemyAudioSnapshot>& enemies,
    std::vector<HearingResult>& hearingBuffer,
    const std::vector<Wall>& walls,
    float dt)
{
    write = read;
    std::fill(hearingBuffer.begin(), hearingBuffer.end(), HearingResult{});

    const int substeps = 4;
    float stepDt = dt / substeps;

    for (int s = 0; s < substeps; ++s)
    {
        StepParticles(write, walls, stepDt);
    }

    ParticleGrid grid;
    BuildGrid(write, grid);

    auto density = ComputeDensityFieldGrid(write, grid);

    ApplyDensitySurvival(write, density);

    ComputeHearingGrid(write, density, grid, enemies, hearingBuffer);
}
