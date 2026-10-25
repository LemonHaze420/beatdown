#ifndef TITLE_ATTRACT_H
#define TITLE_ATTRACT_H

#include <stddef.h>
#include <stdint.h>

enum TitleMenuEntry
{
    TITLE_MENU_NEW_GAME = 0,
    TITLE_MENU_LOAD_GAME = 1,
    TITLE_MENU_VS_NORMAL = 2,
    TITLE_MENU_VS_CUSTOM = 3,
    TITLE_MENU_OPTIONS = 4,
    TITLE_MENU_ENTRY_COUNT = 5
};

struct TitleMenuRecord
{
    uint16_t textId;
    uint8_t action;
    bool enabled;
};

extern const TitleMenuRecord TITLE_MENU_RECORDS[TITLE_MENU_ENTRY_COUNT];

const float TITLE_TRANSITION_SECONDS = 34.633335f;

const int TITLE_ATTRACT_MOVIE_COUNT = 2;

enum TitleAttractMode
{

    TITLE_ATTRACT_VIDEO = 0,
    TITLE_ATTRACT_INTERACTIVE = 1
};

const int TITLE_ATTRACT_HOLD_FRAMES = 349;

const int TITLE_ATTRACT_FADE_STEP = 16;
const int TITLE_ATTRACT_MAX_ALPHA = 255;

const int TITLE_ATTRACT_PROMPT_PERIOD = 64;
const int TITLE_ATTRACT_PROMPT_GAIN = 8;

const uint8_t TITLE_ATTRACT_PROMPT_GREY = 200;

const uint16_t TITLE_ATTRACT_PROMPT_TEXT = 9000;

struct TitleAttractState
{

    uint8_t fadeState;

    int32_t fadeAlpha;

    uint8_t inputState;

    uint32_t promptPulse;
    bool inputActive;
    bool menuReady;

    bool acceptedThisFrame;

    bool sceneDrawn;
};

void titleAttractReset(TitleAttractState& state, bool menuReady);

bool buildAttractMoviePath(char* out, size_t size, int movieIndex,
                           bool sixtyHertz, bool monoVariant);

void titleAttractTick(TitleAttractState& state, int mode, bool menuReady,
                      bool startPressed);

void titleAttractAdvanceElapsed(
    TitleAttractState& state, float& frameAccumulator, float elapsedFrames,
    int mode, bool menuReady);

bool titleAttractSceneVisible(const TitleAttractState& state);
uint8_t titleAttractSceneAlpha(const TitleAttractState& state);

float titleAttractSceneScale(const TitleAttractState& state);

bool titleAttractPromptVisible(const TitleAttractState& state);
uint8_t titleAttractPromptAlpha(const TitleAttractState& state);

#endif
