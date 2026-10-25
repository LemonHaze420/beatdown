#include "game.h"
#include "game_combat_input.h"
#include "game_internal.h"
#include "game_input_directions.h"
#include "action_state_handlers.h"

using namespace game_detail;

void Game::configureCombatInputHold(GameSlotRecord& slot, int opcode,
                                    int16_t value)
{
    switch(opcode)
    {
    case MOVE_BEHAVIOUR_ARM_HOLD_COMBO:

        if(slot.comboStep == 0)
            slot.comboStep = 1;
        if(combatInputHoldSuccessCount > 1)
            slot.comboStep = 0;
        break;
    case MOVE_BEHAVIOUR_HOLD_END_FRAME:
        slot.inputHold.endFrame = static_cast<uint16_t>(value);
        break;
    case MOVE_BEHAVIOUR_HOLD_FOLLOWUP:
        slot.queuedMoveId = value;
        slot.inputHold.followUpMove = value;
        break;
    case MOVE_BEHAVIOUR_HOLD_THRESHOLD:
        slot.inputHold.threshold = static_cast<uint16_t>(value);
        slot.inputHold.kind = 1;
        break;
    case MOVE_BEHAVIOUR_HOLD_KIND:
        slot.inputHold.kind = static_cast<uint8_t>(value);
        break;
    default:
        break;
    }
}

uint32_t Game::sampleCombatHeldInput(const GameSlotRecord& slot) const
{
    if(slot.playerIndex >= 2)
        return slot.playerIndex < STAGE_AI_CONTEXT_COUNT ?
            slot.cpuInput.canonicalButtons : 0;
    const bool first = slot.playerIndex == 0;
    const uint32_t buttons = first ? heldInput | pressedInput :
        secondHeldInput | secondPressedInput;
    return combatStickDirectionCode(
        first ? playerMovementX : secondPlayerMovementX,
        first ? playerMovementZ : secondPlayerMovementZ,
        first ? playerLookX : secondPlayerLookX,
        first ? playerLookZ : secondPlayerLookZ,
        combatButtonCode(buttons) | combatDirectionCode(buttons));
}

uint32_t Game::sampleCombatRecoveryInput(GameSlotRecord& slot)
{
    if(slot.playerIndex < 2)
    {
        const uint32_t pressed = slot.playerIndex == 0 ?
            pressedInput : secondPressedInput;
        return combatButtonCode(pressed) | combatDirectionCode(pressed);
    }
    if(slot.playerIndex >= STAGE_AI_CONTEXT_COUNT)
        return 0;
    return slot.cpuInput.canonicalPressedButtons;
}

void Game::updateCombatInputHold(GameSlotRecord& slot, uint32_t pressed)
{

    if(slot.statePhase == 4)
        return;
    if(slot.statePhase == 3)
    {
        slot.actorTargetingFlags &= ~0x8000u;
        slot.statePhase = slot.inputHold.endFrame != 0 ? 2 : 4;
        return;
    }
    if(slot.statePhase == 1)
    {
        setCombatActorMotion(slot, slot.selectedActionId);
        slot.appliedMotionId = slot.selectedActionId;
        slot.animationCueFlags = 0;
        slot.actorTargetingFlags |= 0x10u;
        slot.statePhase = 2;
    }
    if(slot.statePhase != 2 || slot.inputHold.endFrame == 0)
        return;

    switch(slot.inputHold.kind)
    {
    case 1:
    {
        unsigned buttons = 0;
        unsigned directions = 0;
        for(uint32_t mask = 0x40u; mask <= 0x2000u; mask <<= 1)
            if((pressed & mask) != 0)
                ++buttons;
        for(uint32_t mask = 1u; mask <= 8u; mask <<= 1)
            if((pressed & mask) != 0)
                ++directions;

        slot.inputHold.accumulatedPresses = static_cast<uint16_t>(
            slot.inputHold.accumulatedPresses + buttons + directions *
            (slot.playerIndex < 2 ? 2u : 1u));
        if(slot.inputHold.accumulatedPresses >= slot.inputHold.threshold)
            slot.inputHold.flags |= 2u;
        break;
    }
    case 2:
    case 3:
    {
        const uint32_t success = slot.inputHold.kind == 2 ?
            MOVE_INPUT_SLOT_MASKS[0] : MOVE_INPUT_SLOT_MASKS[1];
        const uint32_t failure = slot.inputHold.kind == 2 ?
            MOVE_INPUT_SLOT_MASKS[1] : MOVE_INPUT_SLOT_MASKS[0];
        if((pressed & success) != 0)
            slot.inputHold.flags |= 2u;
        if((pressed & failure) != 0)
            slot.inputHold.flags |= 4u;
        break;
    }
    default:
        return;
    }
    slot.inputHold.flags |= 1u;
    slot.actorTargetingFlags |= 0x8000u;
    if(slot.playingAnimationFrame < static_cast<float>(slot.inputHold.endFrame))
        return;

    ++slot.statePhase;
    bool success = (slot.inputHold.flags & 6u) == 2u;
    if(!success)
    {
        const MoveRecord* const move = moveRecord(slot.selectedActionId);
        const uint32_t flags = move != NULL ? move->field_0x38 : 0;
        const bool inverted = (flags & 0x80000000u) != 0;
        success = (flags & 1u) != 0 &&
            (((flags & 0x10000u) != 0 &&
              ((slot.flags & 0x200u) != 0) == inverted) ||
             ((flags & 0x20000u) != 0 &&
              ((slot.flags & 0x400u) != 0) == inverted));
    }
    if(success)
    {
        ++combatInputHoldSuccessCount;
        int next = resolveQueuedMotion(slot, slot.queuedMoveId);
        if(next == 0)
            next = 1;
        if(moveRecord(next) != NULL)
        {
            startCombatMove(slot, next, slot.action);
            runCombatStateTask(slot);
            GameSlotRecord* const linked =
                mutableCombatSlotByIndex(slot.linkedSlotIndex);
            int partnerMove = actionStateLinkedMoveId(next);
            if(partnerMove == 0)
                partnerMove = 1;
            if(linked != NULL && moveRecord(partnerMove) != NULL)
            {
                startCombatMove(*linked, partnerMove, linked->action);
                runCombatStateTask(*linked);
            }
        }
    }

    slot.queuedMoveId = 0;
    slot.inputHold.endFrame = 0;
    slot.inputHold.kind = 0;
    slot.inputHold.followUpMove = 0;
    slot.inputHold.flags = 0;
}
