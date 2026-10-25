#ifndef GAME_CPU_INPUT_H
#define GAME_CPU_INPUT_H

#include <stdint.h>

inline constexpr uint16_t CPU_COMBAT_DIRECTION_MASKS[8][4] =
{
    { 4, 2, 8, 1 }, { 4, 2, 8, 1 }, { 1, 4, 2, 8 }, { 4, 1, 8, 2 },
    { 4, 1, 8, 2 }, { 4, 1, 8, 2 }, { 2, 8, 1, 4 }, { 4, 2, 8, 1 }
};

inline constexpr uint32_t CPU_COMBAT_BUTTON_LAYOUTS[6][4] =
{
    { 0x100, 0x040, 0x080, 0x200 },
    { 0x100, 0x080, 0x040, 0x200 },
    { 0x080, 0x100, 0x040, 0x200 },
    { 0x080, 0x200, 0x100, 0x040 },
    { 0x040, 0x200, 0x100, 0x080 },
    { 0x040, 0x080, 0x100, 0x200 }
};
inline constexpr uint8_t CPU_COMBAT_ALTERNATE_LAYOUT_ORDER[6] = { 3, 0, 4, 1, 2, 5 };
inline constexpr uint32_t CPU_COMBAT_RUN_BUTTON = 0x400;

struct CpuCombatInputContext
{
    float deltaX;
    float deltaY;
    float deltaZ;
    float cameraForwardX;
    float cameraForwardZ;
    float actorHeadingDegrees;
    uint32_t actorUpdateFlags;
    uint8_t controllerLayout;
    bool alternateLayoutOrder;
};

struct CpuCombatInputState
{
    uint32_t rawButtons;
    uint32_t currentButtons;
    uint32_t pressedButtons;
    uint32_t releasedButtons;
    uint32_t canonicalButtons;
    uint32_t canonicalPressedButtons;
    uint32_t canonicalReleasedButtons;
    float leftX;
    float leftY;
    float rightX;
    float rightY;
    bool clearMovementFacing;
    bool grabRequested;
    bool buttonsCleared;
};

void updateCpuCombatInput(CpuCombatInputState& state, uint16_t logicalInput,
                          const float channels[4],
                          const CpuCombatInputContext& context);

#endif
