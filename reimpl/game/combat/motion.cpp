#include "game/game.h"
#include "game/internal.h"
#include <math.h>

using namespace game_detail;

void Game::behaviourMovementCallback(void* userData, int16_t x, int16_t y,
                                     int16_t z, int16_t heading, uint8_t frames)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->actor == NULL)
        return;
    GameSlotRecord& slot = *context->actor;
    slot.combatMotion.offset = { x * 0.01f, y * 0.01f, z * 0.01f };
    slot.combatMotion.headingDelta = heading * 0.01f;
    slot.combatMotion.frames = frames;
    slot.actorActionFlags |= 0x200u;

    if(slot.resourceProfileId == 0x4d || slot.resourceProfileId == 0x48 ||
       slot.resourceProfileId == 5)
    {
        if(slot.appliedMotionId == 0x7d)
            slot.combatMotion.offset.z += 1.25f;
        else if(slot.appliedMotionId == 0x1e4)
            slot.combatMotion.offset.z += 1.5f;
    }
}

void Game::behaviourTurnDurationCallback(void* userData, int frames)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->owner == NULL || context->actor == NULL)
        return;
    GameSlotRecord& slot = *context->actor;
    const GameSlotRecord* const target =
        context->owner->combatSlotByIndex(slot.linkedSlotIndex);
    const bool interruptible = slot.appliedMotionId ==
        context->owner->stanceMotion(slot, 0x1b5) ||
        slot.appliedMotionId == 0x239 || slot.appliedMotionId == 0x2dd;

    if(target == NULL && ((slot.actorBehaviorFlags & 0xeu) != 0 || interruptible))
        return;
    const uint8_t count = static_cast<uint8_t>(frames);
    if(count == 0)
        return;
    CombatMotionRuntime& motion = slot.combatMotion;
    motion.turnDuration = count / 60.0f;
    motion.elapsed = count == 1 ? motion.turnDuration : 0.0f;
    motion.turnBone = 1;
}

void Game::behaviourSpacingCorrectionCallback(void* userData, int frames)
{
    BehaviourCombatContext* const context =
        static_cast<BehaviourCombatContext*>(userData);
    if(context == NULL || context->owner == NULL || context->actor == NULL)
        return;
    GameSlotRecord& slot = *context->actor;
    const GameSlotRecord* const target =
        context->owner->combatSlotByIndex(slot.linkedSlotIndex);
    if(target == NULL || (target->actorBehaviorFlags & 0x20u) == 0)
        return;
    const float distance = sqrtf(horizontalDistanceSquared(slot, *target));

    if(distance > 12.0f && distance <= 17.0f)
    {
        slot.combatMotion.spacingDistance = distance - 12.0f;
        slot.combatMotion.spacingDuration = static_cast<uint8_t>(frames) / 60.0f;
        slot.combatMotion.spacingElapsed = 0.0f;
    }
}

bool Game::setCombatPositionTarget(GameSlotRecord& slot)
{
    if((slot.combatBoneMask & 1u) == 0)
        return false;
    CombatMotionRuntime& motion = slot.combatMotion;
    const CombatBoneTransform& root = slot.combatBones[0];
    motion.offset.x -= root.position.x;
    motion.offset.y = 0.0f;
    motion.offset.z -= root.position.z;
    const float heading = atan2f(root.at.x, root.at.z) * 57.29577951308232f;
    motion.headingDelta = normalizeActorDegrees(motion.headingDelta - heading);
    motion.duration = motion.frames == 1 ? 1.0f : motion.frames / 60.0f;
    motion.elapsed = 0.0f;
    return true;
}

bool Game::setCombatMovementTarget(GameSlotRecord& slot,
                                   const GameSlotRecord& target)
{
    const CombatMotionRuntime& request = slot.combatMotion;
    return setCombatMovementTarget(slot, target, request.offset,
        request.headingDelta, request.frames);
}

bool Game::setCombatMovementTarget(GameSlotRecord& slot,
    const GameSlotRecord& target, const Vec3& offset, float heading, int frames)
{

    const uint32_t ordinaryMask = (1u << 1) | (1u << 21);
    if((slot.combatBoneMask & ordinaryMask) != ordinaryMask)
        return false;
    CombatBoneTransform basis;
    if((target.actorActionFlags & 0xc00u) == 0)
    {
        if((target.combatBoneMask & ordinaryMask) != ordinaryMask)
            return false;
        basis = target.combatBones[21];
        basis.position = target.combatBones[1].position;
    }
    else
    {
        if((target.combatBoneMask & 1u) == 0)
            return false;
        basis = target.combatBones[0];
        const float angle = target.combatMotion.headingDelta *
            0.01745329251994329577f;
        const float c = cosf(angle), s = sinf(angle);
        const Vec3 right = basis.right;
        const Vec3 at = basis.at;
        basis.right = { c * right.x - s * at.x,
                        c * right.y - s * at.y,
                        c * right.z - s * at.z };
        basis.at = { s * right.x + c * at.x,
                     s * right.y + c * at.y,
                     s * right.z + c * at.z };
        basis.position.x += target.combatMotion.offset.x;
        basis.position.y += target.combatMotion.offset.y;
        basis.position.z += target.combatMotion.offset.z;
    }
    CombatMotionRuntime& motion = slot.combatMotion;
    const Vec3 desired = transformCombatPoint(basis, offset.x, 0.0f, offset.z);
    motion.offset = { desired.x - slot.combatBones[1].position.x, 0.0f,
                      desired.z - slot.combatBones[1].position.z };
    const float desiredHeading = atan2f(basis.at.x, basis.at.z) *
        57.29577951308232f + heading;
    const CombatBoneTransform& headingBasis = slot.combatBones[21];
    const float currentHeading = atan2f(headingBasis.at.x, headingBasis.at.z) *
        57.29577951308232f;
    motion.headingDelta = normalizeActorDegrees(desiredHeading - currentHeading);
    motion.frames = frames;
    motion.duration = frames == 1 ? 1.0f : frames / 60.0f;
    motion.elapsed = 0.0f;
    return true;
}

void Game::resolveCombatMovementRequests(void)
{

    for(int pass = 0; pass < 2; ++pass)
        for(int index = 0; index < GAME_COMBAT_SLOT_COUNT; ++index)
        {
            GameSlotRecord* const slot = mutableCombatSlotByIndex(index);
            if(slot == NULL || (slot->flags & 1u) == 0 ||
               (slot->actorActionFlags & 0x200u) == 0)
                continue;
            bool resolved = false;
            if(pass == 0 && (slot->actorActionFlags & 0x400u) != 0)
                resolved = setCombatPositionTarget(*slot);
            else if(pass == 1 && (slot->actorActionFlags & 0xc00u) == 0)
            {
                const GameSlotRecord* const target =
                    combatSlotByIndex(slot->linkedSlotIndex);
                if(target != NULL)
                    resolved = setCombatMovementTarget(*slot, *target);
            }

            if(resolved)
                slot->actorActionFlags &= ~0x200u;
        }
}

void Game::updateCombatMovement(GameSlotRecord& slot)
{
    const float seconds = scaledFrameDeltaSeconds;
    if(seconds == 0.0f)
        return;
    CombatMotionRuntime& motion = slot.combatMotion;

    if(motion.duration != 0.0f)
    {
        float fraction = 1.0f;
        if(motion.duration == 1.0f)
            motion.elapsed = 2.0f;
        else
        {
            motion.elapsed += seconds;
            float step = seconds;
            if(motion.elapsed > motion.duration)
                step -= motion.elapsed - motion.duration;
            fraction = step / motion.duration;
        }
        slot.position.x += motion.offset.x * fraction;
        slot.position.z += motion.offset.z * fraction;
        slot.headingDegrees = normalizeCombatDegrees(slot.headingDegrees +
                                                      motion.headingDelta * fraction);
        if(motion.elapsed >= motion.duration)
            motion.duration = 0.0f;
    }
}

void Game::updateAuthoredCombatMotion(GameSlotRecord& slot)
{
    const float seconds = scaledFrameDeltaSeconds;
    if(seconds == 0.0f)
        return;
    CombatMotionRuntime& motion = slot.combatMotion;
    const GameSlotRecord* const target = combatSlotByIndex(slot.linkedSlotIndex);

    if(motion.turnDuration != 0.0f)
    {
        motion.elapsed += seconds;
        if(motion.elapsed > motion.turnDuration)
            motion.elapsed = motion.turnDuration;
        float delta = 0.0f;

        if((slot.motionControlFlags & 0x80000u) != 0 &&
           target != NULL && (target->flags & 1u) != 0)
            slot.actorUpdateFlags &= ~1u;
        if((slot.actorUpdateFlags & 1u) != 0)
        {
            const float desired = atan2f(slot.velocity.x, slot.velocity.z) *
                57.29577951308232f;
            delta = normalizeActorDegrees(desired - slot.headingDegrees) *
                seconds * 19.0f;
        }
        else if(target != NULL && motion.turnBone >= 0 &&
                motion.turnBone < COMBAT_BONE_COUNT &&
                (target->combatBoneMask & (1u << motion.turnBone)) != 0)
        {
            const Vec3& point = target->combatBones[motion.turnBone].position;
            const float desired = atan2f(point.x - slot.position.x,
                                         point.z - slot.position.z) *
                57.29577951308232f;
            delta = normalizeActorDegrees(desired - slot.headingDegrees) *
                (motion.elapsed / motion.turnDuration);
        }
        slot.headingDegrees = normalizeCombatDegrees(slot.headingDegrees + delta);
        if(motion.elapsed >= motion.turnDuration)
        {
            motion.elapsed = 0.0f;
            motion.turnDuration = 0.0f;
            motion.turnBone = 0;
        }
    }
    updateCombatMovement(slot);

    if(motion.spacingDuration != 0.0f)
    {
        motion.spacingElapsed += seconds;
        float step = seconds;
        if(motion.spacingElapsed > motion.spacingDuration)
            step -= motion.spacingElapsed - motion.spacingDuration;
        if(target != NULL)
        {
            const float x = target->position.x - slot.position.x;
            const float z = target->position.z - slot.position.z;
            const float length = sqrtf(x * x + z * z);
            if(length > 0.0f)
            {
                const float distance = motion.spacingDistance *
                    (step / motion.spacingDuration) / length;
                slot.position.x += x * distance;
                slot.position.z += z * distance;
            }
        }
        if(motion.spacingElapsed >= motion.spacingDuration)
            motion.spacingDuration = 0.0f;
    }
}

void Game::updateCombatMotionRuntime(void)
{
    if(scaledFrameDeltaSeconds == 0.0f)
        return;
    resolveCombatMovementRequests();
    for(int index = 0; index < GAME_COMBAT_SLOT_COUNT; ++index)
    {
        GameSlotRecord* const slot = mutableCombatSlotByIndex(index);
        if(slot != NULL && (slot->flags & 1u) != 0)
            updateAuthoredCombatMotion(*slot);
    }
}
