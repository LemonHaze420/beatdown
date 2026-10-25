#include "game/game.h"
#include "game/combat/links.h"
#include "action_state_handlers.h"
#include <math.h>

void Game::behaviourStatusFlagsCallback(void* userData, uint32_t mask, bool set)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->actor == NULL)
        return;
    if(set)
        context->actor->actorStatusFlags |= mask;
    else
        context->actor->actorStatusFlags &= ~mask;
}

void Game::behaviourContactMovementCallback(void* userData, int16_t offset,
                                            int16_t heading, int16_t frames)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->actor == NULL ||
       !context->actor->combatSurfaceValid)
        return;
    GameSlotRecord& slot = *context->actor;
    const GameCombatSurfaceContact& contact = slot.combatSurfaceContact;
    const float distance = offset * 0.001f;
    slot.combatMotion.offset =
    {
        contact.point.x + contact.normal.x * distance,
        contact.point.y + contact.normal.y * distance,
        contact.point.z + contact.normal.z * distance
    };
    slot.combatMotion.headingDelta = heading * 0.01f +
        atan2f(contact.normal.x, contact.normal.z) * 57.29577951308232f;
    slot.combatMotion.frames = static_cast<uint8_t>(frames);
    slot.actorActionFlags |= 0x600u;
}

bool Game::tryCombatGetUpWallMotion(GameSlotRecord& slot)
{

    if(slot.currentHealth <= 0 || (slot.actorStatusFlags & 4u) == 0 ||
       slot.appliedMotionId == 749 || slot.appliedMotionId == 169 ||
       stageNavigation.combatSurface == NULL)
        return false;
    GameCombatSurfaceContact contact;
    if(!stageNavigation.combatSurface(stageNavigation.combatSurfaceContext,
                                       slot.position, 5.5f, &contact))
        return false;
    slot.combatSurfaceContact = contact;
    slot.combatSurfaceValid = true;
    setCombatActorMotion(slot, 749);
    slot.actorTargetingFlags &= ~0x10u;
    return true;
}

bool Game::tryCombatGetUpSurfacePair(GameSlotRecord& attacker,
                                     GameSlotRecord& victim)
{

    if((victim.actorRuntimeFlags & 1u) == 0 ||
       stageNavigation.combatSurface == NULL)
        return false;
    GameCombatSurfaceContact contact;
    if(!stageNavigation.combatSurface(stageNavigation.combatSurfaceContext,
                                       attacker.position, 15.0f, &contact))
    {
        attacker.combatSurfaceId = 0;
        return false;
    }
    attacker.combatSurfaceContact = contact;
    attacker.combatSurfaceValid = true;

    const unsigned stage = selectedStageId();
    const unsigned material = contact.material + 18u;
    if(stage >= COMBAT_SURFACE_STAGE_COUNT ||
       material >= COMBAT_SURFACE_MATERIAL_COUNT)
        return false;
    const uint16_t property = COMBAT_SURFACE_PROPERTIES[stage][material];
    attacker.combatSurfaceId = property & 0x3ffu;
    int move = movePersonalActionForStyle(
        combatStyle(attacker), 110, combatPersonalActionContext(attacker));
    if(move <= 0)
        return false;
    victim.combatSurfaceId = property & 0x3ffu;
    if(((property >> 10) & 0x3fu) == 1)
        move = 760;
    queueCombatPair(attacker, move, actionStateLinkedMoveId(move));
    return true;
}

void Game::updateCombatGetUp(GameSlotRecord& slot, bool entered)
{

    if((slot.actorBehaviorFlags & 0x40u) == 0)
        return;
    if(entered)
    {
        GameSlotRecord* attacker = mutableCombatSlotByIndex(slot.linkedSlotIndex);
        if(attacker != NULL)
        {

            clearCombatReceivedHitContext(slot.receivedContext);
            slot.receivedContext.ownerSlot = attacker->slotIndex;
            slot.receivedContext.generation = attacker->hitGeneration;
            slot.receivedContext.boneIndex = 10;
            slot.receivedContext.contactIndex = 10;
            slot.receivedContext.reactionFamily = 0;
            slot.receivedContact = { attacker->slotIndex, 0, -1, -1, 0 };
        }
        slot.stateWork[0] = 1;
        return;
    }
    if((slot.actorStatusFlags & 4u) != 0)
    {
        tryCombatGetUpWallMotion(slot);

        slot.stateWork[0] = 0;
    }
    else if(slot.stateWork[0] != 0)
    {
        GameSlotRecord* const attacker =
            mutableCombatSlotByIndex(slot.linkedSlotIndex);
        if(attacker != NULL && attacker != &slot)
            tryCombatGetUpSurfacePair(*attacker, slot);
    }
}
