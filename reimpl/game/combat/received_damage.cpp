#include "game/game.h"
#include "game/combat/gauge.h"
#include "game/combat/input.h"
#include "game/combat/links.h"
#include "game/internal.h"
#include "action_state_handlers.h"
#include "move_tables.h"
#include <math.h>
#include <string.h>

using namespace game_detail;

void clearCombatReceivedHitContext(CombatReceivedHitContext& context)
{

    memset(&context, 0, sizeof(context));
    context.ownerSlot = -1;
    context.reactionFamily = -1;
    context.unknown18 = -1;
    context.unknown1c = -1;
}

CombatReceivedReaction selectCombatReceivedReaction(
    const CombatReceivedReactionInput& input)
{

    const CombatReceivedHitContext& hit = input.hit;
    CombatReceivedReaction result = {};
    result.actionRemap = -1;
    result.physicalOverride = (input.physicalState & 4u) != 0;
    switch(input.facing)
    {
    case 2: result.direction = 3; result.column = 2; break;
    case 1: result.direction = 2; result.column = 3; break;
    case 8: result.direction = 1; result.column = 0; break;
    case 4: result.direction = 0; result.column = 1; break;
    default: break;
    }
    result.defeat = !input.alive ||
        (!result.physicalOverride && (hit.flagsC & 0x20u) != 0);
    if(result.defeat)
    {
        result.reaction = input.alive ? 0 : result.physicalOverride ? 2 : 1;
        if(!input.alive && result.physicalOverride)
            result.column = input.grounded ? 2 : 0;
        return result;
    }
    if((hit.flagsC & 0x40u) != 0 && (input.physicalState & 0x20u) == 0)
    {
        if(input.facing == 8)
            result.actionRemap = 240;
        else if((hit.flagsB & 2u) != 0)
            result.actionRemap = 80;
        else if((hit.flagsB & 4u) != 0)
            result.actionRemap = 238;
        else if((hit.flagsB & 8u) != 0)
            result.actionRemap = 239;
        if(result.actionRemap >= 0)
            return result;
    }
    result.family = hit.reactionFamily == -1 ? 0 : hit.reactionFamily;
    if(result.physicalOverride)
    {
        result.reaction = input.grounded ? 10 : 9;
        if((hit.flagsA & 4u) != 0)
            result.direction = 0;
        else if(result.direction == 0 || result.direction == 1)
        {
            if((hit.flagsA & 0x10u) != 0)
                result.direction = result.direction == 0 ? 3 : 2;
            else if((hit.flagsA & 0x20u) != 0)
                result.direction = result.direction == 0 ? 2 : 3;
        }
    }
    else if((hit.flagsC & 0x800u) != 0) result.reaction = 20;
    else if((hit.flagsC & 0x1000u) != 0) result.reaction = 21;
    else if((hit.flagsC & 0x100u) != 0) result.reaction = 14;
    else if((hit.flagsC & 0x10u) != 0) result.reaction = 8;
    else if((hit.flagsC & 0x600u) != 0)
    {
        result.reaction = (hit.flagsC & 0x200u) != 0 ? 13 : 15;
        if(result.direction == 0 || result.direction == 1)
        {
            if((hit.flagsA & 0x10u) != 0)
                result.direction = result.direction == 0 ? 3 : 2;
            else if((hit.flagsA & 0x20u) != 0)
                result.direction = result.direction == 0 ? 2 : 3;
        }
    }
    else if((hit.hitFlags & 2u) != 0 &&
            input.weaponBehavior != 0 && input.weaponBehavior != 1 &&
            input.weaponBehavior != 2 && input.weaponBehavior != 10 &&
            input.weaponBehavior != 11 && input.weaponBehavior != 14)
        result.reaction = input.weaponBehavior == 13 ? 19 : 7;
    else if((input.physicalState & 2u) != 0 && (hit.flagsB & 0xcu) != 0)
        result.reaction = 6;
    else if(input.linkedCombo == input.linkedComboLimit ||
            (input.linkedTargetFlags & 0x400000u) != 0)
        result.reaction = 4;
    else if((input.physicalState & 0x20u) != 0) result.reaction = 5;
    else if((hit.flagsB & 4u) != 0) result.reaction = 3;
    else if((hit.flagsB & 8u) != 0) result.reaction = 11;
    else if((hit.flagsA & 0x10u) != 0) result.reaction = 1;
    else if((hit.flagsA & 0x20u) != 0) result.reaction = 2;
    else if((hit.flagsA & 8u) != 0) result.reaction = 16;
    else if((hit.flagsA & 4u) != 0) result.reaction = 17;

    switch(result.direction)
    {
    case 0: result.column = 1; break;
    case 1: result.column = 0; break;
    case 2: result.column = 3; break;
    case 3: result.column = 2; break;
    }
    return result;
}

void Game::applyPendingCombatReceivedDamage(GameSlotRecord& victim)
{
    if(!victim.pendingReceivedDamage || victim.statePhase != 1 ||
       scaledFrameDeltaSeconds == 0.0f)
        return;
    GameSlotRecord* attacker = mutableCombatSlotByIndex(victim.linkedSlotIndex);

    if(attacker == nullptr)
        return;
    CombatReceivedReactionInput input = {};
    input.hit = victim.receivedContext;
    input.physicalState = victim.impactSelectorState;
    input.facing = victim.facingState;
    input.linkedCombo = attacker->comboStep;
    input.linkedTargetFlags = attacker->actorTargetingFlags;
    const PersistentGameRecord* record = characterRecord(
        static_cast<uint16_t>(attacker->persistentRecordId));
    input.linkedComboLimit = record != nullptr ? record->field_0x384 + 1 : -1;
    input.weaponBehavior = 18;
    const WeaponInstance* held = weaponManager.equippedBy(attacker->slotIndex);
    const WeaponDefinition* definition = held != nullptr ?
        weaponManager.definition(held->objectId) : nullptr;
    if((attacker->actorActionFlags & 0x4000u) != 0 && definition != nullptr)
        input.weaponBehavior = definition->behaviorId;
    int16_t predictedHealth = victim.currentHealth;
    if(combatDamageEnabled &&
       (victim.motionControlFlags & GAME_MOTION_INHIBIT_HEALTH_WRITE) == 0)
    {
        predictedHealth = static_cast<int16_t>(
            predictedHealth - victim.receivedDamage);
        if(predictedHealth < 1)
            predictedHealth = 0;
    }
    input.alive = predictedHealth > 0;
    CombatReceivedReaction selected = selectCombatReceivedReaction(input);
    if((input.physicalState & 4u) != 0 && selected.actionRemap < 0)
    {
        if(!combatGroundedState(victim, input.grounded))
            return;
        selected = selectCombatReceivedReaction(input);
    }
    const MoveReaction* reaction = selected.defeat ?
        &MOVE_DEFEAT_REACTIONS[selected.reaction][selected.column] :
        moveReaction(selected.family, selected.reaction, selected.column);
    if(selected.actionRemap < 0 && reaction == nullptr)
        return;

    victim.pendingReceivedDamage = false;
    victim.receivedDamageTaskActive = true;

    tryCombatDamageWeaponDrop(victim);
    if((victim.actorActionFlags & 0x4000u) != 0)
        updateCombatWeaponBehaviour(victim, MOVE_BEHAVIOUR_WEAPON_HAND, 0);
    if(combatDamageEnabled)
    {
        victim.damageGauge = static_cast<int16_t>(
            victim.damageGauge + victim.receivedDamage);
        if(victim.damageGauge > victim.maxHealth)
            victim.damageGauge = victim.maxHealth;
        reduceGameSlotHealth(victim, victim.receivedDamage);
        applyPrideDamage(victim, *attacker, static_cast<int16_t>(input.hit.pride));
        attacker->participantScore += scaledParticipantBattleScore(victim.receivedDamage);
        victim.hitsTakenCount = static_cast<uint16_t>(victim.hitsTakenCount + 1);
    }
    victim.comboStep = 0;
    victim.actorActionFlags &= 0xffffff07u;
    if((input.physicalState & 4u) == 0)
    {
        int region = -1;
        int16_t bodyDamage = static_cast<int16_t>(
            static_cast<int>(static_cast<float>(victim.receivedDamage) * 0.5f));
        if((input.hit.flagsB & 2u) != 0)
            region = 0;
        else if((input.hit.flagsB & 4u) != 0)
        {
            if((input.physicalState & 1u) != 0)
                region = input.facing == 8 ? 3 : 1;
            else if((input.physicalState & 2u) != 0)
                region = 0;
        }
        else if((input.hit.flagsB & 8u) != 0)
            region = 2;
        else
            bodyDamage = 0;
        if(bodyDamage == 0 && victim.receivedDamage != 0)
            bodyDamage = 1;
        if(region >= 0 && bodyDamage != 0)
        {
            victim.bodyPartHealth[region] = static_cast<int16_t>(
                victim.bodyPartHealth[region] - bodyDamage);
            victim.bodyPartDamage[region] = static_cast<int16_t>(
                victim.bodyPartDamage[region] + bodyDamage);
            recordCombatBodyGaugeDamage(victim, region, bodyDamage);
            if(victim.bodyPartHealth[region] < 1 &&
               (input.physicalState & 0x20u) == 0 &&
               (input.hit.flagsC & 0x1910u) == 0)
                victim.bodyPartHealth[region] = 0;
        }
    }
    int contactReaction = 0;
    bool heldBack = false;
    if(selected.defeat)
    {
        forceDropCombatWeapon(victim, true);
        victim.damageGauge = 0;
        victim.actorBehaviorFlags = 0x200u;
        victim.statePhase = 99;
        if(!input.alive)
        {
            victim.motionControlFlags |= 0x800000u;
            victim.actorActionFlags |= 4u;
            if((persistent.story.battleFlags & BATTLE_REWARD_INTERACTION_FLAG) == 0)
                attacker->actorActionFlags |= 4u;
        }
    }
    else if(selected.actionRemap < 0)
    {
        contactReaction = selected.reaction;
        if(!selected.physicalOverride &&
           (selected.reaction == 8 || selected.reaction == 13 ||
            selected.reaction == 14 || selected.reaction == 15 ||
            selected.reaction == 20 || selected.reaction == 21))
            forceDropCombatWeapon(victim, true);
        if(!selected.physicalOverride && selected.reaction != 8 &&
           selected.reaction != 21 && selected.reaction != 14 &&
           selected.reaction != 20 && selected.reaction != 6 &&
           selected.reaction != 11 && stageNavigation.combatSurface != nullptr)
        {
            GameCombatSurfaceContact surface = {};
            if(stageNavigation.combatSurface(stageNavigation.combatSurfaceContext,
                                             victim.position, 11.0f, &surface))
            {
                victim.combatSurfaceContact = surface;
                victim.combatSurfaceValid = true;
                float delta = normalizeCombatDegrees(
                    atan2f(surface.normal.x, surface.normal.z) * 57.29578f -
                    normalizeCombatDegrees(attacker->headingDegrees + 180.0f));
                if(delta > 180.0f)
                    delta -= 360.0f;
                if(fabsf(delta) <= 60.0f)
                {
                    reaction = &MOVE_BEHIND_REACTIONS[
                        victim.currentHealth <= (victim.maxHealth >> 1) ? 1 : 0]
                        [selected.column];
                    contactReaction = 12;
                }
            }
        }
        if((combatRecoveryHeldInput(victim) & 0xfu) != 0)
        {
            if(input.facing == 8)
                heldBack = true;
            else if(input.facing != 4)
            {
                victim.combatMotion.turnDuration = 5.0f / 60.0f;
                victim.combatMotion.elapsed = 0;
                victim.combatMotion.turnBone = 1;
            }
        }
        ++victim.statePhase;
    }
    const int16_t effectHeading = static_cast<int16_t>(
        static_cast<int>(victim.headingDegrees) +
        (reaction != nullptr ? reaction->headingDelta : 0));
    if(selected.actionRemap >= 0)
    {
        startCombatMove(victim, stanceMotion(victim, selected.actionRemap),
                        COMBAT_ACTION_STAGGER);
        runCombatStateTask(victim);
    }
    else
        setCombatActorMotion(victim, heldBack ? 656 : reaction->moveId);

    const uint8_t bone = combatImpactBone(input.hit.flagsB,
                                          input.physicalState, true);
    if(bone != 0)
    {
        int16_t classifier = 0;
        if((input.hit.hitFlags & 2u) != 0 && definition != nullptr &&
           (attacker->actorActionFlags & 0x4000u) != 0)
            classifier = static_cast<int16_t>(held->objectId);
        uint8_t records[8];
        int count = combatImpactEffectRecords(classifier, records, 8);
        const int primaryCount = count;
        if((input.hit.hitFlags & 1u) != 0)
            count += combatSecondaryEffectRecords(classifier, records + count, 8 - count);
        int16_t angle = effectHeading;
        if(selected.actionRemap >= 0 || heldBack)
            angle = static_cast<int16_t>(static_cast<int>(victim.headingDegrees +
                atan2f(attacker->position.x - victim.position.x,
                       attacker->position.z - victim.position.z) * 57.29578f));
        for(int index = 0; index < count; ++index)
        {
            GameEstEvent event = {};
            event.recordIndex = records[index];
            event.actorSlot = victim.slotIndex;
            event.boneIndex = bone;
            event.placement = index < 3 || index >= primaryCount ?
                GAME_EST_DIRECTION_BASIS : GAME_EST_BONE_Y_ANGLE;
            event.angleBase = angle;
            event.rawClassifierId = classifier;
            event.flagsB = static_cast<uint16_t>(input.hit.flagsB);
            event.direction = { victim.position.x - attacker->position.x,
                                victim.position.y - attacker->position.y,
                                victim.position.z - attacker->position.z };
            enqueueEstEvent(event);
        }
    }
    victim.receivedContact = { attacker->slotIndex, selected.family,
        contactReaction, selected.direction, input.hit.hitFlags };
    victim.reactionAction = static_cast<uint16_t>(contactReaction);
    victim.participantScore += PARTICIPANT_SCORE_HIT_TAKEN;
    if(selected.physicalOverride)
        victim.motionControlFlags |= 0x80u;
    victim.actorTargetingFlags &= 0xff7fffefu;
    victim.actorTargetingFlags |= 0x200u;

    updateCombatReceivedDamage(victim, true);
}

void Game::accumulateCombatReactionRate(GameSlotRecord& slot)
{

    if(slot.animationTimeScale >= 2.2f)
        return;
    const uint32_t pressed = sampleCombatRecoveryInput(slot);
    unsigned directions = 0;
    unsigned buttons = 0;
    for(uint32_t bits = pressed & 0xfu; bits != 0; bits >>= 1)
        directions += bits & 1u;
    if(slot.playerIndex < 2)
    {

        const uint32_t physical = slot.playerIndex == 0 ? pressedInput : secondPressedInput;
        const uint32_t mask = GAME_INPUT_ATTACK | GAME_INPUT_HEAVY |
            GAME_INPUT_GRAB | GAME_INPUT_GUARD | GAME_INPUT_DODGE |
            GAME_INPUT_INTERACT | GAME_INPUT_USE_FIRST | GAME_INPUT_USE_SECOND;
        for(uint32_t bits = physical & mask; bits != 0; bits >>= 1)
            buttons += bits & 1u;
    }
    else
    {

        for(uint32_t bits = pressed & 0x3fc0u; bits != 0; bits >>= 1)
            buttons += bits & 1u;
    }
    const unsigned count = buttons + directions * (slot.playerIndex < 2 ? 2u : 1u);
    slot.combatAnimationPressAccumulator = static_cast<int16_t>(
        slot.combatAnimationPressAccumulator + count);
    if(slot.combatAnimationPressAccumulator > 0)
    {
        slot.animationTimeScale += static_cast<float>(count) * 0.06f;
        if(slot.animationTimeScale > 2.2f)
            slot.animationTimeScale = 2.2f;
        slot.combatAnimationPressAccumulator = 0;
    }
}

void Game::updateCombatReceivedDamage(GameSlotRecord& slot, bool entered)
{
    if(!slot.receivedDamageTaskActive || scaledFrameDeltaSeconds == 0.0f)
        return;
    const int reaction = slot.receivedContact.reaction;
    if(slot.statePhase > 1 && (slot.actorBehaviorFlags & 0x200u) == 0 &&
       reaction != 8 && reaction != 21 && reaction != 14 && reaction != 20 &&
       reaction != 5 && reaction != 13 && reaction != 15 && reaction != 12)
        accumulateCombatReactionRate(slot);
    if(!entered)
        tryCombatGetUpWallMotion(slot);
    tryCombatGroundReaction(slot);
}
