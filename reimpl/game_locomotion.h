#ifndef GAME_LOCOMOTION_H
#define GAME_LOCOMOTION_H

#include <stdint.h>

inline constexpr int COMBAT_WALK_TABLE_DIRECTIONS[8] = { 0, 4, 2, 6, 1, 7, 3, 5 };
inline constexpr float COMBAT_LOCOMOTION_TURN_SECONDS = 1.0f / 12.0f;
inline constexpr float COMBAT_RUN_STOP_DECAY = 0.922f;
inline constexpr int COMBAT_RUN_MOTIONS[13] =
{
    3, 3, 3, 3, 3, 3, 0x69b, 0x2db, 3, 0x37d, 0x467, 0x438, 3
};

struct CombatLocomotionRuntime
{
    int8_t inputSector;
    int8_t facingSector;
    float displacementX;
    float displacementZ;
};

#endif
