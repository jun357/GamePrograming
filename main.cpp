#include <SDL2/SDL.h>
#include <iostream>
#include <cmath>

const int SCREEN_WIDTH = 800;
const int SCREEN_HEIGHT = 600;

bool running = true;

// FOV 그리기 함수
void DrawFOV(SDL_Renderer* renderer, SDL_Rect enemy, float angle, float fov, float dist) {
    int segments = 40;

    float startAngle = angle - fov / 2;
    float step = fov / segments;

    SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);

    SDL_Point center = { enemy.x + enemy.w / 2, enemy.y + enemy.h / 2 };

    for (int i = 0; i < segments; i++) {
        float a1 = startAngle + step * i;
        float a2 = startAngle + step * (i + 1);

        SDL_Point p1 = {
            (int)(center.x + cos(a1) * dist),
            (int)(center.y + sin(a1) * dist)
        };

        SDL_Point p2 = {
            (int)(center.x + cos(a2) * dist),
            (int)(center.y + sin(a2) * dist)
        };

        SDL_RenderDrawLine(renderer, center.x, center.y, p1.x, p1.y);
        SDL_RenderDrawLine(renderer, p1.x, p1.y, p2.x, p2.y);
        SDL_RenderDrawLine(renderer, p2.x, p2.y, center.x, center.y);
    }
}

int main(int argc, char* args[]) {
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window* window = SDL_CreateWindow("Stealth MVP",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT,
        SDL_WINDOW_SHOWN);

    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    SDL_Rect player = { 100, 100, 32, 32 };
    SDL_Rect enemy = { 400, 300, 32, 32 };

    float enemyAngle = 0.0f;
    float fov = 60.0f * M_PI / 180.0f;
    float viewDist = 150.0f;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;
        }

        const Uint8* keystate = SDL_GetKeyboardState(NULL);

        if (keystate[SDL_SCANCODE_W]) player.y -= 3;
        if (keystate[SDL_SCANCODE_S]) player.y += 3;
        if (keystate[SDL_SCANCODE_A]) player.x -= 3;
        if (keystate[SDL_SCANCODE_D]) player.x += 3;

        // 적 회전 (자동 순찰 느낌)
        enemyAngle += 0.01f;

        float dx = (player.x + player.w / 2) - (enemy.x + enemy.w / 2);
        float dy = (player.y + player.h / 2) - (enemy.y + enemy.h / 2);

        float distSq = dx * dx + dy * dy;

        if (distSq < viewDist * viewDist) {
            float angleToPlayer = atan2(dy, dx);
            float angleDiff = fabs(angleToPlayer - enemyAngle);

            if (angleDiff > M_PI)
                angleDiff = 2 * M_PI - angleDiff;

            if (angleDiff < fov / 2) {
                std::cout << "Detected! Game Over\n";
                running = false;
            }
        }

        // 렌더링
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderClear(renderer);

        // FOV 먼저 그리기
        DrawFOV(renderer, enemy, enemyAngle, fov, viewDist);

        // 플레이어
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        SDL_RenderFillRect(renderer, &player);

        // 적
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderFillRect(renderer, &enemy);

        SDL_RenderPresent(renderer);

        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}