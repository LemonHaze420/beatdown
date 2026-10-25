#ifndef GAME_INPUT_DIRECTIONS_H
#define GAME_INPUT_DIRECTIONS_H

#include <stdint.h>
#include <math.h>

inline uint32_t combatStickDirectionCode(float leftX, float leftY,
                           float rightX, float rightY, uint32_t buttons)
{

    if((buttons & 0xfu) == 0)
    {
        const float x = fabsf(leftX) > 0.1f ? leftX : 0.0f;
        const float y = fabsf(leftY) > 0.1f ? leftY : 0.0f;
        if(x != 0.0f || y != 0.0f)
        {
            const float angle = atan2f(x, y) * 57.29577951308232f;
            if(angle > -67.5f && angle <= 67.5f)
                buttons |= 0x40004u;
            if(angle > 22.5f && angle <= 157.5f)
                buttons |= 0x10001u;
            if(angle > -157.5f && angle <= -22.5f)
                buttons |= 0x20002u;
            if(angle > 112.5f || angle <= -112.5f)
                buttons |= 0x80008u;
        }
    }
    const float x = fabsf(rightX) > 0.1f ? rightX : 0.0f;
    const float y = fabsf(rightY) > 0.1f ? rightY : 0.0f;
    if(x != 0.0f || y != 0.0f)
    {
        const float angle = atan2f(x, y) * 57.29577951308232f;
        if(angle > -45.0f && angle <= 45.0f)
            buttons |= 0x100000u;
        if(angle > 45.0f && angle <= 135.0f)
            buttons |= 0x400000u;
        if(angle > -135.0f && angle <= -45.0f)
            buttons |= 0x800000u;
        if(angle > 135.0f || angle <= -135.0f)
            buttons |= 0x200000u;
    }
    return buttons;
}

#endif
