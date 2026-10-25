#ifndef REIMPL_CREDITS_H
#define REIMPL_CREDITS_H

#include <stdint.h>

#include "formats/CREDITS/credits_data.h"

const uint32_t CREDITS_BASE_DURATION_FRAMES = 0x272b;
const uint32_t CREDITS_INITIAL_WAIT_FRAMES = 0x50;
const uint32_t CREDITS_NOTICE_FRAMES = 0x78;
const uint32_t CREDITS_LOGO_FADE_FRAMES = 0x3c;
const uint32_t CREDITS_LOGO_HOLD_FRAMES = 0x78;
const uint32_t CREDITS_EXIT_WAIT_FRAMES = 0x1e;
const uint32_t CREDITS_MUSIC_TRACK = 41;
const uint16_t CREDITS_BOTTOM_TEXT_ID = 0x171d;
const uint16_t CREDITS_NOTICE_DEFAULT_TEXT_ID = 0x7534;
const uint16_t CREDITS_NOTICE_JAPANESE_TEXT_ID = 0x7535;
const int CREDITS_TEXT_HANDLE_CAPACITY = 88;
const bool CREDITS_HAS_ENDING_MOVIE = false;
const int CREDITS_BACKDROP_COUNT = 3;

enum CreditsSequencePhase
{
    CREDITS_PHASE_INITIAL_WAIT = 0,
    CREDITS_PHASE_NOTICE,
    CREDITS_PHASE_SCROLL,
    CREDITS_PHASE_LOGO_FADE_IN,
    CREDITS_PHASE_LOGO_HOLD,
    CREDITS_PHASE_LOGO_FADE_OUT,
    CREDITS_PHASE_PRE_EXIT_WAIT,
    CREDITS_PHASE_WAIT_FOR_CHARACTER_SELECT,
    CREDITS_PHASE_POST_EXIT_WAIT,
    CREDITS_PHASE_FINISHED
};

enum CreditsSequenceEvent
{
    CREDITS_EVENT_NONE = 0,
    CREDITS_EVENT_STOP_MUSIC = 1 << 0,
    CREDITS_EVENT_START_MUSIC = 1 << 1,
    CREDITS_EVENT_SET_CHARACTER_SELECT_STATE_4 = 1 << 2,
    CREDITS_EVENT_SET_CHARACTER_SELECT_STATE_3 = 1 << 3,
    CREDITS_EVENT_RELEASE_RESOURCES = 1 << 4
};

struct CreditsSequenceState
{
    CreditsSequencePhase phase;
    uint32_t phaseFrame;
    uint32_t phaseDuration;
    uint32_t scrollFrameCounter;
    uint16_t nextTextRow;
    uint16_t activeTextCount;
    uint8_t currentLogoPage;
    uint8_t language;
    uint8_t timingMode60Hz;
    uint8_t reserved;
    float scrollY;
    float lineStep;
    bool activeTextRows[CREDITS_TEXT_ROW_COUNT];
};

struct CreditsTextDraw
{
    uint16_t textId;
    float x;
    float y;
    float scale;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t alpha;
    uint16_t layer;
    uint8_t centered;
    uint8_t reserved;
};
struct CreditsBackdropDraw
{
    float x;
    float y;
    float width;
    float height;
    uint16_t layer;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    uint8_t cornerAlpha[4];
};

uint32_t scaleCreditsFrameCount(uint32_t frames, bool timingMode60Hz);
bool creditsShouldTrigger(uint8_t titleProgressClass);
void resetCreditsSequence(CreditsSequenceState& state, uint8_t language,
                          bool timingMode60Hz);
uint32_t updateCreditsSequence(CreditsSequenceState& state, bool flowReady,
                               bool r1Pressed);
bool creditsSequenceFinished(const CreditsSequenceState& state);
uint16_t creditsNoticeTextId(const CreditsSequenceState& state);
int creditsActiveText(const CreditsSequenceState& state,
                      CreditsTextDraw* draws, int capacity);
bool creditsBottomText(const CreditsSequenceState& state,
                       CreditsTextDraw& draw);
int creditsBackdropDraws(const CreditsSequenceState& state,
                         CreditsBackdropDraw* draws, int capacity);
int creditsLogoGraphIndex(const CreditsSequenceState& state);
uint8_t creditsLogoAlpha(const CreditsSequenceState& state);
const CreditsLogoLayout* creditsLogoLayout(const CreditsSequenceState& state);

#endif
