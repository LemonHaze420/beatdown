#include "game/cpu_input.h"
#include "game/input_directions.h"
#include "move_input.h"
#include "stage/ai.h"

#include <math.h>

namespace
{

int buttonLayout(const CpuCombatInputContext& context)
{
    int layout = context.controllerLayout;
    if(layout >= 6)
        layout = 0;
    return context.alternateLayoutOrder ?
        CPU_COMBAT_ALTERNATE_LAYOUT_ORDER[layout] : layout;
}

uint32_t canonicalButtons(uint32_t buttons, int layout)
{
    uint32_t canonical = buttons & ~0x3c0u;
    const uint32_t* masks = CPU_COMBAT_BUTTON_LAYOUTS[layout];
    if((buttons & masks[0]) != 0)
        canonical |= MOVE_INPUT_SLOT_MASKS[0];
    if((buttons & masks[1]) != 0)
        canonical |= MOVE_INPUT_SLOT_MASKS[1];
    if((buttons & masks[2]) != 0)
        canonical |= MOVE_INPUT_SLOT_MASKS[3];
    if((buttons & masks[3]) != 0)
        canonical |= MOVE_INPUT_SLOT_MASKS[6];
    return canonical;
}

}

void updateCpuCombatInput(CpuCombatInputState& state, uint16_t logicalInput,
                          const float channels[4],
                          const CpuCombatInputContext& context)
{
    const uint32_t previous = state.currentButtons;
    const uint32_t previousCanonical = state.canonicalButtons;
    state = {};
    state.leftX = channels[0];
    state.leftY = channels[1];
    state.rightX = channels[2];
    state.rightY = channels[3];

    float deltaX = context.deltaX;
    float deltaZ = context.deltaZ;
    const float length = sqrtf(deltaX * deltaX + deltaZ * deltaZ);
    if(length > 0.0f)
    {
        deltaX /= length;
        deltaZ /= length;
    }
    const float cameraHeading =
        atan2f(context.cameraForwardX, context.cameraForwardZ);
    const float cosine = cosf(cameraHeading);
    const float sine = sinf(cameraHeading);
    const float rotatedX = deltaX * cosine - deltaZ * sine;
    const float rotatedZ = deltaX * sine + deltaZ * cosine;

    uint32_t buttons = 0;
    if((logicalInput & 0xfu) != 0)
    {
        if((context.actorUpdateFlags & 4u) != 0)
        {
            float delta = atan2f(context.deltaX, context.deltaZ) *
                57.29577951308232f - context.actorHeadingDegrees;
            while(delta > 180.0f) delta -= 360.0f;
            while(delta < -180.0f) delta += 360.0f;
            state.clearMovementFacing = fabsf(delta) > 90.0f;
        }
        float angle = atan2f(rotatedX, rotatedZ) *
            57.29577951308232f + 202.5f;

        if(angle > 360.0f)
            angle = 360.0f - angle;
        const unsigned direction =
            static_cast<unsigned>(static_cast<int>(angle / 45.0f) - 2) & 7u;
        const uint16_t* masks = CPU_COMBAT_DIRECTION_MASKS[direction];
        if((logicalInput & AI_DIRECTION_LEFT) != 0) buttons |= masks[0];
        if((logicalInput & AI_DIRECTION_TOWARD) != 0) buttons |= masks[1];
        if((logicalInput & AI_DIRECTION_RIGHT) != 0) buttons |= masks[2];
        if((logicalInput & AI_DIRECTION_AWAY) != 0) buttons |= masks[3];
    }
    if(fmaxf(fabsf(state.leftX), fabsf(state.leftY)) > 0.3f)
    {
        const float x = state.leftX;
        const float y = state.leftY;
        state.leftX = -x * rotatedX - y * rotatedZ;
        state.leftY = -x * rotatedZ + y * rotatedX;
    }

    int approach = 0;
    if((logicalInput & AI_INPUT_RUN_TO_TARGET) != 0)
    {
        if((logicalInput & AI_DIRECTION_TOWARD) != 0) approach = 2;
        else if((logicalInput & AI_DIRECTION_AWAY) != 0) approach = 3;
        else if((logicalInput & AI_DIRECTION_RIGHT) != 0) approach = 4;
        else if((logicalInput & AI_DIRECTION_LEFT) != 0) approach = 5;
        else approach = 1;
    }
    if(fabsf(context.deltaY) >= 35.0f)
        approach = 6;

    state.grabRequested = (logicalInput & AI_INPUT_GRAB) != 0;
    if(approach != 0)
    {
        state.buttonsCleared = true;
        logicalInput = 0;
        buttons = 0;
        state.leftX = state.leftY = state.rightX = state.rightY = 0.0f;
        if(approach != 6)
        {
            float offset = 0.0f;

            if(approach == 2) offset = 70.0f;
            else if(approach == 3) offset = -50.0f;
            else if(approach == 4) offset = 70.0f;
            else if(approach == 5) offset = -70.0f;
            const float angle = atan2f(rotatedX, rotatedZ) +
                offset * 0.01745329251994329577f;
            state.leftX = sinf(angle);
            state.leftY = cosf(angle);
            if(approach <= 3)
                buttons = CPU_COMBAT_RUN_BUTTON;
        }
    }
    const int layout = buttonLayout(context);
    const uint32_t* masks = CPU_COMBAT_BUTTON_LAYOUTS[layout];
    if((logicalInput & AI_INPUT_ATTACK_0) != 0) buttons |= masks[0];
    if((logicalInput & AI_INPUT_ATTACK_1) != 0) buttons |= masks[1];
    if((logicalInput & AI_INPUT_GUARD) != 0) buttons |= masks[2];
    if((logicalInput & AI_INPUT_GRAB) != 0) buttons |= masks[3];
    if((logicalInput & AI_INPUT_EVADE_0) != 0) buttons |= 0x1000u;
    if((logicalInput & AI_INPUT_UNMAPPED_0) != 0) buttons |= 0x800u;
    if((logicalInput & AI_INPUT_UNMAPPED_1) != 0) buttons |= 0x2000u;
    if((logicalInput & AI_INPUT_EVADE_1) != 0) buttons |= 0x8000u;
    state.rawButtons = buttons;
    state.currentButtons = combatStickDirectionCode(
        state.leftX, state.leftY, state.rightX, state.rightY, buttons);
    state.pressedButtons = state.currentButtons & ~previous;
    state.releasedButtons = previous & ~state.currentButtons;
    state.canonicalButtons = canonicalButtons(state.currentButtons, layout);
    state.canonicalPressedButtons = state.canonicalButtons & ~previousCanonical;
    state.canonicalReleasedButtons = previousCanonical & ~state.canonicalButtons;
}
