#include "Sound.h"

#include <algorithm>
#include <cmath>

#include "Math.h"

// =====================================================
// 사운드 생성
// =====================================================

void EmitSound(
    std::vector<SoundParticle>& particles,
    Vec2 origin,
    int count,
    float speed)
{
    for (int i = 0; i < count; i++)
    {
        float angle =
            RandomFloat() * 6.283185f;

        Vec2 dir =
        {
            cosf(angle),
            sinf(angle)
        };

        SoundParticle p;

        p.pos = origin;

        p.vel = dir * speed;

        p.radius = 2.0f;

        p.mass = 1.0f;

        p.alive = true;

        particles.push_back(p);
    }
}

void GeneratePorousWall(Wall& w, float solidProbability) { w.cells.resize( w.gridWidth * w.gridHeight); for (int y = 0; y < w.gridHeight; y++) { for (int x = 0; x < w.gridWidth; x++) { auto& cell = GetCell(w, x, y); cell.solid = RandomFloat() < solidProbability; } } }
void EnsureWallGenerated(Wall& w) { if (!w.generated) { GeneratePorousWall(w, 0.65f); w.generated = true; } } 
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

// =====================================================
// 업데이트
// =====================================================

void UpdateSoundParticles(
    const std::vector<SoundParticle>& read,
    std::vector<SoundParticle>& write,
    const std::vector<Enemy>& enemies,
    std::vector<float>& hearingBuffer,
    std::vector<Wall>& walls,
    float dt)
{
    write = read; // 기본 복사 (중요)

    const int substeps = 4;
    float stepDt = dt / substeps;

    for (int s = 0; s < substeps; s++)
    {
        // =========================================
        // 이동
        // =========================================

        for (int i = 0; i < read.size(); i++)
        {
            if (!read[i].alive) continue;

            write[i].pos += read[i].vel * stepDt;

            // =====================================
            // 🔥 고정 확률 소멸
            // =====================================

            const float deathRate = 0.8f; // 초당 확률

            if (RandomFloat() < deathRate * stepDt)
            {
                write[i].alive = false;
                continue;
            }
        }

        // =========================================
        // 셀 충돌
        // =========================================

        for (int i = 0; i < read.size(); i++)
        {
            if (!read[i].alive) continue;

            for (auto& w : walls)
            {
                ResolveCellCollision(
                    read[i],
                    write[i],
                    w);
            }
        }

        // =========================================
        // 입자 충돌
        // =========================================

        for (int i = 0; i < read.size(); i++)
        for (int j = i + 1; j < read.size(); j++)
        {
            ResolveParticleCollision(
                read[i], read[j],
                write[i], write[j]);
        }

        // =============================================
        // 적 hearing
        // =============================================

        for (size_t i = 0; i < enemies.size(); ++i)
        {
            auto& enemy = enemies[i];
            
            float ex =
                enemy.rect.x +
                enemy.rect.w * 0.5f;

            float ey =
                enemy.rect.y +
                enemy.rect.h * 0.5f;

            for (auto& p : read)
            {
                float dx =
                    ex - p.pos.x;

                float dy =
                    ey - p.pos.y;

                float distSq =
                    dx * dx + dy * dy;

                float hearRadius = 32.0f;

                if (distSq <
                    hearRadius * hearRadius)
                {
                    hearingBuffer[i] += 1.0f;
                }
            }
        }
    }
}

void CleanUpParticles(
    std::vector<SoundParticle>& read,
    std::vector<SoundParticle>& write)
{
    // swap
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
