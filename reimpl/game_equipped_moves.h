#ifndef GAME_EQUIPPED_MOVES_H
#define GAME_EQUIPPED_MOVES_H

#include <stdint.h>

namespace game_detail
{

inline const uint8_t EQUIPPED_MOVE_CATEGORIES[46] =
{
    18, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 4, 4, 5, 5, 5, 13, 18, 11, 10, 2,
    6, 6, 6, 6, 6, 2, 6, 6, 6, 18, 14, 18, 0, 17
};

}

#endif
