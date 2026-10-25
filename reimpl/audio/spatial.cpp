#include "audio/spatial.h"

#include <math.h>

namespace
{

float wrapRadians(float value)
{
    if(value < -3.1415927f)
        return value <= -6.2831855f ? fmodf(value, 6.2831855f) + 6.2831855f
                                    : value + 6.2831855f;
    if(value > 3.1415927f)
        return value > 6.2831855f ? fmodf(value, 6.2831855f) - 6.2831855f
                                  : value - 6.2831855f;
    return value;
}

float wrapDegreesSigned(float value)
{
    if(value < -180.0f)
        return value <= -360.0f ? fmodf(value, 360.0f) + 360.0f
                                : value + 360.0f;
    if(value > 180.0f)
        return value > 360.0f ? fmodf(value, 360.0f) - 360.0f : value - 360.0f;
    return value;
}

float wrapDegreesPositive(float value)
{
    if(value < 0.0f)
        return value <= -360.0f ? fmodf(value, 360.0f) + 360.0f
                                : value + 360.0f;
    if(value > 360.0f)
        return fmodf(value, 360.0f);
    return value;
}

float bearingDelta(float forward, float source)
{
    float delta = wrapRadians(forward) - wrapRadians(source);
    if(delta < -3.1415927f)
        return delta + 6.2831855f;
    if(delta > 3.1415927f)
        return -(6.2831855f - delta);
    return delta;
}

uint8_t attenuationFor(float distance)
{
    if(distance <= AUDIO_NEAR_DISTANCE)
        return AUDIO_MAX_VOLUME;
    if(distance <= AUDIO_MID_DISTANCE)
    {
        const float span = AUDIO_MID_DISTANCE - AUDIO_NEAR_DISTANCE;
        const float fall = (1.0f - (distance - AUDIO_NEAR_DISTANCE) / span) *
            (float)(AUDIO_MAX_VOLUME - AUDIO_MID_VOLUME);
        return (uint8_t)((uint32_t)fall + AUDIO_MID_VOLUME);
    }
    if(distance <= AUDIO_FAR_DISTANCE)
    {

        const float fall =
            (1.0f - (distance - AUDIO_MID_DISTANCE) / AUDIO_FAR_SPAN) *
            (float)AUDIO_MID_VOLUME;
        return (uint8_t)(uint32_t)fall;
    }

    return AUDIO_MIN_VOLUME;
}

float surroundAngleFor(float degrees)
{
    if(fabsf(wrapDegreesSigned(degrees)) < AUDIO_SURROUND_FRONT_ARC)
        return 0.0f;
    if(fabsf(wrapDegreesPositive(degrees) - 180.0f) < AUDIO_SURROUND_FRONT_ARC)
        return 180.0f;
    if(fabsf(wrapDegreesSigned(degrees + 90.0f)) < AUDIO_SURROUND_SIDE_ARC)
        return -90.0f;
    if(fabsf(wrapDegreesSigned(degrees - 90.0f)) < AUDIO_SURROUND_SIDE_ARC)
        return 90.0f;

    return degrees;
}

AudioSpatialVector inverseTransformPoint(const AudioSpatialMatrix& matrix,
                                         const AudioSpatialVector& point)
{
    const float m00 = matrix.right.x;
    const float m01 = matrix.up.x;
    const float m02 = matrix.at.x;
    const float m10 = matrix.right.y;
    const float m11 = matrix.up.y;
    const float m12 = matrix.at.y;
    const float m20 = matrix.right.z;
    const float m21 = matrix.up.z;
    const float m22 = matrix.at.z;
    const float determinant =
        m00 * (m11 * m22 - m12 * m21) -
        m01 * (m10 * m22 - m12 * m20) +
        m02 * (m10 * m21 - m11 * m20);

    const float inverseDeterminant =
        determinant == 0.0f ? 1.0f : 1.0f / determinant;
    const float x = point.x - matrix.pos.x;
    const float y = point.y - matrix.pos.y;
    const float z = point.z - matrix.pos.z;

    AudioSpatialVector transformed;
    transformed.x =
        ((m11 * m22 - m12 * m21) * x +
         (m02 * m21 - m01 * m22) * y +
         (m01 * m12 - m02 * m11) * z) * inverseDeterminant;
    transformed.y =
        ((m12 * m20 - m10 * m22) * x +
         (m00 * m22 - m02 * m20) * y +
         (m02 * m10 - m00 * m12) * z) * inverseDeterminant;
    transformed.z =
        ((m10 * m21 - m11 * m20) * x +
         (m01 * m20 - m00 * m21) * y +
         (m00 * m11 - m01 * m10) * z) * inverseDeterminant;
    return transformed;
}

}

uint8_t
scaleAudioVolume(uint8_t volume, uint8_t setting)
{
    if(setting > AUDIO_VOLUME_SETTING_MAX)
        setting = AUDIO_VOLUME_SETTING_MAX;
    const float scaled = (float)volume * (float)setting /
        (float)AUDIO_VOLUME_SETTING_MAX;
    return (uint8_t)(uint32_t)scaled;
}

uint16_t
convertXpecSoundVolume(int volume)
{
    if(volume < 0)
        volume = 0;
    if(volume > 1000)
        volume = 1000;
    return static_cast<uint16_t>(
        static_cast<float>(volume) / 1000.0f * 16383.0f);
}

AudioOutputGains
computeXpecStereoVoiceGains(float volume, uint32_t channel, int outputMode)
{
    AudioOutputGains gains;
    if(outputMode == AUDIO_OUTPUT_MONO)
    {
        gains.left = volume * 0.5f;
        gains.right = volume * 0.5f;
    }
    else
    {
        gains.left = channel == 0 ? volume : 0.0f;
        gains.right = channel == 0 ? 0.0f : volume;
    }
    return gains;
}

float
audioBearing(float x, float z)
{
    if(x > 0.0f)
        return 1.5707964f - atanf(z / x);
    if(x < 0.0f)
        return 4.712389f - atanf(z / x);
    return z < 0.0f ? 3.1415927f : 0.0f;
}

AudioListener
computeAudioListener(const AudioSpatialMatrix& viewMatrix,
                     const AudioSpatialMatrix& frameMatrix)
{
    const AudioSpatialVector viewPoint = { 0.5f, 0.5f, 1.0f };
    AudioListener listener;
    listener.position = inverseTransformPoint(viewMatrix, viewPoint);
    const float forwardLength = sqrtf(
        frameMatrix.at.x * frameMatrix.at.x +
        frameMatrix.at.y * frameMatrix.at.y +
        frameMatrix.at.z * frameMatrix.at.z);
    const float inverseForwardLength = 1.0f / forwardLength;
    listener.forward.x = frameMatrix.at.x * inverseForwardLength;
    listener.forward.y = frameMatrix.at.y * inverseForwardLength;
    listener.forward.z = frameMatrix.at.z * inverseForwardLength;
    return listener;
}

AudioPlacement
computeAudioPlacement(const AudioListener* listener,
                      const AudioSpatialVector* source, int outputMode)
{
    AudioPlacement placement;
    placement.volume = AUDIO_MAX_VOLUME;
    placement.pan = AUDIO_PAN_UNPLACED;
    placement.surroundAngle = 0.0f;
    placement.surround = false;

    if(source == nullptr)
        return placement;

    const float toSourceX = source->x - listener->position.x;
    const float toSourceY = source->y - listener->position.y;
    const float toSourceZ = source->z - listener->position.z;

    placement.volume = attenuationFor(sqrtf(toSourceX * toSourceX +
                                            toSourceY * toSourceY +
                                            toSourceZ * toSourceZ));

    if(outputMode == AUDIO_OUTPUT_MONO)
        return placement;

    const float radians = bearingDelta(
        audioBearing(listener->forward.x, listener->forward.z),
        audioBearing(toSourceX, toSourceZ));
    if(outputMode == AUDIO_OUTPUT_SURROUND)
    {
        placement.surround = true;
        placement.surroundAngle = surroundAngleFor(radians * 57.295776f);
        return placement;
    }

    const float offset = sinf(radians) * (float)AUDIO_PAN_SPAN;
    placement.pan = (uint8_t)((int)offset + AUDIO_PAN_CENTRE);
    return placement;
}

AudioPlacement
computeAudioPlacement(float listenerX, float listenerY, float listenerZ,
                      float forwardX, float forwardZ, float sourceX,
                      float sourceY, float sourceZ, int outputMode)
{
    const AudioListener listener =
    {
        { listenerX, listenerY, listenerZ },
        { forwardX, 0.0f, forwardZ }
    };
    const AudioSpatialVector source = { sourceX, sourceY, sourceZ };
    return computeAudioPlacement(&listener, &source, outputMode);
}
