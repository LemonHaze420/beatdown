#ifndef GAME_COMBAT_ANIMATION_H
#define GAME_COMBAT_ANIMATION_H

#include "move_tables.h"
#include <stdint.h>

struct GameSlotRecord;
struct CombatAnimationRequest
{
    MoveMotion motion = {};
    uint32_t state = 0;
    bool hasMotion = false;
    bool scheduled = false;
    bool loop = false;
};

CombatAnimationRequest combatAnimationRequest(const GameSlotRecord& slot,
                                              const MoveMotion* scheduledMotion);
bool combatAnimationUsesLoop(int action);
uint32_t combatAnimationPresentationState(const GameSlotRecord& slot);

#endif
