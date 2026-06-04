#include "Tutorial.h"

#include <cmath>
#include <cstdio>

#include "Globals.h"

static const char* GetTutorialTextFromId(const std::string& textId)
{
    if (textId == "tutorial.walk")
    {
        return "Use WASD to walk";
    }

    if (textId == "tutorial.sneak")
    {
        return "Hold Ctrl while moving to Sneak.";
    }

    if (textId == "tutorial.run")
    {
        return "Hold Shift while moving to Run.";
    }

    if (textId == "tutorial.movement_done")
    {
        return "Avoid the guards and head down.";
    }

    return "";
}

static const char* GetTutorialHintFromId(const std::string& textId)
{
    if (textId == "tutorial.walk")
    {
        return "";
    }

    if (textId == "tutorial.sneak")
    {
        return "Sneak makes no noise.";
    }

    if (textId == "tutorial.run")
    {
        return "Run is fast, but it can create loud noise.";
    }

    if (textId == "tutorial.movement_done")
    {
        return "";
    }

    return "";
}

SDL_Point TutorialController::GetRectCenterPoint(const SDL_Rect& rect)
{
    return
    {
        rect.x + rect.w / 2,
        rect.y + rect.h / 2
    };
}

bool TutorialController::RectIntersects(
    const SDL_Rect& a,
    const SDL_Rect& b)
{
    return SDL_HasIntersection(&a, &b) == SDL_TRUE;
}

void TutorialController::Reset(const StageMapSetup& setup)
{
    triggers_ = setup.tutorialTriggers;
    blockers_ = setup.tutorialBlockers;

    blockerActive_.clear();
    blockerActive_.reserve(blockers_.size());

    for (const auto& blocker : blockers_)
    {
        blockerActive_.push_back(blocker.initialActive);
    }

    phase_ = TutorialPhase::None;
    currentTextId_.clear();

    currentDistance_ = 0.0f;

    walkRequired_ = 200.0f;
    sneakRequired_ = 150.0f;
    runRequired_ = 300.0f;

    previousPlayerCenter_ = { 0, 0 };
    hasPreviousPlayerCenter_ = false;
}

void TutorialController::Update(
    const SDL_Rect& player,
    MoveMode moveMode,
    bool playerMoving)
{
    if (phase_ == TutorialPhase::None)
    {
        TryStartMovementTraining(player);
        return;
    }

    UpdateMovementTraining(player, moveMode, playerMoving);
}

void TutorialController::TryStartMovementTraining(const SDL_Rect& player)
{
    for (const auto& trigger : triggers_)
    {
        if (trigger.phase != "movement_training")
        {
            continue;
        }

        if (!RectIntersects(player, trigger.rect))
        {
            continue;
        }

        BeginMovementTraining(trigger);
        return;
    }
}

void TutorialController::BeginMovementTraining(
    const StageTutorialTriggerDef& trigger)
{
    phase_ = TutorialPhase::MovementWalk;

    currentTextId_ =
        trigger.textId.empty()
        ? "tutorial.walk"
        : trigger.textId;

    if (trigger.walkDistance > 0.0f)
    {
        walkRequired_ = trigger.walkDistance;
    }

    if (trigger.sneakDistance > 0.0f)
    {
        sneakRequired_ = trigger.sneakDistance;
    }

    if (trigger.runDistance > 0.0f)
    {
        runRequired_ = trigger.runDistance;
    }

    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;
}

void TutorialController::UpdateMovementTraining(
    const SDL_Rect& player,
    MoveMode moveMode,
    bool playerMoving)
{
    if (phase_ != TutorialPhase::MovementWalk &&
        phase_ != TutorialPhase::MovementSneak &&
        phase_ != TutorialPhase::MovementRun)
    {
        return;
    }

    SDL_Point now = GetRectCenterPoint(player);

    if (!hasPreviousPlayerCenter_)
    {
        previousPlayerCenter_ = now;
        hasPreviousPlayerCenter_ = true;
        return;
    }

    float dx = static_cast<float>(now.x - previousPlayerCenter_.x);
    float dy = static_cast<float>(now.y - previousPlayerCenter_.y);

    previousPlayerCenter_ = now;

    float movedDistance = std::sqrt(dx * dx + dy * dy);

    if (!playerMoving || movedDistance <= 0.001f)
    {
        return;
    }

    bool correctMode = false;

    if (phase_ == TutorialPhase::MovementWalk)
    {
        correctMode = (moveMode == WALK);
    }
    else if (phase_ == TutorialPhase::MovementSneak)
    {
        correctMode = (moveMode == SNEAK);
    }
    else if (phase_ == TutorialPhase::MovementRun)
    {
        correctMode = (moveMode == RUN);
    }

    if (!correctMode)
    {
        return;
    }

    currentDistance_ += movedDistance;

    if (phase_ == TutorialPhase::MovementWalk &&
        currentDistance_ >= walkRequired_)
    {
        AdvanceToSneak();
    }
    else if (phase_ == TutorialPhase::MovementSneak &&
             currentDistance_ >= sneakRequired_)
    {
        AdvanceToRun();
    }
    else if (phase_ == TutorialPhase::MovementRun &&
             currentDistance_ >= runRequired_)
    {
        CompleteMovementTraining();
    }
}

void TutorialController::AdvanceToSneak()
{
    phase_ = TutorialPhase::MovementSneak;
    currentTextId_ = "tutorial.sneak";
    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;
}

void TutorialController::AdvanceToRun()
{
    phase_ = TutorialPhase::MovementRun;
    currentTextId_ = "tutorial.run";
    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;
}

void TutorialController::CompleteMovementTraining()
{
    phase_ = TutorialPhase::MovementDone;
    currentTextId_ = "tutorial.movement_done";
    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;

    UnlockBlockersByCondition("movement_done");
}

void TutorialController::UnlockBlockersByCondition(
    const std::string& condition)
{
    for (size_t i = 0; i < blockers_.size() && i < blockerActive_.size(); ++i)
    {
        if (blockers_[i].unlockWhen == condition)
        {
            blockerActive_[i] = false;
        }
    }
}

void TutorialController::AppendActivePlayerBlockers(
    std::vector<Wall>& walls) const
{
    for (size_t i = 0; i < blockers_.size() && i < blockerActive_.size(); ++i)
    {
        if (!blockerActive_[i])
        {
            continue;
        }

        if (blockers_[i].blocks != "player")
        {
            continue;
        }

        walls.push_back(Wall(blockers_[i].rect));
    }
}

bool TutorialController::IsMovementTrainingDone() const
{
    return phase_ == TutorialPhase::MovementDone;
}

float TutorialController::GetRequiredDistanceForCurrentPhase() const
{
    if (phase_ == TutorialPhase::MovementWalk)
    {
        return walkRequired_;
    }

    if (phase_ == TutorialPhase::MovementSneak)
    {
        return sneakRequired_;
    }

    if (phase_ == TutorialPhase::MovementRun)
    {
        return runRequired_;
    }

    return 1.0f;
}

float TutorialController::GetProgress01() const
{
    float required = GetRequiredDistanceForCurrentPhase();

    if (required <= 0.0f)
    {
        return 1.0f;
    }

    float progress = currentDistance_ / required;

    if (progress < 0.0f)
    {
        return 0.0f;
    }

    if (progress > 1.0f)
    {
        return 1.0f;
    }

    return progress;
}

void TutorialController::DrawUI(
    SDL_Renderer* renderer,
    TTF_Font* font) const
{
    if (!renderer || !font || currentTextId_.empty())
    {
        return;
    }

    const char* message = GetTutorialTextFromId(currentTextId_);

    if (!message || message[0] == '\0')
    {
        return;
    }

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    SDL_Rect panel =
    {
        40,
        SCREEN_HEIGHT - 118,
        SCREEN_WIDTH - 80,
        82
    };

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 185);
    SDL_RenderFillRect(renderer, &panel);

    SDL_SetRenderDrawColor(renderer, 230, 230, 230, 230);
    SDL_RenderDrawRect(renderer, &panel);

    SDL_Color textColor = { 255, 255, 255, 255 };

    SDL_Surface* surface =
        TTF_RenderUTF8_Blended(font, message, textColor);

    if (surface)
    {
        SDL_Texture* texture =
            SDL_CreateTextureFromSurface(renderer, surface);

        if (texture)
        {
            SDL_Rect dst =
            {
                panel.x + 18,
                panel.y + 14,
                surface->w,
                surface->h
            };

            SDL_RenderCopy(renderer, texture, nullptr, &dst);
            SDL_DestroyTexture(texture);
        }

        SDL_FreeSurface(surface);
    }

    const char* hint = GetTutorialHintFromId(currentTextId_);
    
    if (hint && hint[0] != '\0')
    {
        SDL_Color hintColor = { 190, 220, 255, 255 };
        
        SDL_Surface* hintSurface = TTF_RenderUTF8_Blended(font, hint, hintColor);
        
        if (hintSurface)
        {
            SDL_Texture* hintTexture = SDL_CreateTextureFromSurface(renderer, hintSurface);
            if (hintTexture)
            {
                SDL_Rect hintDst = {panel.x + 18, panel.y + panel.h - 30, hintSurface->w, hintSurface->h};
                SDL_RenderCopy(renderer, hintTexture, nullptr, &hintDst);
                SDL_DestroyTexture(hintTexture);
            }
            SDL_FreeSurface(hintSurface);
        }
    }
}