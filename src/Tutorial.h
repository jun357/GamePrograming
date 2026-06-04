#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <string>
#include <vector>

#include "Stage.h"
#include "Wall.h"
#include "Player.h"

enum class TutorialPhase
{
    None,
    MovementWalk,
    MovementSneak,
    MovementRun,
    MovementDone
};

class TutorialController
{
public:
    void Reset(const StageMapSetup& setup);

    void Update(
        const SDL_Rect& player,
        MoveMode moveMode,
        bool playerMoving);

    void AppendActivePlayerBlockers(std::vector<Wall>& walls) const;

    void DrawUI(SDL_Renderer* renderer, TTF_Font* font) const;

    bool IsMovementTrainingDone() const;

private:
    TutorialPhase phase_ = TutorialPhase::None;

    std::vector<StageTutorialTriggerDef> triggers_;
    std::vector<StageTutorialBlockerDef> blockers_;
    std::vector<bool> blockerActive_;

    std::string currentTextId_;

    float currentDistance_ = 0.0f;
    float walkRequired_ = 200.0f;
    float sneakRequired_ = 150.0f;
    float runRequired_ = 300.0f;

    SDL_Point previousPlayerCenter_ = { 0, 0 };
    bool hasPreviousPlayerCenter_ = false;

private:
    void TryStartMovementTraining(const SDL_Rect& player);
    void BeginMovementTraining(const StageTutorialTriggerDef& trigger);
    void UpdateMovementTraining(
        const SDL_Rect& player,
        MoveMode moveMode,
        bool playerMoving);

    void AdvanceToSneak();
    void AdvanceToRun();
    void CompleteMovementTraining();

    void UnlockBlockersByCondition(const std::string& condition);

    float GetRequiredDistanceForCurrentPhase() const;
    float GetProgress01() const;

    static SDL_Point GetRectCenterPoint(const SDL_Rect& rect);
    static bool RectIntersects(const SDL_Rect& a, const SDL_Rect& b);
};