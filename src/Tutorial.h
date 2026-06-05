#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <string>
#include <vector>

#include "Stage.h"
#include "Wall.h"
#include "Player.h"
#include "Math.h"

enum class TutorialPhase
{
    None,
    MovementWalk,
    MovementSneak,
    MovementRun,
    MovementDone,

    WireTraining,
    WireDone,

    BottlePickup,
    BottleThrow,
    BottleLure,
    BottleDone,

    GunApproach,
    GunFirstShotPaused,
    SuppressorAutoPickup,
    SuppressorPickupPaused,
    GunSuppressedShotPaused,
    GunKeyPickup,
    GunDoorApproach,
    GunDoorIntro,
    GunDone,
    
    CabinetWireTraining,
    BodyDragTraining,
    BodyHideTraining,
    CabinetKeyPickup,
    CodebookDoorIntro,
    CodebookPickup,
    EscapeApproach,
    TutorialComplete
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
    bool ShouldForceSneak() const;
    bool IsWireTrainingActive() const;
    void NotifyWireTakedown(bool wasWireTakedownSuccessful);
    bool IsPistolUnlocked() const;
    void UnlockPistol();
    bool IsBottlePickupActive() const;
    bool IsBottleThrowTrainingActive() const;
    bool IsBottleLureActive() const;
    bool CanThrowBottleAt(Vec2 targetWorld) const;
    bool GetBottleThrowZone(SDL_Rect& outRect) const;
    void NotifyBottlePickedUp(bool wasBottlePickedUp);
    void NotifyBottleThrown(bool wasBottleThrown);
    bool NotifyBottleBreakSound(Vec2 noisePos);
    bool NotifyBottleGuardArrived();
    bool IsGunApproachActive() const;
    bool IsGunFirstShotPaused() const;
    bool IsSuppressorPickupPaused() const;
    bool IsSuppressorAutoPickupActive() const;
    bool IsGunSuppressedShotPaused() const;
    bool IsGunKeyPickupActive() const;
    bool IsGunDoorIntroActive() const;
    bool IsTutorialFreezeActive() const;
    
    void NotifyGunSightReached();
    void NotifyGunGuardKilled(bool killed);
    void NotifySuppressorPickedUp(bool picked);
    void NotifySuppressorGuardKilled(bool killed);
    void NotifyKeyPickedUp(bool picked);
    void NotifyLockedDoorOpened(bool opened);
    bool IsCabinetWireTrainingActive() const;
    bool IsBodyHideTrainingActive() const;
    bool IsEscapeApproachActive() const;
    bool IsTutorialComplete() const;

    void NotifyBodyDragStarted(bool started);
    void NotifyBodyHidden(bool hidden);
    void NotifyCodebookPickedUp(bool picked);
    void NotifyEscapeStarted();
    void NotifyGoalReached();

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
    bool pistolUnlocked_ = false;
    bool escapeStarted_ = false;

private:
    void TryStartMovementTraining(const SDL_Rect& player);
    void TryStartWireTraining(const SDL_Rect& player);
    void TryStartBottlePickup(const SDL_Rect& player);
    void TryStartGunApproach(const SDL_Rect& player);
    void TryStartLockedDoorIntro(const SDL_Rect& player);

    void BeginMovementTraining(const StageTutorialTriggerDef& trigger);
    void BeginWireTraining(const StageTutorialTriggerDef& trigger);
    void BeginBottlePickup(const StageTutorialTriggerDef& trigger);
    void BeginBottleThrow();
    void BeginBottleLure();
    void CompleteBottleTraining();
    void BeginGunApproach(const StageTutorialTriggerDef& trigger);
    void BeginLockedDoorIntro(const StageTutorialTriggerDef& trigger);
    void CompleteGunTraining();
    void SetBlockerActiveById(const std::string& id, bool active);

    void UpdateMovementTraining(
        const SDL_Rect& player,
        MoveMode moveMode,
        bool playerMoving);

    void AdvanceToSneak();
    void AdvanceToRun();
    void CompleteMovementTraining();
    void CompleteWireTraining();

    void UnlockBlockersByCondition(const std::string& condition);

    const StageTutorialTriggerDef* FindTriggerByPhase(
        const std::string& phase) const;

    const StageTutorialBlockerDef* FindBlockerById(
        const std::string& id) const;
        
    static bool HasPlayerCompletelyPassedBlockerFromLeft(
        const SDL_Rect& player,
        const StageTutorialBlockerDef& blocker,
        int marginPixels = 4);

    float GetRequiredDistanceForCurrentPhase() const;
    float GetProgress01() const;

    static SDL_Point GetRectCenterPoint(const SDL_Rect& rect);
    static bool RectIntersects(const SDL_Rect& a, const SDL_Rect& b);
    static bool PointInRect(Vec2 point, const SDL_Rect& rect);
};