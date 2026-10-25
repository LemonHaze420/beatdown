#include "stage_effect_resource.h"
#include "stage_effect_resource_table.h"
#include "../common.h"
#include "../RWS/rws.h"
#include "../XAF/xaf_runtime.h"

#include <utility>

StageEffectResource::StageEffectResource()
    : stageId(0), resourceId(0), archiveEntryIndex(0), animationStreamCount(0), archiveEntryName(), packedBundle()
{
}

void StageEffectResource::clear()
{
    stageId = 0;
    resourceId = 0;
    archiveEntryIndex = 0;
    animationStreamCount = 0;
    archiveEntryName.clear();
    packedBundle.clear();
}

bool resolveStageEffectResourceId(uint32_t resourceId, uint32_t* archiveEntryIndex)
{
    if (archiveEntryIndex == NULL || resourceId < STAGE_EFFECT_FIRST_RESOURCE_ID)
        return false;
    const uint32_t row = resourceId - STAGE_EFFECT_FIRST_RESOURCE_ID;
    if (row >= STAGE_EFFECT_TABLE_ROW_COUNT)
        return false;

    const int16_t entry = STAGE_EFFECT_ARCHIVE_ENTRY_BY_RESOURCE[row];
    if (entry < 0)
        return false;
    *archiveEntryIndex = static_cast<uint32_t>(entry);
    return true;
}

bool loadStageEffectResource(const char* stageArchivePath, uint32_t stageId, StageEffectResource* resource)
{
    if (resource == NULL)
        return false;
    resource->clear();
    if (stageArchivePath == NULL || stageId >= STAGE_EFFECT_STAGE_COUNT)
        return false;

    const uint32_t resourceId = STAGE_EFFECT_FIRST_RESOURCE_ID + stageId;
    uint32_t archiveEntryIndex;
    if (!resolveStageEffectResourceId(resourceId, &archiveEntryIndex))
        return false;
    std::vector<XafEntryInfo> entries;
    if (!listXafEntries(stageArchivePath, entries) || archiveEntryIndex >= entries.size())
        return false;

    std::vector<uint8_t> bundle;
    if (!readXafEntry(stageArchivePath, entries[archiveEntryIndex], bundle) || bundle.size() < 8)
        return false;
    const uint32_t streamCount = read32(bundle.data());
    const uint64_t tableEnd = 4 + static_cast<uint64_t>(streamCount) * 4;
    if (streamCount == 0 || streamCount > XAF_MAX_PACKED_BUNDLE_ENTRIES || tableEnd > bundle.size())
        return false;

    std::vector<uint8_t> stream;
    for (uint32_t i = 0; i < streamCount; i++)
    {
        if (!extractPackedBundleEntry(bundle, i, stream) || stream.size() < 12 ||
            read32(stream.data()) != RW_CHUNK_ANIMATION ||
            static_cast<uint64_t>(read32(stream.data() + 4)) + 12 != stream.size())
            return false;
    }

    resource->stageId = stageId;
    resource->resourceId = resourceId;
    resource->archiveEntryIndex = archiveEntryIndex;
    resource->animationStreamCount = streamCount;
    resource->archiveEntryName = entries[archiveEntryIndex].name;
    resource->packedBundle = std::move(bundle);
    return true;
}
