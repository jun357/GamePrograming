#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <iostream>
#include <vector>
#include <cmath>
#include <thread>

#include "Globals.h"
#include "Math.h"
#include "Enemy.h"
#include "Player.h"
#include "Sound.h"
#include "Wall.h"

// =====================================================
// 게임 상태
// =====================================================

enum GameState
{
    PLAYING,
    WIN,
    LOSE
};

bool running = true;

// =====================================================
// 스테이지 리셋
// centerX = x + w / 2
// centerY = y + h / 2
// =====================================================

void ResetStage(
    SDL_Rect& player,
    std::vector<Enemy>& enemies)
{
    player = { 100,100,32,32 };

    enemies.clear();

    AddPatrolGuard(
        enemies,
        {516.0f, 316.0f},
        {
            {650.0f, 316.0f},
            {650.0f, 450.0f},
            {516.0f, 450.0f},
            {516.0f, 316.0f}
        });
    AddSentry(
        enemies,
        {316.0f, 216.0f});
    AddOfficer(
        enemies,
        {666.0f, 466.0f});
}

// =====================================================
// 텍스트 렌더
// =====================================================

void DrawCenteredText(
    SDL_Renderer* renderer,
    TTF_Font* font,
    const char* text)
{
    SDL_Color color =
    {
        255,255,255,255
    };

    SDL_Surface* surface =
        TTF_RenderText_Solid(
            font,
            text,
            color);

    SDL_Texture* texture =
        SDL_CreateTextureFromSurface(
            renderer,
            surface);

    SDL_Rect dst;

    dst.w = surface->w;
    dst.h = surface->h;

    dst.x =
        SCREEN_WIDTH / 2 - dst.w / 2;

    dst.y =
        SCREEN_HEIGHT / 2 - dst.h / 2;

    SDL_RenderCopy(
        renderer,
        texture,
        NULL,
        &dst);

    SDL_FreeSurface(surface);

    SDL_DestroyTexture(texture);
}

// =====================================================
// 메인
// =====================================================

int main(int argc, char* args[])
{
    // =================================================
    // SDL 초기화
    // =================================================

    SDL_Init(SDL_INIT_VIDEO);

    TTF_Init();

    SDL_Window* window =
        SDL_CreateWindow(
            "Stealth Game",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            SCREEN_WIDTH,
            SCREEN_HEIGHT,
            SDL_WINDOW_SHOWN);

    SDL_Renderer* renderer =
        SDL_CreateRenderer(
            window,
            -1,
            SDL_RENDERER_ACCELERATED);

    // =================================================
    // 폰트
    // =================================================

    TTF_Font* font =
        TTF_OpenFont(
            "unscii-16.ttf",
            48);

    if (!font)
    {
        std::cout << "Font load failed\n";
        return 0;
    }

    // =================================================
    // 게임 오브젝트
    // =================================================

    SDL_Rect player =
    {
        100,100,32,32
    };

    SDL_Rect goal =
    {
        700,
        500,
        40,
        40
    };

    std::vector<Wall> walls =
    {
        // concrete
        {
            {200,150,100,200}
        },

        // wood
        {
            {400,100,50,300}
        },

        // thin divider
        {
            {100,400,300,50}
        }
    };

    PrepareSoundWalls(walls);

    std::vector<Enemy> enemies;

    std::vector<SoundParticle> soundParticles;

    ResetStage(player, enemies);

    // =================================================
    // 게임 상태
    // =================================================

    GameState gameState = PLAYING;

    Uint32 stateTimer = 0;

    float soundTimer = 0.0f;

    // =================================================
    // 플레이어 체력 및 경보 상태
    // =================================================

    const int PLAYER_MAX_HP = 100;
    int playerHP = PLAYER_MAX_HP;

    bool alarmActive = false;

    // =================================================
    // 타이머
    // =================================================

    Uint64 previousCounter =
        SDL_GetPerformanceCounter();

    Uint64 performanceFrequency =
        SDL_GetPerformanceFrequency();

    // =================================================
    // 메인 루프
    // =================================================

    while (running)
    {
        // =============================================
        // Delta Time
        // =============================================

        Uint64 currentCounter =
            SDL_GetPerformanceCounter();

        float dt =
            (float)(currentCounter - previousCounter)
            / (float)performanceFrequency;

        previousCounter = currentCounter;

        if (dt > 0.05f)
            dt = 0.05f;

        // =============================================
        // 이벤트
        // =============================================

        SDL_Event e;

        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                running = false;
            }
        }

        // =============================================
        // 게임 업데이트
        // =============================================

        if (gameState == PLAYING)
        {
            const Uint8* keystate =
                SDL_GetKeyboardState(NULL);

            // =========================================
            // 플레이어 이동
            // =========================================

            MoveMode moveMode =
                GetMoveMode(keystate);

            float moveSpeed =
                GetMoveSpeed(moveMode);

            float dirX = 0.0f;
            float dirY = 0.0f;

            if (keystate[SDL_SCANCODE_W]) dirY -= 1.0f;
            if (keystate[SDL_SCANCODE_S]) dirY += 1.0f;
            if (keystate[SDL_SCANCODE_A]) dirX -= 1.0f;
            if (keystate[SDL_SCANCODE_D]) dirX += 1.0f;

            float length =
                sqrtf(dirX * dirX + dirY * dirY);

            bool isMoving = length > 0.0f;

            if (isMoving)
            {
                dirX /= length;
                dirY /= length;
            }

            float dx =
                dirX * moveSpeed * dt;

            float dy =
                dirY * moveSpeed * dt;

            MovePlayer(
                player,
                dx,
                dy,
                walls);

            // =========================================
            // 사운드 발생
            // =========================================

            soundTimer += dt;

            Vec2 playerCenter =
            {
                player.x + player.w * 0.5f,
                player.y + player.h * 0.5f
            };

            if (isMoving)
            {
                if (moveMode == RUN)
                {
                    if (soundTimer >= 0.03f)
                    {
                        EmitSound(
                            soundParticles,
                            playerCenter,
                            23,
                            220.0f);

                        soundTimer = 0.0f;
                    }
                }
                else if (moveMode == WALK)
                {
                    if (soundTimer >= 0.09f)
                    {
                        EmitSound(
                            soundParticles,
                            playerCenter,
                            7,
                            220.0f);

                        soundTimer = 0.0f;
                    }
                }
            }
            else
            {
                soundTimer = 0.0f;
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
            // 사운드 물리 업데이트 thread
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
            // 적 AI 업데이트 thread
            // =========================================
            std::thread enemyThread(
                UpdateEnemies,
                std::ref(enemies),
                std::cref(playerSnapshot),
                std::cref(walls),
                alarmActive,
                std::ref(alarmTriggered),
                std::ref(playerHP),
                dt);
            
            // 두 thread가 끝날 때까지 main thread 대기
            soundThread.join();
            enemyThread.join();
            
            // soundThread 결과를 실제 사운드 파티클 배열에 반영
            CleanUpParticles(soundParticles, particlesNext);
            
            // =========================================
            // soundThread의 청각 결과를 main thread에서 enemies에 병합
            // =========================================
            for (size_t i = 0; i < enemies.size() && i < hearingBuffer.size(); ++i)
            {
                const HearingResult& hearing = hearingBuffer[i];
                if (!hearing.heard)
                {
                    continue;
                }
                enemies[i].hearingEnergy += hearing.energy;
                enemies[i].lastNoisePos = hearing.noisePos;
                
                if (enemies[i].hearingEnergy >= enemies[i].hearingThreshold)
                {
                    RequestEnemyInvestigate(enemies[i], hearing.noisePos);
                    enemies[i].hearingEnergy = 0.0f;
                }
            }
            
            if (alarmTriggered)
            {
                alarmActive = true;
            }
            if (playerHP <= 0)
            {
                playerHP = 0;
                gameState = LOSE;
                stateTimer = SDL_GetTicks();
            }
            if (SDL_HasIntersection(&player, &goal))
            {
                gameState = WIN;

                stateTimer = SDL_GetTicks();
            }
        }
        else
        {
            // =========================================
            // 스테이지 리셋
            // =========================================

            if (SDL_GetTicks() - stateTimer >= 2000)
            {
                ResetStage(
                    player,
                    enemies);

                soundParticles.clear();

                playerHP = PLAYER_MAX_HP;
                alarmActive = false;

                gameState = PLAYING;
            }
        }

        // =============================================
        // 렌더
        // =============================================

        SDL_SetRenderDrawColor(
            renderer,
            20,
            20,
            20,
            255);

        SDL_RenderClear(renderer);

        // =============================================
        // FOV
        // =============================================

        for (auto& enemy : enemies)
        {
            DrawFOV(
                renderer,
                enemy,
                walls);
        }

        // =============================================
        // 벽
        // =============================================

        SDL_SetRenderDrawColor(
            renderer,
            120,
            120,
            120,
            255);

        for (auto& w : walls)
        {
            SDL_RenderFillRect(
                renderer,
                &w.rect);
        }

        // =============================================
        // 목표
        // =============================================

        SDL_SetRenderDrawColor(
            renderer,
            255,
            255,
            0,
            255);

        SDL_RenderDrawRect(
            renderer,
            &goal);

        // =============================================
        // 플레이어
        // =============================================

        SDL_SetRenderDrawColor(
            renderer,
            0,
            255,
            0,
            255);

        SDL_RenderFillRect(
            renderer,
            &player);

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

            SDL_RenderFillRect(
                renderer,
                &enemy.rect);
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
            SDL_RenderDrawPoint(
                renderer,
                (int)p.pos.x,
                (int)p.pos.y);
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

    // =================================================
    // 정리
    // =================================================

    TTF_CloseFont(font);

    SDL_DestroyRenderer(renderer);

    SDL_DestroyWindow(window);

    TTF_Quit();

    SDL_Quit();

    return 0;
}
