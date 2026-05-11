#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <iostream>
#include <vector>
#include <cmath>

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
// =====================================================

void ResetStage(
    SDL_Rect& player,
    std::vector<Enemy>& enemies)
{
    player = { 100,100,32,32 };

    enemies.clear();

    enemies =
    {
        {
            {500,300,32,32},
            0.0f,
            60.0f * M_PI / 180.0f,
            220.0f,
            0.01f
        },

        {
            {300,200,32,32},
            1.5f,
            90.0f * M_PI / 180.0f,
            250.0f,
            -0.008f
        },

        {
            {650,450,32,32},
            3.14f,
            45.0f * M_PI / 180.0f,
            180.0f,
            0.02f
        }
    };
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
            {200,150,100,200},
            0.6f,
            0.35f,
            0.05f
        },

        // wood
        {
            {400,100,50,300},
            0.3f,
            0.4f,
            0.3f
        },

        // thin divider
        {
            {100,400,300,50},
            0.1f,
            0.2f,
            0.7f
        }
    };

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
                            14,
                            1.0f);

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
                            5,
                            0.6f);

                        soundTimer = 0.0f;
                    }
                }
            }
            else
            {
                soundTimer = 0.0f;
            }

            // =========================================
            // 사운드 업데이트
            // =========================================

            UpdateSoundParticles(
                soundParticles,
                enemies,
                walls,
                dt);

            // =========================================
            // 적 업데이트
            // =========================================

            bool playerDetected = false;

            UpdateEnemies(
                enemies,
                player,
                walls,
                playerDetected,
                dt);

            if (playerDetected)
            {
                gameState = LOSE;

                stateTimer =
                    SDL_GetTicks();
            }

            // =========================================
            // 목표 도착
            // =========================================

            if (SDL_HasIntersection(
                &player,
                &goal))
            {
                gameState = WIN;

                stateTimer =
                    SDL_GetTicks();
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