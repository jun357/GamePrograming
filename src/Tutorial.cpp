#include "Tutorial.h"

#include <cmath>
#include <cstdio>
#include <iostream>

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

    if (textId == "tutorial.wire")
    {
        return "Approach the guard from behind and press E to use the Wire.";
    }

    if (textId == "tutorial.wire_done")
    {
        return "Continue forward.";
    }

    if (textId == "tutorial.bottle.pickup")
    {
        return "Press E to pick up the bottle.";
    }

    if (textId == "tutorial.bottle.throw")
    {
        return "Throw the bottle into the highlighted area with Right Click.";
    }

    if (textId == "tutorial.bottle.wait")
    {
        return "";
    }

    if (textId == "tutorial.bottle.guard_moving")
    {
        return "The guard heard the noise and is moving.";
    }

    if (textId == "tutorial.bottle.done")
    {
        return "The guard is distracted. Move forward.";
    }

    if (textId == "tutorial.gun.approach")
    {
        return "";
    }
    
    if (textId == "tutorial.gun.first_shot")
    {
        return "Left Click the guard to fire the pistol.";
    }
    
    if (textId == "tutorial.suppressor.pickup")
    {
        return "Press E to pick up the suppressor.";
    }

    if (textId == "tutorial.suppressor.auto_pickup")
    {
        return "";
    }
    
    if (textId == "tutorial.gun.suppressed_shot")
    {
        return "Left Click the next guard.";
    }
    
    if (textId == "tutorial.key.pickup")
    {
        return "The guard dropped a key. Pick it up with E.";
    }
    
    if (textId == "tutorial.door.approach")
    {
        return "Move forward to the locked door.";
    }
    
    if (textId == "tutorial.door.open")
    {
        return "Press E near the locked door to open it.";
    }
    
    if (textId == "tutorial.gun.done")
    {
        return "Door opened. Continue.";
    }

    if (textId == "tutorial.cabinet.wire")
    {
        return "Use the Wire on the guard from behind.";
    }
    
    if (textId == "tutorial.body.drag")
    {
        return "Press F near the body to drag it.";
    }
    
    if (textId == "tutorial.body.hide")
    {
        return "Drag the body to the cabinet and press E to hide it.";
    }
    
    if (textId == "tutorial.cabinet.key")
    {
        return "Body hidden. Pick up the key on the left.";
    }
    
    if (textId == "tutorial.codebook.door")
    {
        return "Use the key to open the locked door on the right.";
    }
    
    if (textId == "tutorial.codebook.pickup")
    {
        return "Pick up the Codebook.";
    }
    
    if (textId == "tutorial.escape")
    {
        return "Codebook secured. Escape to the right.";
    }
    
    if (textId == "tutorial.complete")
    {
        return "Tutorial Complete.";
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

    if (textId == "tutorial.wire")
    {
        return "The Wire works only from behind, at close range.";
    }
    
    if (textId == "tutorial.wire_done")
    {
        return "";
    }

    if (textId == "tutorial.bottle.pickup")
    {
        return "Items are picked up with the same interaction key.";
    }
    if (textId == "tutorial.bottle.throw")
    {
        return "";
    }
    if (textId == "tutorial.bottle.wait")
    {
        return "";
    }
    if (textId == "tutorial.bottle.guard_moving")
    {
        return "";
    }

    if (textId == "tutorial.bottle.done")
    {
        return "";
    }

    if (textId == "tutorial.gun.approach")
    {
        return "";
    }
    
    if (textId == "tutorial.gun.first_shot")
    {
        return "";
    }
    
    if (textId == "tutorial.suppressor.pickup")
    {
        return "";
    }

    if (textId == "tutorial.suppressor.auto_pickup")
    {
        return "";
    }
    
    if (textId == "tutorial.gun.suppressed_shot")
    {
        return "Suppressed shots are quieter, but not completely silent.";
    }
    
    if (textId == "tutorial.key.pickup")
    {
        return "";
    }
    
    if (textId == "tutorial.door.approach")
    {
        return "";
    }
    
    if (textId == "tutorial.door.open")
    {
        return "Locked doors consume one key.";
    }
    
    if (textId == "tutorial.gun.done")
    {
        return "";
    }

    if (textId == "tutorial.body.drag")
    {
        return "Only Wire takedown bodies can be dragged.";
    }
    
    if (textId == "tutorial.body.hide")
    {
        return "Hidden bodies will not trigger an alarm.";
    }
    
    if (textId == "tutorial.codebook.door")
    {
        return "";
    }
    
    if (textId == "tutorial.escape")
    {
        return "Reach the goal at the far right.";
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
    pistolUnlocked_ = false;
    escapeStarted_ = false;
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

    if (phase_ == TutorialPhase::MovementDone)
    {
        TryStartWireTraining(player);
        return;
    }

    if (phase_ == TutorialPhase::WireDone)
    {
        TryStartBottlePickup(player);
        return;
    }

    if (phase_ == TutorialPhase::BottleDone)
    {
        TryStartGunApproach(player);
        return;
    }

    if (phase_ == TutorialPhase::GunDoorApproach)
    {
        TryStartLockedDoorIntro(player);
        return;
    }

    if (phase_ == TutorialPhase::WireTraining ||
        phase_ == TutorialPhase::BottlePickup ||
        phase_ == TutorialPhase::BottleThrow ||
        phase_ == TutorialPhase::BottleLure ||
        phase_ == TutorialPhase::GunApproach ||
        phase_ == TutorialPhase::GunFirstShotPaused ||
        phase_ == TutorialPhase::SuppressorPickupPaused ||
        phase_ == TutorialPhase::GunSuppressedShotPaused ||
        phase_ == TutorialPhase::GunKeyPickup ||
        phase_ == TutorialPhase::GunDoorIntro ||
        phase_ == TutorialPhase::GunDone)
    {
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

void TutorialController::TryStartWireTraining(const SDL_Rect& player)
{
    for (const auto& trigger : triggers_)
    {
        if (trigger.phase != "wire_training")
        {
            continue;
        }

        if (!RectIntersects(player, trigger.rect))
        {
            continue;
        }

        BeginWireTraining(trigger);
        return;
    }
}

void TutorialController::BeginWireTraining(
    const StageTutorialTriggerDef& trigger)
{
    phase_ = TutorialPhase::WireTraining;
    currentTextId_ = trigger.textId.empty()
        ? "tutorial.wire"
        : trigger.textId;

    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;
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

const StageTutorialTriggerDef* TutorialController::FindTriggerByPhase(
    const std::string& phase) const
{
    for (const auto& trigger : triggers_)
    {
        if (trigger.phase == phase)
        {
            return &trigger;
        }
    }

    return nullptr;
}

const StageTutorialBlockerDef* TutorialController::FindBlockerById(
    const std::string& id) const
{
    for (const auto& blocker : blockers_)
    {
        if (blocker.id == id)
        {
            return &blocker;
        }
    }

    return nullptr;
}

bool TutorialController::HasPlayerCompletelyPassedBlockerFromLeft(
    const SDL_Rect& player,
    const StageTutorialBlockerDef& blocker,
    int marginPixels)
{
    const int blockerRight =
        blocker.rect.x + blocker.rect.w;

    return player.x >= blockerRight + marginPixels;
}

bool TutorialController::PointInRect(Vec2 point, const SDL_Rect& rect)
{
    return point.x >= static_cast<float>(rect.x) &&
           point.x <= static_cast<float>(rect.x + rect.w) &&
           point.y >= static_cast<float>(rect.y) &&
           point.y <= static_cast<float>(rect.y + rect.h);
}

void TutorialController::TryStartBottlePickup(const SDL_Rect& player)
{
    for (const auto& trigger : triggers_)
    {
        if (trigger.phase != "bottle_pickup")
        {
            continue;
        }

        if (!RectIntersects(player, trigger.rect))
        {
            continue;
        }

        BeginBottlePickup(trigger);
        return;
    }
}

void TutorialController::BeginBottlePickup(
    const StageTutorialTriggerDef& trigger)
{
    phase_ = TutorialPhase::BottlePickup;
    currentTextId_ = trigger.textId.empty()
        ? "tutorial.bottle.pickup"
        : trigger.textId;

    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;
}

void TutorialController::BeginBottleThrow()
{
    phase_ = TutorialPhase::BottleThrow;
    currentTextId_ = "tutorial.bottle.throw";
    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;
}

void TutorialController::BeginBottleLure()
{
    phase_ = TutorialPhase::BottleLure;
    currentTextId_ = "tutorial.bottle.wait";
    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;
}

void TutorialController::TryStartGunApproach(const SDL_Rect& player)
{
    for (const auto& trigger : triggers_)
    {
        if (trigger.phase != "gun_encounter_start" &&
            trigger.phase != "gun_training")
        {
            continue;
        }

        if (!RectIntersects(player, trigger.rect))
        {
            continue;
        }

        const StageTutorialBlockerDef* gunBlocker =
            FindBlockerById("block_gun");

        if (gunBlocker &&
            !HasPlayerCompletelyPassedBlockerFromLeft(
                player,
                *gunBlocker,
                4))
        {
            return;
        }

        BeginGunApproach(trigger);
        return;
    }
}

void TutorialController::BeginGunApproach(
    const StageTutorialTriggerDef& trigger)
{
    (void)trigger;

    phase_ = TutorialPhase::GunApproach;
    currentTextId_ = "tutorial.gun.approach";

    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;

    SetBlockerActiveById("block_gun", true);
}

void TutorialController::TryStartLockedDoorIntro(const SDL_Rect& player)
{
    for (const auto& trigger : triggers_)
    {
        if (trigger.phase != "locked_door_intro")
        {
            continue;
        }

        if (!RectIntersects(player, trigger.rect))
        {
            continue;
        }

        BeginLockedDoorIntro(trigger);
        return;
    }
}

void TutorialController::BeginLockedDoorIntro(
    const StageTutorialTriggerDef& trigger)
{
    (void)trigger;

    phase_ = TutorialPhase::GunDoorIntro;
    currentTextId_ = "tutorial.door.open";

    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;
}

void TutorialController::CompleteGunTraining()
{
    phase_ = TutorialPhase::GunDone;
    currentTextId_ = "tutorial.gun.done";
    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;
}

void TutorialController::SetBlockerActiveById(
    const std::string& id,
    bool active)
{
    const size_t count = std::min(
        blockers_.size(),
        blockerActive_.size());

    int changedCount = 0;

    for (size_t i = 0; i < count; ++i)
    {
        if (blockers_[i].id == id)
        {
            blockerActive_[i] = active;
            ++changedCount;
        }
    }

    std::cout
        << "SetBlockerActiveById("
        << id
        << "): "
        << (active ? "active" : "inactive")
        << ", changed "
        << changedCount
        << " blocker(s)."
        << std::endl;
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

void TutorialController::CompleteWireTraining()
{
    phase_ = TutorialPhase::WireDone;
    currentTextId_ = "tutorial.wire_done";
    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;

    UnlockBlockersByCondition("wire_done");
}

void TutorialController::NotifyWireTakedown(bool wasWireTakedownSuccessful)
{
    if (!wasWireTakedownSuccessful)
    {
        return;
    }

    if (phase_ == TutorialPhase::WireTraining)
    {
        CompleteWireTraining();
        return;
    }

    if (phase_ == TutorialPhase::CabinetWireTraining)
    {
        phase_ = TutorialPhase::BodyDragTraining;
        currentTextId_ = "tutorial.body.drag";
        currentDistance_ = 0.0f;
        hasPreviousPlayerCenter_ = false;
        SetBlockerActiveById("block_cabinet", false);
        return;
    }
}

bool TutorialController::ShouldForceSneak() const
{
    return phase_ == TutorialPhase::WireTraining ||
           phase_ == TutorialPhase::CabinetWireTraining;
}

bool TutorialController::IsWireTrainingActive() const
{
    return phase_ == TutorialPhase::WireTraining;
}

bool TutorialController::IsPistolUnlocked() const
{
    if (pistolUnlocked_)
    {
        return true;
    }
    return phase_ == TutorialPhase::GunFirstShotPaused ||
           phase_ == TutorialPhase::GunSuppressedShotPaused ||
           phase_ == TutorialPhase::GunDone;
}

void TutorialController::UnlockPistol()
{
    pistolUnlocked_ = true;
}

bool TutorialController::IsBottlePickupActive() const
{
    return phase_ == TutorialPhase::BottlePickup;
}

bool TutorialController::IsBottleThrowTrainingActive() const
{
    return phase_ == TutorialPhase::BottleThrow;
}

bool TutorialController::IsBottleLureActive() const
{
    return phase_ == TutorialPhase::BottleLure;
}

bool TutorialController::GetBottleThrowZone(SDL_Rect& outRect) const
{
    if (phase_ != TutorialPhase::BottleThrow)
    {
        return false;
    }

    const StageTutorialTriggerDef* trigger =
        FindTriggerByPhase("bottle_throw_zone");

    if (!trigger)
    {
        return false;
    }

    outRect = trigger->rect;
    return true;
}

bool TutorialController::CanThrowBottleAt(Vec2 targetWorld) const
{
    if (phase_ != TutorialPhase::BottleThrow)
    {
        return false;
    }

    SDL_Rect zone;
    if (!GetBottleThrowZone(zone))
    {
        return false;
    }

    return PointInRect(targetWorld, zone);
}

void TutorialController::NotifyBottlePickedUp(bool wasBottlePickedUp)
{
    if (!wasBottlePickedUp)
    {
        return;
    }

    if (phase_ != TutorialPhase::BottlePickup)
    {
        return;
    }

    BeginBottleThrow();
}

void TutorialController::NotifyBottleThrown(bool wasBottleThrown)
{
    if (!wasBottleThrown)
    {
        return;
    }

    if (phase_ != TutorialPhase::BottleThrow)
    {
        return;
    }

    BeginBottleLure();
}

bool TutorialController::NotifyBottleBreakSound(Vec2 noisePos)
{
    (void)noisePos;

    if (phase_ != TutorialPhase::BottleLure)
    {
        return false;
    }

    currentTextId_ = "tutorial.bottle.guard_moving";
    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;

    return true;
}

void TutorialController::CompleteBottleTraining()
{
    phase_ = TutorialPhase::BottleDone;
    currentTextId_ = "tutorial.bottle.done";
    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;

    UnlockBlockersByCondition("bottle_done");
}

bool TutorialController::NotifyBottleGuardArrived()
{
    if (phase_ != TutorialPhase::BottleLure)
    {
        return false;
    }

    CompleteBottleTraining();
    return true;
}

bool TutorialController::IsGunApproachActive() const
{
    return phase_ == TutorialPhase::GunApproach;
}

bool TutorialController::IsGunFirstShotPaused() const
{
    return phase_ == TutorialPhase::GunFirstShotPaused;
}

bool TutorialController::IsSuppressorPickupPaused() const
{
    return phase_ == TutorialPhase::SuppressorPickupPaused;
}

bool TutorialController::IsSuppressorAutoPickupActive() const
{
    return phase_ == TutorialPhase::SuppressorAutoPickup;
}

bool TutorialController::IsGunSuppressedShotPaused() const
{
    return phase_ == TutorialPhase::GunSuppressedShotPaused;
}

bool TutorialController::IsGunKeyPickupActive() const
{
    return phase_ == TutorialPhase::GunKeyPickup;
}

bool TutorialController::IsGunDoorIntroActive() const
{
    return phase_ == TutorialPhase::GunDoorIntro;
}

bool TutorialController::IsTutorialFreezeActive() const
{
    return phase_ == TutorialPhase::GunFirstShotPaused ||
           phase_ == TutorialPhase::SuppressorPickupPaused ||
           phase_ == TutorialPhase::GunSuppressedShotPaused;
}

void TutorialController::NotifyGunSightReached()
{
    if (phase_ != TutorialPhase::GunApproach)
    {
        return;
    }

    phase_ = TutorialPhase::GunFirstShotPaused;
    currentTextId_ = "tutorial.gun.first_shot";
    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;
}

void TutorialController::NotifyGunGuardKilled(bool killed)
{
    if (!killed)
    {
        return;
    }

    if (phase_ != TutorialPhase::GunFirstShotPaused)
    {
        return;
    }

    phase_ = TutorialPhase::SuppressorAutoPickup;
    currentTextId_ = "tutorial.suppressor.auto_pickup";
    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;
}

void TutorialController::NotifySuppressorPickedUp(bool picked)
{
    if (!picked)
    {
        return;
    }

    if (phase_ != TutorialPhase::SuppressorPickupPaused &&
        phase_ != TutorialPhase::SuppressorAutoPickup)
    {
        return;
    }

    phase_ = TutorialPhase::GunSuppressedShotPaused;
    currentTextId_ = "tutorial.gun.suppressed_shot";
    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;
}

void TutorialController::NotifySuppressorGuardKilled(bool killed)
{
    if (!killed)
    {
        return;
    }

    if (phase_ != TutorialPhase::GunSuppressedShotPaused)
    {
        return;
    }

    phase_ = TutorialPhase::GunKeyPickup;
    currentTextId_ = "tutorial.key.pickup";
    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;
}

void TutorialController::NotifyKeyPickedUp(bool picked)
{
    if (!picked)
    {
        return;
    }

    if (phase_ == TutorialPhase::GunKeyPickup)
    {
        phase_ = TutorialPhase::GunDoorApproach;
        currentTextId_ = "tutorial.door.approach";
        currentDistance_ = 0.0f;
        hasPreviousPlayerCenter_ = false;
        return;
    }

    if (phase_ == TutorialPhase::CabinetKeyPickup)
    {
        phase_ = TutorialPhase::CodebookDoorIntro;
        currentTextId_ = "tutorial.codebook.door";
        currentDistance_ = 0.0f;
        hasPreviousPlayerCenter_ = false;
        return;
    }
}

void TutorialController::NotifyLockedDoorOpened(bool opened)
{
    if (!opened)
    {
        return;
    }

    if (phase_ == TutorialPhase::GunDoorIntro ||
        phase_ == TutorialPhase::GunDoorApproach)
    {
        phase_ = TutorialPhase::CabinetWireTraining;
        currentTextId_ = "tutorial.cabinet.wire";
        currentDistance_ = 0.0f;
        hasPreviousPlayerCenter_ = false;
        return;
    }

    if (phase_ == TutorialPhase::CodebookDoorIntro)
    {
        phase_ = TutorialPhase::CodebookPickup;
        currentTextId_ = "tutorial.codebook.pickup";
        currentDistance_ = 0.0f;
        hasPreviousPlayerCenter_ = false;
        return;
    }
}

bool TutorialController::IsCabinetWireTrainingActive() const
{
    return phase_ == TutorialPhase::CabinetWireTraining;
}

bool TutorialController::IsBodyHideTrainingActive() const
{
    return phase_ == TutorialPhase::BodyHideTraining;
}

bool TutorialController::IsEscapeApproachActive() const
{
    return phase_ == TutorialPhase::EscapeApproach;
}

bool TutorialController::IsTutorialComplete() const
{
    return phase_ == TutorialPhase::TutorialComplete;
}

void TutorialController::NotifyBodyDragStarted(bool started)
{
    if (!started)
    {
        return;
    }

    if (phase_ != TutorialPhase::BodyDragTraining)
    {
        return;
    }

    phase_ = TutorialPhase::BodyHideTraining;
    currentTextId_ = "tutorial.body.hide";
    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;
}

void TutorialController::NotifyBodyHidden(bool hidden)
{
    if (!hidden)
    {
        return;
    }

    if (phase_ != TutorialPhase::BodyHideTraining)
    {
        return;
    }

    phase_ = TutorialPhase::CabinetKeyPickup;
    currentTextId_ = "tutorial.cabinet.key";
    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;

    SetBlockerActiveById("block_key", false);
}

void TutorialController::NotifyCodebookPickedUp(bool picked)
{
    if (!picked)
    {
        return;
    }

    if (phase_ != TutorialPhase::CodebookPickup)
    {
        return;
    }

    phase_ = TutorialPhase::EscapeApproach;
    currentTextId_ = "tutorial.escape";
    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;
    escapeStarted_ = false;

    SetBlockerActiveById("block_codebook", false);
}

void TutorialController::NotifyEscapeStarted()
{
    if (phase_ != TutorialPhase::EscapeApproach)
    {
        return;
    }

    if (escapeStarted_)
    {
        return;
    }

    escapeStarted_ = true;

    currentTextId_ = "tutorial.escape";
    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;

    SetBlockerActiveById("block_codebook", true);
}

void TutorialController::NotifyGoalReached()
{
    if (phase_ != TutorialPhase::EscapeApproach)
    {
        return;
    }

    phase_ = TutorialPhase::TutorialComplete;
    currentTextId_ = "tutorial.complete";
    currentDistance_ = 0.0f;
    hasPreviousPlayerCenter_ = false;
}

void TutorialController::UnlockBlockersByCondition(const std::string& condition)
{
    int unlockedCount = 0;

    const size_t count = std::min(
        blockers_.size(),
        blockerActive_.size());

    for (size_t i = 0; i < count; ++i)
    {
        if (blockers_[i].unlockWhen == condition)
        {
            blockerActive_[i] = false;
            ++unlockedCount;
        }
    }

    std::cout << "UnlockBlockersByCondition("
              << condition
              << "): "
              << unlockedCount
              << " blocker(s) unlocked."
              << std::endl;
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