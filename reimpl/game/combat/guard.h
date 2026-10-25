#ifndef GAME_COMBAT_GUARD_H
#define GAME_COMBAT_GUARD_H

#include <stdint.h>

inline constexpr int16_t COMBAT_GUARD_MOTIONS[3][6] =
{
    { 821, 822, 826, 825, 823, 824 },
    { 114, 115, 776, 774, 323, 324 },
    { 862, 863, 866, 867, 864, 865 }
};

inline constexpr int16_t COMBAT_CROUCHED_GUARD_MOTIONS[3][6] =
{
    { 814, 815, 819, 817, 815, 815 },
    { 539, 540, 729, 730, 540, 540 },
    { 868, 869, 870, 871, 869, 869 }
};

#endif
