#include "game.h"
#include "game_combat_input.h"
#include "game_internal.h"
#include "character_records.h"
#include "scene_task.h"

const BattleControllerRuntime& Game::battleControllerState(void) const
{
    return battleController;
}

bool Game::queueBattleSequenceActorTarget(const GameSlotRecord& actor)
{

    if(!battleController.active || (persistent.story.gameStateFlags & 3u) != 1u ||
       (battleController.flow.flags & BATTLE_FLOW_CONTEXT_TARGET_800) != 0)
        return false;
    battleController.targetRequestPending = true;
    battleController.targetRequestSource = actor.slotIndex;
    battleController.flow.targetRequestTarget = actor.linkedSlotIndex;
    return true;
}

void Game::initializeBattleController(void)
{
    const int mode = battleController.nativeBattleMode;
    const uint32_t encounterFlags = battleController.encounterFlags;
    battleController = {};
    battleController.nativeBattleMode = mode;
    battleController.encounterFlags = encounterFlags;
    battleController.active = activeOpponentCount > 0;
    battleController.flow.flags = 7;
    battleController.flow.participants[0] = persistent.player.slotIndex;
    battleController.flow.participants[1] = enemy.slotIndex;

    if(mode == 3)
        battleController.flow.flags &= ~6u;
}

BattleFlowFrameInput Game::battleFlowInput(void) const
{
    BattleFlowFrameInput input;
    input.scaledDeltaSeconds = scaledFrameDeltaSeconds;
    input.targetRequestPending = battleController.targetRequestPending;
    input.gameStateFlags = persistent.story.gameStateFlags;
    input.interaction = (persistent.story.battleFlags & 0x200u) != 0;
    input.pauseGuardEnabled = battleController.pauseGuardEnabled;
    input.sixtyHzTimingMode = persistent.options.videoTimingMode != 0;
    input.slowMotionEnabled = battleFeedbackModeValue;
    input.nativeBattleModeFlag = battleController.nativeBattleMode;
    input.encounterStageId = persistent.story.subBattleId;
    input.stageId = selectedStageId();
    input.storyProgressTaskActive = persistent.story.progression.sequenceTask.active != 0;
    input.outcomeRulesKnown = true;
    input.interactionAttackerRoutingKnown = true;
    const GameSlotRecord* linked = combatSlotByIndex(persistent.player.linkedSlotIndex);
    const PersistentGameRecord* linkedRecord = linked != nullptr ?
        characterRecord(static_cast<uint16_t>(linked->persistentRecordId)) : nullptr;
    if(linkedRecord != nullptr)
        input.primaryLinkedPersistentFlags = linkedRecord->flags_0x388;
    for(int index = 0; index < BATTLE_FLOW_ACTOR_COUNT; ++index)
    {
        const GameSlotRecord* slot = combatSlotByIndex(index);
        if(slot == nullptr || (slot->flags & 1u) == 0)
            continue;
        BattleFlowActorSnapshot& actor = input.actors[index];
        actor.valid = true;
        actor.modelPresent = slot->actorModelPresent;
        actor.loadStateKnown = slot->actorLoadStateKnown;
        actor.loadStateFlags = slot->actorLoadStateFlags;
        actor.flags = slot->flags;
        actor.behaviorFlags = slot->actorBehaviorFlags;
        actor.actorActionFlags = slot->actorActionFlags;
        actor.poseFlags = slot->poseStateWord;
        actor.health = slot->currentHealth;
        actor.pride = slot->prideGauge;
        actor.nativeCharacterId = slot->nativeCharacterId;
        actor.nativeSlotIndex = slot->slotIndex;
        actor.linkedSlot = slot->linkedSlotIndex;
        const CharacterRecord* defaults = characterRecordForId(
            static_cast<uint16_t>(slot->characterId));
        actor.profileCategory = defaults != nullptr ? defaults->profileCategory : -1;
        actor.activeRoster = index == 0 || (allyActive && slot == &ally) ||
            stageActorIsActiveRoster(slot->persistentRecordId);
        actor.battleGroup = slot->battleGroup;
        actor.groundedKnown = combatGroundedState(*slot, actor.grounded);
        const WeaponInstance* held = weaponManager.equippedBy(slot->slotIndex);
        actor.weaponObjectId = held != nullptr ? held->objectId : -1;
    }

    input.opponentResolutionKnown = true;
    input.filteredOpponentsResolved = allOpponentsDefeated();
    input.allOpponentsResolved = !stageAiGeneratorsPending();
    for(int index = 0; index < activeOpponentCount; ++index)
        if((index == 0 ? enemy : additionalEnemies[index - 1]).currentHealth > 0)
            input.allOpponentsResolved = false;
    return input;
}

void Game::beginBattleOutcomePhase(int phase)
{
    BattleSequenceRuntime& sequence = battleController.sequence;
    sequence = {};
    sequence.battleMode = phase;
    sequence.flags = phase == 0 || phase == 3 ? 2u : 0u;
    sequence.flowFlags = battleController.flow.flags;
    battleController.sequenceInitialized = true;
    battleController.choiceVisible = false;
    battleController.outcomeByte = 0;
    GameSlotRecord* attacker = mutableCombatSlotByIndex(battleController.flow.attacker);
    if(attacker != nullptr)
    {
        attacker->actorActionFlags |= 4u;
        GameSlotRecord* target = mutableCombatSlotByIndex(attacker->linkedSlotIndex);
        if(target != nullptr)
            target->actorActionFlags |= 4u;
        if(phase != 2)
            forceDropCombatWeapon(*attacker, true);
    }
}

void Game::applyBattleFlowEffects(const BattleFlowEffects& effects)
{
    BattleFlowRuntime& flow = battleController.flow;
    if(effects.startStoryProgressTask)
    {
        persistent.story.progression.sequenceTask = {};
        persistent.story.progression.sequenceTask.active = 1;
        persistent.story.progression.sequenceTask.state = 1;
    }
    if(effects.clearPauseGuard)
        battleController.pauseGuardEnabled = false;
    if(effects.restorePauseGuard)
        battleController.pauseGuardEnabled = true;
    if(effects.outcomePhase >= 0)
        beginBattleOutcomePhase(effects.outcomePhase);
    for(int index = 0; index < BATTLE_FLOW_ACTOR_COUNT; ++index)
    {
        GameSlotRecord* slot = mutableCombatSlotByIndex(index);
        if(slot == nullptr || (slot->flags & 1u) == 0)
            continue;
        const uint16_t bit = static_cast<uint16_t>(1u << index);
        if((effects.freezeActorMask & bit) != 0)
            slot->actorActionFlags |= 4u;
        if((effects.returnOwnedWeaponMask & bit) != 0)
            returnCombatOwnedWeapon(*slot);
        if((effects.actorStatusSet2Mask & bit) != 0)
            slot->actorStatusFlags |= 2u;
        if(effects.mappedActions[index] != 0)
        {
            startCombatMove(*slot, stanceMotion(*slot, effects.mappedActions[index]), slot->action);
            runCombatStateTask(*slot);
        }
    }
    if(effects.resetAllAttachments || effects.resetAttachmentMask != 0)
        resetCombatWeaponAttachments();
    if(effects.persistentStatusSlot >= 0)
    {
        const GameSlotRecord* slot = combatSlotByIndex(effects.persistentStatusSlot);
        PersistentGameRecord* record = slot != nullptr ?
            characterRecord(static_cast<uint16_t>(slot->persistentRecordId)) : nullptr;
        if(record != nullptr)
            record->status = static_cast<uint8_t>(effects.persistentStatus);
    }
    if(effects.scoreMultiplier > 0)
        queueBattleScoreMultiplier(effects.scoreMultiplier);
    if(effects.directActionSlot >= 0)
    {
        GameSlotRecord* slot = mutableCombatSlotByIndex(effects.directActionSlot);
        if(slot != nullptr)
            startCombatMove(*slot, effects.directAction, slot->action);
    }
    if(effects.resetBattleState)
    {
        const int first = flow.participants[0], second = flow.participants[1];
        flow = {};
        flow.flags = 6;
        flow.participants[0] = first;
        flow.participants[1] = second;
        battleController.sequenceInitialized = false;
    }
    if(effects.eventSoundCue >= 0)
    {
        GameSoundEvent event = {};
        event.kind = GAME_SOUND_EVENT_EVENT_CUE;
        event.cue = static_cast<uint8_t>(effects.eventSoundCue);
        enqueueSoundEvent(event);
    }
    if(effects.stopEventSoundChannel0)
        battleController.pendingSoundStopMask |= 1;
    GameSlotRecord* attacker = mutableCombatSlotByIndex(flow.attacker);
    if(attacker != nullptr)
    {
        if(effects.clearAttackerInput)
            attacker->combatCommand = {};
        if(effects.setAttackerMotion40000)
            attacker->motionControlFlags |= 0x40000u;
        if(effects.playerFeedbackRecovery)
        {
            battleFeedbackModeValue = 2;
            battleController.feedbackRecoverySeconds = BATTLE_FEEDBACK_MODE_2_SECONDS;
        }
        if(effects.setAttackerWeaponMotionFlags)
            attacker->motionControlFlags |= 0x100001u;
    }
}

int Game::dispatchBattleSequence(void* userData, BattleSequenceRuntime& runtime,
                                 const BattleSequenceCommand& command)
{
    return static_cast<Game*>(userData)->consumeBattleSequenceCommand(runtime, command);
}

void Game::updateBattleController(float scaledSeconds)
{
    if(!battleController.active || currentBattleOutcome != BATTLE_OUTCOME_IN_PROGRESS)
        return;
    BattleFlowFrameInput input = battleFlowInput();
    input.scaledDeltaSeconds = scaledSeconds;
    BattleFlowRuntime& flow = battleController.flow;
    BattleFlowEffects effects = ::updateBattleFlow(flow, input);
    applyBattleFlowEffects(effects);
    if(effects.requestSequenceTick)
    {
        GameSlotRecord* attacker = mutableCombatSlotByIndex(flow.attacker);
        GameSlotRecord* target = attacker != nullptr ?
            mutableCombatSlotByIndex(attacker->linkedSlotIndex) : nullptr;
        if(attacker != nullptr && target != nullptr)
        {
            BattleSequenceInput sequenceInput;
            sequenceInput.scaledDeltaSeconds = scaledSeconds;
            sequenceInput.relation = battleRelation(*attacker, *target);
            sequenceInput.attackerFlags = attacker->flags;
            sequenceInput.targetFlags = target->flags;
            sequenceInput.attackerSelectedAction = attacker->selectedActionId;
            sequenceInput.attackerNativeSlot = attacker->slotIndex;
            sequenceInput.targetHasPride = target->prideGauge > 0;
            sequenceInput.condition16 = battleController.condition16;
            battleController.sequence.flowFlags = flow.flags;
            const BattleSequenceCallbacks callbacks = { dispatchBattleSequence, this };
            const int result = ::updateBattleSequence(battleController.sequence, sequenceInput, callbacks);
            flow.flags = battleController.sequence.flowFlags;
            BattleFlowEffects completed;
            completed.requestSequenceTick = true;
            completeBattleFlowSequenceTick(flow, input, result, completed);
            applyBattleFlowEffects(completed);
        }
    }
    if(effects.requestEndSequenceTick)
    {
        if(!battleEndSequenceActive)
        {
            battleEndSequenceActive = true;
            battleEndRuntime = {};
            battleEndNoticeRuntime = {};
        }
        updateBattleEndSequence(scaledSeconds);
        BattleFlowEffects completed;
        completed.requestEndSequenceTick = true;
        completeBattleFlowEndSequenceTick(flow, input,
            battleEndRuntime.state == BATTLE_END_COMPLETE, completed);
        applyBattleFlowEffects(completed);
    }
    if(flow.state == BATTLE_FLOW_CONTROLLER_CLEANUP &&
       (flow.flags & BATTLE_FLOW_CONTEXT_CLEANUP_READY) != 0)
    {
        const BattleOutcome outcome = persistent.player.currentHealth <= 0 ?
            BATTLE_OUTCOME_PLAYER_LOST : BATTLE_OUTCOME_PLAYER_WON;
        battleController.active = false;
        finishBattle(outcome);
    }
}
