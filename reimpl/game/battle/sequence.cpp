#include "game/battle/sequence.h"
#include "action_state_handlers.h"

namespace
{

auto sequenceDispatch(BattleSequenceRuntime& runtime,
                      const BattleSequenceCallbacks& callbacks)
{
    return [&](BattleSequenceOperation operation, int actor = -1,
               int value = 0, int value2 = 0, int value3 = 0,
               float seconds = 0.0f, uint32_t address = 0)
    {
        const BattleSequenceCommand command = {
            operation, actor, value, value2, value3, seconds, address };
        return callbacks.dispatch(callbacks.userData, runtime, command);
    };
}

}

bool updateBattleFinisherSequence(BattleSequenceRuntime& runtime,
                                  const BattleSequenceInput& input,
                                  const BattleSequenceCallbacks& callbacks)
{
    if(callbacks.dispatch == nullptr || input.relation < 0 || input.relation >= 5 ||
       runtime.finisher.variant < 2 || runtime.finisher.variant >= 16)
        return false;
    const auto call = sequenceDispatch(runtime, callbacks);
    const BattleFinisherDefinition& pre = BATTLE_FINISHER_DEFINITIONS[1][1];
    const BattleFinisherDefinition& main =
        BATTLE_FINISHER_DEFINITIONS[runtime.finisher.variant][1];
    const auto pair = [&](int move)
    {
        if(move != 0)
        {
            call(BATTLE_SEQUENCE_SET_ACTION, 0, move, 0, 1);
            call(BATTLE_SEQUENCE_SET_ACTION, 1, actionStateLinkedMoveId(move), 0, 1);
        }
    };
    switch(runtime.finisherStep)
    {
    case 0:
        if(main.resourceVariant < 0 || main.resourceVariant >= 24)
            return false;
        runtime.resources[1] = BATTLE_RESOURCE_ROWS[main.resourceVariant][input.relation];
        call(BATTLE_SEQUENCE_RESOURCE_START, -1, 0, 0, 0, 0, runtime.resources[1]);
        if(runtime.battleMode != 3)
            pair(pre.moves[input.relation]);
        call(BATTLE_SEQUENCE_SET_CAMERA_MODE, -1, 1, 0x1f);
        call(BATTLE_SEQUENCE_SET_FEEDBACK, -1, 1);
        call(BATTLE_SEQUENCE_SET_TIME_SCALE, -1, 0, 0, 0, 0.72f);
        runtime.finisherElapsed = 0;
        runtime.finisherFlags = 0;
        ++runtime.finisherStep;
        break;
    case 1:
        if((runtime.finisherFlags & 1u) == 0 &&
           call(BATTLE_SEQUENCE_RESOURCE_POLL, -1, 0, 0, 0, 0, runtime.resources[1]))
            runtime.finisherFlags |= 1u;
        if(runtime.finisherElapsed >= pre.seconds || runtime.battleMode == 3)
            runtime.finisherFlags |= 0x10u;
        if((runtime.finisherFlags & 0x100u) == 0 &&
           (runtime.finisherFlags & 0x10u) != 0)
        {
            call(BATTLE_SEQUENCE_SET_FADE, -1, 3, 9, 0xc8c8c8);
            runtime.finisherFlags |= 0x100u;
        }
        if((runtime.finisherFlags & 0x100u) != 0 && call(BATTLE_SEQUENCE_FLOW_READY))
            runtime.finisherFlags |= 0x200u;
        if((runtime.finisherFlags & 0x211u) == 0x211u)
        {
            call(BATTLE_SEQUENCE_REPOSITION, -1, 0);
            call(BATTLE_SEQUENCE_SET_FADE, -1, 4, 7, -1);
            runtime.finisherElapsed = 0;
            runtime.finisherFlags = 0;
            ++runtime.finisherStep;
        }
        break;
    case 2:
        pair(main.moves[input.relation]);
        if(runtime.finisher.usesMoveSlot)
            call(BATTLE_SEQUENCE_ATTACH_OWNED_WEAPON, 0,
                 runtime.finisher.moveSlotIndex);
        call(BATTLE_SEQUENCE_SET_TIME_SCALE, -1, 0, 0, 0, 1.0f);
        call(BATTLE_SEQUENCE_SET_FEEDBACK, -1, 1);
        call(BATTLE_SEQUENCE_DIALOGUE, 0, 0x0f);
        call(BATTLE_SEQUENCE_REDUCE_HEALTH, 1, 9999);
        call(BATTLE_SEQUENCE_LOAD_FLAG_CLEARED, 0, 1);
        call(BATTLE_SEQUENCE_LOAD_FLAG_CLEARED, 1, 1);
        call(BATTLE_SEQUENCE_PARTY_LOAD_FLAG_CLEARED, -1, 0);
        call(BATTLE_SEQUENCE_PRIDE_VISIBLE, -1, 0);
        call(BATTLE_SEQUENCE_EVENT_2BC_CLEARED, -1, 0);
        runtime.finisherElapsed = 0;
        runtime.finisherFlags = 0;
        ++runtime.finisherStep;
        break;
    case 3:
        if(runtime.finisherElapsed >= main.seconds)
        {
            runtime.finisherElapsed = 0;
            runtime.finisherFlags = 0;
            ++runtime.finisherStep;
        }
        break;
    case 4:
        if((runtime.finisherFlags & 1u) == 0)
        {
            call(BATTLE_SEQUENCE_SET_FADE, -1, 3, 6, 0);
            runtime.finisherFlags |= 1u;
        }
        if(call(BATTLE_SEQUENCE_FLOW_READY))
        {
            runtime.finisherElapsed = 0;
            runtime.finisherFlags = 0;
            ++runtime.finisherStep;
        }
        break;
    case 5:
    {
        const int grounded = call(BATTLE_SEQUENCE_GROUNDED, 1);
        if(grounded < 0)
            break;
        call(BATTLE_SEQUENCE_RETURN_WEAPON, 0);
        call(BATTLE_SEQUENCE_RETURN_WEAPON, 1);
        call(BATTLE_SEQUENCE_RESET_ATTACHMENTS);
        call(BATTLE_SEQUENCE_SET_ACTION, 0, 1, 1);
        call(BATTLE_SEQUENCE_SET_ACTION, 1,
             grounded != 0 ? 0x6f : 0x70, 1);
        call(BATTLE_SEQUENCE_MOTION_FLAGS, 0, 0x8000, 1);
        call(BATTLE_SEQUENCE_MOTION_FLAGS, 1, 0x8000, 1);
        runtime.finisherElapsed = 0;
        runtime.finisherFlags = 0;
        ++runtime.finisherStep;
        break;
    }
    case 6:
        return true;
    default:
        return false;
    }
    runtime.finisherElapsed += input.scaledDeltaSeconds;
    return false;
}

int updateBattleSequence(BattleSequenceRuntime& runtime,
                         const BattleSequenceInput& input,
                         const BattleSequenceCallbacks& callbacks)
{
    if(callbacks.dispatch == nullptr || input.relation < 0 || input.relation >= 5)
        return 0;
    const auto call = sequenceDispatch(runtime, callbacks);
    const auto action = [&](int actor, int move, bool mapped = false, bool run = false)
    {
        call(BATTLE_SEQUENCE_SET_ACTION, actor, move, mapped, run);
    };
    const auto pair = [&](int move, bool run = false)
    {
        action(0, move, false, run);
        action(1, actionStateLinkedMoveId(move), false, run);
    };
    const auto resources = [&](int slot, uint32_t address)
    {
        runtime.resources[slot] = address;
        if(address != 0)
            call(BATTLE_SEQUENCE_RESOURCE_START, -1, 0, 0, 0, 0, address);
    };
    const auto poll = [&](int slot)
    {
        return runtime.resources[slot] == 0 ||
            call(BATTLE_SEQUENCE_RESOURCE_POLL, -1, 0, 0, 0, 0, runtime.resources[slot]);
    };
    const auto actionMove = [&](int phase)
    {
        return runtime.selectedActionType >= 0 && runtime.selectedActionType < 7 ?
            BATTLE_ACTION_MOVES[runtime.selectedActionType].moves[phase][input.relation] : 0;
    };
    call(BATTLE_SEQUENCE_UPDATE_PAIR_MOTION);
    int result = 0;
    switch(runtime.state)
    {
    case 0:
    {
        runtime.timerSeconds = 0.4f;
        runtime.delaySeconds = 0;
        call(BATTLE_SEQUENCE_LETTERBOX, -1, 0);
        call(BATTLE_SEQUENCE_START_COUNTDOWN);
        call(BATTLE_SEQUENCE_PREPARE_PRESENTATION);
        call(BATTLE_SEQUENCE_SELECT_ACTION);
        int variant = 1;
        if(runtime.battleMode == 3 || runtime.battleMode == 2)
            variant = 3;
        else if(runtime.battleMode == 1)
            variant = 2;
        else
        {
            call(BATTLE_SEQUENCE_EVENT_2BC_CLEARED, -1, 0);
            switch(call(BATTLE_SEQUENCE_SELECT_ACTION))
            {
            case 1: variant = 8; break;
            case 2: variant = 4; break;
            case 3: variant = 5; break;
            case 4: variant = 6; break;
            case 5: variant = 7; break;
            case 6: variant = 23; break;
            default: break;
            }
        }
        resources(0, BATTLE_RESOURCE_ROWS[variant][input.relation]);
        if(runtime.battleMode == 3)
        {
            call(BATTLE_SEQUENCE_SET_CAMERA_MODE, -1, 1, 0x2a);
            call(BATTLE_SEQUENCE_RELEASE_PARTY_ACTOR, -1, 1);
            call(BATTLE_SEQUENCE_RELEASE_PARTY_ACTOR, -1, 2);
        }
        runtime.state = 1;
        break;
    }
    case 1:
    {
        const bool loaded = poll(0);
        runtime.timerSeconds -= input.scaledDeltaSeconds;
        bool ready = false;
        if(runtime.timerSeconds <= 0)
        {
            ready = call(BATTLE_SEQUENCE_UPDATE_PRESENTATION);
            if(runtime.battleMode == 3)
                call(BATTLE_SEQUENCE_LOAD_FLAG_CLEARED, 0, 0);
        }
        if(!loaded || !ready)
            break;
        runtime.outcomeMode = runtime.pendingOutcomeMode;
        call(BATTLE_SEQUENCE_RESET_PRESENTATION);
        if(runtime.outcomeMode == 1 && runtime.reactionType == 1)
        {
            runtime.selectedActionType = runtime.pendingActionType;
            runtime.reactionType = runtime.pendingReactionType;
            runtime.actionVariant = runtime.pendingActionVariant;
        }
        int pride = call(BATTLE_SEQUENCE_COMPUTE_PRIDE_DAMAGE);
        if(pride < 0)
            pride = 0;
        if(runtime.battleMode == 0 && runtime.outcomeMode != 4)
            call(BATTLE_SEQUENCE_REDUCE_TARGET_PRIDE, 1, pride);
        runtime.flags = (runtime.flags & ~8u) |
            (call(BATTLE_SEQUENCE_CAN_APPLY_DAMAGE) ? 8u : 0u);
        if(runtime.battleMode == 3)
            runtime.state = runtime.outcomeMode < 1 ? 1800 : 1500;
        else if(runtime.battleMode == 2)
            runtime.state = 1700;
        else if(runtime.battleMode == 1)
            runtime.state = 1600;
        else
        {
            call(BATTLE_SEQUENCE_SET_OUTCOME_BYTE, -1, runtime.outcomeMode);
            runtime.state = 2;
        }
        break;
    }
    case 2:
        runtime.timerSeconds = 0;
        if(runtime.outcomeMode == 4)
        {
            call(BATTLE_SEQUENCE_SET_OUTCOME_BYTE, -1, 4);
            call(BATTLE_SEQUENCE_EVENT_2BC_CLEARED, -1, 1);
            runtime.state = 1250;
        }
        else
            runtime.state = runtime.outcomeMode == 3 ? 1200 :
                runtime.outcomeMode == 2 ? 1150 : 1000;
        break;
    case 100:
        call(BATTLE_SEQUENCE_DIALOGUE, 1, 7);
        action(0, 1, true, true);
        action(1, 0x186, false, true);
        call(BATTLE_SEQUENCE_LOAD_FLAG_CLEARED, 0, 0);
        call(BATTLE_SEQUENCE_LOAD_FLAG_CLEARED, 1, 1);
        runtime.soundTimeoutSeconds = 30.0f;
        runtime.timerSeconds = 2.3f;
        call(BATTLE_SEQUENCE_PARTY_LOAD_FLAG_CLEARED, -1, 0);
        call(BATTLE_SEQUENCE_PRIDE_VISIBLE, -1, 0);
        runtime.state = 101;
        break;
    case 101:
    {
        call(BATTLE_SEQUENCE_LOAD_FLAG_CLEARED, 0, 0);
        runtime.timerSeconds -= input.scaledDeltaSeconds;
        bool ready = false;
        if(runtime.timerSeconds <= 0)
        {
            if(!call(BATTLE_SEQUENCE_SOUND_CHANNEL2_ACTIVE))
                ready = true;
            else
            {
                runtime.soundTimeoutSeconds -= input.scaledDeltaSeconds;
                if(runtime.soundTimeoutSeconds <= 0)
                {
                    call(BATTLE_SEQUENCE_STOP_SOUND_CHANNEL2);
                    ready = true;
                }
            }
        }
        if(ready)
        {
            call(BATTLE_SEQUENCE_MARK_GAMEPLAY_ACTIVE);
            call(BATTLE_SEQUENCE_ROBBERY, 1);
            runtime.state = 102;
        }
        break;
    }
    case 102:
        if(call(BATTLE_SEQUENCE_SCENE_TRANSITION_COMPLETE))
        {
            call(BATTLE_SEQUENCE_STOP_SCENE_TRANSITION);
            runtime.state = 800;
        }
        break;
    case 300:
        if(runtime.selectedActionType == 3)
        {
            action(0, 0x30b); action(1, 0x30c);
            runtime.timerSeconds = 0.1f;
        }
        else if(runtime.selectedActionType == 4)
        {
            action(0, 0x3b1); action(1, 0x3b2);
            runtime.timerSeconds = 0.1f;
        }
        else if(runtime.selectedActionType == 5)
        {
            action(0, 0x3b5); action(1, 0x3b6);
            runtime.timerSeconds = 0.1f;
        }
        else
        {
            pair(actionMove(2));
            runtime.timerSeconds = 0.7f;
        }
        if(runtime.outcomeMode >= 0 && runtime.outcomeMode <= 3)
            call(BATTLE_SEQUENCE_DIALOGUE, 1, runtime.outcomeMode == 0 ||
                runtime.outcomeMode == 3 ? 0x12 : runtime.outcomeMode == 1 ? 0x13 : 0x14);
        else
            call(BATTLE_SEQUENCE_DIALOGUE, 1, 4);
        call(BATTLE_SEQUENCE_PARTY_LOAD_FLAG_CLEARED, -1, 1);
        runtime.state = 301;
        break;
    case 301:
        runtime.flowFlags |= 0x20u;
        runtime.state = 302;
        break;
    case 302:
        if((input.targetFlags & 8u) != 0)
        {
            action(1, 0x2bd);
            runtime.state = 303;
        }
        break;
    case 303:
        if((input.targetFlags & 8u) != 0)
            runtime.state = 304;
        break;
    case 304:
        if((runtime.flowFlags & 0x20u) != 0 && call(BATTLE_SEQUENCE_EVENT_MESSAGE_READY))
        {
            call(BATTLE_SEQUENCE_STOP_LETTERBOX);
            call(BATTLE_SEQUENCE_MARK_GAMEPLAY_ACTIVE);
            call(BATTLE_SEQUENCE_SET_OUTCOME_BYTE, -1, 4);
            call(BATTLE_SEQUENCE_EVENT_2BC_CLEARED, -1, 1);
            call(BATTLE_SEQUENCE_PAUSE_COUNTDOWN);
            return 2;
        }
        break;
    case 500:
        call(BATTLE_SEQUENCE_RECORD_SPARED_TARGET, 0, 1);
        call(BATTLE_SEQUENCE_DIALOGUE, 1, 5);
        call(BATTLE_SEQUENCE_SET_STATUS, 1, 0);
        action(0, 1, true, true); action(1, 0x75, false, true);
        call(BATTLE_SEQUENCE_CAMDAT, 1, 0x39);
        call(BATTLE_SEQUENCE_LOAD_FLAG_CLEARED, 0, 0);
        call(BATTLE_SEQUENCE_LOAD_FLAG_CLEARED, 1, 1);
        call(BATTLE_SEQUENCE_PARTY_LOAD_FLAG_CLEARED, -1, 0);
        call(BATTLE_SEQUENCE_PRIDE_VISIBLE, -1, 0);
        runtime.timerSeconds = 5.0f;
        runtime.state = 501;
        break;
    case 501:
        runtime.timerSeconds -= input.scaledDeltaSeconds;
        if(runtime.timerSeconds <= 0)
        {
            call(BATTLE_SEQUENCE_SET_FEEDBACK, -1, 0);
            ++runtime.state;
            return 1;
        }
        break;
    case 800:
        resources(2, BATTLE_FOLLOWUP_RESOURCES[0][input.relation]);
        runtime.state = 801;
        [[fallthrough]];
    case 801:
        if(!poll(2))
            break;
        runtime.state = 802;
        [[fallthrough]];
    case 802:
        call(BATTLE_SEQUENCE_DIALOGUE, 0, 3);
        call(BATTLE_SEQUENCE_REPOSITION, -1, 0);
        action(0, 0x170, false, true); action(1, 0x187, false, true);
        call(BATTLE_SEQUENCE_LOAD_FLAG_CLEARED, 0, 1);
        call(BATTLE_SEQUENCE_LOAD_FLAG_CLEARED, 1, 1);
        call(BATTLE_SEQUENCE_RETURN_WEAPON, 0);
        call(BATTLE_SEQUENCE_RETURN_WEAPON, 1);
        call(BATTLE_SEQUENCE_RESET_ATTACHMENTS);
        call(BATTLE_SEQUENCE_PARTY_LOAD_FLAG_CLEARED, -1, 0);
        call(BATTLE_SEQUENCE_PRIDE_VISIBLE, -1, 0);
        call(BATTLE_SEQUENCE_EVENT_2BC_CLEARED, -1, 0);
        runtime.timerSeconds = 5.0f;
        runtime.state = 803;
        break;
    case 803:
        call(BATTLE_SEQUENCE_SET_FEEDBACK, -1, 1);
        runtime.timerSeconds -= input.scaledDeltaSeconds;
        if(runtime.timerSeconds <= 0 || (input.targetFlags & 4u) == 0)
        {
            call(BATTLE_SEQUENCE_MARK_GAMEPLAY_ACTIVE);
            call(BATTLE_SEQUENCE_SET_FADE, -1, 3, 10, 0);
            runtime.state = 804;
        }
        break;
    case 804:
        if(call(BATTLE_SEQUENCE_FLOW_READY))
        {
            call(BATTLE_SEQUENCE_SET_FEEDBACK, -1, 0);
            call(BATTLE_SEQUENCE_STOP_LETTERBOX);
            action(0, 1, true); action(1, 1, true);
            call(BATTLE_SEQUENCE_RESET_CAMERA);
            runtime.state = 805;
            return 3;
        }
        break;
    case 805:
        return 3;
    case 1000:
        call(BATTLE_SEQUENCE_SETUP_ACTION_PAIR);
        runtime.timerSeconds = 0.8f;
        call(BATTLE_SEQUENCE_LOAD_FLAG_CLEARED, 0, 1);
        call(BATTLE_SEQUENCE_LOAD_FLAG_CLEARED, 1, 1);
        call(BATTLE_SEQUENCE_EVENT_2BC_CLEARED, -1, 0);
        call(BATTLE_SEQUENCE_PARTY_LOAD_FLAG_CLEARED, -1, 0);
        if(runtime.outcomeMode >= 0 && runtime.outcomeMode < 3)
            call(BATTLE_SEQUENCE_DIALOGUE, 0, runtime.outcomeMode);
        runtime.state = 1001;
        break;
    case 1001:
        runtime.timerSeconds -= input.scaledDeltaSeconds;
        if(runtime.timerSeconds <= 0 || input.attackerSelectedAction ==
           call(BATTLE_SEQUENCE_MAPPED_ACTION, 0, 0x19b))
        {
            runtime.delaySeconds = 3.0f;
            runtime.state = 1002;
        }
        break;
    case 1002:
        runtime.delaySeconds -= input.scaledDeltaSeconds;
        if(runtime.delaySeconds <= 0)
        {
            runtime.delaySeconds = 0;
            runtime.state = 1003;
        }
        break;
    case 1003:
        if((input.attackerFlags & 8u) != 0)
        {
            call(BATTLE_SEQUENCE_MARK_GAMEPLAY_ACTIVE);
            if((runtime.flags & 8u) != 0)
            {
                call(BATTLE_SEQUENCE_START_COUNTDOWN);
                runtime.state = 1100;
            }
            else
                runtime.state = 300;
        }
        break;
    case 1100:
        resources(3, runtime.outcomeMode == 1 ?
            BATTLE_FOLLOWUP_RESOURCES[1][input.relation] : runtime.outcomeMode == 0 ?
            BATTLE_FOLLOWUP_RESOURCES[2][input.relation] : 0);
        runtime.flowFlags &= ~0x4000u;
        runtime.state = 1101;
        [[fallthrough]];
    case 1101:
    {
        const bool loaded = poll(3);
        if((runtime.flowFlags & 0x4000u) != 0)
        {
            if(loaded)
            {
                runtime.flowFlags &= ~0x4000u;
                runtime.state = 1102;
            }
        }
        else
        {
            runtime.timerSeconds -= input.scaledDeltaSeconds;
            if(runtime.timerSeconds <= 0)
            {
                call(BATTLE_SEQUENCE_SET_FADE, -1, 3, 10, -1);
                runtime.timerSeconds = 0;
                runtime.flowFlags |= 0x4000u;
            }
        }
        break;
    }
    case 1102:
    {
        const int move = actionMove(1);
        if(move != 0)
        {
            action(0, move);
            const int linked = actionStateLinkedMoveId(move);
            action(1, linked == 0 ? 1 : linked, linked == 0);
        }
        runtime.timerSeconds = move == 0 ? 0 : runtime.selectedActionType == 4 ? 0.9f :
            runtime.selectedActionType == 2 ? 1.2f : runtime.selectedActionType == 1 ? 1.5f : 0.6f;
        runtime.state = 1103;
        break;
    }
    case 1103:
        if(call(BATTLE_SEQUENCE_FLOW_READY))
        {
            call(BATTLE_SEQUENCE_SET_FADE, -1, 4, 10, -1);
            call(BATTLE_SEQUENCE_REPOSITION, -1, 1);
            call(BATTLE_SEQUENCE_WEAPON_VISIBLE, 0, 0);
            call(BATTLE_SEQUENCE_RETURN_WEAPON, 0);
            call(BATTLE_SEQUENCE_RESET_ATTACHMENTS);
            call(BATTLE_SEQUENCE_EVENT_2BC_CLEARED, -1, 0);
            if(runtime.outcomeMode == 2)
            {
                runtime.state = 500;
                call(BATTLE_SEQUENCE_SCORE_MULTIPLIER, -1, 0, 0, 0, 1.0f);
            }
            else if(runtime.outcomeMode == 1)
            {
                call(BATTLE_SEQUENCE_SET_STATUS, 1, 2);
                runtime.state = 100;
            }
            else if(runtime.outcomeMode == 0)
                runtime.state = 500;
        }
        break;
    case 1150:
        runtime.state = !input.condition16 && input.targetHasPride ? 1350 : 1300;
        break;
    case 1200:
        runtime.state = !input.condition16 && input.targetHasPride ? 1550 : 1500;
        break;
    case 1250:
        result = 5;
        break;
    case 1300:
        if(call(BATTLE_SEQUENCE_SPECIAL_INIT)) runtime.state = 1301;
        break;
    case 1301:
        if(call(BATTLE_SEQUENCE_SPECIAL_UPDATE)) runtime.state = 1302;
        break;
    case 1302:
        call(BATTLE_SEQUENCE_SET_STATUS, 1, 1);
        call(BATTLE_SEQUENCE_SET_CHARACTER_VALUE0, 1, 1);
        runtime.state = 800;
        break;
    case 1350:
    case 1550:
        runtime.selectedActionType = 1;
        runtime.state = 300;
        break;
    case 1500:
        call(BATTLE_SEQUENCE_SELECT_FINISHER);
        if(runtime.finisher.variant >= 2 && runtime.finisher.variant < 16)
        {
            runtime.finisherStep = 0;
            runtime.finisherFlags = 0;
            runtime.finisherElapsed = 0;
            runtime.state = 1501;
        }
        break;
    case 1501:
        if(updateBattleFinisherSequence(runtime, input, callbacks))
            runtime.state = 1502;
        break;
    case 1502:
        if(input.attackerNativeSlot == 0)
        {
            call(BATTLE_SEQUENCE_RECORD_FINISHER, 0, 1);
            call(BATTLE_SEQUENCE_SET_STATUS, 1, 3);
            result = 9;
        }
        else result = 11;
        break;
    case 1600:
        runtime.state = 1601;
        break;
    case 1601:
        runtime.state = 800;
        break;
    case 1700:
        call(BATTLE_SEQUENCE_DIALOGUE, 1, 6);
        action(0, 1, true, true); action(1, 0x186, false, true);
        call(BATTLE_SEQUENCE_CAMDAT, 1, 0x39);
        call(BATTLE_SEQUENCE_LOAD_FLAG_CLEARED, 0, 0);
        call(BATTLE_SEQUENCE_LOAD_FLAG_CLEARED, 1, 1);
        call(BATTLE_SEQUENCE_RELEASE_ATTACHMENT, 0);
        call(BATTLE_SEQUENCE_EVENT_2BC_CLEARED, -1, 0);
        call(BATTLE_SEQUENCE_PARTY_LOAD_FLAG_CLEARED, -1, 0);
        runtime.timerSeconds = 3.0f;
        call(BATTLE_SEQUENCE_INIT_CONFIRM_CHOICE);
        runtime.elapsedSeconds = 0;
        runtime.state = 1701;
        break;
    case 1701:
        call(BATTLE_SEQUENCE_LOAD_FLAG_CLEARED, 0, 0);
        call(BATTLE_SEQUENCE_SET_FEEDBACK, -1, 0);
        if(runtime.elapsedSeconds > 2.0f)
            call(BATTLE_SEQUENCE_UPDATE_CHOICE);
        runtime.timerSeconds -= input.scaledDeltaSeconds;
        if(runtime.timerSeconds <= 0 && runtime.choiceResult != -1)
            runtime.state = 1702;
        break;
    case 1702:
        call(BATTLE_SEQUENCE_FINALIZE_CHOICE);
        if(runtime.choiceResult == 0)
            runtime.state = 1725;
        else
        {
            call(BATTLE_SEQUENCE_EVENT_2BC_CLEARED, -1, 1);
            call(BATTLE_SEQUENCE_PARTY_LOAD_FLAG_CLEARED, -1, 1);
            runtime.state = 1750;
        }
        break;
    case 1725:
        call(BATTLE_SEQUENCE_MARK_GAMEPLAY_ACTIVE);
        call(BATTLE_SEQUENCE_PRIDE_VISIBLE, -1, 0);
        call(BATTLE_SEQUENCE_START_SCENE_TRANSITION, -1, 0x96, 0x50, 1);
        runtime.state = 1726;
        break;
    case 1726:
        if(call(BATTLE_SEQUENCE_SCENE_TRANSITION_COMPLETE))
        {
            call(BATTLE_SEQUENCE_STOP_SCENE_TRANSITION);
            call(BATTLE_SEQUENCE_SET_FADE, -1, 3, 7, -1);
            runtime.state = 1727;
        }
        break;
    case 1727:
        if(call(BATTLE_SEQUENCE_FLOW_READY))
        {
            call(BATTLE_SEQUENCE_SET_OUTCOME_BYTE, -1, 1);
            call(BATTLE_SEQUENCE_SET_STATUS, 1, 4);
            result = 7;
        }
        break;
    case 1750:
        result = 8;
        break;
    case 1800:
        if(call(BATTLE_SEQUENCE_CINEMATIC_INIT)) runtime.state = 1801;
        break;
    case 1801:
        if(call(BATTLE_SEQUENCE_CINEMATIC_UPDATE) == 1)
        {
            call(BATTLE_SEQUENCE_CINEMATIC_DESTROY);
            runtime.state = 1802;
        }
        break;
    case 1802:
        result = 10;
        break;
    default:
        break;
    }
    if(result == 0)
    {
        call(BATTLE_SEQUENCE_ACTOR_ACTION_FLAGS, 0, 4, 1);
        call(BATTLE_SEQUENCE_ACTOR_ACTION_FLAGS, 1, 4, 1);
        runtime.elapsedSeconds += input.scaledDeltaSeconds;
    }
    else
    {
        call(BATTLE_SEQUENCE_SET_FEEDBACK, -1, 0);
        call(BATTLE_SEQUENCE_STOP_LETTERBOX);
        call(BATTLE_SEQUENCE_MARK_GAMEPLAY_ACTIVE);
        if(result != 5)
            call(BATTLE_SEQUENCE_RESET_CAMERA);
    }
    return result;
}
