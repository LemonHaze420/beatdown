#include "credits.h"

#include <string.h>

namespace
{

const float CREDITS_TEXT_SCALE = 1.1f;
const uint8_t CREDITS_TEXT_ALPHA = 200;
const uint16_t CREDITS_TEXT_LAYER = 0x44c;
const int CREDITS_INITIAL_TEXT_ROWS = 20;

bool resourcesReleased(const CreditsSequenceState& state)
{
    return state.phase == CREDITS_PHASE_POST_EXIT_WAIT ||
        state.phase == CREDITS_PHASE_FINISHED;
}

bool logoPhase(const CreditsSequenceState& state)
{
    return state.phase == CREDITS_PHASE_LOGO_FADE_IN ||
        state.phase == CREDITS_PHASE_LOGO_HOLD ||
        state.phase == CREDITS_PHASE_LOGO_FADE_OUT;
}

void enterPhase(CreditsSequenceState& state, CreditsSequencePhase phase,
                uint32_t duration)
{
    state.phase = phase;
    state.phaseFrame = 0;
    state.phaseDuration = duration;
}

void startLogoPage(CreditsSequenceState& state)
{
    enterPhase(state, CREDITS_PHASE_LOGO_FADE_IN,
               CREDITS_LOGO_FADE_FRAMES);
}

void advanceLogoPage(CreditsSequenceState& state)
{
    ++state.currentLogoPage;

    if(state.currentLogoPage == 1)
        state.currentLogoPage = 2;
    if(state.currentLogoPage < CREDITS_LOGO_COUNT)
        startLogoPage(state);
    else
        enterPhase(state, CREDITS_PHASE_PRE_EXIT_WAIT,
                   scaleCreditsFrameCount(CREDITS_EXIT_WAIT_FRAMES,
                                          state.timingMode60Hz != 0));
}

void advanceTextScroll(CreditsSequenceState& state)
{
    state.scrollY += state.lineStep * 2.0f;

    for(int index = 0; index < CREDITS_TEXT_ROW_COUNT; ++index)
    {
        if(!state.activeTextRows[index])
            continue;
        if(CREDITS_TEXT_ROWS[index].y - state.scrollY < -40.0f)
        {
            state.activeTextRows[index] = false;
            --state.activeTextCount;
        }
    }

    while(state.nextTextRow < CREDITS_TEXT_ROW_COUNT &&
          CREDITS_TEXT_ROWS[state.nextTextRow].y - state.scrollY < 640.0f &&
          state.activeTextCount < CREDITS_TEXT_HANDLE_CAPACITY)
    {
        state.activeTextRows[state.nextTextRow] = true;
        ++state.activeTextCount;
        ++state.nextTextRow;
    }
}

void setTextStyle(CreditsTextDraw& draw, uint8_t style)
{
    draw.scale = CREDITS_TEXT_SCALE;
    draw.alpha = CREDITS_TEXT_ALPHA;
    draw.layer = CREDITS_TEXT_LAYER;
    draw.centered = 1;
    draw.reserved = 0;
    if((style & 0x0f) == 1)
    {
        draw.red = 0x71;
        draw.green = 0xee;
        draw.blue = 0xc8;
    }
    else
    {
        draw.red = 0xc8;
        draw.green = 0xc8;
        draw.blue = 0xc8;
    }
}

}

uint32_t scaleCreditsFrameCount(uint32_t frames, bool timingMode60Hz)
{

    return timingMode60Hz ? frames :
        static_cast<uint32_t>(static_cast<float>(frames) * 0.8333334f);
}

bool creditsShouldTrigger(uint8_t titleProgressClass)
{

    return titleProgressClass >= 3;
}

void resetCreditsSequence(CreditsSequenceState& state, uint8_t language,
                          bool timingMode60Hz)
{
    memset(&state, 0, sizeof(state));
    state.phase = CREDITS_PHASE_INITIAL_WAIT;
    state.phaseDuration = scaleCreditsFrameCount(
        CREDITS_INITIAL_WAIT_FRAMES, timingMode60Hz);
    state.language = language;
    state.timingMode60Hz = timingMode60Hz ? 1 : 0;
    const uint32_t duration = scaleCreditsFrameCount(
        CREDITS_BASE_DURATION_FRAMES, timingMode60Hz);
    state.lineStep = CREDITS_SCROLL_DISTANCE / static_cast<float>(duration);
    state.currentLogoPage = 0;

    for(int index = 0; index < CREDITS_INITIAL_TEXT_ROWS; ++index)
        state.activeTextRows[index] = true;
    state.activeTextCount = CREDITS_INITIAL_TEXT_ROWS;
    state.nextTextRow = CREDITS_INITIAL_TEXT_ROWS;
}

uint32_t updateCreditsSequence(CreditsSequenceState& state, bool flowReady,
                               bool r1Pressed)
{
    uint32_t events = CREDITS_EVENT_NONE;
    switch(state.phase)
    {
    case CREDITS_PHASE_INITIAL_WAIT:
        if(state.phaseFrame == 0)
            events |= CREDITS_EVENT_STOP_MUSIC;
        ++state.phaseFrame;
        if(state.phaseFrame >= state.phaseDuration)
        {
            events |= CREDITS_EVENT_SET_CHARACTER_SELECT_STATE_4;
            enterPhase(state, CREDITS_PHASE_NOTICE,
                       scaleCreditsFrameCount(CREDITS_NOTICE_FRAMES,
                                              state.timingMode60Hz != 0));
        }
        break;

    case CREDITS_PHASE_NOTICE:

        if(flowReady)
        {
            ++state.phaseFrame;
            if(state.phaseFrame >= state.phaseDuration)
            {
                events |= CREDITS_EVENT_START_MUSIC;
                enterPhase(state, CREDITS_PHASE_SCROLL, 0);
            }
        }
        break;

    case CREDITS_PHASE_SCROLL:
        ++state.scrollFrameCounter;
        if((state.scrollFrameCounter & 1u) == 0)
        {
            advanceTextScroll(state);

            if(r1Pressed)
                state.lineStep = 80.0f;
            if(state.scrollY >= CREDITS_SCROLL_DISTANCE)
            {
                events |= CREDITS_EVENT_STOP_MUSIC;
                startLogoPage(state);
            }
        }
        break;

    case CREDITS_PHASE_LOGO_FADE_IN:
        ++state.phaseFrame;
        if(state.phaseFrame >= state.phaseDuration)
            enterPhase(state, CREDITS_PHASE_LOGO_HOLD,
                       scaleCreditsFrameCount(CREDITS_LOGO_HOLD_FRAMES,
                                              state.timingMode60Hz != 0));
        break;

    case CREDITS_PHASE_LOGO_HOLD:
        ++state.phaseFrame;
        if(state.phaseFrame >= state.phaseDuration)
            enterPhase(state, CREDITS_PHASE_LOGO_FADE_OUT,
                       CREDITS_LOGO_FADE_FRAMES);
        break;

    case CREDITS_PHASE_LOGO_FADE_OUT:
        ++state.phaseFrame;
        if(state.phaseFrame >= state.phaseDuration)
            advanceLogoPage(state);
        break;

    case CREDITS_PHASE_PRE_EXIT_WAIT:
        ++state.phaseFrame;
        if(state.phaseFrame >= state.phaseDuration)
        {
            events |= CREDITS_EVENT_SET_CHARACTER_SELECT_STATE_3;
            enterPhase(state, CREDITS_PHASE_WAIT_FOR_CHARACTER_SELECT, 0);
        }
        break;

    case CREDITS_PHASE_WAIT_FOR_CHARACTER_SELECT:
        if(flowReady)
        {
            events |= CREDITS_EVENT_RELEASE_RESOURCES;
            enterPhase(state, CREDITS_PHASE_POST_EXIT_WAIT,
                       scaleCreditsFrameCount(CREDITS_EXIT_WAIT_FRAMES,
                                              state.timingMode60Hz != 0));
        }
        break;

    case CREDITS_PHASE_POST_EXIT_WAIT:
        ++state.phaseFrame;
        if(state.phaseFrame >= state.phaseDuration)
            enterPhase(state, CREDITS_PHASE_FINISHED, 0);
        break;

    case CREDITS_PHASE_FINISHED:
        break;
    }
    return events;
}

bool creditsSequenceFinished(const CreditsSequenceState& state)
{
    return state.phase == CREDITS_PHASE_FINISHED;
}

uint16_t creditsNoticeTextId(const CreditsSequenceState& state)
{
    return state.language == 0 ? CREDITS_NOTICE_JAPANESE_TEXT_ID :
        CREDITS_NOTICE_DEFAULT_TEXT_ID;
}

int creditsActiveText(const CreditsSequenceState& state,
                      CreditsTextDraw* draws, int capacity)
{
    if(draws == 0 || capacity < 1 || resourcesReleased(state))
        return 0;
    int count = 0;
    for(int index = 0;
        index < CREDITS_TEXT_ROW_COUNT && count < capacity; ++index)
    {
        if(!state.activeTextRows[index])
            continue;
        const CreditsTextRow& row = CREDITS_TEXT_ROWS[index];
        CreditsTextDraw& draw = draws[count++];
        draw.textId = row.textId;
        draw.x = row.x;
        draw.y = row.y - state.scrollY;
        setTextStyle(draw, row.style);
    }
    return count;
}

bool creditsBottomText(const CreditsSequenceState& state,
                       CreditsTextDraw& draw)
{
    if(resourcesReleased(state))
        return false;

    draw.textId = CREDITS_BOTTOM_TEXT_ID;
    draw.x = 120.0f;
    draw.y = 480.0f;
    draw.scale = 1.0f;
    draw.red = 0xc0;
    draw.green = 0xc0;
    draw.blue = 0x80;
    draw.alpha = 0xff;
    draw.layer = 0x960;
    draw.centered = 1;
    draw.reserved = 0;
    return true;
}
int creditsBackdropDraws(const CreditsSequenceState& state,
                         CreditsBackdropDraw* draws, int capacity)
{
    if(draws == 0 || capacity < 1 || resourcesReleased(state))
        return 0;

    static const CreditsBackdropDraw BACKDROPS[CREDITS_BACKDROP_COUNT] =
    {
        { 320.0f, 256.0f, 640.0f, 490.0f, 0x3e8,
          0, 0, 0, { 0xff, 0xff, 0xff, 0xff } },
        { 320.0f, 35.0f, 640.0f, 60.0f, 0x5dc,
          0, 0, 0, { 0xff, 0xff, 0, 0 } },
        { 320.0f, 470.0f, 640.0f, 60.0f, 0x5dc,
          0, 0, 0, { 0, 0, 0xff, 0xff } },
    };
    const int count = capacity < CREDITS_BACKDROP_COUNT ?
        capacity : CREDITS_BACKDROP_COUNT;
    memcpy(draws, BACKDROPS,
           static_cast<size_t>(count) * sizeof(CreditsBackdropDraw));
    return count;
}

int creditsLogoGraphIndex(const CreditsSequenceState& state)
{
    if(!logoPhase(state) || state.currentLogoPage >= CREDITS_LOGO_COUNT)
        return -1;
    return CREDITS_LOGO_GRAPH_INDICES[state.currentLogoPage];
}

uint8_t creditsLogoAlpha(const CreditsSequenceState& state)
{
    if(!logoPhase(state) || state.phaseDuration == 0)
        return 0;
    if(state.phase == CREDITS_PHASE_LOGO_HOLD)
        return 0xff;
    if(state.phase == CREDITS_PHASE_LOGO_FADE_IN)
    {
        const uint32_t alpha = state.phaseFrame * 255u / state.phaseDuration;
        return static_cast<uint8_t>(alpha > 255u ? 255u : alpha);
    }
    const uint32_t remaining = state.phaseFrame >= state.phaseDuration ? 0 :
        state.phaseDuration - state.phaseFrame;
    return static_cast<uint8_t>(remaining * 255u / state.phaseDuration);
}

const CreditsLogoLayout* creditsLogoLayout(const CreditsSequenceState& state)
{
    if(!logoPhase(state) || state.currentLogoPage >= CREDITS_LOGO_COUNT)
        return 0;
    return CREDITS_LOGO_LAYOUTS + state.currentLogoPage;
}
