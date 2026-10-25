#include "game.h"
#include "game_combat_gauge.h"
#include "game_combat_input.h"
#include "menu_inventory.h"
#include "scene_task.h"
#include "character_records.h"
#include "combat_styles.h"
#include "versus_tables.h"
#include "xaf_runtime.h"
#include "move_tables.h"
#include "action_state_handlers.h"
#include "options_menu.h"
#include "camera.h"
#include "sound_cues.h"
#include "stage.h"
#include "hud_advice.h"
#include "../formats/MENU_LAYOUT/pause_results_layout.h"
#include "../formats/BATTLE_ROSTER/battle_rosters.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "game_internal.h"

using namespace game_detail;

namespace game_detail
{

int actorMotionIdFromAction(const GameSlotRecord& actor, int actionIndex)
{
    const CharacterRecord* const character =
        characterRecordForId(static_cast<uint16_t>(actor.characterId));
    const int characterIndex =
        character != NULL ? character->combatStyleId : -1;
    if(characterIndex < 0 || characterIndex >= MOVE_CHARACTER_COUNT)
        return 0;
    const uint8_t archetype =
        MOVE_CHARACTER_RECORDS[characterIndex].archetype;
    if(archetype >= MOVE_ARCHETYPE_COUNT)
        return 0;
    int index = actionIndex;
    if(index < 0)
        index = -index;
    if(index > MOVE_REMAP_LIMIT)
        index = 1;
    return MOVE_ARCHETYPE_REMAP[archetype][index];
}

int resolveQueuedMotion(const GameSlotRecord& actor, int rawMotionId)
{
    if(rawMotionId == 0)
        return 0;
    if(rawMotionId > 0)
        return moveRecord(rawMotionId) != NULL ? rawMotionId : 0;
    return actorMotionIdFromAction(actor, -rawMotionId);
}

}

void
Game::resetBehaviourPresentation(void)
{
    soundEventHead = 0;
    soundEventCount = 0;
    estEventHead = 0;
    estEventCount = 0;
    pendingDamageCameraSettle = 0.0f;
    pendingMoveCameraSettle = 0.0f;
    camdatTimeScale = 1.0f;
    pendingCamdatRecord = -1;
    pendingCamdatExclusive = false;
    resetCombatRuntime();
    pendingCamdatActorSlot = 0;
}

void
Game::resetCombatRuntime(void)
{

    resetCombatProjectileState();
    combatRepeatCount = 0;
    combatInputHoldSuccessCount = 0;
    combatRepeatDelaySeconds = 0.0f;
    previousCamdatVariant = 0;
    combatContactHead = 0;
    combatContactCount = 0;
    for(int i = 0; i < GAME_COMBAT_SLOT_COUNT; ++i)
    {
        behaviourRunners[i].stop();
        moveStreamRunners[i].stop();
        memset(&combatActionPairs[i], 0,
               sizeof(combatActionPairs[i]));
        behaviourMoveIds[i] = -1;
        behaviourActions[i] = COMBAT_ACTION_IDLE;
        behaviourContexts[i].owner = this;
        behaviourContexts[i].actor = NULL;
        behaviourContexts[i].footstepRotationCounter = 0;

        BehaviourStreamCallbacks behaviourCallbacks;
        behaviourCallbacks.setHitBlock = behaviourHitBlockCallback;
        behaviourCallbacks.armHitWindow = behaviourHitWindowCallback;
        behaviourCallbacks.applyHit = behaviourApplyHitCallback;
        behaviourCallbacks.queueMove = behaviourQueueMoveCallback;
        behaviourCallbacks.setAction = behaviourSetActionCallback;
        behaviourCallbacks.actionFlags = behaviourActionFlagsCallback;
        behaviourCallbacks.targetFlags = behaviourTargetFlagsCallback;
        behaviourCallbacks.cueFlags = behaviourCueFlagsCallback;
        behaviourCallbacks.behaviourFlags = behaviourSetFlagsCallback;
        behaviourCallbacks.impactState = behaviourImpactStateCallback;
        behaviourCallbacks.recoveryState = behaviourRecoveryStateCallback;
        behaviourCallbacks.movement = behaviourMovementCallback;
        behaviourCallbacks.contactMovement = behaviourContactMovementCallback;
        behaviourCallbacks.statusFlags = behaviourStatusFlagsCallback;
        behaviourCallbacks.turnDuration = behaviourTurnDurationCallback;
        behaviourCallbacks.spacingCorrection = behaviourSpacingCorrectionCallback;
        behaviourCallbacks.inputHold = behaviourInputHoldCallback;
        behaviourCallbacks.comboGate = moveComboGateCallback;
        behaviourCallbacks.testTargetFlags =
            behaviourTestTargetFlagsCallback;
        behaviourCallbacks.runAction = behaviourRunActionCallback;
        behaviourCallbacks.fireProjectile = behaviourFireProjectileCallback;
        behaviourCallbacks.weapon = behaviourWeaponCallback;
        behaviourCallbacks.effect = behaviourEffectCallback;
        behaviourCallbacks.sound = behaviourSoundCallback;
        behaviourCallbacks.cameraSettle = behaviourCameraSettleCallback;
        behaviourCallbacks.camera = behaviourCameraCallback;
        behaviourCallbacks.timeScale = moveTimeScaleCallback;
        behaviourCallbacks.userData = &behaviourContexts[i];
        behaviourRunners[i].setCallbacks(behaviourCallbacks);
        behaviourRunners[i].setEffectSuppressionMask(
            behaviourEffectSuppressionMask);

        MoveStreamCallbacks moveCallbacks;
        moveCallbacks.playMotion = movePlayMotionCallback;
        moveCallbacks.call = moveCallCallback;
        moveCallbacks.setAction = moveSetActionCallback;
        moveCallbacks.comboGate = moveComboGateCallback;
        moveCallbacks.comboArm = moveComboArmCallback;
        moveCallbacks.random = moveRandomCallback;
        moveCallbacks.timeScale = moveTimeScaleCallback;
        moveCallbacks.clearBehaviourWait = moveClearBehaviourWaitCallback;
        moveCallbacks.userData = &behaviourContexts[i];
        moveStreamRunners[i].setCallbacks(moveCallbacks);
    }
}

void
Game::enqueueCombatEstEvents(const GameSlotRecord& attacker,
                             const GameSlotRecord& victim,
                             const MoveHitBlock& block,
                             int16_t)
{
    const uint8_t bone = combatImpactBone(
        block.flagsB, victim.impactSelectorState,
        victim.impactSelectorStateValid);
    if(bone == 0)
        return;

    int16_t rawClassifierId = 0;
    if((block.flagsA & MOVE_HIT_GUARD_BREAK) != 0)
    {
        const WeaponInstance* held =
            weaponManager.equippedBy(attacker.slotIndex);
        rawClassifierId = held == NULL ?
            -1 : static_cast<int16_t>(held->objectId);
    }

    uint8_t records[5];
    const int count = combatImpactEffectRecords(
        rawClassifierId, records, 5);
    Vec3 direction;
    direction.x = victim.position.x - attacker.position.x;
    direction.y = victim.position.y - attacker.position.y;
    direction.z = victim.position.z - attacker.position.z;

    int16_t impactAngle = 0;
    switch(hitDirection(victim, attacker))
    {
    case MOVE_HIT_DIRECTION_FRONT:
        impactAngle = 90;
        break;
    case MOVE_HIT_DIRECTION_SIDE_A:
        impactAngle = 270;
        break;
    case MOVE_HIT_DIRECTION_SIDE_B:
        impactAngle = 180;
        break;
    default:
        break;
    }
    for(int i = 0; i < count; ++i)
    {
        GameEstEvent event = {};
        event.recordIndex = records[i];
        event.actorSlot = victim.slotIndex;
        event.boneIndex = bone;
        event.placement = i < 3 ?
            GAME_EST_DIRECTION_BASIS : GAME_EST_BONE_Y_ANGLE;
        event.angleBase = impactAngle;
        event.rawClassifierId = rawClassifierId;
        event.flagsB = block.flagsB;
        event.direction = direction;
        enqueueEstEvent(event);
    }
}

void
Game::enqueueSoundEvent(const GameSoundEvent& event)
{
    if(soundEventCount == GAME_SOUND_EVENT_CAPACITY)
    {
        soundEventHead = (soundEventHead + 1) % GAME_SOUND_EVENT_CAPACITY;
        --soundEventCount;
    }
    const int tail =
        (soundEventHead + soundEventCount) % GAME_SOUND_EVENT_CAPACITY;
    soundEvents[tail] = event;
    ++soundEventCount;
}

void
Game::enqueueEstEvent(const GameEstEvent& event)
{
    if(estEventCount == GAME_EST_EVENT_CAPACITY)
        return;
    const int tail =
        (estEventHead + estEventCount) % GAME_EST_EVENT_CAPACITY;
    estEvents[tail] = event;
    ++estEventCount;
}

void
Game::behaviourHitBlockCallback(void* userData, int blockIndex)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->actor == NULL ||
       blockIndex < 0 || blockIndex >= MOVE_HIT_BLOCK_COUNT)
        return;
    context->actor->behaviourDecisionMask |=
        COMBAT_BEHAVIOUR_HIT_BLOCK;
    context->actor->activeHitBlock = static_cast<uint8_t>(blockIndex);
    ++context->actor->hitGeneration;
}

void
Game::behaviourHitWindowCallback(void* userData, int16_t centreFrame,
                                 int16_t framesBefore,
                                 int16_t framesAfter,
                                 uint32_t channelMask)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->actor == NULL)
        return;
    context->actor->behaviourDecisionMask |=
        COMBAT_BEHAVIOUR_HIT_WINDOW;
    context->actor->hitWindowStartFrame = static_cast<int16_t>(
        static_cast<int>(centreFrame) - static_cast<int>(framesBefore));
    context->actor->hitWindowEndFrame = static_cast<int16_t>(
        static_cast<int>(centreFrame) + static_cast<int>(framesAfter));
    context->actor->hitChannelMask = channelMask;
    context->actor->hitWindowRadius =
        context->actor->contactRadiusScale * 2.0f;
}

void
Game::behaviourApplyHitCallback(void* userData, int)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->owner == NULL || context->actor == NULL)
        return;
    context->actor->behaviourDecisionMask |=
        COMBAT_BEHAVIOUR_APPLY_HIT;
    GameSlotRecord* const target = context->owner->mutableCombatSlotByIndex(
        context->actor->linkedSlotIndex);
    if(target != NULL)
        context->owner->directCombatHit(*context->actor, *target);
}

void
Game::behaviourQueueMoveCallback(void* userData, int moveId)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->owner == NULL || context->actor == NULL)
        return;
    context->actor->behaviourDecisionMask |=
        COMBAT_BEHAVIOUR_QUEUE_MOVE;

    context->owner->queueCombatPair(*context->actor, moveId, 0);
}

void
Game::queueCombatPair(GameSlotRecord& actor, int selfMove, int linkedMove)
{
    const uint8_t index = actor.slotIndex;
    if(index >= GAME_COMBAT_SLOT_COUNT)
        return;
    CombatActionPairRecord& pair =
        combatActionPairs[index];
    pair.inputPresentationValid = false;
    pair.selfMotionIndex = selfMove < 0 ? -selfMove : 0;
    pair.linkedMotionIndex = linkedMove < 0 ? -linkedMove : 0;
    pair.selfMotionId = static_cast<int16_t>(
        resolveQueuedMotion(actor, selfMove));
    const GameSlotRecord* linked = combatSlotByIndex(actor.linkedSlotIndex);
    pair.linkedMotionId = linked != NULL && linkedMove != 0 ?
        static_cast<int16_t>(resolveQueuedMotion(*linked, linkedMove)) : 0;

}

void
Game::behaviourSetActionCallback(void* userData, int moveId)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->owner == NULL || context->actor == NULL)
        return;
    context->actor->behaviourDecisionMask |=
        COMBAT_BEHAVIOUR_SET_ACTION;
    GameSlotRecord* const target = context->owner->mutableCombatSlotByIndex(
        context->actor->linkedSlotIndex);
    if(target != NULL && moveRecord(moveId) != NULL)
        context->owner->startCombatMove(*target, moveId, target->action);
}

void
Game::behaviourActionFlagsCallback(void* userData, uint32_t mask, bool set)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->actor == NULL)
        return;
    context->actor->behaviourDecisionMask |=
        COMBAT_BEHAVIOUR_ACTION_FLAGS;
    if(set)
        context->actor->actorActionFlags |= mask;
    else
        context->actor->actorActionFlags &= ~mask;
}

void
Game::behaviourCueFlagsCallback(void* userData, uint32_t mask, bool set)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->actor == NULL)
        return;
    if(set)
        context->actor->animationCueFlags |= mask;
    else
        context->actor->animationCueFlags &= ~mask;
}

void
Game::behaviourTargetFlagsCallback(void* userData, uint32_t mask, bool set)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->actor == NULL)
        return;
    context->actor->behaviourDecisionMask |=
        COMBAT_BEHAVIOUR_TARGET_FLAGS;
    if(set)
        context->actor->actorTargetingFlags |= mask;
    else
        context->actor->actorTargetingFlags &= ~mask;
}

void
Game::behaviourImpactStateCallback(void* userData, uint32_t value)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->actor == NULL)
        return;

    context->actor->impactSelectorState = static_cast<uint8_t>(value);
    context->actor->impactSelectorStateValid = true;
}

void Game::behaviourRecoveryStateCallback(void* userData, uint32_t value)
{
    BehaviourCombatContext* context = static_cast<BehaviourCombatContext*>(userData);
    if(context != NULL && context->actor != NULL)
        context->actor->combatRecoveryState = static_cast<uint8_t>(value);
}

void
Game::behaviourSetFlagsCallback(void* userData, uint32_t value)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context != NULL && context->actor != NULL)
    {
        context->actor->behaviourDecisionMask |=
            COMBAT_BEHAVIOUR_FLAGS;
        context->actor->actorBehaviorFlags = value;
    }
}

bool
Game::behaviourTestTargetFlagsCallback(void* userData, uint32_t mask,
                                       bool expectSet)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->actor == NULL)
        return false;
    context->actor->behaviourDecisionMask |=
        COMBAT_BEHAVIOUR_TEST_TARGET;
    const bool set =
        (context->actor->actorTargetingFlags & mask) == mask;
    return set == expectSet;
}

void
Game::behaviourRunActionCallback(void* userData)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->owner == NULL || context->actor == NULL ||
       context->actor->queuedMoveId < 0)
        return;
    context->actor->behaviourDecisionMask |=
        COMBAT_BEHAVIOUR_RUN_ACTION;
    const int moveId = context->actor->queuedMoveId;
    context->actor->queuedMoveId = -1;
    context->owner->startCombatMove(
        *context->actor, moveId, context->actor->action);
}

void
Game::behaviourEffectCallback(void* userData, int16_t recordIndex,
                              int16_t boneIndex, int16_t angleBase,
                              int16_t tuningA, int16_t tuningB)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->owner == NULL || context->actor == NULL ||
       recordIndex < 0 || recordIndex > 8)
        return;
    GameEstEvent event = {};
    event.recordIndex = static_cast<uint8_t>(recordIndex);
    event.actorSlot = context->actor->slotIndex;
    event.boneIndex = static_cast<uint8_t>(boneIndex);
    event.placement = GAME_EST_BONE_Y_ANGLE;
    event.angleBase = angleBase;
    event.addAnimationFrame = true;
    event.tuningA = tuningA;
    event.tuningB = tuningB;
    event.rawClassifierId = -1;
    context->owner->enqueueEstEvent(event);
}

void
Game::behaviourSoundCallback(void* userData, int kind,
                             int first, int second)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->owner == NULL || context->actor == NULL)
        return;

    GameSoundEvent event = {};
    event.kind = GAME_SOUND_EVENT_SPATIAL;
    event.position = context->actor->position;
    if(kind == 0)
    {

        if(first < 0 || first >= SOUND_BANK_SLOT_COUNT ||
           second < 0 || second > 0xff)
            return;
        event.bank = static_cast<uint8_t>(first);
        event.cue = static_cast<uint8_t>(second);
        context->owner->enqueueSoundEvent(event);
        return;
    }
    if(kind == 5)
    {

        if(first < -1 || first > 4)
            return;
        event.bank = 1;
        event.cue = GAME_COMBAT_CUES[first + 1];
        context->owner->enqueueSoundEvent(event);
        return;
    }
    if(kind != 6 && kind != 7)
        return;

    event.kind = GAME_SOUND_EVENT_FOOTSTEP;
    event.surface = 0;
    event.actorFlags = context->actor->flags;
    event.alternate = kind == 7;
    event.footstepRotationCounter = context->footstepRotationCounter;
    context->footstepRotationCounter =
        soundCueAdvance(context->footstepRotationCounter);
    context->owner->enqueueSoundEvent(event);
}

void
Game::behaviourCameraSettleCallback(void* userData,
                                    int closeUpDurationFrames)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->owner == NULL)
        return;

    context->owner->pendingMoveCameraSettle =
        static_cast<float>(closeUpDurationFrames);
}

void
Game::behaviourCameraCallback(void* userData,
                              int highDescriptor,
                              int middleDescriptor,
                              int lowDescriptor,
                              bool exclusive)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->owner == NULL || context->actor == NULL)
        return;

    int descriptor = highDescriptor;
    if(middleDescriptor >= 0)
    {
        const GameSlotRecord& actor = *context->actor;
        if(actor.maxHealth > 0)
        {
            if(actor.currentHealth * 10 <= actor.maxHealth * 4)
                descriptor = lowDescriptor;
            else if(actor.currentHealth * 10 <= actor.maxHealth * 8)
                descriptor = middleDescriptor;
        }
    }
    if(descriptor < 0 || descriptor >= CAMDAT_DESCRIPTOR_COUNT)
        return;

    Game& game = *context->owner;
    game.pendingCamdatRecord = selectCamdatRecordIndexWithRandom(
        static_cast<uint16_t>(descriptor),
        moveRandomCallback, userData, &game.previousCamdatVariant);
    game.pendingCamdatExclusive = exclusive;
    game.pendingCamdatActorSlot = context->actor->slotIndex;
}

void
Game::behaviourInputHoldCallback(void* userData, int opcode, int16_t value)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context != NULL && context->owner != NULL && context->actor != NULL)
        context->owner->configureCombatInputHold(*context->actor, opcode, value);
}

void
Game::movePlayMotionCallback(void* userData, int motionId,
                             int blendFrames, int maxDurationFrames,
                             int transitionFrames)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->actor == NULL)
        return;
    context->actor->scheduledMotion.motionId =
        static_cast<int16_t>(motionId);
    context->actor->scheduledMotion.blendFrames =
        static_cast<int16_t>(blendFrames);
    context->actor->scheduledMotion.maxDurationFrames =
        static_cast<int16_t>(maxDurationFrames);
    context->actor->scheduledMotion.transitionFrames =
        static_cast<int16_t>(transitionFrames);
    context->actor->scheduledMotionValid = true;
    context->actor->scheduledMotionGeneration =
        ++context->owner->combatMotionGenerationCounter;

    context->actor->playingAnimationFrame = 0.0f;

    context->actor->scheduledMotionComplete = false;
    context->actor->flags &= ~8u;
    context->actor->combatPosePublished = false;
}

int
Game::moveCallCallback(void* userData, int argument)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->owner == NULL || context->actor == NULL)
        return 0;
    Game& game = *context->owner;
    GameSlotRecord& actor = *context->actor;
    GameSlotRecord* const partner =
        game.mutableCombatSlotByIndex(actor.linkedSlotIndex);

    if(argument == MOVE_STREAM_CALL_QUEUED)
    {

        const bool pending = actor.queuedMoveId > 0;
        const int moveId = pending ?
            actor.queuedMoveId : MOVE_RECORD_DEFAULT_ACTION;
        actor.queuedMoveId = -1;
        game.startCombatMove(
            actor, moveId, pending ? actor.action : COMBAT_ACTION_IDLE);
        game.runCombatStateTask(actor);
        return 1;
    }

    if(argument == MOVE_STREAM_CALL_SELF_THROW ||
       argument == MOVE_STREAM_CALL_PARTNER_THROW)
    {
        if(partner == NULL)
            return 0;
        const bool self = argument == MOVE_STREAM_CALL_SELF_THROW;
        bool grounded;
        if(!combatGroundedState(self ? *partner : actor, grounded))
            return 0;
        const GameSlotRecord& source = self ? actor : *partner;
        const uint8_t combo = self || actor.comboStep != 0 ?
            actor.comboStep : partner->comboStep;
        const int action = moveThrowBaseAction(grounded, (combo & 1u) != 0);
        const int personal = movePersonalActionForStyle(
            game.combatStyle(source), action,
            game.combatPersonalActionContext(source));
        int moveId;
        if(self)
        {
            moveId = personal > 0 ? personal :
                game.stanceMotion(actor, moveForAction(action));
        }
        else if(personal > 0)
            moveId = actionStateLinkedMoveId(personal);
        else
        {
            const int mapped = game.stanceMotion(*partner, moveForAction(action));
            if(moveRecord(mapped) == NULL)
                return 0;
            const int linked = actionStateLinkedMoveId(mapped);
            moveId = resolveCharacterMove(game.combatStyle(actor), -linked);
        }
        if(moveId == 0)
            moveId = MOVE_RECORD_DEFAULT_ACTION;
        if(moveRecord(moveId) == NULL)
            return 0;

        game.startCombatMove(actor, moveId, actor.action);
        if(self)
        {
            partner->comboStep = actor.comboStep;
            ++actor.comboStep;
        }
        game.runCombatStateTask(actor);
        return 1;
    }

    int moveId = argument == 0 ? MOVE_RECORD_DEFAULT_ACTION : argument;
    bool recoveryPose = false;
    if(argument == MOVE_STREAM_CALL_NEUTRAL && (actor.poseStateWord & 1u) == 0)
    {
        actor.actorActionFlags &= 0xfffe4fffu;
        actor.actorActionFlags &= 0xffffff07u;
        actor.actorTargetingFlags &= 0xff5fffefu;
        actor.comboStep = 0;
        actor.receivedDamage = 0;
        actor.receivedPrideDamage = 0;
        actor.lastHitAttackerSlot = -1;
        if((actor.actorActionFlags & 0x4000u) == 0 && actor.combatRecoveryState != 3)
        {
            if((actor.currentHealth == actor.maxHealth && actor.damageGauge == 0) ||
               ((actor.actorTargetingFlags & 0x40000u) == 0 &&
                (partner == NULL || (partner->actorTargetingFlags & 0x40000u) == 0) &&
                (actor.actorTargetingFlags & 0x1000u) == 0))
                moveId = game.stanceMotion(actor, argument);
            else if((actor.actorTargetingFlags & 0x40000u) == 0 &&
                    (partner == NULL || (partner->actorTargetingFlags & 0x40000u) == 0) &&
                    actor.selectedActionId != game.stanceMotion(actor, 0x20) &&
                    actor.combatRecoveryState == 1)
            {
                moveId = game.stanceMotion(actor, 0x20);
                recoveryPose = true;
            }
        }
    }
    if(moveRecord(moveId) == NULL)
        return 0;
    game.startCombatMove(actor, moveId,
        argument == MOVE_STREAM_CALL_NEUTRAL || argument == 0 ?
            COMBAT_ACTION_IDLE : actor.action);
    if(recoveryPose)
        actor.poseStateWord |= 1u;
    game.runCombatStateTask(actor);
    return 1;
}

void
Game::moveSetActionCallback(void* userData, int moveId)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->owner == NULL || context->actor == NULL ||
       moveRecord(moveId) == NULL)
        return;
    context->owner->startCombatMove(
        *context->actor, moveId, context->actor->action);
}

bool
Game::moveComboGateCallback(void* userData, int argument)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    return context != NULL && context->owner != NULL &&
        context->actor != NULL && context->owner->consumeCombatInputChain(
            *context->actor, argument);
}

void
Game::moveComboArmCallback(void* userData)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context != NULL && context->owner != NULL && context->actor != NULL)
        context->owner->configureCombatInputHold(
            *context->actor, MOVE_BEHAVIOUR_ARM_HOLD_COMBO, 0);
}

void
Game::moveClearBehaviourWaitCallback(void* userData)
{
    BehaviourCombatContext* context = static_cast<BehaviourCombatContext*>(userData);
    if(context != NULL && context->owner != NULL && context->actor != NULL)
        context->owner->behaviourRunners[context->actor->slotIndex].clearWait();
}

uint32_t
Game::moveRandomCallback(void* userData)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    return context != NULL && context->owner != NULL ?
        context->owner->nextRandom() : 0;
}

void Game::moveTimeScaleCallback(void* userData, float rate)
{
    BehaviourCombatContext* context = static_cast<BehaviourCombatContext*>(userData);
    if(context != NULL && context->actor != NULL)
        context->actor->animationTimeScale = rate;
}

void Game::setCombatActorMotion(GameSlotRecord& slot, int moveId)
{

    if(moveRecord(moveId) == NULL || slot.slotIndex >= GAME_COMBAT_SLOT_COUNT)
        return;
    slot.flags = (slot.flags & ~0x800u) | 0xcu;
    slot.appliedMotionId = static_cast<int16_t>(moveId);
    slot.recoveredMoveId = static_cast<int16_t>(moveId);
    slot.animationTimeScale = 1.0f;
    slot.animationCueFlags = 0;
    slot.scheduledMotionValid = false;
    slot.scheduledMotionComplete = false;
    behaviourContexts[slot.slotIndex].actor = &slot;
    moveStreamRunners[slot.slotIndex].start(moveId);
    behaviourRunners[slot.slotIndex].start(moveId);
}

void
Game::updateCombatMoveRuntime(GameSlotRecord& slot)
{
    const int index = slot.slotIndex;
    if(index < 0 || index >= GAME_COMBAT_SLOT_COUNT)
        return;

    if((slot.flags & 0x1u) == 0)
    {
        moveStreamRunners[index].stop();
        behaviourRunners[index].stop();
        return;
    }
    updateCombatGaugeRecovery(slot, scaledFrameDeltaSeconds);
    BehaviourCombatContext& context =
        behaviourContexts[index];
    context.actor = &slot;

    const int moveId = slot.recoveredMoveId;
    if(moveRecord(moveId) == NULL)
    {
        moveStreamRunners[index].stop();
        behaviourRunners[index].stop();
        return;
    }
    if(moveStreamRunners[index].moveId() != moveId)
        moveStreamRunners[index].start(moveId);
    if((slot.flags & 0x4u) == 0)
    {
        behaviourRunners[index].stop();
        return;
    }
    if(behaviourRunners[index].moveId() != moveId)
        behaviourRunners[index].start(moveId);

    if(!slot.combatPosePublished && !slot.actorModelPresent)
    {
        slot.playingAnimationFrame +=
            scaledFrameDeltaSeconds * 60.0f * slot.animationTimeScale;
        if(slot.scheduledMotionValid)
        {
            const int cap = slot.scheduledMotion.maxDurationFrames;
            slot.scheduledMotionComplete = cap >= 0 ?
                slot.playingAnimationFrame >= static_cast<float>(cap) :
                !behaviourRunners[index].running() ||
                    behaviourRunners[index].parked();
        }
    }
    if(slot.scheduledMotionComplete)
        slot.flags |= 8u;
    if(!slot.scheduledMotionValid || slot.scheduledMotionComplete)
    {
        slot.scheduledMotionValid = false;
        slot.scheduledMotionComplete = false;
        moveStreamRunners[index].update(scaledFrameDeltaSeconds);
        if(moveStreamRunners[index].ended())
            slot.flags &= ~4u;
    }
    behaviourRunners[index].setWaitInSeconds(
        (slot.flags & 0x800u) != 0);
    behaviourRunners[index].setEffectSuppressionMask(
        behaviourEffectSuppressionMask);
    behaviourRunners[index].update(
        scaledFrameDeltaSeconds, slot.playingAnimationFrame);
}

void
Game::drainCombatActionPairs(void)
{

    if(scaledFrameDeltaSeconds == 0.0f)
        return;

    for(int i = 0; i < GAME_COMBAT_SLOT_COUNT; ++i)
    {
        combatActionPairs[i].eligibleSelf = 0;
        combatActionPairs[i].eligibleLinked = 0;
    }

    for(int i = 0; i < GAME_COMBAT_SLOT_COUNT; ++i)
    {
        GameSlotRecord* const actor =
            mutableCombatSlotByIndex(static_cast<uint8_t>(i));
        if(actor == NULL || (actor->flags & 0x1u) == 0)
            continue;
        CombatActionPairRecord& pair = combatActionPairs[i];
        if(pair.selfMotionId == 0)
        {
            if(pair.linkedMotionId != 0)
                pair.eligibleLinked = 1;
        }
        else if(pair.linkedMotionId == 0)
            pair.eligibleSelf = 1;
        else
        {
            const GameSlotRecord* const linked =
                combatSlotByIndex(actor->linkedSlotIndex);
            if(linked == NULL)
                continue;
            if((actor->actorBehaviorFlags & 4u) != 0 &&
               (linked->actorBehaviorFlags & 2u) != 0 &&
               (linked->actorTargetingFlags & 0x240u) == 0)
                continue;
            pair.eligibleSelf = 1;
            pair.eligibleLinked = 1;
        }
    }

    for(int i = 0; i < GAME_COMBAT_SLOT_COUNT; ++i)
    {
        GameSlotRecord* const actor =
            mutableCombatSlotByIndex(static_cast<uint8_t>(i));
        if(actor == NULL || (actor->flags & 0x1u) == 0)
            continue;
        GameSlotRecord* const linked =
            mutableCombatSlotByIndex(actor->linkedSlotIndex);
        if(linked == NULL || linked->slotIndex >= GAME_COMBAT_SLOT_COUNT)
            continue;
        CombatActionPairRecord& pair = combatActionPairs[i];
        CombatActionPairRecord& linkedPair =
            combatActionPairs[linked->slotIndex];
        if(!((pair.eligibleSelf != 0 && linkedPair.eligibleLinked != 0) ||
             (pair.eligibleLinked != 0 && linkedPair.eligibleSelf != 0)))
            continue;

        const uint32_t actorFlags = actor->actorBehaviorFlags;
        const uint32_t linkedFlags = linked->actorBehaviorFlags;
        if(actorFlags == linkedFlags)
        {
            if(actorFlags == 4u)
            {
                CombatActionPairRecord& loser =
                    (nextRandom() & 1u) == 0 ? linkedPair : pair;
                loser.eligibleSelf = 0;
                loser.eligibleLinked = 0;
            }
            else
            {
                pair.eligibleSelf = 0;
                pair.eligibleLinked = 0;
                linkedPair.eligibleSelf = 0;
                linkedPair.eligibleLinked = 0;
            }
        }
        else
        {
            const MoveRecord* const linkedMove =
                moveRecord(linkedPair.selfMotionId);
            if((actorFlags & 4u) != 0 &&
               ((linkedFlags & 2u) != 0 ||
                (linkedPair.selfMotionId != 0 && linkedMove != NULL &&
                 linkedMove->behaviorFlags == 2u)))
            {
                CombatActionPairRecord& loser =
                    (linked->actorTargetingFlags & 0x200u) == 0 ?
                        linkedPair : pair;
                loser.eligibleSelf = 0;
                loser.eligibleLinked = 0;
            }
            if((actorFlags & 6u) != 0 && (linkedFlags & 6u) == 0)
            {
                linkedPair.eligibleSelf = 0;
                linkedPair.eligibleLinked = 0;
            }
        }
    }

    for(int i = 0; i < GAME_COMBAT_SLOT_COUNT; ++i)
    {
        GameSlotRecord* const actor =
            mutableCombatSlotByIndex(static_cast<uint8_t>(i));
        if(actor == NULL || (actor->flags & 0x1u) == 0)
            continue;
        CombatActionPairRecord& pair = combatActionPairs[i];
        GameSlotRecord* const linked =
            mutableCombatSlotByIndex(actor->linkedSlotIndex);
        if(pair.eligibleSelf != 0 && pair.selfMotionId != 0)
        {

            if(pair.selfStateValue != 0)
                actor->queuedMoveId = pair.selfStateValue;
            startCombatMove(*actor, pair.selfMotionId,
                pair.inputPresentationValid ? pair.inputPresentation : actor->action);
        }
        if(pair.eligibleLinked != 0 && linked != NULL &&
           pair.linkedMotionId != 0)
        {

            if(pair.linkedStateValue != 0)
                linked->queuedMoveId = pair.linkedStateValue;
            startCombatMove(*linked, pair.linkedMotionId, linked->action);
        }

        pair.inputPresentationValid = false;
        pair.selfMotionIndex = 0;
        pair.linkedMotionIndex = 0;
        pair.selfMotionId = static_cast<int16_t>(
            actorMotionIdFromAction(*actor, 0));
        pair.linkedMotionId = linked != NULL ? static_cast<int16_t>(
            actorMotionIdFromAction(*linked, 0)) : 0;
        pair.selfStateIndex = 0;
        pair.linkedStateIndex = 0;
        pair.selfStateValue = pair.selfMotionId;
        pair.linkedStateValue = pair.linkedMotionId;

        if(actor->pendingReceivedDamage &&
           (actor->selectedActionId != 4 || actor->statePhase != 1))
            actor->pendingReceivedDamage = false;
    }
}

int
Game::combatStyle(const GameSlotRecord& slot) const
{
    const CharacterRecord* const record =
        characterRecordForId(static_cast<uint16_t>(slot.characterId));
    return record != NULL ? record->combatStyleId : -1;
}

int
Game::combatMoveForAction(const GameSlotRecord& actor, int actionIndex) const
{
    const int style = combatStyle(actor);

    if(actionIndex == 0x4c || actionIndex == 0x4d)
    {
        const size_t slotIndex = actionIndex == 0x4d ? 1 : 0;
        const PersistentMoveSlot& moveSlot = actor.moveSlots[slotIndex];
        if(actor.moveSlotActive[slotIndex] == 0)
            return -1;
        if(moveSlot.type == 1)
            return moveEquippedTypeOne(moveSlot.id);
        if(moveSlot.type == 2 && moveSlot.id != -1)
            return 0x18c;
        return -1;
    }

    const MovePersonalActionContext personalContext =
        combatPersonalActionContext(actor);
    int moveId = movePersonalActionForStyle(
        style, actionIndex, personalContext);
    bool remapDefaultAction = false;
    if(moveId < 0)
    {
        int fallbackAction = actionIndex;
        if(actionIndex == MOVE_ACTION_INDEX_FACING_ALT ||
           actionIndex == MOVE_ACTION_INDEX_FACING_STRONG)
            fallbackAction = MOVE_ACTION_INDEX_FACING;
        moveId = moveForAction(fallbackAction);
        remapDefaultAction = moveId > 0;
    }
    if(moveId < 0)
        return -1;

    return resolveCharacterMove(style,
        remapDefaultAction ? -moveId : moveId);
}

void
Game::startCombatMove(GameSlotRecord& slot, int moveId,
                      CombatAction presentationAction)
{
    if(moveRecord(moveId) == NULL ||
       slot.slotIndex >= GAME_COMBAT_SLOT_COUNT)
        return;

    startAction(slot, presentationAction, false);
    slot.recoveredMoveId = static_cast<int16_t>(moveId);

    slot.actorActionFlags &= 0x001df07cu;
    if((slot.actorActionFlags & 0x10u) != 0)
        slot.actorActionFlags |= 0x80u;
    slot.actorTargetingFlags &= 0x00a41010u;

    slot.appliedMotionId = static_cast<int16_t>(moveId);
    slot.animationCueFlags = 0;
    slot.scheduledMotionValid = false;
    slot.playingAnimationFrame = 0.0f;
    slot.scheduledMotionComplete = false;
    slot.activeHitBlock = 0;
    slot.hitWindowStartFrame = 0;
    slot.hitWindowEndFrame = 0;
    ++slot.hitGeneration;
    slot.hitChannelMask = 0;
    slot.behaviourDecisionHistoryMask |=
        slot.behaviourDecisionMask;
    slot.behaviourDecisionMask = 0;
    recordCombatTiredCost(slot, moveRecord(moveId)->tiredCost);
    setGameSlotSelectedAction(slot, moveId, true);
    behaviourMoveIds[slot.slotIndex] =
        static_cast<int16_t>(moveId);
    behaviourActions[slot.slotIndex] = presentationAction;
    setCombatActorMotion(slot, moveId);
}

bool
Game::combatMotion(uint8_t slotIndex, MoveMotion* motion) const
{
    const GameSlotRecord* const slot =
        slotIndex < GAME_COMBAT_SLOT_COUNT ?
            combatSlotByIndex(slotIndex) : NULL;
    if(slot == NULL || motion == NULL || !slot->scheduledMotionValid)
        return false;
    *motion = slot->scheduledMotion;
    return true;
}

void
Game::setCombatModelState(uint8_t slotIndex, bool modelPresent,
                          uint8_t loadStateFlags)
{
    const auto publish = [&](GameSlotRecord& slot)
    {
        slot.actorModelPresent = modelPresent;
        slot.actorLoadStateKnown = true;
        slot.actorLoadStateFlags = loadStateFlags;
    };
    GameSlotRecord* slot = mutableCombatSlotByIndex(slotIndex);
    if(slot != nullptr)
        publish(*slot);
    for(int index = 0; index < GAME_STAGE_ACTOR_CAPACITY; ++index)
        if(stageActors[index].occupied && stageActors[index].slot.slotIndex == slotIndex)
            publish(stageActors[index].slot);
}

void
Game::setCombatPose(uint8_t slotIndex, float animationFrame,
                    bool animationComplete,
                    const CombatBoneTransform* bones, int boneCount)
{
    GameSlotRecord* const slot = mutableCombatSlotByIndex(slotIndex);
    if(slot == NULL)
        return;
    slot->playingAnimationFrame = animationFrame;
    slot->scheduledMotionComplete = animationComplete;
    if(animationComplete)
        slot->flags |= 8u;
    slot->combatPosePublished = true;
    slot->combatBoneMask = 0;
    if(bones == NULL || boneCount <= 0)
        return;

    if(boneCount > COMBAT_BONE_COUNT)
        boneCount = COMBAT_BONE_COUNT;
    for(int i = 0; i < boneCount; ++i)
    {
        slot->combatBones[i] = bones[i];
        slot->combatBoneMask |= uint32_t(1) << i;
    }
}

void
Game::updateCombatDistanceFields(void)
{
    for(int i = -2; i < activeOpponentCount; ++i)
    {
        if(i == -1 && !allyActive)
            continue;
        GameSlotRecord& slot = i < -1 ? persistent.player :
            (i < 0 ? ally :
             (i == 0 ? enemy : additionalEnemies[i - 1]));
        const GameSlotRecord* const linked =
            combatSlotByIndex(slot.linkedSlotIndex);
        if(linked == NULL || linked == &slot)
            continue;
        slot.field_0x439c = computeCombatCameraField0x439c(
            slot.position.x, slot.position.z,
            linked->position.x, linked->position.z);
    }
}

bool
Game::pollSoundEvent(GameSoundEvent* event)
{
    if(event == NULL || soundEventCount == 0)
        return false;
    *event = soundEvents[soundEventHead];
    soundEventHead = (soundEventHead + 1) % GAME_SOUND_EVENT_CAPACITY;
    --soundEventCount;
    return true;
}

bool
Game::pollEstEvent(GameEstEvent* event)
{
    if(event == NULL || estEventCount == 0)
        return false;
    *event = estEvents[estEventHead];
    estEventHead = (estEventHead + 1) % GAME_EST_EVENT_CAPACITY;
    --estEventCount;
    return true;
}

void
Game::setEffectSuppressionMask(uint32_t mask)
{
    behaviourEffectSuppressionMask = mask;
    for(int i = 0; i < GAME_COMBAT_SLOT_COUNT; ++i)
        behaviourRunners[i].setEffectSuppressionMask(mask);
}

uint32_t
Game::effectSuppressionMask(void) const
{
    return behaviourEffectSuppressionMask;
}

bool
Game::consumeCombatDamageCameraSettle(float* damage)
{
    if(damage == NULL || pendingDamageCameraSettle <= 0.0f)
        return false;
    *damage = pendingDamageCameraSettle;
    pendingDamageCameraSettle = 0.0f;
    return true;
}

bool
Game::consumeCombatMoveCameraSettle(float* closeUpDurationFrames)
{
    if(closeUpDurationFrames == NULL || pendingMoveCameraSettle == 0.0f)
        return false;
    *closeUpDurationFrames = pendingMoveCameraSettle;
    pendingMoveCameraSettle = 0.0f;
    return true;
}

const CamdatDatabase& Game::camdat(void) const
{
    return camdatDatabase;
}

bool
Game::consumeCamdatCameraRequest(int* recordIndex, bool* exclusive,
                                 uint8_t* actorSlot)
{
    if(recordIndex == NULL || exclusive == NULL || actorSlot == NULL ||
       pendingCamdatRecord < 0)
        return false;
    *recordIndex = pendingCamdatRecord;
    *exclusive = pendingCamdatExclusive;
    *actorSlot = pendingCamdatActorSlot;
    pendingCamdatRecord = -1;
    pendingCamdatExclusive = false;
    pendingCamdatActorSlot = 0;
    return true;
}

const char*
Game::combatActionName(CombatAction action)
{
    switch(action)
    {
    case COMBAT_ACTION_IDLE:         return "IDLE";
    case COMBAT_ACTION_MOVE:         return "MOVE";
    case COMBAT_ACTION_LIGHT_ATTACK: return "LIGHT";
    case COMBAT_ACTION_HEAVY_ATTACK: return "HEAVY";
    case COMBAT_ACTION_GRAB:         return "GRAB";
    case COMBAT_ACTION_GUARD:        return "GUARD";
    case COMBAT_ACTION_DODGE:        return "DODGE";
    case COMBAT_ACTION_STAGGER:      return "STAGGER";
    case COMBAT_ACTION_KNOCKDOWN:    return "DOWN";
    case COMBAT_ACTION_DEFEATED:     return "DEFEATED";
    case COMBAT_ACTION_VICTORY:      return "VICTORY";
    }
    return "";
}

int
Game::combatInputCommand(void) const
{
    return combatInputLastCommand;
}

int
Game::combatInputActionIndex(void) const
{
    return combatInputLastActionIndex;
}

int
Game::combatInputMoveId(void) const
{
    return combatInputLastMoveId;
}

int
Game::combatInputHitWindows(MoveHitWindow* windows, int capacity) const
{
    const int16_t latched = persistent.player.recoveredMoveId;
    if(latched <= 0 || latched >= MOVE_RECORD_COUNT)
        return 0;
    return moveHitWindows(latched, windows, capacity);
}
