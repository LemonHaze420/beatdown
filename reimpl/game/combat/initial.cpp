#include "game/game.h"
#include "game/combat/input.h"
#include "game/internal.h"
#include "action_state_handlers.h"

bool Game::combatantActive(const GameSlotRecord& actor) const
{

    const GameSlotRecord* target = combatSlotByIndex(actor.linkedSlotIndex);
    return activeOpponentCount > 0 && target != nullptr && target != &actor &&
        (target->flags & 1u) != 0 &&
        (currentMode != GAME_MODE_STORY || persistent.story.battleActive != 0);
}

int Game::dispatchCombatInput(GameSlotRecord& actor, int command)
{

    if(command < 0 || command >= MOVE_ACTION_COUNT)
        return -1;
    bool direct = false;
    int move = -1;
    int rawAction = 0;
    if(command == 0x4c || command == 0x4d)
    {
        move = combatOwnedWeaponInputMove(actor, command);
        direct = true;
    }
    else
    {
        move = combatWeaponInputMove(actor, command, direct);
        if(!direct)
        {
            move = movePersonalActionForStyle(combatStyle(actor), command,
                combatPersonalActionContext(actor));
            direct = move > 0;
        }
    }
    if(!direct)
    {
        rawAction = moveForAction(command);
        move = rawAction > 0 ? stanceMotion(actor, rawAction) : -1;
        if(move <= 0 && command != 0x46)
            return -1;
        if((actor.actorActionFlags & 2u) != 0 && command != 5 && command != 0x13 &&
           command != 0x40 && (moveRecord(move) == nullptr ||
                              (moveRecord(move)->behaviorFlags & 0x400u) == 0))
            return -1;
        actor.motionControlFlags &= ~0x300u;

        if(command >= 0x40 && command <= 0x46)
            return -1;
        if(command == 1 && ((persistent.story.gameStateFlags & 1u) == 0 ||
            (persistent.story.battleFlags & BATTLE_REWARD_INTERACTION_FLAG) != 0))
            return -1;
        if(command >= 0x47 && command <= 0x4b)
        {
            if(!combatGuardAllowed(actor))
                return -1;
        }
    }
    const int preparedMove = prepareCombatInputAction(actor, command, move);
    if(command == 0x1e && preparedMove != move)
        direct = true;
    move = preparedMove;
    const MoveRecord* record = moveRecord(move);
    if(record == nullptr || move <= 0)
        return -1;

    actor.combatCommand.actions[0] = static_cast<int16_t>(command);
    actor.combatCommand.actionCount = 1;
    actor.combatCommand.command = static_cast<int16_t>(command);
    CombatAction presentation = COMBAT_ACTION_LIGHT_ATTACK;
    if((record->behaviorFlags & 0x400u) != 0)
        presentation = COMBAT_ACTION_GUARD;
    else if(command >= 0x47 && command < 0x4b)
        presentation = COMBAT_ACTION_DODGE;
    else if((record->behaviorFlags & 4u) != 0 || (command >= 0x29 && command <= 0x33))
        presentation = COMBAT_ACTION_GRAB;
    else if((command > 0 && command <= 0x0f) ||
            (command >= 0x3c && command <= 0x3f) || command == 0x59 || command == 0x5c ||
            (command >= 0x74 && command <= 0x7a && (command & 1) == 0) ||
            (command >= 0x7c && command <= 0x7f))
        presentation = COMBAT_ACTION_HEAVY_ATTACK;

    const GameSlotRecord* target = combatSlotByIndex(actor.linkedSlotIndex);
    const int targetMove = target != nullptr && rawAction > 0 ?
        stanceMotion(*target, rawAction) : -1;
    const MoveRecord* targetRecord = moveRecord(targetMove);
    const bool delayed = targetRecord != nullptr && targetRecord->handler == COMBAT_MOVE_DELAYED_POSE_HANDLER;
    if(!direct && actor.facingTarget == 0 && rawAction != 99 && !delayed &&
       (actor.poseStateWord & 8u) == 0 && command != 0x4e &&
       !actor.combatCommand.preserveInputFacing)
    {

        CombatActionPairRecord& pair = combatActionPairs[actor.slotIndex];
        pair.selfStateIndex = 0;
        pair.linkedStateIndex = record->linkedMoveId < 0 ? -record->linkedMoveId : 0;
        pair.selfStateValue = static_cast<int16_t>(move);
        pair.linkedStateValue = target != nullptr ? static_cast<int16_t>(
            game_detail::resolveQueuedMotion(*target, record->linkedMoveId)) : 0;
        queueCombatPair(actor, -0x1e, 0);
    }
    else
        queueCombatPair(actor, move, record->linkedMoveId);
    CombatActionPairRecord& pair = combatActionPairs[actor.slotIndex];
    pair.inputPresentation = presentation;
    pair.inputPresentationValid = true;
    return move;
}
