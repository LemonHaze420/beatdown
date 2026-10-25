#include "game/game.h"
#include "game/combat/links.h"
#include "game/combat/input.h"
#include "action_state_handlers.h"
#include <algorithm>
#include <math.h>

void Game::initializeCombatRecovery(GameSlotRecord& slot)
{

    const int damage = std::clamp(static_cast<int>(slot.receivedDamage), 500, 1500);
    slot.combatRecoverySeconds =
        (1.0f - static_cast<float>(1000 - (damage - 500)) / 1000.0f) * 3.0f;
    slot.combatRecoveryAttack = 0;
    slot.combatRecoveryOther = 0;
    slot.comboStep = 0;
    slot.inputHold.accumulatedPresses = 0;
    combatInputHoldSuccessCount = 0;
    slot.damageGauge = 0;
}

int Game::selectCombatRecoveryMove(const GameSlotRecord& slot) const
{

    bool grounded;
    if(!combatGroundedState(slot, grounded))
        return -1;
    if(slot.currentHealth <= 0)
        return stanceMotion(slot, grounded ? 111 : 112);

    const uint32_t held = combatRecoveryHeldInput(slot);
    bool forward = (held & 4u) != 0;
    bool backward = !forward && (held & 8u) != 0;
    if((slot.facingRelation == 2) == ((slot.facingState & 8u) != 0))
        std::swap(forward, backward);
    int selector = forward ? 1 : (backward ? 2 : 0);
    if(selector == 0 && slot.combatRecoveryOther == 0 &&
       slot.combatRecoveryAttack == 1)
        selector = 3;
    if(!grounded && selector == 3)
    {
        const int personal = movePersonalActionForStyle(
            combatStyle(slot), 0x5e, combatPersonalActionContext(slot));
        if(personal > 0)
            return personal;
    }
    return stanceMotion(slot, COMBAT_RECOVERY_REMAPS[grounded ? 1 : 0][selector]);
}

void Game::updateCombatRecovery(GameSlotRecord& slot)
{

    const uint32_t pressed = sampleCombatRecoveryInput(slot);
    unsigned presses = 0;
    for(uint32_t mask = 1; mask <= 8; mask <<= 1)
        if((pressed & mask) != 0)
            ++presses;
    if((pressed & MOVE_INPUT_SLOT_MASKS[1]) != 0)
    {
        presses += 2;
        slot.combatRecoveryAttack = 1;
    }
    if((combatRecoveryHeldInput(slot) & MOVE_INPUT_SLOT_MASKS[1]) != 0)
        slot.combatRecoveryAttack = 1;
    const uint32_t otherMasks = MOVE_INPUT_SLOT_MASKS[0] |
        MOVE_INPUT_SLOT_MASKS[2] | MOVE_INPUT_SLOT_MASKS[6] |
        MOVE_INPUT_SLOT_MASKS[3];
    for(uint32_t bits = pressed & otherMasks; bits != 0; bits >>= 1)
        if((bits & 1u) != 0)
        {
            presses += 2;
            slot.combatRecoveryOther = 1;
        }
    slot.combatRecoverySeconds -= static_cast<float>(presses) * 0.5f;
    slot.combatRecoverySeconds -= scaledFrameDeltaSeconds;
    if(slot.combatRecoverySeconds > 0.0f)
        return;
    slot.combatRecoverySeconds = 0.0f;
    const int recovery = selectCombatRecoveryMove(slot);

    if(recovery < 0 || moveRecord(recovery == 0 ? 1 : recovery) == NULL)
        return;
    slot.motionControlFlags &= ~0x80u;
    slot.statePhase = 3;
    startCombatMove(slot, recovery == 0 ? 1 : recovery, slot.action);
    runCombatStateTask(slot);
}

uint32_t Game::combatRecoveryHeldInput(const GameSlotRecord& slot) const
{
    if(slot.playerIndex >= 2)
        return slot.cpuInput.canonicalButtons;
    const uint32_t held = slot.playerIndex == 0 ?
        heldInput | pressedInput : secondHeldInput | secondPressedInput;
    return combatButtonCode(held) | combatDirectionCode(held);
}

void Game::updateCombatSpin(GameSlotRecord& slot)
{

    if(slot.spinPhase != 2)
        return;
    GameSlotRecord* const partner =
        mutableCombatSlotByIndex(slot.linkedSlotIndex);
    bool grounded;
    if(partner == NULL || !combatGroundedState(*partner, grounded))
        return;
    const uint32_t pressed = sampleCombatRecoveryInput(slot);
    unsigned count = 0;
    for(uint32_t bits = pressed; bits != 0; bits >>= 1)
        count += bits & 1u;
    slot.spinPresses += static_cast<float>(count);
    const int presses = std::clamp(static_cast<int>(slot.spinPresses), 0, 200);
    const float progress =
        (1.0f - static_cast<float>(200 - presses) / 200.0f) * 0.4f;
    slot.animationTimeScale = progress + 1.0f;
    partner->animationTimeScale = slot.animationTimeScale;
    slot.spinPresses -= 0.03f;
    if(slot.spinPresses < 0.0f)
        slot.spinPresses = 0.0f;
    const float direction = (slot.headingDegrees + 180.0f) *
        0.01745329251994329577f;
    const float distance = progress + 0.18f;
    slot.position.x += sinf(direction) * distance;
    slot.position.z += cosf(direction) * distance;

    CombatMotionRuntime& motion = partner->combatMotion;
    const CombatMotionRuntime previousMotion = motion;
    motion.offset = { 0.17f, 0.0f, 9.8f };
    motion.headingDelta = grounded ? 0.0f : 180.0f;
    motion.frames = 2;
    if(setCombatMovementTarget(*partner, slot))
        updateCombatMovement(*partner);
    else
        motion = previousMotion;

    const uint32_t held = combatRecoveryHeldInput(slot);
    bool left = (held & 4u) != 0;
    bool right = !left && (held & 8u) != 0;
    if(slot.facingRelation == 1)
        std::swap(left, right);
    if(left)
        slot.headingDegrees -= 0.3f;
    else if(right)
        slot.headingDegrees += 0.3f;

    if((sampleCombatRecoveryInput(*partner) & 0xfbc0u) != 0)
    {
        const int descriptor = partner->inputHold.followUpMove;
        const int escape = descriptor == 0 ? 1 : descriptor;
        if(moveRecord(escape) != NULL)
        {
            startCombatMove(*partner, escape, partner->action);
            runCombatStateTask(*partner);
            const int paired = actionStateLinkedMoveId(descriptor);
            startCombatMove(slot, paired == 0 ? 1 : paired, slot.action);
            runCombatStateTask(slot);
        }
    }
    else if((held & 0x1000u) == 0)
    {
        const int release = stanceMotion(slot, grounded ? 0xd4 : 0x1be);
        if(release != slot.selectedActionId && moveRecord(release) != NULL)
        {
            startCombatMove(slot, release, slot.action);
            runCombatStateTask(slot);
        }
    }
}

void Game::updateCombatLandingDamage(GameSlotRecord& slot)
{

    if((slot.animationCueFlags & 2u) != 0 && stageNavigation.valid &&
       stageNavigation.groundColumn != NULL)
    {
        float height;
        uint16_t material;
        if(stageNavigation.groundColumn(
               stageNavigation.context, slot.position.x, slot.position.z,
               slot.position.y, &height, &material) &&
           slot.position.y - height < 5.0f)
        {
            slot.groundHeight = height;
            slot.groundHeightValid = true;
            const uint16_t stage = selectedStageId();
            if(stage < COMBAT_SURFACE_STAGE_COUNT &&
               material < COMBAT_SURFACE_MATERIAL_COUNT)
            {
                const uint16_t property =
                    COMBAT_SURFACE_PROPERTIES[stage][material];

                if((property & 0x2000u) != 0 &&
                   (property & 0x3fu) == 0x1bu)
                    for(uint8_t record = 0x25; record <= 0x27; ++record)
                    {
                        GameEstEvent event = {};
                        event.recordIndex = record;
                        event.actorSlot = slot.slotIndex;
                        event.placement = GAME_EST_ACTOR_POSITION;
                        event.position = slot.position;
                        event.rawClassifierId = -1;
                        enqueueEstEvent(event);
                    }
            }
            reduceGameSlotHealth(slot, 9999);
            queueCombatPair(slot, slot.currentHealth > 0 ? 0x305 : 0x2e, 0);
        }
    }
    slot.actorStatusFlags |= 0x400u;
}
