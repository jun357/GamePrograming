#include "Stage.h"

#include "Sound.h"

#include <fstream>
#include <string>
#include <cmath>
#include <exception>

#include <nlohmann/json.hpp>

using json = nlohmann::json;

static int RoundToInt(double value)
{
    return static_cast<int>(std::lround(value));
}

static std::string GetObjectClassOrType(const json& object)
{
    if (object.contains("class") &&
        object["class"].is_string() &&
        !object["class"].get<std::string>().empty())
    {
        return object["class"].get<std::string>();
    }

    if (object.contains("type") &&
        object["type"].is_string() &&
        !object["type"].get<std::string>().empty())
    {
        return object["type"].get<std::string>();
    }

    return "";
}

static const json* FindProperty(const json& object, const char* name)
{
    if (!object.contains("properties") || !object["properties"].is_array())
    {
        return nullptr;
    }

    for (const auto& property : object["properties"])
    {
        if (!property.contains("name") || !property["name"].is_string())
        {
            continue;
        }

        if (property["name"].get<std::string>() == name)
        {
            return &property;
        }
    }

    return nullptr;
}

static std::string GetPropertyString(
    const json& object,
    const char* name,
    const std::string& fallback = "")
{
    const json* property = FindProperty(object, name);
    if (!property || !property->contains("value"))
    {
        return fallback;
    }

    const json& value = (*property)["value"];

    if (value.is_string())
    {
        return value.get<std::string>();
    }

    if (value.is_boolean())
    {
        return value.get<bool>() ? "true" : "false";
    }

    if (value.is_number_integer())
    {
        return std::to_string(value.get<int>());
    }

    if (value.is_number_float())
    {
        return std::to_string(value.get<double>());
    }

    return fallback;
}

static bool GetPropertyBool(
    const json& object,
    const char* name,
    bool fallback = false)
{
    const json* property = FindProperty(object, name);
    if (!property || !property->contains("value"))
    {
        return fallback;
    }

    const json& value = (*property)["value"];

    if (value.is_boolean())
    {
        return value.get<bool>();
    }

    if (value.is_string())
    {
        std::string text = value.get<std::string>();
        return text == "true" || text == "True" || text == "1";
    }

    if (value.is_number_integer())
    {
        return value.get<int>() != 0;
    }

    return fallback;
}

static float GetPropertyFloat(
    const json& object,
    const char* name,
    float fallback = 0.0f)
{
    const json* property = FindProperty(object, name);
    if (!property || !property->contains("value"))
    {
        return fallback;
    }

    const json& value = (*property)["value"];

    if (value.is_number())
    {
        return value.get<float>();
    }

    if (value.is_string())
    {
        try
        {
            return std::stof(value.get<std::string>());
        }
        catch (...)
        {
            return fallback;
        }
    }

    return fallback;
}

static SDL_Rect MakeRectFromObject(
    const json& object,
    int fallbackW = 0,
    int fallbackH = 0)
{
    int x = RoundToInt(object.value("x", 0.0));
    int y = RoundToInt(object.value("y", 0.0));
    int w = RoundToInt(object.value("width", 0.0));
    int h = RoundToInt(object.value("height", 0.0));

    if (w <= 0)
    {
        w = fallbackW;
    }

    if (h <= 0)
    {
        h = fallbackH;
    }

    return { x, y, w, h };
}

StageMapSetup LoadStageMapFromJson(const char* path)
{
    StageMapSetup setup;

    setup.baseWalls.clear();
    setup.anomalyWalls.clear();
    setup.enemySpawns.clear();
    setup.itemSpawns.clear();
    setup.interactables.clear();
    setup.tutorialTriggers.clear();
    setup.tutorialBlockers.clear();

    setup.goalNormal = { -1000, -1000, 1, 1 };
    setup.goalAnomaly = { -1000, -1000, 1, 1 };

    std::ifstream file(path);
    if (!file.is_open())
    {
        SDL_Log("Cannot open map JSON: %s", path);
        return setup;
    }

    try
    {
        json map = json::parse(file);

        int tileW = map.value("tilewidth", 1);
        int tileH = map.value("tileheight", 1);

        setup.mapWidth = map.value("width", 1600) * tileW;
        setup.mapHeight = map.value("height", 1200) * tileH;

        if (!map.contains("layers") || !map["layers"].is_array())
        {
            SDL_Log("Map has no layers: %s", path);
            return setup;
        }

        for (const auto& layer : map["layers"])
        {
            if (layer.value("type", std::string("")) != "objectgroup")
            {
                continue;
            }

            std::string layerName = layer.value("name", std::string(""));

            if (!layer.contains("objects") || !layer["objects"].is_array())
            {
                continue;
            }

            for (const auto& object : layer["objects"])
            {
                if (!object.value("visible", true))
                {
                    continue;
                }

                std::string objectClass = GetObjectClassOrType(object);
                std::string objectName = object.value("name", std::string(""));

                if (objectClass.empty())
                {
                    if (layerName == "Walls")
                    {
                        objectClass = "wall";
                    }
                    else if (layerName == "Spawns")
                    {
                        objectClass = "player_spawn";
                    }
                    else if (layerName == "Goals")
                    {
                        objectClass = "goal";
                    }
                    else if (layerName == "Items")
                    {
                        objectClass = "item";
                    }
                    else if (layerName == "Enemies")
                    {
                        objectClass = "enemy";
                    }
                    else if (layerName == "Interactables")
                    {
                        objectClass = "interactable";
                    }
                    else if (layerName == "TutorialTriggers")
                    {
                        objectClass = "tutorial_trigger";
                    }
                    else if (layerName == "TutorialBlockers")
                    {
                        objectClass = "tutorial_blocker";
                    }
                }

                if (objectClass == "wall")
                {
                    SDL_Rect rect = MakeRectFromObject(object);

                    if (rect.w <= 0 || rect.h <= 0)
                    {
                        continue;
                    }

                    std::string group =
                        GetPropertyString(object, "group", "base");

                    if (group == "anomaly")
                    {
                        setup.anomalyWalls.push_back(Wall(rect));
                    }
                    else
                    {
                        setup.baseWalls.push_back(Wall(rect));
                    }
                }
                else if (objectClass == "player_spawn" ||
                         objectName == "player_start")
                {
                    SDL_Rect rect = MakeRectFromObject(object, 32, 32);

                        if (rect.w <= 0)
                        {
                            rect.w = 64;
                        }
                        
                        if (rect.h <= 0)
                        {
                            rect.h = 64;
                        }
                        
                        setup.playerStart = rect;
                }
                else if (objectClass == "goal")
                {
                    SDL_Rect rect = MakeRectFromObject(object, 80, 80);

                    std::string mode =
                        GetPropertyString(object, "mode", "normal");

                    if (mode == "anomaly" || objectName == "goal_anomaly")
                    {
                        setup.goalAnomaly = rect;
                    }
                    else
                    {
                        setup.goalNormal = rect;
                    }
                }
                else if (objectClass == "item")
                {
                    StageItemSpawnDef item;
                    item.id = GetPropertyString(object, "id", objectName);
                    item.itemType = GetPropertyString(object, "itemType", "");
                    if (item.itemType.empty())
                    {
                        item.itemType = item.id;
                    }
                    if (item.itemType.empty())
                    {
                        item.itemType = objectName;
                    }
                    if (item.itemType.empty())
                    {
                        item.itemType = "bottle";
                    }
                    item.rect = MakeRectFromObject(object, 24, 24);
                    setup.itemSpawns.push_back(item);
                }
                else if (objectClass == "interactable")
                {
                    StageInteractableDef interactable;
                    interactable.id = GetPropertyString(object, "id", objectName);
                    if (interactable.id.empty())
                    {
                        interactable.id = objectName;
                    }
                    interactable.rect = MakeRectFromObject(object, 1, 1);
                    setup.interactables.push_back(interactable);
                }
                else if (objectClass == "enemy")
                {
                    StageEnemySpawnDef enemy;

                    enemy.id = GetPropertyString(object, "id", objectName);
                    if (enemy.id.empty())
                    {
                        enemy.id = objectName;
                    }

                    enemy.kind = GetPropertyString(object, "kind", "sentry");
                    enemy.dir = GetPropertyString(object, "dir", "down");
                    enemy.scripted = GetPropertyBool(object, "scripted", false);
                    enemy.tutorialPassive = GetPropertyBool(object, "tutorialPassive", false);
                    enemy.rect = MakeRectFromObject(object, 64, 64);

                    setup.enemySpawns.push_back(enemy);
                }
                else if (objectClass == "tutorial_trigger")
                {
                    StageTutorialTriggerDef trigger;

                    trigger.id = GetPropertyString(object, "id", objectName);
                    if (trigger.id.empty())
                    {
                        trigger.id = objectName;
                    }

                    trigger.phase = GetPropertyString(object, "phase", "");
                    trigger.textId = GetPropertyString(object, "textId", "");
                    trigger.displayMode =
                        GetPropertyString(object, "displayMode", "untilNext");

                    trigger.once = GetPropertyBool(object, "once", true);
                    trigger.pause = GetPropertyBool(object, "pause", false);

                    trigger.walkDistance =
                        GetPropertyFloat(object, "walkDistance", 0.0f);
                    trigger.sneakDistance =
                        GetPropertyFloat(object, "sneakDistance", 0.0f);
                    trigger.runDistance =
                        GetPropertyFloat(object, "runDistance", 0.0f);

                    trigger.rect = MakeRectFromObject(object, 1, 1);

                    setup.tutorialTriggers.push_back(trigger);
                }
                else if (objectClass == "tutorial_blocker")
                {
                    StageTutorialBlockerDef blocker;

                    blocker.id = GetPropertyString(object, "id", objectName);
                    if (blocker.id.empty())
                    {
                        blocker.id = objectName;
                    }

                    blocker.blocks =
                        GetPropertyString(object, "blocks", "player");
                    blocker.unlockWhen =
                        GetPropertyString(object, "unlockWhen", "");

                    blocker.initialActive =
                        GetPropertyBool(object, "initialActive", true);
                    blocker.visible =
                        GetPropertyBool(object, "visible", false);

                    blocker.rect = MakeRectFromObject(object, 1, 1);

                    setup.tutorialBlockers.push_back(blocker);
                }
            }
        }

        PrepareSoundWalls(setup.baseWalls);
        PrepareSoundWalls(setup.anomalyWalls);

        SDL_Log(
            "Loaded map JSON: %s | walls=%d enemies=%d items=%d blockers=%d map=%dx%d",
            path,
            static_cast<int>(setup.baseWalls.size()),
            static_cast<int>(setup.enemySpawns.size()),
            static_cast<int>(setup.itemSpawns.size()),
            static_cast<int>(setup.tutorialBlockers.size()),
            setup.mapWidth,
            setup.mapHeight);

        return setup;
    }
    catch (const std::exception& e)
    {
        SDL_Log("Failed to parse map JSON [%s]: %s", path, e.what());
        return setup;
    }
}

StageMapSetup MakeTutorialStageMap()
{
    StageMapSetup loaded =
        LoadStageMapFromJson("assets/maps/tutorial.json");

    if (!loaded.baseWalls.empty())
    {
        return loaded;
    }

    StageMapSetup setup;

    setup.playerStart = { 160, 320, 32, 32 };

    setup.baseWalls.clear();
    setup.anomalyWalls.clear();

    // =====================================================
    // 1번 구간
    // =====================================================

    setup.baseWalls.push_back(Wall({ 0, 140, 3000, 24 }));

    setup.baseWalls.push_back(Wall({ 0, 560, 1080, 24 }));

    setup.baseWalls.push_back(Wall({ 0, 140, 24, 444 }));

    // =====================================================
    // 2번 구간
    // =====================================================

    setup.baseWalls.push_back(Wall({ 1280, 340, 1720, 24 }));

    setup.baseWalls.push_back(Wall({ 1280, 340, 24, 520 }));

    setup.baseWalls.push_back(Wall({ 1080, 560, 24, 504 }));

    setup.baseWalls.push_back(Wall({ 1080, 1040, 200, 24 }));

    // =====================================================
    // 3번 구간
    // =====================================================

    setup.baseWalls.push_back(Wall({ 1280, 900, 360, 24 }));

    setup.baseWalls.push_back(Wall({ 1280, 1040, 360, 24 }));

    setup.baseWalls.push_back(Wall({ 1640, 760, 840, 24 }));

    setup.baseWalls.push_back(Wall({ 1640, 760, 24, 140 }));

    setup.baseWalls.push_back(Wall({ 1640, 1040, 24, 136 }));

    setup.baseWalls.push_back(Wall({ 1640, 1176, 840, 24 }));

    // =====================================================
    // 4번 구간
    // =====================================================

    setup.baseWalls.push_back(Wall({ 2480, 760, 24, 140 }));

    setup.baseWalls.push_back(Wall({ 2480, 1040, 24, 136 }));

    setup.baseWalls.push_back(Wall({ 2480, 900, 520, 24 }));

    setup.baseWalls.push_back(Wall({ 2480, 1040, 80, 24 }));

    setup.baseWalls.push_back(Wall({ 2680, 1040, 320, 24 }));

    setup.baseWalls.push_back(Wall({ 2560, 1040, 24, 560 }));

    setup.baseWalls.push_back(Wall({ 2680, 1040, 24, 560 }));

    setup.baseWalls.push_back(Wall({ 2560, 1576, 144, 24 }));

    setup.goalNormal = { -1000, -1000, 1, 1 };
    setup.goalAnomaly = { -1000, -1000, 1, 1 };

    PrepareSoundWalls(setup.baseWalls);
    PrepareSoundWalls(setup.anomalyWalls);

    return setup;
}

StageMapSetup MakePrototypeMainStageMap()
{
    StageMapSetup setup;

    setup.playerStart = { 100, 100, 32, 32 };

    setup.baseWalls =
    {
        Wall({ 200, 150, 20, 200 }),
        Wall({ 400, 100, 20, 300 }),
        Wall({ 100, 400, 300, 20 })
    };

    setup.anomalyWalls =
    {
        Wall({ 500, 350, 200, 20 }),
        Wall({ 250, 250, 20, 200 })
    };

    setup.goalNormal = { 700, 500, 40, 40 };
    setup.goalAnomaly = { 700, 100, 40, 40 };

    PrepareSoundWalls(setup.baseWalls);
    PrepareSoundWalls(setup.anomalyWalls);

    return setup;
}
