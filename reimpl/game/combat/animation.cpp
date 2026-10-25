#include "game/combat/animation.h"
#include "game/game.h"

bool combatAnimationUsesLoop(int action)
{
    return action == COMBAT_ACTION_IDLE || action == COMBAT_ACTION_MOVE ||
           action == COMBAT_ACTION_GUARD || action == COMBAT_ACTION_VICTORY;
}

uint32_t combatAnimationPresentationState(const GameSlotRecord& slot)
{
    const uint16_t detail = slot.action == COMBAT_ACTION_IDLE ||
        slot.action == COMBAT_ACTION_MOVE ?
        static_cast<uint16_t>(slot.locomotionMoveId) :
        static_cast<uint16_t>(slot.reaction);
    return (static_cast<uint32_t>(slot.action) << 16) |
           (static_cast<uint32_t>(slot.comboStep) << 8) | detail;
}

CombatAnimationRequest combatAnimationRequest(const GameSlotRecord& slot,
                                              const MoveMotion* scheduledMotion)
{
    CombatAnimationRequest request;
    request.state = combatAnimationPresentationState(slot);
    request.loop = combatAnimationUsesLoop(slot.action);
    if(scheduledMotion != nullptr && scheduledMotion->motionId > 0)
    {
        request.motion = *scheduledMotion;
        request.hasMotion = true;
        request.scheduled = true;
        request.loop = false;
        request.state = 0x80000000u | (slot.scheduledMotionGeneration & 0x7fffffffu);
    }
    else if((slot.action == COMBAT_ACTION_IDLE || slot.action == COMBAT_ACTION_MOVE) &&
            slot.locomotionMoveId > 0)
    {
        request.motion.motionId = slot.locomotionMoveId;
        request.motion.maxDurationFrames = -1;
        request.hasMotion = true;
    }
    else if(slot.recoveredMoveId > 0)
        request.hasMotion = moveMotions(slot.recoveredMoveId, &request.motion, 1) == 1 &&
            request.motion.motionId > 0;
    if(request.hasMotion && !request.scheduled)
        request.state ^= static_cast<uint16_t>(request.motion.motionId);
    return request;
}
