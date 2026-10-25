#ifndef AUDIO_H
#define AUDIO_H

#include <stdint.h>

enum AudioChannel
{
    AUDIO_CHANNEL_EFFECT,
    AUDIO_CHANNEL_MUSIC,
    AUDIO_CHANNEL_MUSIC_SECONDARY,
    AUDIO_CHANNEL_MUSIC_AMBIENT,
    AUDIO_CHANNEL_VOICE,
    AUDIO_CHANNEL_MOVIE
};

const uint32_t AUDIO_SOUND_HANDLE_INVALID = 0xffffffffu;

bool initializeSoundEffects(const char* soundArchivePath);
bool bindSoundBankSlot(uint8_t slot, uint32_t soundArchiveEntry);
bool bindSoundBankSlotByName(uint8_t slot, const char* entryName);
bool bindStageSoundBanks(int32_t sceneResourceId, int32_t audioResourceId);
bool bindStageSoundBanksForStage(int stageId);

uint32_t playSoundCue(uint8_t bankSlot, uint8_t cue, uint8_t volume,
                      uint8_t pan);
uint32_t playUnplacedSoundCue(uint8_t bankSlot, uint8_t cue,
                              uint8_t effectsVolumeSetting);
uint32_t playSpatialSoundCue(uint8_t bankSlot, uint8_t cue,
                             float listenerX, float listenerY,
                             float listenerZ, float forwardX,
                             float forwardZ, float sourceX,
                             float sourceY, float sourceZ,
                             int outputMode, uint8_t effectsVolumeSetting);
uint32_t playFootstepSound(uint16_t surface, uint32_t actorFlags,
                           bool alternate, uint32_t* rotationCounter,
                           float listenerX, float listenerY,
                           float listenerZ, float forwardX,
                           float forwardZ, float sourceX,
                           float sourceY, float sourceZ,
                           int outputMode, uint8_t effectsVolumeSetting);

void stopSoundHandle(uint32_t handle);
bool setSoundHandlePaused(uint32_t handle, bool paused);
bool setSoundHandleVolume(uint32_t handle, int volume);
bool setSoundHandleLooping(uint32_t handle, bool enabled);
bool isSoundHandlePlaying(uint32_t handle);

bool playMusicTrack(int32_t entryIndex, float volume);
bool playTitleMenuMusic(bool alternate, float volume);
bool playStageMusic(int stageId, int storyPromptPhase, float volume);
bool playAmbienceTrack(int32_t entryIndex, float volume);
bool playStageAmbience(int stageId, bool eventFlagSet, float volume);

bool initializeAudio(const char* archivePath);
bool playAudioClip(const char* entryName, AudioChannel channel,
                   bool loop, float volume);
bool playAudioClipByIndex(uint32_t entryIndex, AudioChannel channel,
                          bool loop, float volume);

int32_t resolveEventMusicTrack(int characterGroupId, int relationshipState);
void updateAudio(void);

void setAudioOutputMode(int outputMode);
void stopAudioChannel(AudioChannel channel);
void setAudioChannelVolume(AudioChannel channel, float volume);
void setAudioChannelPaused(AudioChannel channel, bool paused);
bool isAudioChannelPlaying(AudioChannel channel);
bool beginMoviePcmStream(uint32_t sampleRate, uint16_t channels);
bool queueMoviePcm(const int16_t* samples, uint32_t frameCount);
void endMoviePcmStream(void);
void terminateAudio(void);

#endif
