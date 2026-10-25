#include "game_battle_flow.h"

namespace
{

const BattleFlowActorSnapshot* actorAt(const BattleFlowFrameInput& input, int index)
{
    if(index < 0 || index >= BATTLE_FLOW_ACTOR_COUNT || !input.actors[index].valid)
        return nullptr;
    return &input.actors[index];
}

uint16_t actorBit(int index)
{
    return index >= 0 && index < BATTLE_FLOW_ACTOR_COUNT ?
        static_cast<uint16_t>(1u << index) : 0;
}

void freezeOccupied(const BattleFlowFrameInput& input, BattleFlowEffects& effects)
{
    for(int index = 0; index < BATTLE_FLOW_ACTOR_COUNT; ++index)
    {
        const BattleFlowActorSnapshot& actor = input.actors[index];
        if(actor.valid && (actor.flags & 1u) != 0)
            effects.freezeActorMask |= actorBit(index);
    }
}

void preparePhase(BattleFlowRuntime& runtime, BattleFlowEffects& effects, int phase)
{
    runtime.transitionType = 1;
    runtime.sequenceState = 0;
    runtime.flags = (runtime.flags | BATTLE_FLOW_CONTEXT_SEQUENCE_ACTIVE) &
        ~BATTLE_FLOW_CONTEXT_SEQUENCE_20;
    effects.outcomePhase = phase;
    effects.stopBattleCommand = true;
    effects.freezeActorMask |= actorBit(runtime.attacker);

}

void finishTick(BattleFlowRuntime& runtime, const BattleFlowFrameInput& input)
{
    runtime.frameAccumulator += input.scaledDeltaSeconds;
    runtime.frameCounter = static_cast<uint16_t>(runtime.frameCounter + 1);
    if(runtime.frameAccumulator > 1000.0f)
        runtime.frameAccumulator -= 1000.0f;
    runtime.flags &= ~BATTLE_FLOW_CONTEXT_OUTCOME_REQUEST;
}

}

bool battleFlowCombatReady(const BattleFlowActorSnapshot& actor)
{
    return actor.valid && actor.modelPresent && actor.loadStateKnown &&
        (actor.flags & 1u) != 0 && (actor.loadStateFlags & 1u) == 0 &&
        actor.health > 0;
}

bool battleFlowOutcomeBlocked(const BattleFlowOutcomeBlockInput& input)
{
    return input.nativeGlobal_0x5f3f90 == 0 &&
        (input.encounterStageId == 0 || (input.nativeGlobal_0x5f3dcc & 1u) == 0) &&
        (input.gameStateFlags & 0x1000u) == 0 && input.primaryAlive &&
        (input.primaryBehaviorFlags & 0x1e0u) == 0;
}

bool battleFlowExitRequested(const BattleFlowRuntime& runtime,
                             const BattleFlowFrameInput& input)
{
    return !input.interaction && (input.gameStateFlags & 1u) != 0 &&
        (input.gameStateFlags & 2u) == 0 &&
        (runtime.flags & BATTLE_FLOW_CONTEXT_EXIT_ENABLED) != 0;
}

bool battleFlowCanComplete(const BattleFlowFrameInput& input)
{
    return input.outcomeRulesKnown && (input.gameStateFlags & 2u) == 0 &&
        !input.interaction && (input.primaryLinkedPersistentFlags & 0x80000000u) == 0 &&
        input.nativeBattleModeFlag != 3;
}

bool battleFlowCanTriggerOpponentFollowup(const BattleFlowRuntime& runtime,
                                          const BattleFlowFrameInput& input,
                                          int attackerSlot, int targetSlot)
{
    const BattleFlowActorSnapshot* attacker = actorAt(input, attackerSlot);
    const BattleFlowActorSnapshot* target = actorAt(input, targetSlot);
    if(!input.outcomeRulesKnown || !battleFlowExitRequested(runtime, input) ||
       attacker == nullptr || target == nullptr || attacker->nativeSlotIndex != 0 ||
       target->profileCategory < 0 || target->profileCategory == 5)
        return false;
    switch(target->nativeCharacterId)
    {
    case -1:
    case 0x6c:
    case 0x16:
    case 0x6e:
    case 0x44:
    case 0x6b:
    case 0x35:
    case 0x9f:
    case 0x9e:
        return false;
    default:
        return true;
    }
}

bool queueBattleFlowTarget(BattleFlowRuntime& runtime,
                           const BattleFlowFrameInput& input,
                           int targetSlot, bool updateAction)
{
    const BattleFlowActorSnapshot* target = actorAt(input, targetSlot);
    if(runtime.pendingCombatant >= 0 || target == nullptr)
        return false;
    const BattleFlowActorSnapshot* linked = actorAt(input, target->linkedSlot);
    runtime.sequenceValue = linked != nullptr && (linked->flags & 1u) != 0 &&
        (linked->actorActionFlags & 0x4000u) != 0 ? linked->weaponObjectId : -1;
    runtime.sequenceOptionFlags = (runtime.sequenceOptionFlags & ~1u) |
        static_cast<uint32_t>(updateAction);
    runtime.pendingCombatant = targetSlot;
    runtime.flags |= BATTLE_FLOW_CONTEXT_TARGET_PENDING;
    return true;
}

BattleFlowEffects updateBattleFlow(BattleFlowRuntime& runtime,
                                   const BattleFlowFrameInput& input)
{
    BattleFlowEffects effects;
    if(input.pauseGuardEnabled && input.scaledDeltaSeconds == 0.0f)
        return effects;
    if(runtime.state == BATTLE_FLOW_CONTROLLER_ACTIVE)
    {
        const BattleFlowActorSnapshot* first = actorAt(input, runtime.participants[0]);
        const BattleFlowActorSnapshot* second = actorAt(input, runtime.participants[1]);
        if(first == nullptr || second == nullptr)
        {
            effects.waitingForInput = true;
            return effects;
        }
        if(input.interaction)
        {
            if(!input.opponentResolutionKnown)
                effects.waitingForInput = true;
            else if(input.filteredOpponentsResolved)
            {
                bool updateAction = false;
                bool ready = true;
                if(!input.allOpponentsResolved)
                {
                    updateAction = runtime.opponentDefeatDelaySeconds >=
                        BATTLE_FLOW_DISTANCE_OUTCOME_DELAY;
                    runtime.opponentDefeatDelaySeconds += input.scaledDeltaSeconds;
                    ready = updateAction;
                }
                if(ready)
                    queueBattleFlowTarget(runtime, input, runtime.participants[1], updateAction);
            }
            else
            {
                if(first->health <= 0)
                    queueBattleFlowTarget(runtime, input, runtime.participants[0], false);
                runtime.opponentDefeatDelaySeconds = 0;
            }
        }
        else
        {
            if(first->health <= 0)
                queueBattleFlowTarget(runtime, input, runtime.participants[0], false);
            if(second->health <= 0)
                queueBattleFlowTarget(runtime, input, runtime.participants[1], false);
        }
        if(input.requestFlag0400)
            runtime.flags |= BATTLE_FLOW_CONTEXT_END_400;
        if(input.targetRequestPending)
            runtime.flags |= BATTLE_FLOW_CONTEXT_TARGET_800;
        effects.updatePrideGaugeMask = actorBit(runtime.participants[0]) |
            actorBit(runtime.participants[1]);
        if((runtime.flags & BATTLE_FLOW_CONTEXT_TARGET_PENDING) != 0)
        {
            const BattleFlowActorSnapshot* pending = actorAt(input, runtime.pendingCombatant);
            const BattleFlowActorSnapshot* linked = pending != nullptr ?
                actorAt(input, pending->linkedSlot) : nullptr;
            if(pending == nullptr || linked == nullptr ||
               (input.interaction && !input.interactionAttackerRoutingKnown))
            {
                effects.waitingForInput = true;
                return effects;
            }
            effects.stopBattleCommand = true;
            effects.clearSlowMotionRequest = true;
            runtime.targetDelaySeconds = input.interaction ?
                (pending->nativeSlotIndex == 0 ? BATTLE_FLOW_PLAYER_TARGET_DELAY :
                 BATTLE_FLOW_OPPONENT_TARGET_DELAY) : 0;
            runtime.outcomeKind = 0;
            runtime.attacker = pending->linkedSlot;
            if(input.interaction && (linked->activeRoster || linked->battleGroup == 0))
                runtime.attacker = 0;
            runtime.transitionType = 2;
            runtime.pendingActionSlot = runtime.pendingCombatant;
            runtime.state = BATTLE_FLOW_CONTROLLER_WAIT_TARGET;
        }
        else if((runtime.flags & (BATTLE_FLOW_CONTEXT_END_200 |
                                 BATTLE_FLOW_CONTEXT_END_400 |
                                 BATTLE_FLOW_CONTEXT_TARGET_40 |
                                 BATTLE_FLOW_CONTEXT_TARGET_80)) != 0)
        {
            if((runtime.flags & BATTLE_FLOW_CONTEXT_END_200) != 0)
            {
                runtime.targetDelaySeconds = 0;
                runtime.transitionType = 2;
                runtime.outcomeKind = 1;
                runtime.attacker = 0;
                runtime.pendingActionSlot = -1;
            }
            else if((runtime.flags & BATTLE_FLOW_CONTEXT_END_400) != 0)
            {
                runtime.transitionType = 6;
                runtime.attacker = -1;
                runtime.outcomeKind = 0;
                runtime.flags &= ~BATTLE_FLOW_CONTEXT_END_400;
            }
            else if((runtime.flags & BATTLE_FLOW_CONTEXT_TARGET_40) != 0)
            {
                runtime.transitionType = 3;
                runtime.outcomeKind = 0;
                runtime.flags &= ~BATTLE_FLOW_CONTEXT_TARGET_40;
                effects.updateOpponentRangeState = true;
                effects.startPostBattleScene = true;
            }
            else
            {
                runtime.transitionType = 4;
                runtime.outcomeKind = 0;
                runtime.attacker = runtime.pendingOutcomeSlot;
                runtime.flags &= ~BATTLE_FLOW_CONTEXT_TARGET_80;
            }
            runtime.state = BATTLE_FLOW_CONTROLLER_END_WAIT;
            effects.returnOwnedWeaponMask = actorBit(runtime.participants[0]) |
                actorBit(runtime.participants[1]);
            effects.resetAttachmentMask = effects.returnOwnedWeaponMask;
            effects.actorStatusSet2Mask = effects.returnOwnedWeaponMask;
        }
        else if((runtime.flags & BATTLE_FLOW_CONTEXT_OUTCOME_REQUEST) != 0)
        {
            const BattleFlowActorSnapshot* attacker = actorAt(input, runtime.attacker);
            const BattleFlowActorSnapshot* target = attacker != nullptr ?
                actorAt(input, attacker->linkedSlot) : nullptr;
            if(attacker == nullptr || target == nullptr)
            {
                effects.waitingForInput = true;
                return effects;
            }
            runtime.outcomeKind = 1;
            preparePhase(runtime, effects, runtime.field_0x05c == 3 ? 2 : 0);
            runtime.state = BATTLE_FLOW_CONTROLLER_WAIT_SLOW_MOTION;
            effects.freezeActorMask |= actorBit(attacker->linkedSlot);

            if(runtime.field_0x05c == 3 && input.slowMotionEnabled == 0)
            {
                if((target->behaviorFlags & 0x10u) != 0)
                {
                    if(!target->groundedKnown)
                        effects.waitingForInput = true;
                    else
                    {
                        effects.mappedActions[attacker->linkedSlot] =
                            target->grounded ? 0x6f : 0x70;
                        runtime.state = BATTLE_FLOW_CONTROLLER_SEQUENCE;
                    }
                }
                else
                    runtime.state = BATTLE_FLOW_CONTROLLER_SEQUENCE;
            }
        }
        else if((runtime.flags & BATTLE_FLOW_CONTEXT_TARGET_800) != 0)
        {
            runtime.transitionType = 7;
            runtime.outcomeKind = 1;
            runtime.attacker = runtime.targetRequestTarget;
            runtime.state = BATTLE_FLOW_CONTROLLER_LETTERBOX;
        }
        else if((runtime.flags & BATTLE_FLOW_CONTEXT_END_1000) != 0)
        {
            runtime.transitionType = 8;
            runtime.attacker = runtime.participants[0];
            runtime.state = BATTLE_FLOW_CONTROLLER_END_WAIT;
        }
        finishTick(runtime, input);
        return effects;
    }
    if(runtime.state == BATTLE_FLOW_CONTROLLER_WAIT_SLOW_MOTION)
    {
        const BattleFlowActorSnapshot* attacker = actorAt(input, runtime.attacker);
        const BattleFlowActorSnapshot* target = attacker != nullptr ?
            actorAt(input, attacker->linkedSlot) : nullptr;
        if(attacker == nullptr || target == nullptr)
        {
            effects.waitingForInput = true;
            return effects;
        }
        effects.freezeActorMask = actorBit(runtime.attacker) | actorBit(attacker->linkedSlot);
        if(input.slowMotionEnabled == 0)
        {
            if((target->behaviorFlags & 0x10u) != 0)
            {
                if(!target->groundedKnown)
                    effects.waitingForInput = true;
                else
                {
                    effects.mappedActions[attacker->linkedSlot] =
                        target->grounded ? 0x6f : 0x70;
                    runtime.state = BATTLE_FLOW_CONTROLLER_SEQUENCE;
                }
            }
            else
                runtime.state = BATTLE_FLOW_CONTROLLER_SEQUENCE;
        }
    }
    else if(runtime.state == BATTLE_FLOW_CONTROLLER_WAIT_TARGET)
    {
        freezeOccupied(input, effects);
        const BattleFlowActorSnapshot* attacker = actorAt(input, runtime.attacker);
        const int targetSlot = input.interaction ? runtime.pendingActionSlot :
            attacker != nullptr ? attacker->linkedSlot : -1;
        const BattleFlowActorSnapshot* target = actorAt(input, targetSlot);
        if(attacker == nullptr || target == nullptr)
        {
            effects.waitingForInput = true;
            return effects;
        }
        bool ready = false;
        if(input.interaction)
        {
            if(target->nativeSlotIndex == 0)
            {
                if((target->behaviorFlags & 0x10u) != 0)
                {
                    runtime.targetDelaySeconds -= input.scaledDeltaSeconds;
                    ready = runtime.targetDelaySeconds <= 0;
                }
            }
            else if(attacker->behaviorFlags != 4)
            {
                runtime.targetDelaySeconds -= input.scaledDeltaSeconds;
                ready = runtime.targetDelaySeconds <= 0 && (attacker->poseFlags & 1u) != 0;
            }
        }
        else
        {
            runtime.targetDelaySeconds -= input.scaledDeltaSeconds;
            ready = runtime.targetDelaySeconds <= 0 && (target->behaviorFlags & 0x10u) != 0;
        }
        if(runtime.outcomeKind != 0)
            ready = true;
        if(ready)
        {
            runtime.targetDelaySeconds = 0;
            const bool requestedTarget = (runtime.flags & BATTLE_FLOW_CONTEXT_TARGET_PENDING) != 0;
            if(requestedTarget &&
               (!input.outcomeRulesKnown || attacker->nativeCharacterId < 0 ||
                target->nativeCharacterId < 0 || attacker->nativeSlotIndex < 0 ||
                target->nativeSlotIndex < 0 || target->profileCategory < 0))
            {
                effects.waitingForInput = true;
                return effects;
            }
            const bool suppressDeadPose = requestedTarget &&
                (input.gameStateFlags & 1u) != 0 && !input.interaction;
            if(target->health <= 0 && !suppressDeadPose)
            {
                if(!target->groundedKnown)
                {
                    effects.waitingForInput = true;
                    return effects;
                }
                effects.mappedActions[targetSlot] = target->grounded ? 0x6f : 0x70;
            }
            if(requestedTarget)
            {
                for(int participant : runtime.participants)
                {
                    const BattleFlowActorSnapshot* actor = actorAt(input, participant);
                    if(actor != nullptr && (actor->flags & 0x2000u) == 0)
                        effects.returnOwnedWeaponMask |= actorBit(participant);
                }
                effects.resetAllAttachments = true;
                if(input.interaction && attacker->nativeSlotIndex == 0)
                {
                    effects.playerFeedbackRecovery = true;
                    effects.mappedActions[runtime.attacker] = 1;
                    const BattleFlowActorSnapshot* primary = actorAt(input, 0);
                    effects.setAttackerWeaponMotionFlags = primary != nullptr &&
                        (primary->actorActionFlags & 0x4000u) != 0;
                }
                const bool finisher = battleFlowExitRequested(runtime, input) &&
                    battleFlowCanComplete(input) && target->nativeSlotIndex == 0 &&
                    target->nativeCharacterId == 0 && input.encounterStageId == 0 &&
                    target->pride == 0 &&
                    !(input.stageId == 19 && attacker->nativeCharacterId == 0x19);
                effects.persistentStatusSlot = targetSlot;
                effects.persistentStatus = 4;
                if(!input.interaction)
                    effects.scoreMultiplier = 1.2f;
                if(finisher)
                {
                    preparePhase(runtime, effects, 3);
                    runtime.state = BATTLE_FLOW_CONTROLLER_STORY_DELAY;
                    effects.finisherRequested = true;
                    effects.phase3StoryDelayRequested = true;
                    effects.phase3DelayFrameCount = 0x50;
                    runtime.outcomeReturnState = BATTLE_FLOW_CONTROLLER_SEQUENCE;
                    runtime.outcomeTimerFrames = input.sixtyHzTimingMode ?
                        80.0f : 80.0f * 0.8333334f;
                    effects.clearPauseGuard = true;
                }
                else if(battleFlowCanTriggerOpponentFollowup(runtime, input,
                                                           runtime.attacker, targetSlot))
                {
                    preparePhase(runtime, effects, 1);
                    runtime.state = BATTLE_FLOW_CONTROLLER_SEQUENCE;
                }
                else
                {
                    runtime.flags &= ~BATTLE_FLOW_CONTEXT_TARGET_PENDING;
                    runtime.state = BATTLE_FLOW_CONTROLLER_END_WAIT;
                }
            }
            else
                runtime.state = BATTLE_FLOW_CONTROLLER_SEQUENCE;
        }
    }
    else if(runtime.state == BATTLE_FLOW_CONTROLLER_STORY_DELAY ||
            runtime.state == BATTLE_FLOW_CONTROLLER_STORY_WAIT ||
            runtime.state == BATTLE_FLOW_CONTROLLER_SEQUENCE)
    {
        if(runtime.state == BATTLE_FLOW_CONTROLLER_STORY_DELAY)
            runtime.outcomeTimerFrames -= 1.0f;
        effects.requestSequenceTick = true;
        return effects;
    }
    else if(runtime.state == BATTLE_FLOW_CONTROLLER_RESOLVE)
    {
        effects.resetBattleState = true;
        runtime.state = BATTLE_FLOW_CONTROLLER_ACTIVE;
    }
    else if(runtime.state == BATTLE_FLOW_CONTROLLER_LETTERBOX)
    {
        effects.requestEndSequenceTick = true;
        return effects;
    }
    else if(runtime.state == BATTLE_FLOW_CONTROLLER_END_WAIT)
    {
        runtime.flags |= BATTLE_FLOW_CONTEXT_CLEANUP_READY;
        runtime.state = BATTLE_FLOW_CONTROLLER_CLEANUP;
    }

    finishTick(runtime, input);
    return effects;
}

void completeBattleFlowSequenceTick(BattleFlowRuntime& runtime,
    const BattleFlowFrameInput& input, int result, BattleFlowEffects& effects)
{
    if(!effects.requestSequenceTick)
        return;
    effects.requestSequenceTick = false;
    if(runtime.state == BATTLE_FLOW_CONTROLLER_STORY_DELAY)
    {
        if(runtime.outcomeTimerFrames < 0)
        {
            effects.startStoryProgressTask = true;
            runtime.state = BATTLE_FLOW_CONTROLLER_STORY_WAIT;
        }
    }
    else if(runtime.state == BATTLE_FLOW_CONTROLLER_STORY_WAIT)
    {
        if(!input.storyProgressTaskActive)
        {
            effects.restorePauseGuard = true;
            runtime.state = runtime.outcomeReturnState;
        }
    }
    else if(runtime.state == BATTLE_FLOW_CONTROLLER_SEQUENCE && result != 0)
    {
        const BattleFlowActorSnapshot* attacker = actorAt(input, runtime.attacker);
        const int linked = attacker != nullptr ? attacker->linkedSlot : -1;
        if(result < 1 || result > 11 || attacker == nullptr)
        {
            effects.waitingForInput = true;
            finishTick(runtime, input);
            return;
        }
        effects.stopEventSoundChannel0 = true;
        effects.createStageEventAudioTask = result != 10;
        effects.eventSoundCue = result == 1 ? 0x15 : result == 4 ? 0x0f :
            result == 9 ? -1 : 0x1c;
        runtime.state = BATTLE_FLOW_CONTROLLER_END_WAIT;
        switch(result)
        {
        case 2:
            effects.releaseParticipantAttachments = true;
            runtime.state = BATTLE_FLOW_CONTROLLER_RESOLVE;
            break;
        case 3:
        case 6:
            runtime.transitionType = 3;
            runtime.attacker = linked;
            break;
        case 4:
            runtime.transitionType = 5;
            break;
        case 5:
            effects.pauseCountdown = true;
            runtime.nativeSequenceFlags |= 1u;
            effects.clearAttackerInput = true;
            effects.setAttackerMotion40000 = true;
            runtime.state = BATTLE_FLOW_CONTROLLER_RESOLVE;
            break;
        case 7:
            effects.scoreMultiplier = 1.0f;
            effects.returnOwnedWeaponMask |= actorBit(runtime.attacker);
            effects.resetAllAttachments = true;
            runtime.transitionType = 3;
            runtime.attacker = linked;
            break;
        case 8:
            effects.pauseCountdown = true;
            effects.directActionSlot = linked;
            effects.directAction = 0x29e;
            effects.releaseAttackerAttachment = true;
            runtime.state = BATTLE_FLOW_CONTROLLER_RESOLVE;
            break;
        case 9:
            effects.scoreMultiplier = 1.5f;
            runtime.sequenceValue = 5;
            runtime.sequenceOptionFlags |= 2u;
            runtime.transitionType = 2;
            runtime.pendingActionSlot = linked;
            break;
        case 10:
            runtime.transitionType = 9;
            runtime.attacker = linked;
            break;
        case 11:
            runtime.transitionType = 10;
            runtime.attacker = linked;
            break;
        default: break;
        }
    }
    finishTick(runtime, input);
}

void completeBattleFlowEndSequenceTick(BattleFlowRuntime& runtime,
    const BattleFlowFrameInput& input, bool complete, BattleFlowEffects& effects)
{
    if(!effects.requestEndSequenceTick)
        return;
    effects.requestEndSequenceTick = false;
    if(complete)
        runtime.state = BATTLE_FLOW_CONTROLLER_END_WAIT;
    finishTick(runtime, input);
}
