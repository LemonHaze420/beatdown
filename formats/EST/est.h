#ifndef EST_H
#define EST_H

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

constexpr uint32_t EFFECT_FLAGS_ATOMIC_BRANCH = 0x04000000;
constexpr uint32_t EST_MAX_RECORD_COUNT = 0x10000;
constexpr size_t EST_RECORD_SIZE = 0x1c;
constexpr size_t EST_NAME_SIZE = 12;

enum class EffectRecordType : int32_t
{
    Absent = -1,
    Atomic = 0,
    Clump = 1,
    Custom = 2,
    TextureFallback = 4,
    Unknown = 0x7fffffff
};

struct EffectObjectChunk
{
    uint32_t id;
    uint32_t libraryId;
    std::vector<uint8_t> bytes;
};

struct EffectRecord
{
    EffectRecordType type;
    EffectRecordType sourceType;
    uint32_t flags;
    int32_t payloadReference;
    float defaultScale;

    float initialLifetimeSeconds;
    std::string textureName;
    int32_t payloadObject;
    bool active;
    bool fallback;
};

struct EffectAsset
{
    std::vector<uint8_t> stream;
    std::vector<EffectRecord> records;
    std::vector<EffectObjectChunk> objects;
    size_t textureChunkOffset;
    size_t directoryChunkOffset;
    size_t unknownActiveRecords;

    EffectAsset();
    void clear();
};

bool parseEstEffectStream(const uint8_t* data, size_t length, EffectAsset* asset);
bool parseEstEffectFile(const char* path, EffectAsset* asset);

#endif
