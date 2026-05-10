#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <iostream>
#include <vector>
#include <cmath>

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

bool running = true;

// =====================================================
// 게임 상태
// =====================================================

enum GameState
{
    PLAYING,
    WIN,
    LOSE
};

// =====================================================
// 플레이어 이동 상태
// =====================================================
enum MoveMode
{
    SNEAK,
    WALK,
    RUN
};

float GetMoveSpeed(MoveMode mode)
{
    switch (mode)
    {
    case SNEAK: return 90.0f;
    case WALK: return 180.0f;
    case RUN: return 300.0f;
    default: return 180.0f;
    }
}

MoveMode GetMoveMode(const Uint8* keystate)
{
    if (keystate[SDL_SCANCODE_LCTRL] || keystate[SDL_SCANCODE_RCTRL])
    {
        return SNEAK;
    }

    if (keystate[SDL_SCANCODE_LSHIFT] || keystate[SDL_SCANCODE_RSHIFT])
    {
        return RUN;
    }

    return WALK;
}

// =====================================================
// 적 구조체
// =====================================================

struct Enemy
{
    SDL_Rect rect;

    float angle;

    float fov;
    float viewDist;

    float rotateSpeed;
};

// =====================================================
// 선분 교차
// =====================================================

bool LineIntersect(float x1, float y1, float x2, float y2,
    float x3, float y3, float x4, float y4)
{
    float denom = (y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1);

    if (denom == 0.0f)
        return false;

    float ua =
        ((x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3)) / denom;

    float ub =
        ((x2 - x1) * (y1 - y3) - (y2 - y1) * (x1 - x3)) / denom;

    return (ua >= 0 && ua <= 1 &&
        ub >= 0 && ub <= 1);
}

// =====================================================
// 선 vs 사각형
// =====================================================

bool LineIntersectsRect(float x1, float y1,
    float x2, float y2,
    SDL_Rect rect)
{
    float rx = rect.x;
    float ry = rect.y;
    float rw = rect.w;
    float rh = rect.h;

    if (LineIntersect(x1, y1, x2, y2, rx, ry, rx + rw, ry)) return true;
    if (LineIntersect(x1, y1, x2, y2, rx, ry, rx, ry + rh)) return true;
    if (LineIntersect(x1, y1, x2, y2, rx + rw, ry, rx + rw, ry + rh)) return true;
    if (LineIntersect(x1, y1, x2, y2, rx, ry + rh, rx + rw, ry + rh)) return true;

    return false;
}

// =====================================================
// FOV 렌더
// =====================================================

void DrawFOV(SDL_Renderer* renderer,
    Enemy& enemy,
    std::vector<SDL_Rect>& walls)
{
    int rays = 120;

    float start = enemy.angle - enemy.fov / 2;
    float step = enemy.fov / rays;

    SDL_Point center =
    {
        enemy.rect.x + enemy.rect.w / 2,
        enemy.rect.y + enemy.rect.h / 2
    };

    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 80);

    for (int i = 0; i < rays; i++)
    {
        float a = start + step * i;

        float dx = cos(a);
        float dy = sin(a);

        float rayLength = enemy.viewDist;

        for (auto& w : walls)
        {
            for (float t = 0; t < enemy.viewDist; t += 2.0f)
            {
                float rx = center.x + dx * t;
                float ry = center.y + dy * t;

                if (rx >= w.x && rx <= w.x + w.w &&
                    ry >= w.y && ry <= w.y + w.h)
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

        SDL_RenderDrawLine(renderer,
            center.x, center.y,
            end.x, end.y);
    }
}

// =====================================================
// 플레이어 이동
// =====================================================

void MovePlayer(SDL_Rect& player,
    float dx,
    float dy,
    std::vector<SDL_Rect>& walls)
{
    SDL_Rect next = player;

    next.x += (int)dx;

    for (auto& w : walls)
    {
        if (SDL_HasIntersection(&next, &w))
        {
            dx = 0.0f;
            break;
        }
    }

    player.x += (int)dx;

    next = player;

    next.y += (int)dy;

    for (auto& w : walls)
    {
        if (SDL_HasIntersection(&next, &w))
        {
            dy = 0.0f;
            break;
        }
    }

    player.y += (int)dy;
}

// =====================================================
// 스테이지 리셋
// =====================================================

void ResetStage(SDL_Rect& player,
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
// 메인
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
        SDL_CreateRenderer(window,
            -1,
            SDL_RENDERER_ACCELERATED);

    // 폰트
    TTF_Font* font =
        TTF_OpenFont("unscii-16.ttf", 48);

    if (!font)
    {
        std::cout << "Font load failed\n";
        return 0;
    }

    // 플레이어
    SDL_Rect player = { 100,100,32,32 };

    // 목적지
    SDL_Rect goal =
    {
        700,
        500,
        40,
        40
    };

    // 벽
    std::vector<SDL_Rect> walls =
    {
        {200,150,100,200},
        {400,100,50,300},
        {100,400,300,50}
    };

    // 적
    std::vector<Enemy> enemies;

    ResetStage(player, enemies);

    // 게임 상태
    GameState gameState = PLAYING;

    Uint32 stateTimer = 0;
    Uint64 previousCounter = SDL_GetPerformanceCounter();
    Uint64 performanceFrequency = SDL_GetPerformanceFrequency();

    // =================================================
    // 루프
    // =================================================

    while (running)
    {
        Uint64 currentCounter = SDL_GetPerformanceCounter();

        float dt = (float)(currentCounter - previousCounter) / (float)performanceFrequency;

        previousCounter = currentCounter;

        if (dt > 0.05f)
            dt = 0.05f;
        
        SDL_Event e;

        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                running = false;
        }

        // =============================================
        // 플레이 중
        // =============================================

        if (gameState == PLAYING)
        {
            const Uint8* keystate =
                SDL_GetKeyboardState(NULL);

            MoveMode moveMode = GetMoveMode(keystate);
            float moveSpeed = GetMoveSpeed(moveMode);

            float dirX = 0.0f;
            float dirY = 0.0f;

            if (keystate[SDL_SCANCODE_W]) dirY -= 1.0f;
            if (keystate[SDL_SCANCODE_S]) dirY += 1.0f;
            if (keystate[SDL_SCANCODE_A]) dirX -= 1.0f;
            if (keystate[SDL_SCANCODE_D]) dirX += 1.0f;

            float length = sqrt(dirX * dirX + dirY * dirY);

            if (length > 0.0f)
            {
                dirX /= length;
                dirY /= length;
            }

            float dx = dirX * moveSpeed * dt;
            float dy = dirY * moveSpeed * dt;

            MovePlayer(player, dx, dy, walls);

            // =========================================
            // 소음
            // =========================================

            if (moveMode == RUN)
            {
                // 달리는 중 소음 발생 가능
            }

            if (moveMode == SNEAK)
            {
                // 소음 없음
            }

            // =========================================
            // 목표 도착
            // =========================================

            if (SDL_HasIntersection(&player, &goal))
            {
                gameState = WIN;
                stateTimer = SDL_GetTicks();
            }

            // =========================================
            // 적 AI
            // =========================================

            for (auto& enemy : enemies)
            {
                enemy.angle += enemy.rotateSpeed;

                float ex =
                    enemy.rect.x + enemy.rect.w / 2;

                float ey =
                    enemy.rect.y + enemy.rect.h / 2;

                float px =
                    player.x + player.w / 2;

                float py =
                    player.y + player.h / 2;

                // 적 방향
                float dirX = cos(enemy.angle);
                float dirY = sin(enemy.angle);

                // 적 -> 플레이어
                float toPX = px - ex;
                float toPY = py - ey;

                float distSq =
                    toPX * toPX + toPY * toPY;

                if (distSq <
                    enemy.viewDist * enemy.viewDist)
                {
                    if (distSq < 0.0001f)
                        continue;

                    float dist = sqrt(distSq);

                    float normX = toPX / dist;
                    float normY = toPY / dist;

                    float dot =
                        dirX * normX +
                        dirY * normY;

                    float fovLimit =
                        cos(enemy.fov / 2);

                    if (dot > fovLimit)
                    {
                        bool blocked = false;

                        for (auto& w : walls)
                        {
                            if (LineIntersectsRect(
                                ex, ey,
                                px, py,
                                w))
                            {
                                blocked = true;
                                break;
                            }
                        }

                        if (!blocked)
                        {
                            gameState = LOSE;
                            stateTimer = SDL_GetTicks();
                        }
                    }
                }
            }
        }
        else
        {
            // =========================================
            // 2초 후 리셋
            // =========================================

            if (SDL_GetTicks() - stateTimer >= 2000)
            {
                ResetStage(player, enemies);

                gameState = PLAYING;
            }
        }

        // =============================================
        // 렌더
        // =============================================

        SDL_SetRenderDrawColor(renderer,
            20, 20, 20, 255);

        SDL_RenderClear(renderer);

        // FOV
        for (auto& enemy : enemies)
        {
            DrawFOV(renderer, enemy, walls);
        }

        // 벽
        SDL_SetRenderDrawColor(renderer,
            120, 120, 120, 255);

        for (auto& w : walls)
        {
            SDL_RenderFillRect(renderer, &w);
        }

        // 목표 지점
        SDL_SetRenderDrawColor(renderer,
            255, 255, 0, 255);

        SDL_RenderDrawRect(renderer, &goal);

        // 플레이어
        SDL_SetRenderDrawColor(renderer,
            0, 255, 0, 255);

        SDL_RenderFillRect(renderer, &player);

        // 적
        SDL_SetRenderDrawColor(renderer,
            255, 0, 0, 255);

        for (auto& enemy : enemies)
        {
            SDL_RenderFillRect(renderer,
                &enemy.rect);
        }

        // =============================================
        // 중앙 텍스트
        // =============================================

        if (gameState == WIN ||
            gameState == LOSE)
        {
            const char* text =
                (gameState == WIN)
                ? "STAGE CLEAR"
                : "GAME OVER";

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

            SDL_RenderCopy(renderer,
                texture,
                NULL,
                &dst);

            SDL_FreeSurface(surface);
            SDL_DestroyTexture(texture);
        }

        SDL_RenderPresent(renderer);

        SDL_Delay(16);
    }

    // 정리
    TTF_CloseFont(font);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);

    TTF_Quit();
    SDL_Quit();

    return 0;
}
