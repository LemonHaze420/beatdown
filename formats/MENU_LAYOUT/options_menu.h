#ifndef OPTIONS_MENU_H
#define OPTIONS_MENU_H
#include <stdint.h>
const int OPTION_PAGE_COUNT = 6;
const float OPTIONS_HEADING_X = 320.0f;
const float OPTIONS_HEADING_Y = 44.0f;
const float OPTIONS_HEADING_SCALE = 0.7f;
const float OPTIONS_LABEL_RIGHT_X = 310.0f;
const float OPTIONS_ROW_BASE_Y = 48.0f;
const float OPTIONS_ROW_PITCH = 22.0f;
const float OPTIONS_ROW_SCALE = 0.7f;
const unsigned OPTIONS_ALIGN_CENTRE = 1;
const unsigned OPTIONS_ALIGN_RIGHT = 2;
const unsigned OPTIONS_ALIGN_LEFT = 0;
enum OptionValueFormat
{
    OPTION_VALUE_NONE,
    OPTION_VALUE_TABLE,
    OPTION_VALUE_SIGNED_DECIMAL
};
enum OptionBoundary
{
    OPTION_BOUNDARY_WRAP = 0,
    OPTION_BOUNDARY_CLAMP = 1
};
enum OptionAction
{
    OPTION_ACTION_EXIT_PAGE = 0,
    OPTION_ACTION_AUDIO = 1,
    OPTION_ACTION_CONTROL = 2,
    OPTION_ACTION_SCREEN = 3,
    OPTION_ACTION_BRIGHTNESS = 4,
    OPTION_ACTION_MOVIE_VIEWER = 5,
    OPTION_ACTION_DEFAULT = 6,
    OPTION_ACTION_PLAY_MOVIE = 7,
    OPTION_ACTION_COMMIT_EXIT = 8,
    OPTION_ACTION_VALUE = 9
};
enum OptionPromptStyle
{
    OPTION_PROMPT_EDIT_OR_BACK = 0,
    OPTION_PROMPT_SELECT_OR_BACK = 1,
    OPTION_PROMPT_MOVIE_OR_BACK = 2
};
enum OptionStorage
{
    OPTION_STORAGE_NONE,
    OPTION_STORAGE_DIFFICULTY,
    OPTION_STORAGE_SUBTITLES,
    OPTION_STORAGE_MAP,
    OPTION_STORAGE_GAUGE,
    OPTION_STORAGE_BLOOD_MALE,
    OPTION_STORAGE_BLOOD_FEMALE,
    OPTION_STORAGE_BRUISING_MALE,
    OPTION_STORAGE_BRUISING_FEMALE,
    OPTION_STORAGE_AUDIO_OUTPUT,
    OPTION_STORAGE_BGM_LEVEL,
    OPTION_STORAGE_SE_LEVEL,
    OPTION_STORAGE_CONTROLLER_P1,
    OPTION_STORAGE_VIBRATION_P1,
    OPTION_STORAGE_CONTROLLER_P2,
    OPTION_STORAGE_VIBRATION_P2,
    OPTION_STORAGE_CAMERA_X,
    OPTION_STORAGE_CAMERA_Y,
    OPTION_STORAGE_CAMERA_FOLLOW,
    OPTION_STORAGE_SCREEN_X,
    OPTION_STORAGE_SCREEN_Y,
    OPTION_STORAGE_MOVIE_RAVEN,
    OPTION_STORAGE_MOVIE_JASON,
    OPTION_STORAGE_MOVIE_GINA,
    OPTION_STORAGE_MOVIE_LOLA,
    OPTION_STORAGE_MOVIE_AARON
};
struct OptionEntry
{
    const char* literal;
    uint16_t textId;
    const char* const* valueLiterals;
    const uint16_t* valueTextIds;
    OptionValueFormat valueFormat;
    uint16_t helpTitle;
    uint16_t helpInGame;
    int8_t defaultValue;
    int8_t minimum;
    int8_t maximumExclusive;
    OptionBoundary boundary;
    bool visibleAtTitle;
    bool visibleInGame;
    OptionPromptStyle promptStyle;
    OptionAction action;
    OptionStorage storage;
    const uint16_t* movieSceneIds;
};
struct OptionPage
{
    uint16_t headingTitle;
    uint16_t headingInGame;
    const OptionEntry* entries;
    int entryCount;
};
extern const OptionPage OPTION_PAGES[OPTION_PAGE_COUNT];
float optionsRowY(int visibleIndex);
bool optionEntryVisible(const OptionEntry& entry, bool inGame);
bool optionEntryHasValue(const OptionEntry& entry);
int optionAdjustedValue(const OptionEntry& entry, int value, int direction);
const char* optionValueLiteral(const OptionEntry& entry, int value);
uint16_t optionValueTextId(const OptionEntry& entry, int value);
uint16_t optionHelpTextId(const OptionEntry& entry, bool inGame);
uint16_t optionMovieSceneId(const OptionEntry& entry, int value);
#endif
