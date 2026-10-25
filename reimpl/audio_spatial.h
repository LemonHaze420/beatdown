#ifndef AUDIO_SPATIAL_H
#define AUDIO_SPATIAL_H

#include <stdint.h>

enum AudioOutputMode
{
    AUDIO_OUTPUT_STEREO = 0,
    AUDIO_OUTPUT_MONO = 1,
    AUDIO_OUTPUT_SURROUND = 2
};

struct AudioOutputGains
{
    float left;
    float right;
};

struct AudioSpatialVector
{
    float x;
    float y;
    float z;
};

struct AudioSpatialMatrix
{
    AudioSpatialVector right;
    AudioSpatialVector up;
    AudioSpatialVector at;
    AudioSpatialVector pos;
};

struct AudioListener
{
    AudioSpatialVector position;
    AudioSpatialVector forward;
};

uint16_t convertXpecSoundVolume(int volume);

AudioOutputGains computeXpecStereoVoiceGains(float volume, uint32_t channel,
                                             int outputMode);

const float AUDIO_NEAR_DISTANCE = 50.0f;
const float AUDIO_MID_DISTANCE = 275.0f;
const float AUDIO_FAR_DISTANCE = 500.0f;
const float AUDIO_FAR_SPAN = 275.0f;
const uint8_t AUDIO_MAX_VOLUME = 127;
const uint8_t AUDIO_MID_VOLUME = 30;
const uint8_t AUDIO_MIN_VOLUME = 1;

const uint8_t AUDIO_PAN_CENTRE = 63;
const uint8_t AUDIO_PAN_UNPLACED = 64;
const uint8_t AUDIO_PAN_SPAN = 63;

const float AUDIO_SURROUND_FRONT_ARC = 12.5f;
const float AUDIO_SURROUND_SIDE_ARC = 50.0f;

const uint8_t AUDIO_VOLUME_SETTING_MAX = 15;

struct AudioPlacement
{
    uint8_t volume;

    uint8_t pan;

    float surroundAngle;
    bool surround;
};

float audioBearing(float x, float z);

uint8_t scaleAudioVolume(uint8_t volume, uint8_t setting);

AudioListener computeAudioListener(const AudioSpatialMatrix& viewMatrix,
                                   const AudioSpatialMatrix& frameMatrix);

AudioPlacement computeAudioPlacement(float listenerX, float listenerY,
                                     float listenerZ, float forwardX,
                                     float forwardZ, float sourceX,
                                     float sourceY, float sourceZ,
                                     int outputMode);

AudioPlacement computeAudioPlacement(const AudioListener* listener,
                                     const AudioSpatialVector* source,
                                     int outputMode);

#endif
