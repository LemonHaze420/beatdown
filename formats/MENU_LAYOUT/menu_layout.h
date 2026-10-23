#ifndef MENU_LAYOUT_H
#define MENU_LAYOUT_H
#include <stdint.h>

const int MENU_ITEM_COUNT = 8;
const int MENU_COLOUR_COUNT = 3;

const float MENU_SCREEN_WIDTH = 640.0f;
const float MENU_SCREEN_HEIGHT = 448.0f;

enum MenuItemIndex
{
    MENU_ITEM_SPACER = 0,
    MENU_ITEM_PRESS_START = 1,
    MENU_ITEM_NEW_GAME = 2,
    MENU_ITEM_LOAD_GAME = 3,
    MENU_ITEM_VS_NORMAL = 4,
    MENU_ITEM_VS_CUSTOM = 5,
    MENU_ITEM_OPTIONS = 6,
    MENU_ITEM_DEBUG = 7
};
enum MenuColourIndex
{
    MENU_COLOUR_SELECTED = 0,
    MENU_COLOUR_ENABLED = 1,
    MENU_COLOUR_DISABLED = 2
};
struct MenuItemLayout
{
    float x;
    float y;
    float scale;

    uint16_t textId;

    const char* literal;
};
struct MenuItemColour
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

const int MENU_VERSUS_LANGUAGE_COUNT = 6;
extern const char* const MENU_VERSUS_SEPARATOR[MENU_VERSUS_LANGUAGE_COUNT];

const int VERSUS_BANNER_RECORD_COUNT = 2;
const int VERSUS_BANNER_SUBRECORD_COUNT = 9;
struct VersusBannerAuthoredSubrecord
{
    float initialX;
    float y;
    float velocityX;
    float entryClampX;
    float reserved[4];
};
struct VersusBannerAuthoredRecord
{
    VersusBannerAuthoredSubrecord subrecords[
        VERSUS_BANNER_SUBRECORD_COUNT];
};
extern const VersusBannerAuthoredRecord
    VERSUS_BANNER_AUTHORED_RECORDS[VERSUS_BANNER_RECORD_COUNT];
enum VersusBannerPhase
{
    VERSUS_BANNER_ENTRY = 1,
    VERSUS_BANNER_EXIT_PREPARE = 2,
    VERSUS_BANNER_EXIT = 3,
    VERSUS_BANNER_FINISHED = 4
};
struct VersusBannerMotionState
{
    float x[VERSUS_BANNER_RECORD_COUNT];
    float y[VERSUS_BANNER_RECORD_COUNT];
    float velocityX[VERSUS_BANNER_RECORD_COUNT];
    float entryClampX[VERSUS_BANNER_RECORD_COUNT];
    float separatorY;
    float holdSeconds;
    VersusBannerPhase phase;
    bool active;
};
void initializeVersusBannerMotion(VersusBannerMotionState& state);
void requestVersusBannerExit(VersusBannerMotionState& state);
bool advanceVersusBannerMotion(VersusBannerMotionState& state,
                               float scaledDeltaSeconds);

const float STAGE_SELECT_TITLE_X = 320.0f;
const float STAGE_SELECT_TITLE_Y = 112.0f;
const float STAGE_SELECT_TITLE_SCALE_X = 0.7f;
const float STAGE_SELECT_TITLE_SCALE_Y = 0.9f;
const float STAGE_SELECT_ITEM_X = 320.0f;
const float STAGE_SELECT_CENTRE_Y = 240.0f;
const float STAGE_SELECT_PITCH = 26.0f;
const int STAGE_SELECT_ROW_FIRST = -2;
const int STAGE_SELECT_ROW_LAST = 2;
const float STAGE_SELECT_ITEM_SCALE_X = 0.7f;
const float STAGE_SELECT_ITEM_SCALE_Y = 1.0f;

const int MENU_PULSE_STEP = 16;
const int MENU_PULSE_MAX = 255;
const int MENU_PULSE_HIGHLIGHT_SCALE = 0xe0;
struct MenuPulse
{
    int value;
    int step;
};
void menuPulseReset(MenuPulse& pulse);

int menuPulseAdvance(MenuPulse& pulse);

int menuPulseAdvanceElapsed(
    MenuPulse& pulse, float& frameAccumulator, float elapsedFrames);

uint8_t menuPulseHighlight(int value);

float stageSelectRowY(int offset);

int stageSelectRowIndex(int selected, int offset, int count);

const float CHARACTER_SELECT_COLUMN_ONE_X = 160.0f;
const float CHARACTER_SELECT_COLUMN_TWO_X = 480.0f;
const float CHARACTER_SELECT_STYLE_Y = 344.0f;
extern const MenuItemColour CHARACTER_SELECT_STYLE_COLOUR;
extern const MenuItemColour STAGE_SELECT_TITLE_COLOUR;
extern const MenuItemColour STAGE_SELECT_ROW_COLOUR;
extern const MenuItemLayout MENU_ITEM_LAYOUT[MENU_ITEM_COUNT];
extern const MenuItemColour MENU_ITEM_COLOURS[MENU_COLOUR_COUNT];
#endif
