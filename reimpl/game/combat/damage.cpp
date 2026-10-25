#include "game/game.h"
#include "game/combat/gauge.h"
#include "ui/menu/inventory.h"
#include "scene/task.h"
#include "character_records.h"
#include "combat_styles.h"
#include "versus_tables.h"
#include "xaf_runtime.h"
#include "move_tables.h"
#include "action_state_handlers.h"
#include "options_menu.h"
#include "camera/camera.h"
#include "sound_cues.h"
#include "stage/stage.h"
#include "ui/hud/advice.h"
#include "formats/MENU_LAYOUT/pause_results_layout.h"
#include "formats/BATTLE_ROSTER/battle_rosters.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "game/internal.h"

using namespace game_detail;

namespace game_detail
{

Vec3 transformCombatPoint(const CombatBoneTransform& transform,
                          float x, float y, float z)
{
    Vec3 result;
    result.x = transform.position.x + transform.right.x * x +
        transform.up.x * y + transform.at.x * z;
    result.y = transform.position.y + transform.right.y * x +
        transform.up.y * y + transform.at.y * z;
    result.z = transform.position.z + transform.right.z * x +
        transform.up.z * y + transform.at.z * z;
    return result;
}

float normalizeCombatDegrees(float value)
{
    value = normalizeActorDegrees(value);
    return value == -180.0f ? 180.0f : value;
}

}

bool
strictCombatSphereOverlap(const CombatSphere& first,
                          const CombatSphere& second)
{
    const float x = first.center.x - second.center.x;
    const float y = first.center.y - second.center.y;
    const float z = first.center.z - second.center.z;
    const float radius = first.radius + second.radius;
    return x * x + y * y + z * z < radius * radius;
}

uint16_t
stagedCombatDamage(uint16_t baseDamage, int16_t weaponBonus,
                   float attackerPower, float victimMultiplier,
                   float contactFactor, bool exitScale)
{
    if(baseDamage == 0)
        return 0;
    const uint16_t augmented = static_cast<uint16_t>(
        static_cast<int32_t>(baseDamage) + weaponBonus);
    uint16_t damage = static_cast<uint16_t>(
        static_cast<int>(static_cast<float>(augmented) *
                         attackerPower + 0.01f));
    damage = static_cast<uint16_t>(
        static_cast<int>(static_cast<float>(damage) *
                         victimMultiplier + 0.01f));
    damage = static_cast<uint16_t>(
        static_cast<int>(static_cast<float>(damage) *
                         contactFactor + 0.01f));
    if(exitScale)
    {
        damage = static_cast<uint16_t>(
            static_cast<int>(static_cast<float>(damage) * 1.2f + 0.01f));
    }
    return damage == 0 ? 1 : damage;
}

int16_t
combatPrideDamageAmount(int16_t blockPride, uint32_t randomValue,
                        uint8_t movesetPercent,
                        int16_t currentHealth, int16_t maxHealth,
                        int8_t primaryModifier)
{
    int16_t amount = static_cast<int16_t>(
        static_cast<int>(blockPride) +
        static_cast<int>(randomValue % 10u) + 30);
    if(movesetPercent != 0)
    {
        amount = static_cast<int16_t>(static_cast<int>(
            static_cast<float>(static_cast<int>(amount)) *
            (static_cast<float>(movesetPercent) / 100.0f)));
    }
    const float ratio = maxHealth == 0 ? 0.0f :
        static_cast<float>(static_cast<int>(currentHealth)) /
        static_cast<float>(static_cast<int>(maxHealth));
    const int16_t healthTerm = static_cast<int16_t>(static_cast<int>(
        (50.0f - ratio * 100.0f) * 0.2f));
    int64_t work = static_cast<int16_t>(amount + healthTerm);
    if(work < 0)
        work = 0;
    const int16_t extra = static_cast<int16_t>(static_cast<int>(
        static_cast<float>(static_cast<int>(work)) * 0.2f *
        static_cast<float>(primaryModifier)));
    return static_cast<int16_t>(
        static_cast<int16_t>(work) + extra);
}

uint32_t
combatHitDirection(const Vec3& victimPosition,
                   float victimHeadingDegrees,
                   const Vec3& attackerPosition)
{
    const float heading = atan2f(attackerPosition.x - victimPosition.x,
                                 attackerPosition.z - victimPosition.z) *
        57.29577951308232f;
    const float delta =
        normalizeCombatDegrees(heading - victimHeadingDegrees);
    if(delta > -45.0f && delta <= 45.0f)
        return MOVE_HIT_DIRECTION_FRONT;
    if(delta > 45.0f && delta <= 135.0f)
        return MOVE_HIT_DIRECTION_SIDE_A;
    if(delta > 135.0f || delta <= -135.0f)
        return MOVE_HIT_DIRECTION_BACK;
    return MOVE_HIT_DIRECTION_SIDE_B;
}

int
combatReactionDirectionIndex(uint32_t direction)
{
    if(direction == MOVE_HIT_DIRECTION_FRONT)
        return 1;
    if(direction == MOVE_HIT_DIRECTION_BACK)
        return 0;
    if(direction == MOVE_HIT_DIRECTION_SIDE_A)
        return 3;
    if(direction == MOVE_HIT_DIRECTION_SIDE_B)
        return 2;
    return 0;
}

uint8_t
combatImpactBone(uint16_t flagsB, uint32_t selectorState,
                 bool selectorStateValid)
{
    if((flagsB & 0x0002u) != 0)
        return 10;
    if((flagsB & 0x0004u) != 0)
        return 1;
    if((flagsB & 0x0008u) == 0 || !selectorStateValid)
        return 0;
    if((selectorState & 0x0002u) != 0)
        return 10;
    if((selectorState & 0x0004u) != 0)
        return 1;
    return 6;
}

uint8_t
combatPrimaryEffectClass(int16_t rawClassifierId)
{
    const int effective =
        rawClassifierId >= 0 && rawClassifierId < 48 ?
            rawClassifierId : 0;
    return COMBAT_PRIMARY_EFFECT_CLASS[effective];
}

uint8_t
combatSecondaryEffectClass(int16_t rawClassifierId)
{
    const int effective =
        rawClassifierId >= 0 && rawClassifierId < 48 ?
            rawClassifierId : 0;
    return COMBAT_SECONDARY_EFFECT_CLASS[effective];
}

int
combatImpactEffectRecords(int16_t rawClassifierId, uint8_t* records,
                          int capacity)
{

    int count = 0;
    for(int i = 0; i < 3; ++i)
    {
        if(records != NULL && count < capacity)
            records[count] = GAME_COMBAT_DAMAGE_BASE_RECORDS[i];
        ++count;
    }
    uint8_t mapped[2];
    const int mappedCount =
        combatPrimaryEffectRecords(rawClassifierId, mapped, 2);
    for(int i = 0; i < mappedCount; ++i)
    {
        if(records != NULL && count < capacity)
            records[count] = mapped[i];
        ++count;
    }
    return count;
}

int
combatPrimaryEffectRecords(int16_t rawClassifierId, uint8_t* records,
                           int capacity)
{
    const uint8_t effectClass =
        combatPrimaryEffectClass(rawClassifierId);
    int count = 0;
    for(int i = 0; i < 2; ++i)
    {
        const uint8_t record =
            COMBAT_PRIMARY_EFFECT_RECORDS[effectClass][i];
        if(record == 0xff)
            continue;
        if(records != NULL && count < capacity)
            records[count] = record;
        ++count;
    }
    return count;
}

int
combatSecondaryEffectRecords(int16_t rawClassifierId, uint8_t* records,
                             int capacity)
{
    const uint8_t effectClass =
        combatSecondaryEffectClass(rawClassifierId);
    if(effectClass >= 5)
        return 0;
    int count = 0;
    for(int i = 0; i < 3; ++i)
    {
        const uint8_t record =
            COMBAT_SECONDARY_EFFECT_RECORDS[effectClass][i];
        if(record == 0xff)
            continue;
        if(records != NULL && count < capacity)
            records[count] = record;
        ++count;
    }
    return count;
}

Vec3
resolveCombatCameraSubjectPosition(
    const GameSlotRecord& slot, const Vec3& renderedRootPosition)
{
    if((slot.actorBehaviorFlags & 0x10) != 0 ||
       slot.selectedActionId == 1 ||
       slot.poseStateWord == 1)
        return renderedRootPosition;
    return slot.position;
}

uint8_t
classifyCombatDamage(const CombatDamageRecord& damage)
{
    const uint8_t channels[8] =
    {
        damage.channel_0x02,
        damage.channel_0x03,
        damage.channel_0x04,
        damage.channel_0x05,
        damage.channel_0x0a,
        damage.channel_0x0b,
        damage.channel_0x12,
        damage.channel_0x13
    };

    uint8_t sum = 0;
    for(int i = 0; i < 8; i++)
        sum = static_cast<uint8_t>(sum + DAMAGE_CHANNEL_WEIGHTS[i][channels[i]]);

    if(sum == 0)
        return 0;
    if(sum < 12)
        return 1;
    if(sum < 22)
        return 2;
    return 3;
}

void
reduceGameSlotHealth(GameSlotRecord& slot, int16_t amount)
{

    if((slot.motionControlFlags & GAME_MOTION_INHIBIT_HEALTH_WRITE) != 0)
        return;

    const int16_t health = static_cast<int16_t>(
        static_cast<int>(slot.currentHealth) - static_cast<int>(amount));
    slot.currentHealth = health < 1 ? 0 : health;
}

void
applyHealthFloor(GameSlotRecord& slot, int16_t amount)
{
    reduceGameSlotHealth(slot, amount);
    if(slot.currentHealth < 1)
        slot.currentHealth = 1;
}

float
getGameSlotHealthRatio(const GameSlotRecord& slot)
{
    if(slot.maxHealth == 0)
        return 0.0f;
    return static_cast<float>(slot.currentHealth) / static_cast<float>(slot.maxHealth);
}

void
applyCombatHealthDamage(GameSlotRecord& target, int16_t amount)
{
    int gauge = static_cast<int>(target.damageGauge) + amount;
    if(gauge > target.maxHealth)
        gauge = target.maxHealth;
    target.damageGauge = static_cast<int16_t>(gauge);
    reduceGameSlotHealth(target, amount);
}

float
Game::combatContactFactor(const GameSlotRecord& victim,
                          const GameSlotRecord& attacker,
                          uint8_t* reactionSet,
                          bool* directionFlag) const
{
    int state = 0;
    float factor = 1.0f;
    if(victim.currentHealth < 300)
    {
        state = 2;
        factor = 0.5f;
    }
    else if(victim.currentHealth < 1000)
    {
        state = 1;
        factor = 0.8f;
    }

    bool flag = false;
    if(victim.actorBehaviorFlags == 2)
    {
        state = 3;
        factor *= 1.5f;
        flag = true;
    }
    if((victim.actorBehaviorFlags & 0x1e0u) != 0)
    {
        state = 4;
        float scale =
            0.8f - static_cast<float>(attacker.comboStep) * 0.1f;
        if(scale < 0.3f)
            scale = 0.3f;
        factor *= scale;
        flag = false;
    }
    if(victim.actorBehaviorFlags == 4)
    {
        state = 5;
        factor *= 1.5f;
        flag = true;
    }
    if((victim.impactSelectorState & 4u) != 0)
    {
        state = 0;
        factor = 1.0f;
        flag = false;
    }

    if(reactionSet != NULL)
        *reactionSet = GAME_COMBAT_DAMAGE_REACTION_SETS[state];
    if(directionFlag != NULL)
        *directionFlag = flag;
    return factor;
}

CombatSphere
Game::combatSphere(const GameSlotRecord& slot, bool attacker,
                   int descriptor) const
{
    CombatSphere sphere = {};
    const CombatSphereDescriptor* row = NULL;
    if(attacker)
    {
        if(descriptor >= 0 && descriptor < 12)
            row = &COMBAT_ATTACK_SPHERES[descriptor];
    }
    else if(descriptor >= 0 && descriptor < 18)
        row = &COMBAT_BODY_SPHERES[descriptor];
    if(row == NULL || row->bone < 0 ||
       row->bone >= COMBAT_BONE_COUNT ||
       (slot.combatBoneMask & (uint32_t(1) << row->bone)) == 0)
        return sphere;
    sphere.transform = slot.combatBones[row->bone];
    sphere.center = transformCombatPoint(
        sphere.transform, row->x, row->y, row->z);
    sphere.radius = row->radius *
        (attacker ? slot.contactRadiusScale : 1.0f);
    return sphere;
}

bool
Game::combatChannelContact(const GameSlotRecord& attacker,
                           const GameSlotRecord& victim,
                           int channel, int* contactIndex) const
{
    if(channel < 0 || channel >= 12)
        return false;
    const CombatSphere attackSphere =
        combatSphere(attacker, true, channel);
    if(attackSphere.radius <= 0.0f)
        return false;
    for(int i = 0; i < 18; ++i)
    {
        const CombatSphere bodySphere =
            combatSphere(victim, false, i);
        if(bodySphere.radius > 0.0f &&
           strictCombatSphereOverlap(attackSphere, bodySphere))
        {
            if(contactIndex != NULL)
                *contactIndex = i;
            return true;
        }
    }
    return false;
}

int
Game::attackGaugeForChannel(int channel) const
{
    switch(channel)
    {
    case 0: case 4: return 2;
    case 1: case 5: return 1;
    case 2: case 6: return 4;
    case 3: case 7: return 3;
    case 8: case 9: return 5;
    case 10: return 0;
    default: return -1;
    }
}

void
Game::drainAttackGauge(GameSlotRecord& attacker, int channel, int damage)
{
    const int gauge = attackGaugeForChannel(channel);
    if(gauge < 0 || damage == 0)
        return;
    int drain = static_cast<int>(
        static_cast<float>(damage) * 0.333f);
    if(drain == 0)
        drain = 1;
    attacker.attackGauges[gauge] = static_cast<int16_t>(
        static_cast<int>(attacker.attackGauges[gauge]) - drain);
    attacker.attackGaugeDamage[gauge] = static_cast<int16_t>(
        static_cast<int>(attacker.attackGaugeDamage[gauge]) + drain);
    if(attacker.attackGauges[gauge] < 0)
        attacker.attackGauges[gauge] = 0;
    recordCombatAttackGaugeDrain(attacker, gauge, drain);
    ++attacker.attackGaugeHits[gauge];
}

void
Game::applyPrideDamage(GameSlotRecord& victim,
                       const GameSlotRecord& attacker, int16_t amount)
{
    if(victim.prideGauge < 0)
        return;
    uint8_t percent = 0;
    const int style = combatStyle(attacker);
    if(style >= 0 && style < MOVE_CHARACTER_COUNT)
    {
        const uint8_t moveset =
            MOVE_CHARACTER_RECORDS[style].personalMovesetId;
        if(moveset < 37)
            percent = PRIDE_DAMAGE_PERCENT[moveset];
    }

    const int8_t modifier = (persistent.story.gameStateFlags & 2u) == 0 &&
        attacker.nativeCharacterId == 0 ? static_cast<int8_t>(
            primarySaveImage.payload.coreGameStateBlock[
                GAME_SAVE_CORE_PRIMARY_COMBAT_MODIFIER_OFFSET]) : 0;
    const int16_t applied = combatPrideDamageAmount(
        amount, nextRandom(), percent,
        victim.currentHealth, victim.maxHealth, modifier);
    victim.prideGauge = static_cast<int16_t>(
        static_cast<int>(victim.prideGauge) - applied);
    if(victim.prideGauge < 0)
        victim.prideGauge = 0;
}

void
Game::applyCombatReaction(GameSlotRecord& victim,
                          const GameSlotRecord&,
                          const MoveHitBlock& block,
                          uint32_t direction)
{
    int directionIndex = combatReactionDirectionIndex(direction);
    const MoveReaction* row = NULL;
    int reaction = 0;

    if(victim.currentHealth == 0)
    {
        reaction =
            (victim.impactSelectorState & 4u) != 0 ? 2 : 1;
        row = &MOVE_DEFEAT_REACTIONS[reaction][directionIndex];
    }
    else
    {
        if((victim.impactSelectorState & 4u) != 0)
        {
            bool reversed = false;
            if((victim.impactSelectorState & 8u) == 0)
            {
                reversed =
                    (victim.impactSelectorState & 0x10u) != 0;
                if(!reversed &&
                   (victim.combatBoneMask & (uint32_t(1) << 9)) != 0)
                    reversed = victim.combatBones[9].at.z <= 0.0f;
            }
            reaction = reversed ? 10 : 9;
            if((block.flagsA & 4u) != 0)
                directionIndex = 0;
        }
        else if((block.flagsC & 0x800u) != 0)
            reaction = 0x14;
        else if((block.flagsC & 0x1000u) != 0)
            reaction = 0x15;
        else if((block.flagsC & 0x100u) != 0)
            reaction = 0x0e;
        else if((block.flagsC & 0x10u) != 0)
            reaction = 8;
        else if((block.flagsC & 0x200u) != 0)
            reaction = 0x0d;
        else if((block.flagsC & 0x400u) != 0)
            reaction = 0x0f;
        else if((block.flagsA & MOVE_HIT_GUARD_BREAK) != 0)
            reaction = 7;
        else if((victim.impactSelectorState & 2u) != 0 &&
                (block.flagsB & 0x0cu) != 0)
            reaction = 6;
        else if((victim.impactSelectorState & 0x20u) != 0)
            reaction = 5;
        else if((block.flagsB & 4u) != 0)
            reaction = 3;
        else if((block.flagsB & 8u) != 0)
            reaction = 0x0b;
        else if((block.flagsA & 0x10u) != 0)
            reaction = 1;
        else if((block.flagsA & 0x20u) != 0)
            reaction = 2;
        else if((block.flagsA & 8u) != 0)
            reaction = 0x10;
        else if((block.flagsA & 4u) != 0)
            reaction = 0x11;

        row = moveReaction(
            victim.receivedReactionSet, reaction, directionIndex);
    }

    victim.reactionAction = static_cast<uint16_t>(reaction);
    if(row == NULL || moveRecord(row->moveId) == NULL)
        return;
    victim.headingDegrees = normalizeCombatDegrees(
        victim.headingDegrees + static_cast<float>(row->headingDelta));
    startCombatMove(
        victim, row->moveId,
        victim.currentHealth == 0 ?
            COMBAT_ACTION_DEFEATED : COMBAT_ACTION_STAGGER);
}

void
Game::applyGrabOutcome(GameSlotRecord& attacker,
                       GameSlotRecord& victim, uint32_t direction)
{
    MovePairedOutcome outcome = {};
    const WeaponInstance* const held =
        weaponManager.equippedBy(attacker.slotIndex);
    if(held != NULL)
    {
        const WeaponDefinition* const definition =
            weaponManager.definition(held->objectId);
        if(definition != NULL)
        {
            const uint8_t category = static_cast<uint8_t>(
                definition->rawWords[17] & 0xffu);
            outcome = moveWeaponGrabOutcome(category);
        }
    }
    if(outcome.primaryMove == 0 && outcome.secondaryMove == 0)
    {
        outcome = moveGrabPairedOutcome(
            combatStyle(attacker), combatStyle(victim), direction);
    }
    if(outcome.secondaryMove != 0)
    {
        startCombatMove(
            attacker, outcome.secondaryMove, attacker.action);
        if(outcome.secondaryLinkedMove != 0)
            startCombatMove(
                victim, outcome.secondaryLinkedMove, victim.action);
    }
    if(outcome.primaryMove != 0)
        startCombatMove(attacker, outcome.primaryMove, attacker.action);
    if(outcome.primaryLinkedMove != 0)
        startCombatMove(victim, outcome.primaryLinkedMove, victim.action);
}

bool
combatPhysicalStateAcceptsHit(const GameSlotRecord& attacker,
                              const GameSlotRecord& target,
                              const MoveHitBlock& block)
{

    const bool crouchedGuard =
        (target.actorBehaviorFlags & 0x400u) != 0 &&
        (target.impactSelectorState & 2u) != 0;
    if((target.impactSelectorState & 2u) != 0 &&
       (block.flagsB & 0x0cu) == 0 &&
       (attacker.actorActionFlags & 0x4000u) == 0 && !crouchedGuard)
        return false;

    return (target.impactSelectorState & 4u) == 0 ||
           (block.flagsC & 0x80u) == 0;
}

bool
Game::queueCombatAction(GameSlotRecord& attacker,
                        GameSlotRecord& target,
                        uint8_t channel, int contactIndex)
{
    const MoveRecord* const move = moveRecord(attacker.recoveredMoveId);
    if(move == NULL || attacker.activeHitBlock >= MOVE_HIT_BLOCK_COUNT ||
       target.currentHealth == 0)
        return false;
    const MoveHitBlock& block =
        move->hitBlocks[attacker.activeHitBlock];
    if(target.lastHitAttackerSlot == attacker.slotIndex &&
       target.lastHitMoveId == attacker.recoveredMoveId &&
       target.lastHitGeneration == attacker.hitGeneration &&
       target.lastHitBlock == attacker.activeHitBlock)
        return false;

    const uint32_t direction = hitDirection(target, attacker);
    target.facingState = static_cast<uint8_t>(direction);
    uint8_t reactionSet = 0;
    bool directionFlag = false;
    const float contactFactor = combatContactFactor(
        target, attacker, &reactionSet, &directionFlag);
    int16_t weaponBonus = 0;
    if((attacker.actorActionFlags & 0x4000u) != 0)
        weaponBonus = wieldedDamageBonus(attacker);
    const uint16_t damage = stagedCombatDamage(
        block.damage, weaponBonus,
        attacker.attackPowerMultiplier,
        target.dynamicAttackMultiplier,
        contactFactor,
        target.prideGauge == 0 && battleExitEligible());

    bool receivedPayloadKnown = channel < 12 &&
        contactIndex >= 0 && contactIndex < 18 && move->attributeAddress != 0;
    if(receivedPayloadKnown)
    {
        const int attackBone = COMBAT_ATTACK_SPHERES[channel].bone;
        const int targetBone = COMBAT_BODY_SPHERES[contactIndex].bone;
        receivedPayloadKnown = attackBone >= 0 && targetBone >= 0 &&
            (attacker.combatBoneMask & (uint32_t(1) << attackBone)) != 0 &&
            (target.combatBoneMask & (uint32_t(1) << targetBone)) != 0;
    }

    const bool deferredReceived = receivedPayloadKnown &&
        (attacker.actorBehaviorFlags & 2u) != 0 &&
        (attacker.actorBehaviorFlags & 4u) == 0 &&
        (move->behaviorFlags & 2u) != 0 &&
        (move->behaviorFlags & 4u) == 0 &&
        static_cast<int16_t>(static_cast<uint32_t>(move->handlerParam) >> 16) == 0 &&
        move->alternateLinkedMoveId == 4 &&
        (currentMode != GAME_MODE_STORY || target.prideGauge < 0) &&
        target.linkedSlotIndex == attacker.slotIndex &&
        (target.actorBehaviorFlags & 0x400u) == 0 &&
        (target.poseStateWord & 0x10u) == 0 &&
        (block.flagsB & 4u) == 0;

    if(receivedPayloadKnown)
    {
        CombatReceivedHitContext& context = target.receivedContext;
        context.attributeAddress = move->attributeAddress + 8u +
            static_cast<uint32_t>(attacker.activeHitBlock) * 0x10u;
        context.boneIndex = COMBAT_BODY_SPHERES[contactIndex].bone;
        context.contactIndex = COMBAT_ATTACK_SPHERES[channel].bone;
        context.pride = block.prideDamage;
        context.reactionFamily = reactionSet;
        context.hitFlags = (directionFlag ? 1u : 0u) |
            ((block.flagsA & 0x100u) != 0 ? 2u : 0u);
        context.flagsA = block.flagsA;
        context.flagsB = block.flagsB;
        context.flagsC = block.flagsC;

    }
    target.receivedContext.ownerSlot = attacker.slotIndex;
    target.receivedContext.generation = attacker.hitGeneration;
    target.receivedDamage = static_cast<int16_t>(damage);
    target.receivedPrideDamage = block.prideDamage;
    target.receivedContactIndex = static_cast<int16_t>(contactIndex);
    target.receivedContactBone =
        channel < 12 ? COMBAT_ATTACK_SPHERES[channel].bone : -1;
    target.receivedChannel = channel;
    target.receivedFlagsA = block.flagsA;
    target.receivedFlagsB = block.flagsB;
    target.receivedFlagsC = block.flagsC;
    target.receivedReactionSet = reactionSet;
    target.receivedDirectionFlag = directionFlag;
    target.lastHitAttackerSlot = attacker.slotIndex;
    target.lastHitMoveId = attacker.recoveredMoveId;
    target.lastHitGeneration = attacker.hitGeneration;
    target.lastHitBlock = attacker.activeHitBlock;

    if((attacker.actorBehaviorFlags & 2u) != 0 &&
       channel != 0xffu && damage != 0)
        drainAttackGauge(attacker, channel, damage);

    if(guardStopsHit(target, attacker))
    {
        target.actorTargetingFlags |= 0x400u;
        target.participantScore += PARTICIPANT_SCORE_GUARD_HELD;
        ++target.guardCount;
        return true;
    }

    if(!combatPhysicalStateAcceptsHit(attacker, target, block))
        return false;

    attacker.inputHold.kind = 0;
    attacker.inputHold.followUpMove = 0;
    attacker.inputHold.flags = 0;
    if((attacker.actorBehaviorFlags & 4u) != 0)
    {
        if((target.actorBehaviorFlags & 0x400u) != 0)
        {
            target.inputHold.flags |= 4u;
            target.actorActionFlags |= 0x10000u;
            target.actorTargetingFlags |= 0x200000u;
        }
        if((target.actorTargetingFlags & 0x200u) != 0)
        {
            target.inputHold.flags |= 4u;
            target.actorActionFlags |= 0x10000u;
        }
    }

    const bool hitTransition = moveHitTransitionApplies(
        attacker.actorBehaviorFlags, target.actorBehaviorFlags);
    bool startsFinisher = false;
    if(deferredReceived)
    {
        target.pendingReceivedDamage = true;
        target.actorRuntimeFlags |= 8u;
        queueCombatPair(target, 4, 0);
    }
    else
    {

        target.pendingReceivedDamage = false;
        applyPrideDamage(target, attacker, block.prideDamage);
        startsFinisher = shouldRunBattleFinisher(attacker, target);
        if(startsFinisher)
            applyHealthFloor(target, static_cast<int16_t>(damage));
        else
            applyCombatHealthDamage(target, static_cast<int16_t>(damage));
        attacker.participantScore += scaledParticipantBattleScore(damage);
        target.participantScore += PARTICIPANT_SCORE_HIT_TAKEN;
        ++target.hitsTakenCount;

        int bodyRegion = -1;
        if((target.impactSelectorState & 4u) == 0)
        {
            if((block.flagsB & 2u) != 0)
                bodyRegion = 0;
            else if((block.flagsB & 4u) != 0)
            {
                if((target.impactSelectorState & 1u) != 0)
                    bodyRegion =
                        target.facingState == MOVE_HIT_DIRECTION_BACK ? 3 : 1;
                else if((target.impactSelectorState & 2u) != 0)
                    bodyRegion = 0;
            }
            else if((block.flagsB & 8u) != 0)
                bodyRegion = 2;
        }
        int bodyDamage =
            static_cast<int>(static_cast<float>(damage) * 0.5f);
        if(bodyDamage == 0 && damage != 0)
            bodyDamage = 1;
        if(bodyRegion >= 0 && bodyDamage != 0)
        {
            target.bodyPartHealth[bodyRegion] = static_cast<int16_t>(
                static_cast<int>(target.bodyPartHealth[bodyRegion]) -
                bodyDamage);
            target.bodyPartDamage[bodyRegion] = static_cast<int16_t>(
                static_cast<int>(target.bodyPartDamage[bodyRegion]) +
                bodyDamage);
            recordCombatBodyGaugeDamage(target, bodyRegion, bodyDamage);
            const uint32_t preserve =
                0x10u | 0x100u | 0x800u | 0x1000u;
            if(target.bodyPartHealth[bodyRegion] < 1 &&
               (target.impactSelectorState & 0x20u) == 0 &&
               (block.flagsC & preserve) == 0)
                target.bodyPartHealth[bodyRegion] = 0;
        }

        enqueueCombatEstEvents(attacker, target, block,
                               static_cast<int16_t>(damage));
        pendingDamageCameraSettle = static_cast<float>(damage);
        if((attacker.actorBehaviorFlags & 4u) != 0)
            applyGrabOutcome(attacker, target, direction);
        applyCombatReaction(target, attacker, block, direction);
    }

    if(hitTransition)
    {
        target.actorActionFlags |= MOVE_ACTION_HIT_PENDING;
        target.actorTargetingFlags |= MOVE_TARGET_HIT_TRANSITION;
    }
    target.actorActionFlags &= 0xffffff87u;
    attacker.actorTargetingFlags |= 0x400u;

    if(channel != 0xffu)
    {
        GameCombatContact contact = {};
        contact.damage = damage;
        contact.attackerSlot = attacker.slotIndex;
        contact.victimSlot = target.slotIndex;
        contact.channel = static_cast<int8_t>(channel);
        contact.contactIndex = static_cast<int8_t>(contactIndex);
        if(channel < 12)
        {
            const CombatSphere sphere =
                combatSphere(attacker, true, channel);
            contact.center = sphere.center;
            contact.radius = sphere.radius;
            contact.transform = sphere.transform;
        }
        else
        {
            contact.center = attacker.position;
            contact.radius = attacker.hitWindowRadius;
            if((attacker.combatBoneMask & 1u) != 0)
                contact.transform = attacker.combatBones[0];
        }
        enqueueCombatContact(contact);
    }
    if(startsFinisher)
        startBattleFinisher(attacker, target);
    return true;
}

bool
Game::directCombatHit(GameSlotRecord& attacker, GameSlotRecord& target)
{
    return queueCombatAction(attacker, target, 0xffu, 0);
}

void
Game::enqueueCombatContact(const GameCombatContact& contact)
{
    if(contact.damage == 0)
        return;
    if(combatContactCount == GAME_COMBAT_CONTACT_CAPACITY)
    {
        combatContactHead =
            (combatContactHead + 1) % GAME_COMBAT_CONTACT_CAPACITY;
        --combatContactCount;
    }
    const int tail =
        (combatContactHead + combatContactCount) %
        GAME_COMBAT_CONTACT_CAPACITY;
    combatContacts[tail] = contact;
    ++combatContactCount;
}

bool
Game::pollCombatContact(GameCombatContact* contact)
{
    if(contact == NULL || combatContactCount == 0)
        return false;
    *contact = combatContacts[combatContactHead];
    combatContactHead =
        (combatContactHead + 1) % GAME_COMBAT_CONTACT_CAPACITY;
    --combatContactCount;
    return true;
}

void
Game::updateCombatInteractions(void)
{
    resetCombatInteractionFlags();
    if(scaledFrameDeltaSeconds == 0.0f)
    {
        for(int index = 0; index < GAME_COMBAT_SLOT_COUNT; ++index)
        {
            GameSlotRecord* actor = mutableCombatSlotByIndex(static_cast<uint8_t>(index));
            if(actor != nullptr)
                actor->actorRuntimeFlags &= ~4u;
        }
        return;
    }
    for(int i = 0; i < GAME_COMBAT_SLOT_COUNT; ++i)
    {
        GameSlotRecord* const attacker =
            mutableCombatSlotByIndex(static_cast<uint8_t>(i));
        if(attacker == NULL ||
           (attacker->behaviourDecisionMask &
            COMBAT_BEHAVIOUR_HIT_WINDOW) == 0 ||
           attacker->hitChannelMask == 0)
            continue;
        if(attacker->playingAnimationFrame >
           static_cast<float>(attacker->hitWindowEndFrame))
        {
            attacker->hitChannelMask = 0;
            attacker->actorTargetingFlags &= ~0x100u;
            attacker->actorTargetingFlags |= 0x2200u;
            continue;
        }
        if(attacker->playingAnimationFrame <
           static_cast<float>(attacker->hitWindowStartFrame))
            continue;
        GameSlotRecord* const target =
            mutableCombatSlotByIndex(attacker->linkedSlotIndex);
        if(target == NULL || target == attacker ||
           target->currentHealth == 0)
            continue;

        for(int channel = 0; channel < MOVE_HIT_CHANNEL_COUNT; ++channel)
        {
            if((attacker->hitChannelMask &
                (uint32_t(1) << channel)) == 0)
                continue;
            int contactIndex = -1;
            bool landed = false;
            if(channel < 12)
            {
                landed = combatChannelContact(
                    *attacker, *target, channel, &contactIndex);
            }
            else if(channel == 13 &&
                    (attacker->facingState &
                     MOVE_HIT_DIRECTION_FRONT) != 0)
            {
                const float duration = static_cast<float>(
                    attacker->hitWindowEndFrame -
                    attacker->hitWindowStartFrame);
                const float progress = duration == 0.0f ? 1.0f :
                    (attacker->playingAnimationFrame -
                     static_cast<float>(attacker->hitWindowStartFrame)) /
                    duration;
                const float radius = attacker->hitWindowRadius * progress;
                const float x =
                    target->position.x - attacker->position.x;
                const float y =
                    target->position.y - attacker->position.y;
                const float z =
                    target->position.z - attacker->position.z;
                landed = x * x + y * y + z * z <= radius * radius;
            }
            if(!landed)
                continue;
            attacker->actorTargetingFlags |= 0x100u;
            if(queueCombatAction(
                *attacker, *target,
                static_cast<uint8_t>(channel), contactIndex))
                break;
        }
    }
    updateCombatProjectiles();
    for(int i = 0; i < GAME_COMBAT_SLOT_COUNT; ++i)
    {
        GameSlotRecord* slot = mutableCombatSlotByIndex(static_cast<uint8_t>(i));
        if(slot != nullptr)
            slot->actorRuntimeFlags &= ~4u;
    }
}

int32_t
scaledParticipantBattleScore(int32_t damageDealt)
{
    int32_t headroom = 500 - damageDealt;
    if(headroom < 0)
        headroom = 0;
    return static_cast<int32_t>(static_cast<float>(headroom) * 0.25f + 10.0f);
}

int16_t
resolveBattleResourceProfile(int16_t authoredCharacterId,
                             int16_t persistentResourceId)
{
    switch(persistentResourceId)
    {
    case 0x54: return 0x4a;
    case 0x74: return 0x4d;
    case 0x23: return 0x4b;
    case 0x71:
    case 0x9a: return 0x6c;
    case 0x35:
    case 0x73:
    case 0x9b: return 0x6b;
    case 0x4e:
    case 0x99: return 0x44;
    case 0x9f:
    case 0x3f:
    case 0x9c: return 0x35;
    case 0x43: return 0x39;
    case 0x49: return 0x3f;
    case 0x9d:
    case 0x18: return 0x16;
    case 0x02: return 0x00;
    default: return authoredCharacterId;
    }
}

int16_t
Game::wieldedDamageBonus(const GameSlotRecord& attacker) const
{
    const WeaponInstance* const held =
        weaponManager.equippedBy(attacker.slotIndex);
    if(held == NULL)
        return 0;
    return weaponManager.damageBonus(held->objectId);
}

bool
Game::guardStopsHit(const GameSlotRecord& target,
                    const GameSlotRecord& attacker) const
{
    const MoveRecord* const move = moveRecord(attacker.recoveredMoveId);
    if(move == NULL || attacker.activeHitBlock >= MOVE_HIT_BLOCK_COUNT)
        return false;
    const MoveHitBlock& block =
        move->hitBlocks[attacker.activeHitBlock];
    if(target.action != COMBAT_ACTION_GUARD ||
       (attacker.actorBehaviorFlags & 2u) == 0 ||
       (target.actorBehaviorFlags & 0x400u) == 0 ||
       (target.facingState & MOVE_HIT_DIRECTION_FRONT) == 0 ||
       (block.flagsA & MOVE_HIT_GUARD_BREAK) != 0)
        return false;
    if((target.actorBehaviorFlags & 0x400u) != 0)
    {
        if((target.impactSelectorState & 2u) != 0)
            return (block.flagsB & 4u) == 0;
        return (block.flagsB & 8u) == 0;
    }
    return true;
}

bool Game::loadWeaponDataFromXaf(const char* archivePath,
                                 const char* entryName)
{
    std::vector<uint8_t> bytes;
    if(!readXafEntry(archivePath, entryName, bytes))
    {
        weaponDataAvailable = false;
        weaponLoadDiagnostic =
            "weapon archive entry is missing or unreadable";
        return false;
    }
    std::string error;
    if(!weaponManager.loadMemory(bytes.data(), bytes.size(), &error))
    {
        weaponDataAvailable = false;
        weaponLoadDiagnostic = error.empty() ?
            "weapon archive entry is invalid" : error;
        return false;
    }
    if(!pendingWeaponStates.empty())
    {
        if(!weaponManager.restoreState(pendingWeaponStates))
        {
            weaponManager.releaseAll();
            weaponDataAvailable = false;
            weaponLoadDiagnostic =
                "weapon save state references unavailable object records";
            return false;
        }
        pendingWeaponStates.clear();
    }
    weaponDataAvailable = true;
    weaponLoadDiagnostic.clear();
    return true;
}

bool Game::weaponDataLoaded(void) const
{
    return weaponDataAvailable;
}

const char* Game::weaponDiagnostic(void) const
{
    return weaponLoadDiagnostic.c_str();
}

uint32_t
Game::hitDirection(const GameSlotRecord& victim,
                   const GameSlotRecord& attacker) const
{
    return combatHitDirection(
        victim.position, victim.headingDegrees, attacker.position);
}
