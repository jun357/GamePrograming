#pragma once

#include <SDL2/SDL.h>
#include <string>
#include <vector>

#include "Wall.h"

struct StageEnemySpawnDef
{
    std::string id;
    std::string kind = "sentry";
    std::string dir = "down";
    SDL_Rect rect = { 0, 0, 64, 64 };

    bool scripted = false;
    bool tutorialPassive = false;
};

struct StageItemSpawnDef
{
    std::string id;
    std::string itemType = "bottle";
    SDL_Rect rect = { 0, 0, 24, 24 };
};

struct StageInteractableDef
{
    std::string id;
    SDL_Rect rect = { 0, 0, 1, 1 };
};

struct StageTutorialTriggerDef
{
    std::string id;
    std::string phase;
    std::string textId;
    std::string displayMode = "untilNext";

    SDL_Rect rect = { 0, 0, 1, 1 };

    bool once = true;
    bool pause = false;

    float walkDistance = 0.0f;
    float sneakDistance = 0.0f;
    float runDistance = 0.0f;
};

struct StageTutorialBlockerDef
{
    std::string id;
    SDL_Rect rect = { 0, 0, 1, 1 };

    bool initialActive = true;
    bool visible = false;

    std::string blocks = "player";
    std::string unlockWhen;
};

struct StageMapSetup
{
    SDL_Rect playerStart = { 100, 100, 32, 32 };

    std::vector<Wall> baseWalls;
    std::vector<Wall> anomalyWalls;

    std::vector<StageEnemySpawnDef> enemySpawns;
    std::vector<StageItemSpawnDef> itemSpawns;
    std::vector<StageInteractableDef> interactables;
    std::vector<StageTutorialTriggerDef> tutorialTriggers;
    std::vector<StageTutorialBlockerDef> tutorialBlockers;

    SDL_Rect goalNormal = { 700, 500, 40, 40 };
    SDL_Rect goalAnomaly = { 700, 100, 40, 40 };

    int mapWidth = 1600;
    int mapHeight = 1200;
};

StageMapSetup LoadStageMapFromJson(const char* path);

StageMapSetup MakeTutorialMovementStageMap();

StageMapSetup MakePrototypeMainStageMap();