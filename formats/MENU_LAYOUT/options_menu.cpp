#include "options_menu.h"
namespace
{
const char* const ON_OFF_LITERALS[] = { "OFF", "ON" };
const uint16_t ON_OFF_IDS[] = { 9022, 9023 };
const char* const DIFFICULTY_LITERALS[] = { "EASY", "NORMAL", "HARD" };
const uint16_t DIFFICULTY_IDS[] = { 9019, 9020, 9021 };
const char* const AUDIO_LITERALS[] =
    { "STEREO", "MONO", "Dolby ProLogic II" };
const uint16_t AUDIO_IDS[] = { 9027, 9028, 9029 };
const char* const VOLUME_LITERALS[] =
{
    "MUTE", "1", "2", "3", "4", "5", "6", "7",
    "8", "9", "10", "11", "12", "13", "14", "MAXIMUM"
};
const uint16_t VOLUME_IDS[] =
{
    9030, 9031, 9032, 9033, 9034, 9035, 9036, 9037,
    9038, 9039, 9040, 9041, 9042, 9043, 9044, 9045
};
const char* const CONTROLLER_LITERALS[] =
    { "TYPE 1", "TYPE 2", "TYPE 3", "TYPE 4", "TYPE 5", "TYPE 6" };
const uint16_t CONTROLLER_IDS[] = { 9046, 9047, 9048, 9049, 9050, 9051 };
const char* const CAMERA_LITERALS[] = { "NORMAL", "REVERSE" };
const uint16_t CAMERA_IDS[] = { 9052, 9053 };
const uint16_t RAVEN_SCENES[40] =
{
    0,1,74,75,2,76,3,8,11,12,15,16,22,25,27,28,33,34,77,37,
    39,40,43,44,73,57,58,46,47,48,52,53,54,69,70,71,72,0,0,0
};
const uint16_t JASON_SCENES[40] =
{
    1,1,74,75,2,76,4,8,11,12,15,16,20,22,24,25,27,28,33,34,
    77,37,39,40,43,44,73,57,58,46,49,50,51,52,53,54,68,70,71,72
};
const uint16_t GINA_SCENES[40] =
{
    4,1,74,75,2,76,5,8,11,12,15,16,22,25,27,29,33,34,77,37,
    39,40,43,44,73,57,58,46,47,48,49,50,51,53,54,68,69,71,72,0
};
const uint16_t LOLA_SCENES[40] =
{
    3,1,74,75,2,76,6,8,11,12,15,16,22,25,27,28,33,34,77,37,
    39,40,43,44,73,57,59,47,48,49,50,51,52,53,54,68,69,70,72,0
};
const uint16_t AARON_SCENES[40] =
{
    2,1,74,75,2,76,7,8,11,12,15,16,22,25,27,28,33,34,77,37,
    39,41,43,44,73,57,58,46,47,48,49,50,51,52,68,69,70,71,0,0
};
#define VALUE_ROW(literal, id, literals, ids, help, def, min, max, edge, title, ingame, storage) \
    { literal, id, literals, ids, OPTION_VALUE_TABLE, help, help, def, min, max, edge, title, ingame, OPTION_PROMPT_EDIT_OR_BACK, OPTION_ACTION_VALUE, storage, 0 }
#define COMMAND_ROW(literal, id, helpTitle, helpGame, action, title, ingame, prompt) \
    { literal, id, 0, 0, OPTION_VALUE_NONE, helpTitle, helpGame, 0, 0, 0, OPTION_BOUNDARY_WRAP, title, ingame, prompt, action, OPTION_STORAGE_NONE, 0 }
#define NUMERIC_ROW(literal, id, help, def, min, max, storage) \
    { literal, id, 0, 0, OPTION_VALUE_SIGNED_DECIMAL, help, help, def, min, max, OPTION_BOUNDARY_CLAMP, true, true, OPTION_PROMPT_EDIT_OR_BACK, OPTION_ACTION_VALUE, storage, 0 }
#define MOVIE_ROW(literal, help, max, storage, scenes) \
    { literal, 0, 0, 0, OPTION_VALUE_SIGNED_DECIMAL, help, help, 1, 1, max, OPTION_BOUNDARY_CLAMP, true, true, OPTION_PROMPT_MOVIE_OR_BACK, OPTION_ACTION_PLAY_MOVIE, storage, scenes }
const OptionEntry PAGE_0[] =
{
    VALUE_ROW("DIFFICULTY", 9005, DIFFICULTY_LITERALS, DIFFICULTY_IDS, 30105, 2, 1, 4, OPTION_BOUNDARY_CLAMP, true, false, OPTION_STORAGE_DIFFICULTY),
    VALUE_ROW("SUBTITLES", 9006, ON_OFF_LITERALS, ON_OFF_IDS, 30124, 1, 0, 2, OPTION_BOUNDARY_WRAP, true, true, OPTION_STORAGE_SUBTITLES),
    VALUE_ROW("MAP", 9007, ON_OFF_LITERALS, ON_OFF_IDS, 52265, 1, 0, 2, OPTION_BOUNDARY_WRAP, true, true, OPTION_STORAGE_MAP),
    VALUE_ROW("GAGE", 9008, ON_OFF_LITERALS, ON_OFF_IDS, 9065, 1, 0, 2, OPTION_BOUNDARY_WRAP, true, true, OPTION_STORAGE_GAUGE),
    VALUE_ROW("BLOOD(MALE)", 9009, ON_OFF_LITERALS, ON_OFF_IDS, 30109, 1, 0, 2, OPTION_BOUNDARY_WRAP, true, false, OPTION_STORAGE_BLOOD_MALE),
    VALUE_ROW("BLOOD(FEMALE)", 9010, ON_OFF_LITERALS, ON_OFF_IDS, 30110, 1, 0, 2, OPTION_BOUNDARY_WRAP, true, false, OPTION_STORAGE_BLOOD_FEMALE),
    VALUE_ROW("BRUISING(MALE)", 9011, ON_OFF_LITERALS, ON_OFF_IDS, 30111, 1, 0, 2, OPTION_BOUNDARY_WRAP, true, false, OPTION_STORAGE_BRUISING_MALE),
    VALUE_ROW("BRUISING(FEMALE)", 9012, ON_OFF_LITERALS, ON_OFF_IDS, 30112, 1, 0, 2, OPTION_BOUNDARY_WRAP, true, false, OPTION_STORAGE_BRUISING_FEMALE),
    COMMAND_ROW("AUDIO CONFIG", 9013, 52272, 52272, OPTION_ACTION_AUDIO, true, true, OPTION_PROMPT_SELECT_OR_BACK),
    COMMAND_ROW("CONTROL CONFIG", 9014, 30113, 30113, OPTION_ACTION_CONTROL, true, true, OPTION_PROMPT_SELECT_OR_BACK),
    COMMAND_ROW("SCREEN ADJUST", 9015, 65504, 65504, OPTION_ACTION_SCREEN, true, true, OPTION_PROMPT_SELECT_OR_BACK),
    COMMAND_ROW("BRIGHTNESS", 9016, 30121, 30121, OPTION_ACTION_BRIGHTNESS, true, true, OPTION_PROMPT_SELECT_OR_BACK),
    COMMAND_ROW("MOVIE VIEWER", 9063, 30123, 30123, OPTION_ACTION_MOVIE_VIEWER, true, false, OPTION_PROMPT_SELECT_OR_BACK),
    COMMAND_ROW("DEFAULT SETTING", 9017, 30114, 30114, OPTION_ACTION_DEFAULT, true, true, OPTION_PROMPT_SELECT_OR_BACK),
    COMMAND_ROW("EXIT", 9018, 65505, 65506, OPTION_ACTION_COMMIT_EXIT, true, true, OPTION_PROMPT_SELECT_OR_BACK)
};
const OptionEntry PAGE_1[] =
{
    VALUE_ROW("AUDIO", 9026, AUDIO_LITERALS, AUDIO_IDS, 30106, 0, 0, 3, OPTION_BOUNDARY_CLAMP, true, true, OPTION_STORAGE_AUDIO_OUTPUT),
    VALUE_ROW("BGM LEVEL", 9080, VOLUME_LITERALS, VOLUME_IDS, 30107, 15, 0, 16, OPTION_BOUNDARY_CLAMP, true, true, OPTION_STORAGE_BGM_LEVEL),
    VALUE_ROW("SE LEVEL", 9081, VOLUME_LITERALS, VOLUME_IDS, 30108, 15, 0, 16, OPTION_BOUNDARY_CLAMP, true, true, OPTION_STORAGE_SE_LEVEL),
    COMMAND_ROW("DEFAULT SETTING", 9017, 30114, 30114, OPTION_ACTION_DEFAULT, true, true, OPTION_PROMPT_SELECT_OR_BACK),
    COMMAND_ROW("EXIT", 9018, 30116, 65508, OPTION_ACTION_EXIT_PAGE, true, true, OPTION_PROMPT_SELECT_OR_BACK)
};
const OptionEntry PAGE_2[] =
{
    VALUE_ROW("TYPE (PLAYER1)", 9054, CONTROLLER_LITERALS, CONTROLLER_IDS, 30101, 0, 0, 6, OPTION_BOUNDARY_CLAMP, true, true, OPTION_STORAGE_CONTROLLER_P1),
    VALUE_ROW("VIBRATION(PLAYER1)", 9055, ON_OFF_LITERALS, ON_OFF_IDS, 30102, 0, 0, 2, OPTION_BOUNDARY_WRAP, true, true, OPTION_STORAGE_VIBRATION_P1),
    VALUE_ROW("TYPE (PLAYER2)", 9056, CONTROLLER_LITERALS, CONTROLLER_IDS, 30103, 0, 0, 6, OPTION_BOUNDARY_CLAMP, true, true, OPTION_STORAGE_CONTROLLER_P2),
    VALUE_ROW("VIBRATION(PLAYER2)", 9057, ON_OFF_LITERALS, ON_OFF_IDS, 30104, 0, 0, 2, OPTION_BOUNDARY_WRAP, true, true, OPTION_STORAGE_VIBRATION_P2),
    VALUE_ROW("CAMERA TYPE(X)", 9058, CAMERA_LITERALS, CAMERA_IDS, 30120, 0, 0, 2, OPTION_BOUNDARY_WRAP, true, true, OPTION_STORAGE_CAMERA_X),
    VALUE_ROW("CAMERA TYPE(Y)", 9059, CAMERA_LITERALS, CAMERA_IDS, 30119, 0, 0, 2, OPTION_BOUNDARY_WRAP, true, true, OPTION_STORAGE_CAMERA_Y),
    VALUE_ROW("CAMERA FOLLOW", 9060, ON_OFF_LITERALS, ON_OFF_IDS, 30125, 1, 0, 2, OPTION_BOUNDARY_WRAP, true, true, OPTION_STORAGE_CAMERA_FOLLOW),
    COMMAND_ROW("DEFAULT SETTING", 9017, 30114, 30114, OPTION_ACTION_DEFAULT, true, true, OPTION_PROMPT_SELECT_OR_BACK),
    COMMAND_ROW("EXIT", 9018, 30116, 65508, OPTION_ACTION_EXIT_PAGE, true, true, OPTION_PROMPT_SELECT_OR_BACK)
};
const OptionEntry PAGE_3[] =
{
    NUMERIC_ROW("X POSITION", 9061, 52266, 0, -20, 21, OPTION_STORAGE_SCREEN_X),
    NUMERIC_ROW("Y POSITION", 9062, 52267, 0, -20, 21, OPTION_STORAGE_SCREEN_Y),
    COMMAND_ROW("DEFAULT SETTING", 9017, 30114, 30114, OPTION_ACTION_DEFAULT, true, true, OPTION_PROMPT_SELECT_OR_BACK),
    COMMAND_ROW("EXIT", 9018, 30116, 65508, OPTION_ACTION_EXIT_PAGE, true, true, OPTION_PROMPT_SELECT_OR_BACK)
};
const OptionEntry PAGE_4[] =
{
    COMMAND_ROW("BRIGHTNESS", 9016, 30121, 30121, OPTION_ACTION_EXIT_PAGE, true, true, OPTION_PROMPT_EDIT_OR_BACK)
};
const OptionEntry PAGE_5[] =
{
    MOVIE_ROW("RAVEN", 30123, 37, OPTION_STORAGE_MOVIE_RAVEN, RAVEN_SCENES),
    MOVIE_ROW("JASON G.", 30123, 40, OPTION_STORAGE_MOVIE_JASON, JASON_SCENES),
    MOVIE_ROW("GINA", 30123, 39, OPTION_STORAGE_MOVIE_GINA, GINA_SCENES),
    MOVIE_ROW("LOLA", 30123, 39, OPTION_STORAGE_MOVIE_LOLA, LOLA_SCENES),
    MOVIE_ROW("AARON", 30123, 38, OPTION_STORAGE_MOVIE_AARON, AARON_SCENES),
    COMMAND_ROW("EXIT", 9018, 65508, 65508, OPTION_ACTION_EXIT_PAGE, true, true, OPTION_PROMPT_SELECT_OR_BACK)
};
#undef MOVIE_ROW
#undef NUMERIC_ROW
#undef COMMAND_ROW
#undef VALUE_ROW
}
const OptionPage OPTION_PAGES[OPTION_PAGE_COUNT] =
{
    { 52121, 52116, PAGE_0, 15 },
    { 9013, 9013, PAGE_1, 5 },
    { 9014, 9014, PAGE_2, 9 },
    { 9015, 9015, PAGE_3, 4 },
    { 9016, 9016, PAGE_4, 1 },
    { 9063, 9063, PAGE_5, 6 }
};
bool optionEntryVisible(const OptionEntry& entry, bool inGame)
{
    return inGame ? entry.visibleInGame : entry.visibleAtTitle;
}
bool optionEntryHasValue(const OptionEntry& entry)
{
    return entry.valueFormat != OPTION_VALUE_NONE;
}
int optionAdjustedValue(const OptionEntry& entry, int value, int direction)
{
    if(!optionEntryHasValue(entry) || direction == 0 ||
       entry.maximumExclusive <= entry.minimum)
        return value;
    int adjusted = value + (direction < 0 ? -1 : 1);
    if(entry.boundary == OPTION_BOUNDARY_WRAP)
    {
        if(adjusted < entry.minimum)
            adjusted = entry.maximumExclusive - 1;
        else if(adjusted >= entry.maximumExclusive)
            adjusted = entry.minimum;
    }
    else
    {
        if(adjusted < entry.minimum)
            adjusted = entry.minimum;
        else if(adjusted >= entry.maximumExclusive)
            adjusted = entry.maximumExclusive - 1;
    }
    return adjusted;
}
const char* optionValueLiteral(const OptionEntry& entry, int value)
{
    if(entry.valueFormat != OPTION_VALUE_TABLE || entry.valueLiterals == 0 ||
       value < entry.minimum || value >= entry.maximumExclusive)
        return 0;
    return entry.valueLiterals[value - entry.minimum];
}
uint16_t optionValueTextId(const OptionEntry& entry, int value)
{
    if(entry.valueFormat != OPTION_VALUE_TABLE || entry.valueTextIds == 0 ||
       value < entry.minimum || value >= entry.maximumExclusive)
        return 0;
    return entry.valueTextIds[value - entry.minimum];
}
uint16_t optionHelpTextId(const OptionEntry& entry, bool inGame)
{
    return inGame ? entry.helpInGame : entry.helpTitle;
}
uint16_t optionMovieSceneId(const OptionEntry& entry, int value)
{
    if(entry.action != OPTION_ACTION_PLAY_MOVIE || entry.movieSceneIds == 0 ||
       value < entry.minimum || value >= entry.maximumExclusive)
        return 0;
    return entry.movieSceneIds[value];
}
