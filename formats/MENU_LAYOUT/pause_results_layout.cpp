#include "pause_results_layout.h"
const PauseResultsColour PAUSE_OVERLAY_COLOUR =
    { 0x00, 0x00, 0x00, 0x80 };
const PauseTextSpriteLayout PAUSE_TEXT_SPRITES[PAUSE_TEXT_SPRITE_COUNT] =
{
    { PAUSE_SPRITE_HEADING, 320.0f, 128.0f, 1.0f, 1.0f,
      { 0xff, 0xff, 0x5f, 0xc0 }, 5, 12000, 10470, "ＰＡＵＳＥ" },
    { PAUSE_SPRITE_RESUME, 320.0f, 174.0f, 1.0f, 1.0f,
      { 0xb7, 0xb7, 0xb7, 0xc0 }, 5, 12000, 10471, "ＲＥＳＵＭＥ" },
    { PAUSE_SPRITE_MOVE_LIST, 320.0f, 208.0f, 1.0f, 1.0f,
      { 0x71, 0xee, 0xc8, 0xc0 }, 5, 12000, 10472, "ＭＯＶＥ　ＬＩＳＴ" },
    { PAUSE_SPRITE_QUIT_GAME, 320.0f, 242.0f, 1.0f, 1.0f,
      { 0x71, 0xee, 0xc8, 0xc0 }, 5, 12000, 10476, "ＱＵＩＴ　ＧＡＭＥ" },
    { PAUSE_SPRITE_DESCRIPTION, 320.0f, 410.0f, 1.0f, 1.0f,
      { 0xc0, 0xc0, 0x80, 0xc0 }, 8, 12000, 65528, "Ｒｅｔｕｒｎ　ｔｏ　ｂａｔｔｌｅ．" },
    { PAUSE_SPRITE_LEFT_EQUIPPED, 48.0f, 387.0f, 0.8f, 0.8f,
      { 0xff, 0xff, 0x5f, 0xc0 }, 4, 12000, 12004, "②Ｂ　ＫＮＵＣＫＬＥＳ" },
    { PAUSE_SPRITE_RIGHT_EQUIPPED, 593.0f, 387.0f, 0.8f, 0.8f,
      { 0xff, 0xff, 0x5f, 0xc0 }, 6, 12000, 12001, "⑤ＣＨＩＣＫＥＮ" },
    { PAUSE_SPRITE_SELECT_PROMPT, 209.0f, 450.0f, 0.8f, 0.8f,
      { 0xff, 0xff, 0x5f, 0xff }, 4, 7001, 10402, "ＳＥＬＥＣＴ" },
    { PAUSE_SPRITE_OK_PROMPT, 341.0f, 450.0f, 0.8f, 0.8f,
      { 0xff, 0xff, 0x5f, 0xff }, 4, 7001, 10452, "ＯＫ" },
};
const uint16_t PAUSE_ROW_TEXT_IDS[PAUSE_ROW_COUNT] =
{ 10471, 10472, 10476 };
const uint16_t PAUSE_DESCRIPTION_TEXT_IDS[PAUSE_ROW_COUNT] =
{ 65528, 65529, 30845 };
const uint16_t BATTLE_PAUSE_ROW_TEXT_IDS[BATTLE_PAUSE_ROW_COUNT] =
{ 10471, 10472, 10473, 10474, 10475, 10476 };
const uint16_t BATTLE_PAUSE_DESCRIPTION_TEXT_IDS[BATTLE_PAUSE_ROW_COUNT] =
{ 65528, 65529, 10904, 10905, 10906, 30845 };
const char* const PAUSE_DESCRIPTION_ENGLISH[PAUSE_ROW_COUNT] =
{
    "Ｒｅｔｕｒｎ　ｔｏ　ｂａｔｔｌｅ．",
    "Ｃｈｅｃｋ　ｙｏｕｒ　ｍｏｖｅ　ｌｉｓｔ．",
    "Ｒｅｔｕｒｎ　ｔｏ　ｔｉｔｌｅ　ｓｃｒｅｅｎ．",
};
const PauseEquippedDefinition PAUSE_EQUIPPED_DEFINITIONS[PAUSE_EQUIPPED_DEFINITION_COUNT] =
{
    { 1, 1, 12000 },
    { 1, 2, 12001 },
    { 1, 3, 12002 },
    { 1, 4, 12003 },
    { 2, 1, 12004 },
    { 2, 2, 12005 },
    { 2, 3, 12006 },
    { 2, 4, 12007 },
    { 2, 5, 12008 },
    { 2, 6, 12009 },
    { 2, 7, 12010 },
    { 2, 8, 12011 },
    { 2, 9, 12012 },
    { 2, 10, 12013 },
    { 2, 11, 12014 },
    { 2, 12, 12015 },
    { 2, 13, 12016 },
    { 2, 14, 12017 },
    { 2, 15, 12018 },
    { 2, 16, 12019 },
    { 2, 17, 12020 },
    { 2, 18, 12021 },
    { 2, 19, 12022 },
    { 2, 20, 12023 },
    { 2, 21, 12024 },
    { 2, 22, 12025 },
    { 2, 23, 12026 },
    { 2, 24, 12027 },
    { 2, 25, 12028 },
    { 2, 26, 12029 },
    { 2, 27, 12030 },
    { 2, 29, 12031 },
    { 2, 30, 12032 },
    { 2, 31, 12033 },
    { 2, 32, 12034 },
    { 2, 33, 12035 },
    { 2, 34, 12036 },
    { 2, 35, 12037 },
    { 2, 36, 12038 },
    { 2, 37, 12039 },
    { 2, 38, 12040 },
    { 2, 39, 12041 },
    { 2, 40, 12042 },
    { 2, 41, 12043 },
    { 2, 42, 12044 },
    { 2, 43, 12045 },
    { 2, 44, 12046 },
    { 2, 45, 12047 },
    { 3, 2, 12048 },
    { 3, 3, 12049 },
    { 3, 4, 12050 },
    { 3, 5, 12051 },
    { 3, 6, 12052 },
    { 3, 7, 12053 },
    { 3, 8, 12054 },
    { 3, 9, 12055 },
    { 3, 10, 12056 },
    { 3, 11, 12057 },
    { 3, 12, 12058 },
    { 3, 13, 12059 },
    { 3, 14, 12060 },
    { 3, 15, 12061 },
    { 3, 16, 12062 },
    { 3, 17, 12063 },
    { 3, 18, 12064 },
    { 3, 19, 12065 },
    { 3, 20, 12066 },
    { 3, 21, 12067 },
    { 3, 22, 12068 },
    { 3, 23, 12069 },
    { 3, 24, 12070 },
    { 3, 25, 12071 },
    { 3, 26, 12072 },
    { 3, 27, 12073 },
    { 3, 28, 12074 },
    { 3, 29, 12075 },
    { 3, 30, 12076 },
    { 3, 31, 12077 },
    { 3, 32, 12078 },
    { 3, 33, 12079 },
    { 3, 34, 12080 },
    { 3, 35, 12081 },
    { 3, 36, 12082 },
    { 3, 37, 12083 },
    { 3, 38, 12084 },
    { 3, 39, 12085 },
    { 3, 40, 12086 },
    { 3, 41, 12087 },
    { 3, 42, 12088 },
    { 3, 43, 12089 },
    { 1, 5, 12090 },
    { 1, 6, 12091 },
    { 1, 7, 12092 },
    { 1, 8, 12093 },
    { 1, 9, 12094 },
    { 1, 10, 12095 },
    { 1, 11, 12096 },
    { 1, 12, 12097 },
    { 1, 13, 12098 },
    { 1, 14, 12099 },
    { 2, 32, 12100 },
    { 1, 15, 12101 },
    { 1, 16, 12102 },
    { 3, 44, 12103 },
    { 3, 45, 12104 },
    { 3, 46, 12105 },
    { 3, 47, 12106 },
    { 1, 17, 12107 },
    { 1, 18, 12108 },
    { 1, 19, 12109 },
    { 1, 20, 12110 },
    { 1, 21, 12111 },
    { 3, 48, 12112 },
    { 0, 0, 12113 },
    { 420354320, 2842, 12114 },
    { 65535, 0, 12115 },
    { 0, 52429, 12116 },
    { 1036831949, 0, 12117 },
    { 0, 62915, 12118 },
    { 1038174126, 0, 12119 },
    { 0, 28836, 12120 },
    { 1022739087, 0, 12121 },
    { 0, 15729, 12122 },
    { 1034147594, 0, 12123 },
    { 1127481344, 52429, 12124 },
    { 1056964608, 0, 12125 },
    { 1119092736, 52429, 12126 },
    { 1056964608, 0, 12127 },
};
const ResultChoiceLayout RESULT_CHOICES[RESULT_CHOICE_COUNT] =
{
    { 10473, 10904, RESULT_ACTION_RESTART_MATCH, 320.0f, 192.0f, 1.0f, 1.2f, 5,
      { 0x70, 0x70, 0x70, 0xc0 },
      { 0xff, 0xff, 0xff, 0xc0 }, "ＲＥＳＴＡＲＴ　ＴＨＩＳ　ＭＡＴＣＨ", "Ｒｅｔｒｙ　ｂａｔｔｌｅ．" },
    { 10475, 10906, RESULT_ACTION_RETURN_CHARACTER_SELECT, 320.0f, 230.0f, 1.0f, 1.2f, 5,
      { 0x70, 0x70, 0x70, 0xc0 },
      { 0xff, 0xff, 0xff, 0xc0 }, "ＲＥＴＵＲＮ　ＴＯ　ＣＨＡＲＡＣＴＥＲ　ＳＥＬＥＣＴ", "Ｒｅｔｕｒｎ　ｔｏ　ｃｈａｒａｃｔｅｒ　ｓｅｌｅｃｔ．" },
    { 10474, 10905, RESULT_ACTION_RETURN_MODE_SELECT, 320.0f, 268.0f, 1.0f, 1.2f, 5,
      { 0x70, 0x70, 0x70, 0xc0 },
      { 0xff, 0xff, 0xff, 0xc0 }, "ＲＥＴＵＲＮ　ＴＯ　ＭＯＤＥ　ＳＥＬＥＣＴ", "Ｒｅｔｕｒｎ　ｔｏ　ｍｏｄｅ　ｓｅｌｅｃｔ．" },
    { 10476, 30845, RESULT_ACTION_QUIT_GAME, 320.0f, 306.0f, 1.0f, 1.2f, 5,
      { 0x70, 0x70, 0x70, 0xc0 },
      { 0xff, 0xff, 0xff, 0xc0 }, "ＱＵＩＴ　ＧＡＭＥ", "Ｒｅｔｕｒｎ　ｔｏ　ｔｉｔｌｅ　ｓｃｒｅｅｎ．" },
};
const ResultDescriptionLayout RESULT_DESCRIPTION_LAYOUT =
{ 320.0f, 410.0f, 1.0f, 1.0f, { 0xc0, 0xc0, 0x80, 0xff } };
const ResultBannerLayout RESULT_BANNER_LAYOUT =
{ 342.0f, 297.0f, 256.0f, 128.0f, 256.0f, 256.0f };
const ResultBannerVariant RESULT_BANNER_VARIANTS[RESULT_BANNER_LANGUAGE_COUNT] =
{
    { 0x144, "RES_LOGO.STX" },
    { 0x144, "RES_LOGO.STX" },
    { 0x146, "RES_LOGO_FRA.STX" },
    { 0x147, "RES_LOGO_GER.STX" },
    { 0x145, "RES_LOGO_ESP.STX" },
    { 0x148, "RES_LOGO_ITA.STX" },
};
uint16_t
pauseEquippedTextId(uint32_t type, uint16_t id)
{
    for(int i = 0; i < PAUSE_EQUIPPED_DEFINITION_COUNT; ++i)
    {
        const PauseEquippedDefinition& entry = PAUSE_EQUIPPED_DEFINITIONS[i];
        if(entry.type == type && entry.id == id)
            return entry.textId;
    }
    return 0;
}
const ResultBannerVariant&
resultBannerVariant(uint8_t language)
{
    if(language >= RESULT_BANNER_LANGUAGE_COUNT || language == 0)
        language = 1;
    return RESULT_BANNER_VARIANTS[language];
}
