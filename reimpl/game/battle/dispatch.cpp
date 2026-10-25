#include "game/game.h"
#include "game/combat/input.h"
#include "game/combat/links.h"
#include "game/internal.h"
#include "action_state_handlers.h"
#include "scene/task.h"
#include "rwworld.h"

int Game::consumeBattleSequenceCommand(BattleSequenceRuntime& runtime,
                                       const BattleSequenceCommand& command)
{
    GameSlotRecord* attacker = mutableCombatSlotByIndex(battleController.flow.attacker);
    GameSlotRecord* target = attacker != nullptr ?
        mutableCombatSlotByIndex(attacker->linkedSlotIndex) : nullptr;
    GameSlotRecord* actor = command.actorRole == 0 ? attacker : target;
    if(attacker == nullptr || target == nullptr)
        return 0;
    const auto action = [this](GameSlotRecord& slot, int move, bool run)
    {
        startCombatMove(slot, move == 0 ? 1 : move, slot.action);
        if(run)
            runCombatStateTask(slot);
    };
    switch(command.operation)
    {
    case BATTLE_SEQUENCE_UPDATE_PAIR_MOTION:

        for(GameSlotRecord* slot : { attacker, target })
        {
            updateAuthoredCombatMotion(*slot);
            if((slot->actorActionFlags & 0x200u) != 0)
            {
                setCombatMovementTarget(*slot, slot == attacker ? *target : *attacker);
                slot->actorActionFlags &= ~0x200u;
            }
            updateCombatMovement(*slot);
        }
        battleController.pairMotionUpdated = true;
        return 1;
    case BATTLE_SEQUENCE_SET_ACTION:
        action(*actor, command.value2 != 0 ? stanceMotion(*actor, command.value) :
            command.value, command.value3 != 0);
        return 1;
    case BATTLE_SEQUENCE_PREPARE_PRESENTATION:
        return prepareBattlePresentation();
    case BATTLE_SEQUENCE_UPDATE_PRESENTATION:
        return updateBattlePresentation();
    case BATTLE_SEQUENCE_RESET_PRESENTATION:
        battleController.choiceVisible = false;
        return 1;
    case BATTLE_SEQUENCE_SELECT_ACTION:
        return selectBattleSequenceAction();
    case BATTLE_SEQUENCE_RESOURCE_START:
        updateBattleResourceSequence(battleResources, false, command.address,
            battleResourceCallbacks);
        return 0;
    case BATTLE_SEQUENCE_RESOURCE_POLL:
        return updateBattleResourceSequence(battleResources, true, command.address,
            battleResourceCallbacks);
    case BATTLE_SEQUENCE_LETTERBOX:
        battleController.letterbox = true;
        return 1;
    case BATTLE_SEQUENCE_STOP_LETTERBOX:
        battleController.letterbox = false;
        return 1;
    case BATTLE_SEQUENCE_START_COUNTDOWN:
        if(battleController.countdownState >= 0 && battleController.countdownState != 4)
            battleController.countdownState = 2;
        return 1;
    case BATTLE_SEQUENCE_PAUSE_COUNTDOWN:
        if(battleController.countdownState >= 0 && battleController.countdownState != 4)
            battleController.countdownState = 3;
        return 1;
    case BATTLE_SEQUENCE_MARK_GAMEPLAY_ACTIVE:
        persistent.story.scratch005f33f4 = 1;
        return 1;
    case BATTLE_SEQUENCE_SET_FEEDBACK:
        battleFeedbackModeValue = command.value;
        return 1;
    case BATTLE_SEQUENCE_SET_TIME_SCALE:
        battleTimeScale = command.seconds;
        return 1;
    case BATTLE_SEQUENCE_SET_FADE:
        setSceneCharacterSelectCommand(command.value);
        setSceneCharacterSelectFadeStep(command.value2);
        if(command.value3 >= 0)
            setSceneCharacterSelectOverlayColor(
                static_cast<uint8_t>(command.value3 >> 16),
                static_cast<uint8_t>(command.value3 >> 8),
                static_cast<uint8_t>(command.value3));
        return 1;
    case BATTLE_SEQUENCE_FLOW_READY:
        return sceneCharacterSelectReady(1);
    case BATTLE_SEQUENCE_LOAD_FLAG_CLEARED:
        actor->actorLoadStateFlags = command.value != 0 ?
            actor->actorLoadStateFlags & ~1u : actor->actorLoadStateFlags | 1u;
        return 1;
    case BATTLE_SEQUENCE_PARTY_LOAD_FLAG_CLEARED:
        if(allyActive)
            ally.actorLoadStateFlags = command.value != 0 ?
                ally.actorLoadStateFlags & ~1u : ally.actorLoadStateFlags | 1u;
        return 1;
    case BATTLE_SEQUENCE_EVENT_2BC_CLEARED:
        setStorySaveFlag(persistent.story, 0x2bc, command.value == 0);
        return 1;
    case BATTLE_SEQUENCE_SET_STATUS:
    case BATTLE_SEQUENCE_SET_CHARACTER_VALUE0:
    {
        PersistentGameRecord* record = characterRecord(actor->persistentRecordId);
        if(record == nullptr)
            return 0;
        if(command.operation == BATTLE_SEQUENCE_SET_STATUS)
            record->status = static_cast<uint8_t>(command.value);
        else
            record->characterValues[0] = static_cast<int16_t>(command.value);
        return 1;
    }
    case BATTLE_SEQUENCE_DIALOGUE:
        queueBattleDialogue(*actor, command.value);
        return 1;
    case BATTLE_SEQUENCE_CAMDAT:
        pendingCamdatRecord = command.value;
        pendingCamdatActorSlot = actor->slotIndex;
        pendingCamdatExclusive = false;
        return 1;
    case BATTLE_SEQUENCE_RELEASE_ATTACHMENT:

        updateCombatWeaponBehaviour(*actor, MOVE_BEHAVIOUR_WEAPON_VISIBLE, 0);
        return 1;
    case BATTLE_SEQUENCE_RETURN_WEAPON:
        returnCombatOwnedWeapon(*actor);
        return 1;
    case BATTLE_SEQUENCE_RESET_ATTACHMENTS:
        resetCombatWeaponAttachments();
        return 1;
    case BATTLE_SEQUENCE_WEAPON_VISIBLE:
        updateCombatWeaponBehaviour(*actor, MOVE_BEHAVIOUR_WEAPON_VISIBLE, command.value);
        return 1;
    case BATTLE_SEQUENCE_ATTACH_OWNED_WEAPON:
    {
        if(command.value < 0 || command.value >= 2)
            return 0;
        actor->heldWorldWeapon = actor->ownedWorldWeapons[command.value];
        const int16_t manager = ensureCombatWorldWeapon(actor->heldWorldWeapon);
        if(manager < 0)
            return 0;
        weaponManager.pickup(manager, actor->slotIndex);
        updateCombatWeaponBehaviour(*actor, MOVE_BEHAVIOUR_WEAPON_ACTION, 1);
        updateCombatWeaponBehaviour(*actor, MOVE_BEHAVIOUR_WEAPON_VISIBLE, 1);
        return 1;
    }
    case BATTLE_SEQUENCE_REPOSITION:
        repositionBattlePair(command.value == 0 ? *attacker : *target,
                              command.value == 0 ? *target : *attacker);
        return 1;
    case BATTLE_SEQUENCE_PRIDE_VISIBLE:
        battleController.prideVisible = command.value != 0;
        return 1;
    case BATTLE_SEQUENCE_SCORE_MULTIPLIER:
        queueBattleScoreMultiplier(command.seconds);
        return 1;
    case BATTLE_SEQUENCE_ROBBERY:
        applyRobberyCashAward(actor->persistentRecordId, nextGameRandom(random));
        battleController.sceneTransitionActive = true;
        battleController.sceneTransitionComplete = false;
        battleController.sceneTransitionKind = 0;
        battleController.sceneTransitionArguments[0] = robberyAwardPresentationAmount;
        return 1;
    case BATTLE_SEQUENCE_START_SCENE_TRANSITION:
        battleController.sceneTransitionActive = true;
        battleController.sceneTransitionComplete = false;
        battleController.sceneTransitionKind = 1;
        battleController.sceneTransitionArguments[0] = command.value;
        battleController.sceneTransitionArguments[1] = command.value2;
        battleController.sceneTransitionArguments[2] = command.value3;
        persistent.story.gameStateFlags |= 0x8000000u;
        return 1;
    case BATTLE_SEQUENCE_SCENE_TRANSITION_COMPLETE:
        return !battleController.sceneTransitionActive || battleController.sceneTransitionComplete;
    case BATTLE_SEQUENCE_STOP_SCENE_TRANSITION:
        battleController.sceneTransitionActive = false;
        persistent.story.gameStateFlags &= ~0x8000000u;
        persistent.story.scratch005f33f4 = 1;
        persistent.story.scratch005f333c = 0;
        return 1;
    case BATTLE_SEQUENCE_INIT_CONFIRM_CHOICE:
        return prepareBattlePresentation(true);
    case BATTLE_SEQUENCE_UPDATE_CHOICE:
        return updateBattlePresentation();
    case BATTLE_SEQUENCE_FINALIZE_CHOICE:

        battleController.choiceVisible = false;
        return 1;
    case BATTLE_SEQUENCE_SOUND_CHANNEL2_ACTIVE:
        return sceneEventSoundChannelActive[2];
    case BATTLE_SEQUENCE_STOP_SOUND_CHANNEL2:
        battleController.pendingSoundStopMask |= 4;
        return 1;
    case BATTLE_SEQUENCE_SET_OUTCOME_BYTE:
        battleController.outcomeByte = command.value;
        return 1;
    case BATTLE_SEQUENCE_COMPUTE_PRIDE_DAMAGE:
        return battleSequencePrideDamage();
    case BATTLE_SEQUENCE_REDUCE_TARGET_PRIDE:
        actor->prideGauge = static_cast<int16_t>(actor->prideGauge - command.value);
        if(actor->prideGauge < 0)
            actor->prideGauge = 0;
        return 1;
    case BATTLE_SEQUENCE_CAN_APPLY_DAMAGE:
        return target->prideGauge <= 0 || battleController.condition16;
    case BATTLE_SEQUENCE_MAPPED_ACTION:
        return stanceMotion(*actor, command.value);
    case BATTLE_SEQUENCE_SETUP_ACTION_PAIR:
    {
        if(runtime.selectedActionType < 0 || runtime.selectedActionType >= 7)
            return 0;
        const bool equipped = runtime.selectedActionType >= 4 && runtime.actionVariant != 0;
        if(equipped)
        {
            BattleSequenceCommand attach = { BATTLE_SEQUENCE_ATTACH_OWNED_WEAPON,
                0, runtime.actionSelector, 0, 0, 0, 0 };
            consumeBattleSequenceCommand(runtime, attach);
        }
        const int move = BATTLE_ACTION_MOVES[runtime.selectedActionType].moves[0]
            [battleRelation(*attacker, *target)];
        if(move != 0)
        {
            action(*attacker, move, true);
            action(*target, actionStateLinkedMoveId(move), true);
        }
        if(runtime.selectedActionType == 3 ||
           ((runtime.selectedActionType == 4 || runtime.selectedActionType == 5) && !equipped))
            attacker->actorStatusFlags |= 8u;
        return runtime.selectedActionType == 2;
    }
    case BATTLE_SEQUENCE_RESET_CAMERA:
        battleController.cameraModeRequest = 9;
        return 1;
    case BATTLE_SEQUENCE_SET_CAMERA_MODE:
        battleController.cameraModeRequest = command.value2;
        return 1;
    case BATTLE_SEQUENCE_RELEASE_PARTY_ACTOR:
        if(allyActive && ally.persistentRecordId == command.value)
            ally.actorLoadStateFlags |= 1u;
        return 1;
    case BATTLE_SEQUENCE_EVENT_MESSAGE_READY:
        battleController.eventMessageTimeout -= scaledFrameDeltaSeconds;
        return !battleController.eventMessageActive ||
            !sceneEventSoundChannelActive[2] || battleController.eventMessageTimeout < -24.0f;
    case BATTLE_SEQUENCE_SELECT_FINISHER:
        selectBattleFinisherVariant(*attacker, *target);
        runtime.finisher = battleFinisherSelection;
        return runtime.finisher.variant;
    case BATTLE_SEQUENCE_RECORD_FINISHER:
        battleController.finisherCount = static_cast<int16_t>(battleController.finisherCount + 2);
        return 1;
    case BATTLE_SEQUENCE_RECORD_SPARED_TARGET:
        battleController.sparedCount = static_cast<int16_t>(battleController.sparedCount + 2);
        if(command.value != 0)
            queueBattleScoreMultiplier(1.0f);
        return 1;
    case BATTLE_SEQUENCE_ACTOR_ACTION_FLAGS:
        if(command.value2 != 0)
            actor->actorActionFlags |= static_cast<uint32_t>(command.value);
        else
            actor->actorActionFlags &= ~static_cast<uint32_t>(command.value);
        return 1;
    case BATTLE_SEQUENCE_REDUCE_HEALTH:
        reduceGameSlotHealth(*actor, static_cast<int16_t>(command.value));
        return 1;
    case BATTLE_SEQUENCE_GROUNDED:
    {
        bool grounded;
        return combatGroundedState(*actor, grounded) ? static_cast<int>(grounded) : -1;
    }
    case BATTLE_SEQUENCE_MOTION_FLAGS:
        if(command.value2 != 0)
            actor->motionControlFlags |= static_cast<uint32_t>(command.value);
        else
            actor->motionControlFlags &= ~static_cast<uint32_t>(command.value);
        return 1;
    case BATTLE_SEQUENCE_SPECIAL_INIT:
        battleController.special = {};
        battleController.special.variant = 3;
        return 1;
    case BATTLE_SEQUENCE_SPECIAL_UPDATE:
        return updateBattleSpecial();
    case BATTLE_SEQUENCE_CINEMATIC_INIT:
        battleController.cinematic = {};
        return 1;
    case BATTLE_SEQUENCE_CINEMATIC_UPDATE:
        return updateBattleCinematic();
    case BATTLE_SEQUENCE_CINEMATIC_DESTROY:
        battleController.eventMessageActive = false;
        return 1;
    default:
        battleController.unresolvedOperation = command.operation;
        return 0;
    }
}
