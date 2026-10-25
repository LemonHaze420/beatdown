#ifndef GAME_BITS_H
#define GAME_BITS_H

#include <stdint.h>
#include <string.h>

namespace game_detail
{
inline float floatFromBits(uint32_t bits)
{
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}
}

#endif
