#include "title_attract.h"

#include <stdio.h>

const TitleMenuRecord TITLE_MENU_RECORDS[TITLE_MENU_ENTRY_COUNT] =
{
    {  9001, 0, true },
    {  9002, 1, true },
    {  9003, 2, true },
    {  9004, 3, true },
    { 52121, 4, true }
};

void
titleAttractReset(TitleAttractState& state, bool menuReady)
{
    state.fadeState = 1;
    state.fadeAlpha = 0;
    state.inputState = 1;
    state.promptPulse = 0;
    state.inputActive = false;
    state.menuReady = menuReady;
    state.acceptedThisFrame = false;
    state.sceneDrawn = false;
}

namespace
{

void tickFade(TitleAttractState& state, int mode)
{

    state.sceneDrawn = state.fadeState >= 3;
    switch(state.fadeState)
    {
    case 1:

        if(mode != TITLE_ATTRACT_VIDEO)
        {
            state.fadeAlpha = TITLE_ATTRACT_MAX_ALPHA;
            state.fadeState = 4;
        }
        else
        {
            state.fadeAlpha = 0;
            state.fadeState = 2;
        }
        break;
    case 2:

        if(state.fadeAlpha + 1 < TITLE_ATTRACT_HOLD_FRAMES)
        {
            ++state.fadeAlpha;
            break;
        }
        state.fadeAlpha = 0;
        state.fadeState = 3;
        break;
    case 3:
        state.fadeAlpha += TITLE_ATTRACT_FADE_STEP;
        if(state.fadeAlpha > TITLE_ATTRACT_MAX_ALPHA)
        {
            state.fadeAlpha = TITLE_ATTRACT_MAX_ALPHA;
            state.fadeState = 4;
        }
        break;
    case 4:
        break;
    case 5:
        state.fadeAlpha = TITLE_ATTRACT_MAX_ALPHA;
        state.fadeState = 4;
        break;
    default:
        break;
    }
}

void tickInput(TitleAttractState& state, int mode, bool startPressed)
{
    switch(state.inputState)
    {
    case 1:
        if(mode == TITLE_ATTRACT_VIDEO)
        {
            state.inputActive = false;
            state.menuReady = false;
            state.inputState = 2;
        }
        else
        {
            state.inputActive = true;
            state.inputState = 3;
        }
        break;
    case 2:

        if(state.menuReady && startPressed)
        {
            state.acceptedThisFrame = true;
            state.inputActive = true;
            state.inputState = 3;
        }
        break;
    case 3:
        if(!state.inputActive)
            state.inputState = 2;
        break;
    default:
        break;
    }
}

}

void
titleAttractTick(TitleAttractState& state, int mode, bool menuReady,
                 bool startPressed)
{
    state.acceptedThisFrame = false;

    if(state.inputState != 1)
        state.menuReady = menuReady;
    tickFade(state, mode);
    tickInput(state, mode, startPressed);
    if(titleAttractPromptVisible(state))
        state.promptPulse =
            (state.promptPulse + 1) % (uint32_t)TITLE_ATTRACT_PROMPT_PERIOD;
}

void
titleAttractAdvanceElapsed(
    TitleAttractState& state, float& frameAccumulator, float elapsedFrames,
    int mode, bool menuReady)
{
    if(elapsedFrames <= 0.0f)
        return;

    frameAccumulator += elapsedFrames;
    const int wholeFrames = (int)(frameAccumulator + 0.00001f);
    frameAccumulator -= wholeFrames;
    if(frameAccumulator < 0.0f)
        frameAccumulator = 0.0f;
    for(int i = 0; i < wholeFrames; ++i)
        titleAttractTick(state, mode, menuReady, false);
}

bool
titleAttractSceneVisible(const TitleAttractState& state)
{
    return state.sceneDrawn;
}

uint8_t
titleAttractSceneAlpha(const TitleAttractState& state)
{
    if(state.fadeAlpha <= 0)
        return 0;
    if(state.fadeAlpha >= TITLE_ATTRACT_MAX_ALPHA)
        return (uint8_t)TITLE_ATTRACT_MAX_ALPHA;
    return (uint8_t)state.fadeAlpha;
}

float
titleAttractSceneScale(const TitleAttractState& state)
{
    const float alpha = (float)titleAttractSceneAlpha(state);
    return 1.5f - (alpha / (float)TITLE_ATTRACT_MAX_ALPHA) / 2.0f;
}

bool
titleAttractPromptVisible(const TitleAttractState& state)
{
    return state.inputState == 2 && state.menuReady;
}

uint8_t
titleAttractPromptAlpha(const TitleAttractState& state)
{

    const uint32_t half = (uint32_t)TITLE_ATTRACT_PROMPT_PERIOD / 2;
    int32_t level;
    if(state.promptPulse < half)
        level = (int32_t)(state.promptPulse * TITLE_ATTRACT_PROMPT_GAIN);
    else
        level = (int32_t)(TITLE_ATTRACT_PROMPT_PERIOD *
                          TITLE_ATTRACT_PROMPT_GAIN) -
            (int32_t)state.promptPulse * TITLE_ATTRACT_PROMPT_GAIN;
    if(level > TITLE_ATTRACT_MAX_ALPHA)
        level = TITLE_ATTRACT_MAX_ALPHA;
    if(level < 0)
        level = 0;
    return (uint8_t)level;
}

bool
buildAttractMoviePath(char* out, size_t size, int movieIndex, bool sixtyHertz,
                      bool monoVariant)
{
    if(out == NULL || size == 0)
        return false;
    if(movieIndex < 0 || movieIndex >= TITLE_ATTRACT_MOVIE_COUNT)
        return false;

    const int written = snprintf(out, size, "raw/MOVIES/BD%d_%dP%s.PSS",
                                 movieIndex + 1, sixtyHertz ? 60 : 50,
                                 monoVariant ? "M" : "");
    return written > 0 && (size_t)written < size;
}
