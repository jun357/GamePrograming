#include "Stage.h"

#include "Sound.h"

StageMapSetup MakeTutorialMovementStageMap()
{
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
