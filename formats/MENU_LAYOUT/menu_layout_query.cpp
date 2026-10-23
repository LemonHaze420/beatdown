#include "menu_layout.h"

const char* const MENU_VERSUS_SEPARATOR[MENU_VERSUS_LANGUAGE_COUNT] =
{
    "VS",
    "VS",
    "/",
    "GEGEN",
    "-",
    "GEGEN"
};

const MenuItemColour STAGE_SELECT_TITLE_COLOUR = { 0x80, 0xc0, 0xff, 0x80 };
const MenuItemColour STAGE_SELECT_ROW_COLOUR = { 0xc0, 0xc0, 0xc0, 0x80 };

const MenuItemColour CHARACTER_SELECT_STYLE_COLOUR = { 0xff, 0xff, 0x5f, 0xc0 };
void
menuPulseReset(MenuPulse& pulse)
{
    pulse.value = 0;
    pulse.step = MENU_PULSE_STEP;
}
int
menuPulseAdvance(MenuPulse& pulse)
{
    pulse.value += pulse.step;
    if(pulse.value > MENU_PULSE_MAX)
    {

        pulse.step = -MENU_PULSE_STEP;
        pulse.value = MENU_PULSE_MAX;
    }
    else if(pulse.value < 0)
    {
        pulse.value = 0;
        pulse.step = MENU_PULSE_STEP;
    }
    return pulse.value;
}
int
menuPulseAdvanceElapsed(
    MenuPulse& pulse, float& frameAccumulator, float elapsedFrames)
{
    if(elapsedFrames <= 0.0f)
        return pulse.value;

    frameAccumulator += elapsedFrames;

    const int wholeFrames = (int)(frameAccumulator + 0.00001f);
    frameAccumulator -= wholeFrames;
    if(frameAccumulator < 0.0f)
        frameAccumulator = 0.0f;
    for(int i = 0; i < wholeFrames; ++i)
        menuPulseAdvance(pulse);
    return pulse.value;
}
uint8_t
menuPulseHighlight(int value)
{
    if(value < 0)
        value = 0;
    if(value > MENU_PULSE_MAX)
        value = MENU_PULSE_MAX;
    return (uint8_t)((value * MENU_PULSE_HIGHLIGHT_SCALE) / MENU_PULSE_MAX);
}
float
stageSelectRowY(int offset)
{
    return (float)offset * STAGE_SELECT_PITCH + STAGE_SELECT_CENTRE_Y;
}
int
stageSelectRowIndex(int selected, int offset, int count)
{
    if(count <= 0)
        return 0;

    int index = selected + offset;
    if(index < 0)
        index += count;
    if(index >= count)
        index -= count;
    return index;
}
