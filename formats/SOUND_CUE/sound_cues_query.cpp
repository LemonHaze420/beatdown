#include "sound_cues.h"
int32_t
soundBankEntryForSlot(uint8_t slot, int32_t sceneResourceId,
                      int32_t audioResourceId)
{
    if(slot >= SOUND_BANK_SLOT_COUNT)
        return -1;
    if(slot == 3)
        return audioResourceId;
    if(slot == 4)
        return sceneResourceId;
    return SOUND_FIXED_BANK_ENTRIES[slot];
}
int32_t
stageSceneSoundBankEntry(int stageId)
{
    return stageId >= 0 && stageId < SOUND_STAGE_COUNT ?
        SOUND_STAGE_SCENE_BANK_ENTRIES[stageId] : -1;
}
int32_t
stageAudioSoundBankEntry(int stageId)
{
    return stageId >= 0 && stageId < SOUND_STAGE_COUNT ?
        SOUND_STAGE_AUDIO_BANK_ENTRIES[stageId] : -1;
}
int32_t
titleMenuMusicTrack(bool alternate)
{
    return alternate ? SOUND_MUSIC_TITLE_ALTERNATE :
                       SOUND_MUSIC_TITLE_PRIMARY;
}
int32_t
stageMusicTrack(int stageId, int storyPromptPhase)
{
    if(stageId < 0 || stageId >= SOUND_STAGE_COUNT)
        return -1;
    if(stageId == 19 && storyPromptPhase == 7)
        return 20;
    return SOUND_STAGE_MUSIC_TRACKS[stageId];
}
int32_t
stageAmbienceTrack(int stageId, bool eventFlagSet)
{
    if(stageId < 0 || stageId >= SOUND_STAGE_COUNT)
        return -1;
    const int32_t eventTrack = SOUND_STAGE_EVENT_AMBIENCE_TRACKS[stageId];
    return eventFlagSet && eventTrack != 0 ?
        eventTrack : SOUND_STAGE_AMBIENCE_TRACKS[stageId];
}
int
soundCueGroupFor(uint16_t surface, uint32_t flags)
{
    if((flags & SOUND_ACTOR_FLAG_SURFACED) == 0)
        return SOUND_CUE_GROUP_COUNT - 1;

    switch(surface)
    {
    case 4:
        return 1;
    case 5:
        return 2;
    case 3:
    case 6:
        return 3;
    case 7:
        return 4;
    default:
        return 0;
    }
}
uint8_t
soundCueFor(uint16_t surface, uint32_t flags, bool alternate, uint32_t counter)
{
    const int group = soundCueGroupFor(surface, flags);
    const uint8_t slot = SOUND_CUE_ROTATION[counter % SOUND_CUE_ROTATION_LENGTH];
    return SOUND_CUE_GROUPS[group][alternate ? 1 : 0][slot];
}
uint32_t
soundCueAdvance(uint32_t counter)
{
    return (counter + 1) & SOUND_CUE_COUNTER_MASK;
}
