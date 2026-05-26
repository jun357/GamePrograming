#include "Sound.h"

#include <algorithm>
#include <cmath>

#include "Math.h"

// =====================================================
// 직접 소음 이벤트 hearing 보정
// =====================================================
//
// 현재 파티클 hearing은 particle.pos가 적 근처를 지나갈 때만 작동한다.
// 그래서 큰 소리라도 랜덤 파티클 경로가 적을 스치지 않으면
// 적이 못 듣는 것처럼 보일 수 있다.
//
// 아래 보정은 새로 발생한 EmitSound() 이벤트의 source를 기준으로
// 1회 hearing을 추가한다. 소리 파티클 물리 자체는 그대로 유지한다.

static constexpr float DIRECT_SOUND_BASE_RADIUS = 120.0f;
static constexpr float DIRECT_SOUND_RADIUS_PER_LOUDNESS = 220.0f;
static constexpr float DIRECT_SOUND_ENERGY_SCALE = 8.0f;

static constexpr float DIRECT_SOUND_WALL_OCCLUSION = 0.65f;
static constexpr float DIRECT_SOUND_MIN_OCCLUSION = 0.25f;

static constexpr float NEW_SOUND_EVENT_EPSILON = 0.0001f;

static bool LineIntersect(
    float x1,
    float y1,
    float x2,
    float y2,
    float x3,
    float y3,
    float x4,
    float y4)
{
    float denom =
        (y4 - y3) * (x2 - x1) -
        (x4 - x3) * (y2 - y1);

    if (fabsf(denom) <= 0.000001f)
    {
        return false;
    }

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

static bool LineIntersectsRect(
    Vec2 a,
    Vec2 b,
    const SDL_Rect& rect)
{
    float rx = static_cast<float>(rect.x);
    float ry = static_cast<float>(rect.y);
    float rw = static_cast<float>(rect.w);
    float rh = static_cast<float>(rect.h);

    if (LineIntersect(a.x, a.y, b.x, b.y, rx, ry, rx + rw, ry))
    {
        return true;
    }

    if (LineIntersect(a.x, a.y, b.x, b.y, rx, ry, rx, ry + rh))
    {
        return true;
    }

    if (LineIntersect(a.x, a.y, b.x, b.y, rx + rw, ry, rx + rw, ry + rh))
    {
        return true;
    }

    if (LineIntersect(a.x, a.y, b.x, b.y, rx, ry + rh, rx + rw, ry + rh))
    {
        return true;
    }

    return false;
}

static float ComputeSoundOcclusion(
    Vec2 source,
    Vec2 listener,
    const std::vector<Wall>& walls)
{
    float occlusion = 1.0f;

    for (const auto& wall : walls)
    {
        if (LineIntersectsRect(source, listener, wall.rect))
        {
            occlusion *= DIRECT_SOUND_WALL_OCCLUSION;
        }
    }

    if (occlusion < DIRECT_SOUND_MIN_OCCLUSION)
    {
        occlusion = DIRECT_SOUND_MIN_OCCLUSION;
    }

    return occlusion;
}

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

static void AddDirectSoundEventHearing(
    const SoundParticle& particle,
    Vec2 enemyCenter,
    const std::vector<Wall>& walls,
    HearingResult& result)
{
    float hearRadius =
        DIRECT_SOUND_BASE_RADIUS +
        particle.loudness * DIRECT_SOUND_RADIUS_PER_LOUDNESS;

    float distSq = DistanceSq(enemyCenter, particle.source);
    float hearRadiusSq = hearRadius * hearRadius;

    if (distSq >= hearRadiusSq)
    {
        return;
    }

    float dist = sqrtf(distSq);

    float attenuation =
        1.0f - ClampFloat(dist / hearRadius, 0.0f, 1.0f);

    float occlusion =
        ComputeSoundOcclusion(
            particle.source,
            enemyCenter,
            walls);

    float energy =
        particle.loudness *
        attenuation *
        occlusion *
        DIRECT_SOUND_ENERGY_SCALE;
    
    AddHearingEnergy(
        result,
        particle.source,
        energy,
        particle.kind,
        particle.eventId);
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
    float life
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
        p.life = life;
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
    const std::vector<EnemyAudioSnapshot>& enemies,
    std::vector<HearingResult>& hearingBuffer,
    const std::vector<Wall>& walls,
    float dt)
{
    write = read; // 기본 복사 (중요)

    for (auto& result : hearingBuffer)
    {
        result = HearingResult{};
    }

    const int substeps = 4;
    float stepDt = dt / substeps;

    for (int s = 0; s < substeps; ++s)
    {
        // =========================================
        // 이동
        // =========================================
        for (int i = 0; i < static_cast<int>(write.size()); ++i)
        {
            if (!write[i].alive)
            {
                continue;
            }
            
            write[i].pos += write[i].vel * stepDt;
            write[i].age += stepDt;
            write[i].life -= stepDt;
            if (write[i].life <= 0.0f)
            {
                write[i].alive = false;
                continue;
            }
        }

        // =========================================
        // 셀 충돌
        // =========================================
        for (int i = 0; i < static_cast<int>(write.size()); ++i)
        {
            if (!write[i].alive)
            {
                continue;
            }

            for (const auto& w : walls)
            {
                SoundParticle before = write[i];

                ResolveCellCollision(
                    before,
                    write[i],
                    w);
            }
        }

        // =========================================
        // 입자 충돌
        // =========================================
        
        for (int i = 0; i < static_cast<int>(write.size()); ++i)
        {
            if (!write[i].alive)
            {
                continue;
            }

            for (int j = i + 1; j < static_cast<int>(write.size()); ++j)
            {
                if (!write[j].alive)
                {
                    continue;
                }
                
                SoundParticle aBefore = write[i];
                SoundParticle bBefore = write[j];

                ResolveParticleCollision(
                    aBefore,
                    bBefore,
                    write[i],
                    write[j]);
            }
        }
    }

    // =============================================
    // 적 hearing
    // =============================================
    for (size_t i = 0; i < enemies.size() && i < hearingBuffer.size(); ++i)
    {
        const EnemyAudioSnapshot& enemy = enemies[i];

        if (!enemy.alive)
        {
            continue;
        }

        Vec2 enemyCenter =
        {
            enemy.rect.x + enemy.rect.w * 0.5f,
            enemy.rect.y + enemy.rect.h * 0.5f
        };

        HearingResult& result = hearingBuffer[i];

        std::vector<int> processedEventIds;
        processedEventIds.reserve(16);

        for (const auto& particle : write)
        {
            if (!particle.alive)
            {
                continue;
            }

            // =====================================================
            // 1. 직접 소음 이벤트 hearing
            // =====================================================
            if (particle.eventId != 0 &&
                particle.age <= dt + NEW_SOUND_EVENT_EPSILON)
            {
                bool alreadyProcessed =
                    std::find(
                        processedEventIds.begin(),
                        processedEventIds.end(),
                        particle.eventId)
                    != processedEventIds.end();

                if (!alreadyProcessed)
                {
                    processedEventIds.push_back(particle.eventId);

                    AddDirectSoundEventHearing(
                        particle,
                        enemyCenter,
                        walls,
                        result);
                }
            }

            // =====================================================
            // 2. 파티클 위치 기반 hearing
            // =====================================================
            float hearRadius = 34.0f + particle.loudness * 10.0f;

            float distSq = DistanceSq(enemyCenter, particle.pos);
            float hearRadiusSq = hearRadius * hearRadius;

            if (distSq >= hearRadiusSq)
            {
                continue;
            }

            float dist = sqrtf(distSq);

            float attenuation =
                1.0f - ClampFloat(dist / hearRadius, 0.0f, 1.0f);

            float energy = particle.loudness * attenuation;

            AddHearingEnergy(
                result,
                particle.source,
                energy,
                particle.kind,
                particle.eventId);
        }
    }
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
