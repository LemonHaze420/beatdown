#include "menu_layout.h"
const MenuItemLayout MENU_ITEM_LAYOUT[MENU_ITEM_COUNT] =
{
    {    320.0f,    389.0f, 0.500f,     0, " " },
    {    320.0f,    360.0f, 0.700f,  9000, "PRESS START BUTTON" },
    {    320.0f,    293.0f, 0.700f,  9001, "NEW GAME" },
    {    320.0f,    315.0f, 0.700f,  9002, "LOAD GAME" },
    {    320.0f,    337.0f, 0.700f,  9003, "VS MODE NORMAL" },
    {    320.0f,    359.0f, 0.700f,  9004, "VS MODE CUSTOM" },
    {    320.0f,    381.0f, 0.700f, 52121, "OPTIONS" },
    {    320.0f,    403.0f, 0.700f,     0, "DEBUG" },
};
const MenuItemColour MENU_ITEM_COLOURS[MENU_COLOUR_COUNT] =
{
    { 0x77, 0xff, 0xce, 0xff },
    { 0x71, 0xee, 0xc8, 0xc0 },
    { 0x80, 0x80, 0x80, 0x80 },
};
const VersusBannerAuthoredRecord
    VERSUS_BANNER_AUTHORED_RECORDS[VERSUS_BANNER_RECORD_COUNT] =
{
    {
        {
            { -320.0f, 140.0f, 16.0f, 160.0f, { 0.0f, 0.0f, 0.0f, 0.0f } },
            { 0.0f, 0.0f, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } },
            { 0.0f, 0.0f, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } },
            { 0.0f, 0.0f, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } },
            { 0.0f, 0.0f, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } },
            { 0.0f, 0.0f, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } },
            { 0.0f, 0.0f, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } },
            { 0.0f, 0.0f, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } },
            { 0.0f, 0.0f, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } }
        }
    },
    {
        {
            { 960.0f, 140.0f, -16.0f, 480.0f, { 0.0f, 0.0f, 0.0f, 0.0f } },
            { 0.0f, 0.0f, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } },
            { 0.0f, 0.0f, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } },
            { 0.0f, 0.0f, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } },
            { 0.0f, 0.0f, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } },
            { 0.0f, 0.0f, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } },
            { 0.0f, 0.0f, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } },
            { 0.0f, 0.0f, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } },
            { 0.0f, 0.0f, 0.0f, 0.0f, { 0.0f, 0.0f, 0.0f, 0.0f } }
        }
    }
};
void initializeVersusBannerMotion(VersusBannerMotionState& state)
{
    for(int side = 0; side < VERSUS_BANNER_RECORD_COUNT; ++side)
    {
        const VersusBannerAuthoredSubrecord& authored =
            VERSUS_BANNER_AUTHORED_RECORDS[side].subrecords[0];
        state.x[side] = authored.initialX;
        state.y[side] = authored.y;
        state.velocityX[side] = authored.velocityX;
        state.entryClampX[side] = authored.entryClampX;
    }
    state.separatorY = 479.0f;
    state.holdSeconds = 0.0f;
    state.phase = VERSUS_BANNER_ENTRY;
    state.active = true;
}
void requestVersusBannerExit(VersusBannerMotionState& state)
{
    if(!state.active)
        return;
    state.phase = VERSUS_BANNER_EXIT_PREPARE;
}
bool advanceVersusBannerMotion(VersusBannerMotionState& state,
                               float scaledDeltaSeconds)
{
    if(!state.active)
        return false;
    if(scaledDeltaSeconds < 0.0f)
        scaledDeltaSeconds = 0.0f;
    if(state.phase == VERSUS_BANNER_ENTRY)
    {
        state.separatorY -= 16.0f;
        if(state.separatorY < 239.0f)
            state.separatorY = 239.0f;
        bool settled = true;
        for(int side = 0; side < VERSUS_BANNER_RECORD_COUNT; ++side)
        {
            state.x[side] += state.velocityX[side];
            if(state.velocityX[side] < 0.0f)
            {
                if(state.x[side] < state.entryClampX[side])
                    state.x[side] = state.entryClampX[side];
            }
            else if(state.entryClampX[side] < state.x[side])
                state.x[side] = state.entryClampX[side];
            if(state.x[side] != state.entryClampX[side])
                settled = false;
        }
        if(settled)
        {
            state.holdSeconds += scaledDeltaSeconds;
            if(state.holdSeconds > 4.0f)
                requestVersusBannerExit(state);
        }
        return true;
    }
    if(state.phase == VERSUS_BANNER_EXIT_PREPARE)
    {

        state.x[0] = -100.0f;
        state.x[1] = 740.0f;
        state.separatorY = 239.0f;
        state.phase = VERSUS_BANNER_EXIT;
    }
    if(state.phase == VERSUS_BANNER_EXIT)
    {
        state.separatorY += 16.0f;
        if(state.separatorY > 512.0f)
            state.separatorY = 512.0f;
        bool finished = true;
        for(int side = 0; side < VERSUS_BANNER_RECORD_COUNT; ++side)
        {
            state.x[side] -= state.velocityX[side];
            const float exitClamp = VERSUS_BANNER_AUTHORED_RECORDS[side].
                subrecords[0].initialX;
            if(state.velocityX[side] < 0.0f)
            {
                if(exitClamp < state.x[side])
                    state.x[side] = exitClamp;
            }
            else if(state.x[side] < exitClamp)
                state.x[side] = exitClamp;
            if(state.x[side] != exitClamp)
                finished = false;
        }
        if(finished)
        {
            state.phase = VERSUS_BANNER_FINISHED;
            state.active = false;
        }
        return true;
    }
    return false;
}
