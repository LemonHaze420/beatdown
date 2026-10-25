#include "game/combat/input.h"
#include "game/internal.h"

#include <math.h>

int combatInputTrigger(uint32_t current, uint32_t previous)
{
    const uint32_t pressed = current & ~previous;
    const uint32_t released = previous & ~current;
    if((pressed & (MOVE_INPUT_ABANDON_MASK | MOVE_INPUT_SLOT_MASKS[4])) != 0)
        return 0;
    if((released & MOVE_INPUT_ABANDON_MASK) != 0)
        return 1;
    if((pressed & 15u) != 0)
        return 4;
    if((current & MOVE_INPUT_SLOT_MASKS[3]) != 0)
        return 2;
    if((pressed & 0xf00000u) != 0)
        return 3;
    return -2;
}

bool combatInputScanAllowed(const GameSlotRecord& actor)
{

    const bool standingGuard = (actor.actorBehaviorFlags & 0x400u) != 0 &&
        (actor.impactSelectorState & 2u) == 0;
    return (standingGuard || (actor.actorActionFlags & 3u) != 0) &&
        (actor.actorActionFlags & 4u) == 0;
}

void publishCombatFacing(GameSlotRecord& actor, const Vec3& target,
                          const Vec3& camera)
{

    const float targetAngle = atan2f(actor.position.x - target.x,
        actor.position.z - target.z) * 57.29577951308232f;
    const float cameraAngle = atan2f(actor.position.x - camera.x,
        actor.position.z - camera.z) * 57.29577951308232f;
    const float angle = game_detail::normalizeActorDegrees(targetAngle - cameraAngle);
    actor.facingRelation = angle >= 0.0f ? 2 : 1;
    actor.facingDirections = 0;
    if(angle > -67.5f && angle <= 67.5f) actor.facingDirections |= 4;
    if(angle > 22.5f && angle <= 157.5f) actor.facingDirections |= 1;
    if(angle > -157.5f && angle <= -22.5f) actor.facingDirections |= 2;
    if(angle > 112.5f || angle <= -112.5f) actor.facingDirections |= 8;

    const float heading = atan2f(target.x - actor.position.x,
        target.z - actor.position.z) * 57.29577951308232f;
    const float relative = game_detail::normalizeActorDegrees(heading - actor.headingDegrees);
    actor.facingTarget = relative >= 90.0f || relative < -90.0f ? 0 : 1;
    if(relative > 45.0f && relative <= 135.0f) actor.facingState = 1;
    else if(relative > -135.0f && relative <= -45.0f) actor.facingState = 2;
    else if(relative > -45.0f && relative <= 45.0f) actor.facingState = 4;
    else actor.facingState = 8;
    actor.combatFacingValid = true;
}

bool combatTargetInteractable(const GameSlotRecord& actor)
{

    if((actor.actorStatusFlags & 0x200000u) != 0 ||
       (actor.actorTargetingFlags & 0x10u) == 0)
        return (actor.actorStatusFlags & 0x3000u) != 0;
    return true;
}

bool combatGroundedState(const GameSlotRecord& actor, bool& grounded)
{

    if((actor.impactSelectorState & 8u) != 0)
        grounded = false;
    else if((actor.impactSelectorState & 0x10u) != 0)
        grounded = true;
    else
    {
        if((actor.combatBoneMask & (1u << 9)) == 0)
            return false;
        grounded = actor.combatBones[9].up.y <= 0.0f;
    }
    return true;
}

uint32_t combatDirectionCode(uint32_t gameButtons)
{
    uint32_t code = 0;
    if((gameButtons & GAME_INPUT_RIGHT) != 0) code |= 1u;
    if((gameButtons & GAME_INPUT_LEFT) != 0) code |= 2u;
    if((gameButtons & GAME_INPUT_UP) != 0) code |= 4u;
    if((gameButtons & GAME_INPUT_DOWN) != 0) code |= 8u;
    return code;
}

uint32_t combatButtonCode(uint32_t gameButtons)
{
    uint32_t code = 0;
    if((gameButtons & GAME_INPUT_ATTACK) != 0)
        code |= MOVE_INPUT_SLOT_MASKS[1];
    if((gameButtons & GAME_INPUT_HEAVY) != 0)
        code |= MOVE_INPUT_SLOT_MASKS[0];
    if((gameButtons & GAME_INPUT_GRAB) != 0)
        code |= MOVE_INPUT_SLOT_MASKS[2];
    if((gameButtons & GAME_INPUT_GUARD) != 0)
        code |= MOVE_INPUT_SLOT_MASKS[3];
    if((gameButtons & GAME_INPUT_DODGE) != 0)
        code |= CPU_COMBAT_RUN_BUTTON;
    if((gameButtons & GAME_INPUT_INTERACT) != 0)
        code |= MOVE_INPUT_SLOT_MASKS[2];
    if((gameButtons & GAME_INPUT_USE_FIRST) != 0)
        code |= MOVE_INPUT_SLOT_MASKS[7];
    if((gameButtons & GAME_INPUT_USE_SECOND) != 0)
        code |= MOVE_INPUT_SLOT_MASKS[8];
    return code;
}

uint32_t combatMovementRelation(const GameSlotRecord& actor,
                                const GameSlotRecord& target,
                                float worldX, float worldZ)
{
    if(worldX * worldX + worldZ * worldZ < 0.0001f)
        return 0;
    const float dx = target.position.x - actor.position.x;
    const float dz = target.position.z - actor.position.z;
    if(dx * dx + dz * dz < 0.0001f)
        return 0;

    const float angle = atan2f(worldX * dz - worldZ * dx,
                              worldX * dx + worldZ * dz) * 57.29577951308232f;
    if(angle >= -80.0f && angle <= 80.0f)
        return MOVE_INPUT_RELATION_TOWARD;
    if(angle >= 100.0f || angle <= -100.0f)
        return MOVE_INPUT_RELATION_AWAY;
    return 0;
}

bool readMovementInput(float rawX, float rawZ, uint32_t buttons,
                       float& x, float& z, float& magnitude)
{
    const bool analog = game_detail::normalizeStick(rawX, rawZ, x, z, magnitude);
    if(!analog)
    {
        if((buttons & GAME_INPUT_LEFT) != 0) x -= 1.0f;
        if((buttons & GAME_INPUT_RIGHT) != 0) x += 1.0f;
        if((buttons & GAME_INPUT_UP) != 0) z += 1.0f;
        if((buttons & GAME_INPUT_DOWN) != 0) z -= 1.0f;
        const float length = sqrtf(x * x + z * z);
        if(length > 0.0f)
        {
            x /= length;
            z /= length;
            magnitude = 1.0f;
        }
    }
    return analog;
}

namespace game_detail
{

float clampNormalizedAxis(float value)
{
    if(value < -1.0f)
        return -1.0f;
    if(value > 1.0f)
        return 1.0f;
    return value;
}

bool normalizeStick(float rawX, float rawZ, float& x, float& z,
                    float& magnitude)
{
    rawX = clampNormalizedAxis(rawX);
    rawZ = clampNormalizedAxis(rawZ);
    const float rawMagnitude = sqrtf(rawX * rawX + rawZ * rawZ);
    if(rawMagnitude <= PLAYER_AXIS_DEAD_ZONE)
    {
        x = 0.0f;
        z = 0.0f;
        magnitude = 0.0f;
        return false;
    }

    if(rawMagnitude >= PLAYER_AXIS_OUTER_RADIUS)
    {
        x = rawX / rawMagnitude;
        z = rawZ / rawMagnitude;
        magnitude = 1.0f;
    }
    else
    {
        magnitude = (rawMagnitude - PLAYER_AXIS_DEAD_ZONE) /
            (rawMagnitude * (PLAYER_AXIS_OUTER_RADIUS -
                             PLAYER_AXIS_DEAD_ZONE));
        x = rawX * magnitude;
        z = rawZ * magnitude;
        magnitude = sqrtf(x * x + z * z);
    }
    return true;
}

}
