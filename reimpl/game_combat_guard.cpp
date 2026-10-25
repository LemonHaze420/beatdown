#include "game.h"
#include "game_combat_input.h"
#include "game_combat_guard.h"
#include "game_internal.h"

bool Game::combatGuardAllowed(const GameSlotRecord& actor) const
{

    if((actor.actorActionFlags & 0x4000u) == 0)
        return true;
    const WeaponInstance* held = weaponManager.equippedBy(actor.slotIndex);
    const WeaponDefinition* definition = held != nullptr ?
        weaponManager.definition(held->objectId) : nullptr;
    if(definition != nullptr && (definition->behaviorId == 0x11 ||
       definition->behaviorId == 5 || definition->behaviorId == 4))
        return false;
    return true;
}

void Game::updateCombatGuard(GameSlotRecord& actor, bool crouched)
{

    GameSlotRecord* target = mutableCombatSlotByIndex(actor.linkedSlotIndex);
    const WeaponInstance* held = (actor.actorActionFlags & 0x4000u) != 0 ?
        weaponManager.equippedBy(actor.slotIndex) : nullptr;
    const WeaponDefinition* definition = held != nullptr ?
        weaponManager.definition(held->objectId) : nullptr;
    const int family = definition != nullptr ? definition->behaviorId : 0x12;
    const int row = family == 3 ? 1 : (family == 10 ? 2 : 0);
    const int16_t* motions = crouched ? COMBAT_CROUCHED_GUARD_MOTIONS[row] :
        COMBAT_GUARD_MOTIONS[row];
    if(actor.statePhase == 4)
    {
        actor.actorTargetingFlags &= ~0x400u;
        actor.statePhase = 3;
        return;
    }
    if(actor.statePhase == 1)
    {
        actor.comboStep = 0;
        actor.actorActionFlags &= 0xfffecfffu;
        actor.actorTargetingFlags &= 0xff5fffefu;
        if(!crouched && (actor.motionControlFlags & 0x20u) != 0)
            actor.flags |= 0x10u;
        setCombatActorMotion(actor, motions[0]);
        if(crouched)
        {
            actor.statePhase = 3;
            return;
        }
        const WeaponInstance* targetHeld = target != nullptr &&
            (target->actorActionFlags & 0x4000u) != 0 ?
                weaponManager.equippedBy(target->slotIndex) : nullptr;
        const WeaponDefinition* targetDefinition = targetHeld != nullptr ?
            weaponManager.definition(targetHeld->objectId) : nullptr;
        const int targetFamily = targetDefinition != nullptr ? targetDefinition->behaviorId : 0x12;
        const bool matchingFamily = row == 0 ? family == 0x12 :
            (targetFamily == 3 || targetFamily == 10);
        const int category = combatStanceCategory(actor);
        if((actor.actorActionFlags & 0x8000u) == 0 && matchingFamily &&
           (category == 0x12 || category == 0x11))
        {
            actor.poseStateWord |= 0x10u;
            actor.actorActionFlags |= 0x8000u;
            actor.statePhase = 2;
        }
        else
            actor.statePhase = 3;
        return;
    }
    if(!crouched && actor.statePhase == 2)
    {
        const MoveRecord* attack = target != nullptr ? moveRecord(target->selectedActionId) : nullptr;
        int counter = attack != nullptr ?
            game_detail::resolveQueuedMotion(*target, attack->field_0xC) : 0;
        if(target != nullptr && (actor.actorTargetingFlags & 0x400u) != 0 &&
           (actor.actorBehaviorFlags & 0x400u) != 0 &&
           (target->actorBehaviorFlags & 2u) != 0 && counter != 0)
        {
            int self;
            if(held != nullptr && (family == 3 || family == 10))
            {
                counter = 0x112;
                self = 0x111;
            }
            else
            {
                const int style = combatStyle(actor);
                const int variant = style >= 0 && style < MOVE_CHARACTER_COUNT ?
                    MOVE_CHARACTER_RECORDS[style].locomotionVariant : -1;
                self = variant == 0 ? 0x3e3 : (variant == 1 ? 0x443 :
                    (moveRecord(counter) != nullptr ? moveRecord(counter)->linkedMoveId : 0));
                if(variant == 0)
                    counter = 0x3e4;
            }
            if(self > 0)
            {
                startCombatMove(actor, self, COMBAT_ACTION_GUARD);
                runCombatStateTask(actor);
                startCombatMove(*target, counter, target->action);
                runCombatStateTask(*target);
                actor.actorStatusFlags |= 0x40u;
                return;
            }
        }
        if(actor.playingAnimationFrame >= 6.0f)
        {
            actor.poseStateWord &= ~0x10u;
            actor.statePhase = 3;
        }
        return;
    }
    if(actor.statePhase != 3)
        return;
    if((actor.actorTargetingFlags & 0x400u) != 0)
    {

        const Vec3 retreat = { 0.0f, 0.0f, -0.5f };
        setCombatMovementTarget(actor, actor, retreat, 0.0f, 1);
        int direction = 1;
        if((actor.facingState & 1u) != 0) direction = 3;
        else if((actor.facingState & 2u) != 0) direction = 2;
        else
        {
            switch(actor.receivedContext.flagsA & 0x3cu)
            {
            case 8: direction = 5; break;
            case 4: direction = 4; break;
            case 0x20: direction = 3; break;
            case 0x10: direction = 2; break;
            default: break;
            }
        }
        setCombatActorMotion(actor, motions[direction]);
        actor.statePhase = 4;
        if(actor.actorBehaviorFlags == 0x20u)
            accumulateCombatReactionRate(actor);
        return;
    }
    const uint32_t pad = sampleCombatHeldInput(actor);
    const auto& rows = crouched ? COMBAT_CROUCHED_GUARD_MOTIONS : COMBAT_GUARD_MOTIONS;
    bool base = false;
    for(const auto& guard : rows)
        base = base || actor.appliedMotionId == guard[0];
    if((!combatGuardAllowed(actor) || (pad & MOVE_INPUT_SLOT_MASKS[3]) == 0) && base)
    {
        const int neutral = stanceMotion(actor, 1);
        startCombatMove(actor, neutral, COMBAT_ACTION_IDLE);
        runCombatStateTask(actor);
    }
    else if(crouched)
    {
        if((pad & 8u) == 0 && base)
        {
            startCombatMove(actor, stanceMotion(actor, 0x1f1), COMBAT_ACTION_GUARD);
            runCombatStateTask(actor);
        }
    }
    else if(combatGuardAllowed(actor) && (actor.motionControlFlags & 0x4000u) == 0 &&
            (pad & 8u) != 0)
    {
        startCombatMove(actor, stanceMotion(actor, 0x214), COMBAT_ACTION_GUARD);
        runCombatStateTask(actor);
    }
    if(actor.actorBehaviorFlags == 0x20u)
        accumulateCombatReactionRate(actor);
}
