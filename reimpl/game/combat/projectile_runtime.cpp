#include "game/game.h"
#include "game/combat/input.h"
#include "game/internal.h"
#include "game/bits.h"
#include "action_state_handlers.h"
#include <math.h>

void Game::resetCombatProjectileState()
{
    combatProjectiles.reset();
    combatDamageEnabled = false;
    combatProjectileGroupsReady = false;
    for(int32_t& group : combatProjectileGroups)
        group = 2;
}

void Game::initializeCombatProjectileGroups()
{

    for(int32_t& group : combatProjectileGroups)
        group = 2;
    for(int party = 0; party < 3; ++party)
    {
        const int16_t record = activeRecordId(party);
        if(record < 0)
            continue;
        for(int index = 0; index < GAME_COMBAT_SLOT_COUNT; ++index)
        {
            const GameSlotRecord* actor = combatSlotByIndex(static_cast<uint8_t>(index));
            if(actor != nullptr && (actor->flags & 1u) != 0 &&
               actor->persistentRecordId == record)
            {
                combatProjectileGroups[index] = 0;
                break;
            }
        }
    }
    for(int index = 0; index < GAME_COMBAT_SLOT_COUNT; ++index)
    {
        GameSlotRecord* actor = mutableCombatSlotByIndex(static_cast<uint8_t>(index));
        if(actor == nullptr || (actor->flags & 1u) == 0)
            continue;
        if(combatProjectileGroups[index] == 2)
            combatProjectileGroups[index] = 1;

        if(selectedStageId() == 99 && persistent.story.subBattleId != 0 &&
           combatProjectileGroups[index] == 1 && (actor->flags & 0x100000u) == 0 &&
           actor->characterId != 0x233 && actor->characterId != 0x234)
        {
            combatProjectileGroups[index] = 0;

            actor->motionControlFlags &= ~0x10000u;
        }
    }
    combatProjectileGroupsReady = true;
}

void Game::resetCombatInteractionFlags()
{
    for(int index = 0; index < GAME_COMBAT_SLOT_COUNT; ++index)
    {
        GameSlotRecord* actor = mutableCombatSlotByIndex(static_cast<uint8_t>(index));
        if(actor == nullptr)
            continue;
        actor->actorRuntimeFlags &= ~8u;
        actor->actorStatusFlags &= ~0x4000u;
    }
}

void Game::behaviourFireProjectileCallback(void* userData)
{
    BehaviourCombatContext* context = static_cast<BehaviourCombatContext*>(userData);
    if(context != nullptr && context->owner != nullptr && context->actor != nullptr)
        context->owner->fireCombatProjectile(*context->actor);
}

void Game::fireCombatProjectile(GameSlotRecord& actor)
{
    if((actor.actorActionFlags & 0x4000u) == 0)
        return;
    const WeaponInstance* held = weaponManager.equippedBy(actor.slotIndex);
    const WeaponDefinition* definition = held != nullptr ?
        weaponManager.definition(held->objectId) : nullptr;
    const WeaponAmmunitionDefinition* ammunition = definition != nullptr ?
        weaponAmmunitionDefinition(definition->rawWords[29]) : nullptr;
    if(ammunition == nullptr || !weaponManager.consumeAmmunition(held->managerId))
        return;

    actor.hitGeneration = static_cast<uint16_t>(actor.hitGeneration + 1);
    if(actor.hitGeneration == 0xffffu)
        actor.hitGeneration = 0;
    if((actor.combatBoneMask & (1u << 14)) == 0 ||
       actor.slotIndex >= GAME_COMBAT_SLOT_COUNT)
        return;
    const MoveRecord* move = moveRecord(actor.selectedActionId);
    if(move == nullptr || actor.activeHitBlock >= MOVE_HIT_BLOCK_COUNT)
        return;
    const MoveHitBlock& block = move->hitBlocks[actor.activeHitBlock];
    CombatProjectile projectile;
    initializeCombatProjectile(projectile);
    float descriptor[3];
    for(int i = 0; i < 3; ++i)
        descriptor[i] = game_detail::floatFromBits(ammunition->rawWords[i + 1]);
    projectile.ownerSlot = actor.slotIndex;
    projectile.ignoredSlots[0] = actor.slotIndex;
    projectile.group = combatProjectileGroups[actor.slotIndex];
    projectile.positionMode = 1;
    const float angle = actor.headingDegrees * 0.017453292f;
    projectile.direction = { sinf(angle), 0.0f, cosf(angle) };
    const float offset = 1.5f + descriptor[1] +
        (definition->behaviorId == 17 ? 10.0f : 0.0f);
    const Vec3& muzzle = actor.combatBones[14].position;
    projectile.position = { muzzle.x + projectile.direction.x * offset, muzzle.y,
                            muzzle.z + projectile.direction.z * offset };
    projectile.radius = descriptor[1];
    projectile.remainingDistance = descriptor[0];
    projectile.movementFlags = 1;
    projectile.speed = descriptor[2];
    projectile.unknownac = 1;
    projectile.damage = static_cast<uint16_t>(definition->damageBonus + block.damage);
    projectile.hitsRemaining = static_cast<int16_t>(ammunition->rawWords[0] >> 16);
    projectile.hit.ownerSlot = actor.slotIndex;
    projectile.hit.boneIndex = 10;
    projectile.hit.contactIndex = 10;
    projectile.hit.generation = actor.hitGeneration;
    projectile.hit.pride = static_cast<uint8_t>(block.prideDamage);
    projectile.hit.flagsA = 2;
    projectile.hit.flagsB = 2;
    projectile.hit.flagsC = block.flagsC;
    combatProjectiles.spawn(projectile);
}

void Game::recordCombatProjectileHit(GameSlotRecord& victim,
                                     const CombatReceivedHitContext& context,
                                     int16_t damage)
{

    victim.receivedContext = context;
    victim.receivedDamage = damage;
    victim.receivedPrideDamage = static_cast<int16_t>(context.pride);
    victim.receivedContactBone = static_cast<int8_t>(context.boneIndex);
    victim.receivedContactIndex = static_cast<int16_t>(context.contactIndex);
    victim.receivedFlagsA = static_cast<uint16_t>(context.flagsA);
    victim.receivedFlagsB = static_cast<uint16_t>(context.flagsB);
    victim.receivedFlagsC = context.flagsC;
    victim.receivedReactionSet = static_cast<uint8_t>(context.reactionFamily);
    victim.pendingReceivedDamage = true;
    victim.actorRuntimeFlags |= 8u;
    queueCombatPair(victim, 4, 0);
}

void Game::updateCombatProjectiles()
{
    if(scaledFrameDeltaSeconds == 0.0f || !combatProjectileGroupsReady)
        return;
    const int targetCount =
        (persistent.story.battleFlags & BATTLE_REWARD_INTERACTION_FLAG) != 0 ? 13 : 2;
    for(int index = 0; index < targetCount; ++index)
    {
        GameSlotRecord* target = mutableCombatSlotByIndex(static_cast<uint8_t>(index));

        if(target == nullptr || (target->flags & 1u) == 0 || target->currentHealth <= 0 ||
           (target->combatBoneMask & (1u << 1)) == 0 || combatTargetInteractable(*target) ||
           (target->actorRuntimeFlags & 0xcu) != 0 || (target->actorTargetingFlags & 1u) != 0)
            continue;

        for(int shot = 0; shot < COMBAT_PROJECTILE_CAPACITY; ++shot)
        {
            CombatProjectileRuntime& runtime = *combatProjectiles.entry(shot);
            const CombatProjectile& projectile = runtime.projectile;
            if(!runtime.active || projectile.ownerSlot == target->slotIndex ||
               projectile.group == combatProjectileGroups[index])
                continue;
            bool ignored = false;
            for(int32_t slot : projectile.ignoredSlots)
                ignored |= slot == target->slotIndex;
            if(ignored || (target->receivedContext.ownerSlot == projectile.hit.ownerSlot &&
                           target->receivedContext.generation == projectile.hit.generation))
                continue;
            Vec3 contact;
            if(!combatProjectileContact(runtime, target->combatBones[1].position, 10.0f, contact))
                continue;
            recordCombatProjectileHit(*target, projectile.hit,
                                       static_cast<int16_t>(projectile.damage));
            combatProjectiles.recordImpact(shot, contact);
        }
    }
    combatProjectiles.advance(scaledFrameDeltaSeconds);
}
