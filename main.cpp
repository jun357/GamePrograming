#include <SDL2/SDL.h>
#include <iostream>
#include <vector>
#include <cmath>

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

bool running = true;

// -----------------------------
// 선분 교차
// -----------------------------
bool LineIntersect(float x1, float y1, float x2, float y2,
                   float x3, float y3, float x4, float y4)
{
    float denom = (y4 - y3)*(x2 - x1) - (x4 - x3)*(y2 - y1);
    if (denom == 0.0f) return false;

    float ua = ((x4 - x3)*(y1 - y3) - (y4 - y3)*(x1 - x3)) / denom;
    float ub = ((x2 - x1)*(y1 - y3) - (y2 - y1)*(x1 - x3)) / denom;

    return (ua >= 0 && ua <= 1 && ub >= 0 && ub <= 1);
}

// -----------------------------
// 선 vs 사각형 (시야 차단)
// -----------------------------
bool LineIntersectsRect(float x1, float y1, float x2, float y2, SDL_Rect rect)
{
    float rx = rect.x;
    float ry = rect.y;
    float rw = rect.w;
    float rh = rect.h;

    if (LineIntersect(x1,y1,x2,y2, rx,ry, rx+rw,ry)) return true;
    if (LineIntersect(x1,y1,x2,y2, rx,ry, rx,ry+rh)) return true;
    if (LineIntersect(x1,y1,x2,y2, rx+rw,ry, rx+rw,ry+rh)) return true;
    if (LineIntersect(x1,y1,x2,y2, rx,ry+rh, rx+rw,ry+rh)) return true;

    return false;
}

// -----------------------------
// FOV 그리기
// -----------------------------
void DrawFOV(SDL_Renderer* renderer, SDL_Rect enemy,
    float angle, float fov, float maxDist,
    std::vector<SDL_Rect>& walls)
{
    int rays = 120;

    float start = angle - fov / 2;
    float step = fov / rays;

    SDL_Point center = {
        enemy.x + enemy.w / 2,
        enemy.y + enemy.h / 2
    };

    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 80);

    for (int i = 0; i < rays; i++)
    {
        float a = start + step * i;

        float dx = cos(a);
        float dy = sin(a);

        float rayLength = maxDist;

        // 🔥 벽과 충돌 검사
        for (auto& w : walls)
        {
            for (float t = 0; t < maxDist; t += 2.0f)
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

        SDL_Point end = {
            (int)(center.x + dx * rayLength),
            (int)(center.y + dy * rayLength)
        };

        SDL_RenderDrawLine(renderer, center.x, center.y, end.x, end.y);
    }
}

// -----------------------------
// 플레이어 이동 + 벽 충돌
// -----------------------------
void MovePlayer(SDL_Rect& player, int dx, int dy, std::vector<SDL_Rect>& walls)
{
    SDL_Rect next = player;
    next.x += dx;

    for (auto& w : walls)
    {
        if (SDL_HasIntersection(&next, &w))
            dx = 0;
    }
    player.x += dx;

    next = player;
    next.y += dy;

    for (auto& w : walls)
    {
        if (SDL_HasIntersection(&next, &w))
            dy = 0;
    }
    player.y += dy;
}

// -----------------------------
// 메인
// -----------------------------
int main(int argc, char* args[])
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow("Stealth Game",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    SDL_Rect player = {100,100,32,32};
    SDL_Rect enemy  = {500,300,32,32};

    float enemyAngle = 0.0f;
    float fov = 60.0f * M_PI / 180.0f;
    float viewDist = 200.0f;

    // ?? 여러 벽
    std::vector<SDL_Rect> walls = {
        {200,150,100,200},
        {400,100,50,300},
        {100,400,300,50}
    };

    while (running)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                running = false;
        }

        const Uint8* keystate = SDL_GetKeyboardState(NULL);

        int dx = 0, dy = 0;
        if (keystate[SDL_SCANCODE_W]) dy -= 3;
        if (keystate[SDL_SCANCODE_S]) dy += 3;
        if (keystate[SDL_SCANCODE_A]) dx -= 3;
        if (keystate[SDL_SCANCODE_D]) dx += 3;

        MovePlayer(player, dx, dy, walls);

        // 적 회전
        enemyAngle += 0.01f;

        float ex = enemy.x + enemy.w / 2;
        float ey = enemy.y + enemy.h / 2;
        float px = player.x + player.w / 2;
        float py = player.y + player.h / 2;

        // 방향 벡터 (적이 보는 방향)
        float dirX = cos(enemyAngle);
        float dirY = sin(enemyAngle);

        // 적 → 플레이어 벡터
        float toPX = px - ex;
        float toPY = py - ey;

        // 거리 체크
        float distSq = toPX * toPX + toPY * toPY;

        if (distSq < viewDist * viewDist)
        {
            float dist = sqrt(distSq);

            // 정규화
            float normX = toPX / dist;
            float normY = toPY / dist;

            // 🔥 내적
            float dot = dirX * normX + dirY * normY;

            // 🔥 FOV 판정 (cos 사용)
            float fovLimit = cos(fov / 2);

            if (dot > fovLimit)
            {
                // 👉 여기서 벽 체크
                bool blocked = false;
                for (auto& w : walls)
                {
                    if (LineIntersectsRect(ex, ey, px, py, w))
                    {
                        blocked = true;
                        break;
                    }
                }

                if (!blocked)
                {
                    std::cout << "Detected! Game Over\n";
                    running = false;
                }
            }
        }

        // ---------------- 렌더 ----------------
        SDL_SetRenderDrawColor(renderer, 20,20,20,255);
        SDL_RenderClear(renderer);

        DrawFOV(renderer, enemy, enemyAngle, fov, viewDist, walls);

        // 벽
        SDL_SetRenderDrawColor(renderer, 120,120,120,255);
        for (auto& w : walls)
            SDL_RenderFillRect(renderer, &w);

        // 플레이어
        SDL_SetRenderDrawColor(renderer, 0,255,0,255);
        SDL_RenderFillRect(renderer, &player);

        // 적
        SDL_SetRenderDrawColor(renderer, 255,0,0,255);
        SDL_RenderFillRect(renderer, &enemy);

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}