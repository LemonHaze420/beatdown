#ifndef SOUND_CUES_H
#define SOUND_CUES_H
#include <stdint.h>

const int SOUND_CUE_GROUP_COUNT = 6;
const int SOUND_CUES_PER_GROUP = 4;
const int SOUND_CUE_ROTATION_LENGTH = 25;
const int SOUND_ARCHIVE_COUNT = 10;
const int SOUND_BANK_SLOT_COUNT = 7;
const int SOUND_STAGE_COUNT = 56;

const int16_t SOUND_BANK_ENTRY_DYNAMIC = -1;
extern const int16_t SOUND_FIXED_BANK_ENTRIES[SOUND_BANK_SLOT_COUNT];
extern const int16_t SOUND_STAGE_SCENE_BANK_ENTRIES[SOUND_STAGE_COUNT];
extern const int16_t SOUND_STAGE_AUDIO_BANK_ENTRIES[SOUND_STAGE_COUNT];

const int16_t SOUND_MUSIC_TITLE_PRIMARY = 300;
const int16_t SOUND_MUSIC_TITLE_ALTERNATE = 5;
const int16_t SOUND_MUSIC_BATTLE_SETUP = 38;
const int16_t SOUND_MUSIC_RESULTS = 399;
const int16_t SOUND_MUSIC_CREDITS = 41;
extern const int16_t SOUND_STAGE_MUSIC_TRACKS[SOUND_STAGE_COUNT];
extern const int16_t SOUND_STAGE_AMBIENCE_TRACKS[SOUND_STAGE_COUNT];
extern const int16_t SOUND_STAGE_EVENT_AMBIENCE_TRACKS[SOUND_STAGE_COUNT];

const uint8_t SOUND_BANK_FOOTSTEP = 2;

const uint32_t SOUND_ACTOR_FLAG_SURFACED = 0x100;

const uint32_t SOUND_CUE_COUNTER_MASK = 0x7f;
extern const uint8_t SOUND_CUE_GROUPS[SOUND_CUE_GROUP_COUNT][2]
                                     [SOUND_CUES_PER_GROUP];
extern const uint8_t SOUND_CUE_ROTATION[SOUND_CUE_ROTATION_LENGTH];

extern const char* const SOUND_ARCHIVE_PATHS[SOUND_ARCHIVE_COUNT];

int32_t soundBankEntryForSlot(uint8_t slot, int32_t sceneResourceId,
                              int32_t audioResourceId);
int32_t stageSceneSoundBankEntry(int stageId);
int32_t stageAudioSoundBankEntry(int stageId);

int32_t titleMenuMusicTrack(bool alternate);
int32_t stageMusicTrack(int stageId, int storyPromptPhase);
int32_t stageAmbienceTrack(int stageId, bool eventFlagSet);

int soundCueGroupFor(uint16_t surface, uint32_t flags);

uint8_t soundCueFor(uint16_t surface, uint32_t flags, bool alternate,
                    uint32_t counter);
uint32_t soundCueAdvance(uint32_t counter);
#endif
