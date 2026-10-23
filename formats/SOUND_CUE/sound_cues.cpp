#include "sound_cues.h"
const uint8_t SOUND_CUE_GROUPS[SOUND_CUE_GROUP_COUNT][2][SOUND_CUES_PER_GROUP] =
{
    { {   1,   2,   3,   4 }, {   9,  10,  11,  12 } },
    { {  29,  30,  31,  32 }, {  33,  34,  35,  36 } },
    { {  37,  38,  39,  40 }, {  41,  42,  43,  44 } },
    { {  45,  46,  47,  48 }, {  49,  50,  51,  52 } },
    { {  53,  54,  55,  56 }, {  57,  58,  59,  60 } },
    { {  61,  62,  63,  64 }, {  65,  66,  67,  68 } },
};
const uint8_t SOUND_CUE_ROTATION[SOUND_CUE_ROTATION_LENGTH] =
{
    1, 0, 3, 0, 2, 1, 2, 3, 2, 3, 0, 1, 2,
    1, 0, 3, 1, 3, 0, 3, 1, 2, 3, 1, 2,
};
const char* const SOUND_ARCHIVE_PATHS[SOUND_ARCHIVE_COUNT] =
{
    "xaf\\data.xaf",
    "xaf\\stage.xaf",
    "xaf\\graph.xaf",
    "xaf\\fbgm.xaf",
    "xaf\\sound.xaf",
    "xaf\\fstage.xaf",
    "xaf\\cdata.xaf",
    "xaf\\fscene.xaf",
    "xaf\\script.xaf",
    "xaf\\archive.xaf",
};
const int16_t SOUND_FIXED_BANK_ENTRIES[SOUND_BANK_SLOT_COUNT] =
{
    94, 0, 42, SOUND_BANK_ENTRY_DYNAMIC, SOUND_BANK_ENTRY_DYNAMIC,
    SOUND_BANK_ENTRY_DYNAMIC, SOUND_BANK_ENTRY_DYNAMIC
};
const int16_t SOUND_STAGE_SCENE_BANK_ENTRIES[SOUND_STAGE_COUNT] =
{
     0, 93, 93, 93, 89, 43, 44, 45, 90, 44, 44, 45, 48, 47,
    49, 49, 49, 51, 52, 52, 53, 57, 56, 58, 59, 55, 54, 52,
    81, 62, 63, 66, 64, 69, 65, 68, 81, 67, 71, 72, 77, 87,
    75, 76, 73, 74, 52, 81, 82, 83, 84, 85, 86, 87, 88,  0
};
const int16_t SOUND_STAGE_AUDIO_BANK_ENTRIES[SOUND_STAGE_COUNT] =
{
      0, 145, 145, 145,  98,  95,  96,  97, 142,  96,  96,  97,  99, 100,
    101, 101, 101, 103, 104, 104, 105, 109, 108, 110, 111, 107, 106, 104,
    133, 114, 115, 116, 117, 118, 119, 120, 133, 122, 123, 124, 125, 139,
    127, 128, 129, 130, 131, 133, 134, 135, 136, 137, 138, 139, 141,   0
};
const int16_t SOUND_STAGE_MUSIC_TRACKS[SOUND_STAGE_COUNT] =
{
      0, 403, 403, 403,  21, 403, 402, 403, 401, 402, 402, 403, 401, 401,
    401, 401, 401, 401,  13,  13,  13, 402, 402, 402, 402, 402,  13,  13,
     21, 404, 404, 404, 404, 404, 404, 402,  21, 404,  33,  33,  33,  21,
     33,  33,  33,  33,  13,  21,  21,  21,  21,  21,  21,  21,  21,   0
};
const int16_t SOUND_STAGE_AMBIENCE_TRACKS[SOUND_STAGE_COUNT] =
{
      0, 83, 83, 83, 86, 82, 60, 84, 92, 85, 85, 84, 90, 89,
     91, 91, 91, 97, 93, 93, 93, 85, 85, 85, 85, 60, 96, 93,
     93, 94, 94, 85,  0,  0,  0,  0, 93, 94, 95, 95, 95, 85,
    100, 87, 98, 99, 88, 83, 97, 97, 87, 97, 85, 85, 85,  0
};
const int16_t SOUND_STAGE_EVENT_AMBIENCE_TRACKS[SOUND_STAGE_COUNT] =
{
      0, 83, 83, 83, 86, 82, 85, 84, 92, 85, 85, 84, 90, 89,
     91, 91, 91, 97, 93, 93, 93, 85, 85, 85, 85, 85, 96, 93,
     93, 94, 94, 85,  0,  0,  0,  0, 93, 94, 95, 95, 95, 85,
    100, 87, 98, 99, 88, 83, 97, 97, 87, 97, 85, 85, 85,  0
};
