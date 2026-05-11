#include "Sound.h"

#include <algorithm>
#include <cmath>

void EmitSound(
    std::vector<SoundParticle>& particles,
    Vec2 origin,
    int count,
    float energy)
{
    for (int i = 0; i < count; i++)
    {
        float angle =
            RandomFloat() * 6.283185f;

        SoundParticle p;

        p.pos = origin;

        p.dir =
        {
            cosf(angle),
            sinf(angle)
        };

        p.energy = energy;

        particles.push_back(p);
    }
}

void UpdateSoundParticles(
    std::vector<SoundParticle>& particles,
    std::vector<Enemy>& enemies,
    std::vector<Wall>& walls,
    float dt)
{
    const float speed = 220.0f;

    for (auto& p : particles)
    {
        if (!p.alive)
            continue;

        Vec2 oldPos = p.pos;

        p.pos =
            p.pos + p.dir * speed * dt;

        p.energy *= expf(-1.5f * dt);

        float deathChance =
            (1.0f - p.energy) * 0.4f * dt;

        if (RandomFloat() < deathChance)
        {
            p.alive = false;
            continue;
        }

        for (auto& w : walls)
        {
            SDL_Rect rect = w.rect;

            if (p.pos.x >= rect.x &&
                p.pos.x <= rect.x + rect.w &&
                p.pos.y >= rect.y &&
                p.pos.y <= rect.y + rect.h)
            {
                float r = RandomFloat();

                // =====================================
                // ABSORB
                // =====================================

                if (r < w.absorption)
                {
                    p.alive = false;
                    break;
                }

                // =====================================
                // REFLECT
                // =====================================

                else if (r < w.absorption + w.reflection)
                {
                    float cx = rect.x + rect.w * 0.5f;
                    float cy = rect.y + rect.h * 0.5f;

                    float dx = p.pos.x - cx;
                    float dy = p.pos.y - cy;

                    float px =
                        (rect.w * 0.5f) - fabs(dx);

                    float py =
                        (rect.h * 0.5f) - fabs(dy);

                    if (px < py)
                        p.dir.x *= -1.0f;
                    else
                        p.dir.y *= -1.0f;

                    p.energy *= 0.75f;

                    p.bounces++;

                    p.pos = oldPos;
                }

                // =====================================
                // TRANSMIT
                // =====================================

                else
                {
                    // transmission: sound passes through wall

                    p.energy *= w.transmission;

                    // slight scattering (prevents perfect straight tunneling)
                    float spread = 0.25f;

                    float angle =
                        atan2f(p.dir.y, p.dir.x);

                    angle += (RandomFloat() - 0.5f) * spread;

                    p.dir =
                    {
                        cosf(angle),
                        sinf(angle)
                    };
                }
            }
        }

        for (auto& enemy : enemies)
        {
            float ex =
                enemy.rect.x +
                enemy.rect.w * 0.5f;

            float ey =
                enemy.rect.y +
                enemy.rect.h * 0.5f;

            float dx = ex - p.pos.x;
            float dy = ey - p.pos.y;

            float distSq =
                dx * dx + dy * dy;

            float hearRadius = 24.0f;

            if (distSq <
                hearRadius * hearRadius)
            {
                enemy.hearingEnergy +=
                    p.energy;
            }
        }
    }

    particles.erase(
        std::remove_if(
            particles.begin(),
            particles.end(),
            [](const SoundParticle& p)
            {
                return !p.alive;
            }),
        particles.end());
}