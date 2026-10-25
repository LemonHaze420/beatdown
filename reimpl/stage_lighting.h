#ifndef STAGE_LIGHTING_H
#define STAGE_LIGHTING_H

#include <stddef.h>
#include <stdint.h>

struct StageLightingPreset
{
    bool fogEnabled;
    uint32_t fogColor;
    float fogDensity;
    float ambientColor[3];
    float directionalRotation[2][3];
    float directionalColor[2][3];
};

const size_t STAGE_LIGHTING_PRESET_COUNT = 54;

const StageLightingPreset* stageLightingPreset(uint16_t stageId);
float stageWorldLightScale(uint16_t stageId);

#endif
