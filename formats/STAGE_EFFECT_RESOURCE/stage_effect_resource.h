#ifndef STAGE_EFFECT_RESOURCE_H
#define STAGE_EFFECT_RESOURCE_H

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

struct StageEffectResource
{
    uint32_t stageId;
    uint32_t resourceId;
    uint32_t archiveEntryIndex;
    uint32_t animationStreamCount;
    std::string archiveEntryName;
    std::vector<uint8_t> packedBundle;

    StageEffectResource();
    void clear();
};

bool resolveStageEffectResourceId(uint32_t resourceId, uint32_t* archiveEntryIndex);
bool loadStageEffectResource(const char* stageArchivePath, uint32_t stageId, StageEffectResource* resource);

#endif
