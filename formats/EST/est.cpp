#include "est.h"
#include "../common.h"
#include "../RWS/rws.h"

#include <cmath>
#include <cstdio>
#include <utility>

namespace
{

EffectRecordType recordType(uint32_t rawType)
{
    switch (rawType)
    {
    case 0:
        return EffectRecordType::Atomic;
    case 1:
        return EffectRecordType::Clump;
    case 2:
        return EffectRecordType::Custom;
    case 0xffffffffu:
        return EffectRecordType::Absent;
    default:
        return EffectRecordType::Unknown;
    }
}

uint32_t expectedChunkId(EffectRecordType type)
{
    switch (type)
    {
    case EffectRecordType::Atomic:
        return RW_CHUNK_ATOMIC;
    case EffectRecordType::Clump:
        return RW_CHUNK_CLUMP;
    case EffectRecordType::Custom:
        return RW_CHUNK_CUSTOM;
    default:
        return 0;
    }
}

std::string readName(const uint8_t* name)
{
    if (name == NULL || name[0] == 0)
        return std::string();
    size_t length = 0;
    while (length < EST_NAME_SIZE && name[length] != 0)
        length++;
    return std::string(reinterpret_cast<const char*>(name), length);
}

}

EffectAsset::EffectAsset()
    : stream(), records(), objects(), textureChunkOffset(SIZE_MAX), directoryChunkOffset(SIZE_MAX),
      unknownActiveRecords(0)
{
}

void EffectAsset::clear()
{
    stream.clear();
    records.clear();
    objects.clear();
    textureChunkOffset = SIZE_MAX;
    directoryChunkOffset = SIZE_MAX;
    unknownActiveRecords = 0;
}

bool parseEstEffectStream(const uint8_t* data, size_t length, EffectAsset* asset)
{
    if (asset == NULL)
        return false;
    asset->clear();
    if (data == NULL || length < 12)
        return false;

    for (size_t offset = 0; offset < length;)
    {
        RwChunkSpan chunk;
        if (!readRwChunk(data, length, offset, chunk))
            return false;
        if (chunk.id == RW_CHUNK_TEXDICTIONARY && asset->textureChunkOffset == SIZE_MAX)
            asset->textureChunkOffset = chunk.offset;
        if (chunk.id == RW_CHUNK_OBJECT_DIRECTORY && asset->directoryChunkOffset == SIZE_MAX)
            asset->directoryChunkOffset = chunk.offset;
        offset = chunk.end;
    }
    if (asset->directoryChunkOffset == SIZE_MAX)
        return false;

    RwChunkSpan directory;
    if (!readRwChunk(data, length, asset->directoryChunkOffset, directory) || directory.dataSize < 4)
        return false;
    const uint32_t count = read32(data + directory.offset + 12);
    if (count > EST_MAX_RECORD_COUNT ||
        static_cast<uint64_t>(4) + static_cast<uint64_t>(count) * EST_RECORD_SIZE > directory.dataSize)
        return false;

    asset->stream.assign(data, data + length);
    asset->records.reserve(count);
    const uint8_t* recordData = data + directory.offset + 16;
    for (uint32_t i = 0; i < count; i++)
    {
        const uint8_t* source = recordData + i * EST_RECORD_SIZE;
        const uint32_t rawType = read32(source);
        EffectRecord record;
        record.type = recordType(rawType);
        record.sourceType = record.type;
        record.flags = read32(source + 4);
        record.payloadReference = readI32(source + 8);
        record.defaultScale = readFloat(source + 24);
        if (record.defaultScale == 0.0f)
            record.defaultScale = 1.0f;
        if (!std::isfinite(record.defaultScale) || record.defaultScale < 0.0f)
            record.defaultScale = 1.0f;

        record.initialLifetimeSeconds = 0.8f;
        record.textureName = readName(source + 12);
        record.payloadObject = -1;
        record.active = false;
        record.fallback = false;
        asset->records.push_back(record);
    }

    size_t objectCursor = directory.end;
    for (size_t i = 0; i < asset->records.size(); i++)
    {
        EffectRecord& record = asset->records[i];
        if (record.sourceType == EffectRecordType::Absent)
        {
            if (!record.textureName.empty())
            {
                record.type = EffectRecordType::TextureFallback;
                record.fallback = true;
                record.active = true;
            }
            continue;
        }
        if (record.payloadReference < 0)
            continue;

        if (record.payloadReference < static_cast<int32_t>(i))
        {
            const EffectRecord& owner = asset->records[static_cast<size_t>(record.payloadReference)];
            if (owner.active && owner.payloadObject >= 0)
            {
                record.payloadObject = owner.payloadObject;
                record.active = true;
            }
            else
            {
                record.type = EffectRecordType::Unknown;
                asset->unknownActiveRecords++;
            }
            continue;
        }
        if (record.payloadReference != static_cast<int32_t>(i))
        {
            record.type = EffectRecordType::Unknown;
            asset->unknownActiveRecords++;
            continue;
        }

        RwChunkSpan object;
        if (!readRwChunk(data, length, objectCursor, object))
            return false;
        const uint32_t expected = expectedChunkId(record.sourceType);
        if (expected == 0 || object.id != expected)
        {
            record.type = EffectRecordType::Unknown;
            asset->unknownActiveRecords++;
            objectCursor = object.end;
            continue;
        }
        EffectObjectChunk payload;
        payload.id = object.id;
        payload.libraryId = object.libraryId;
        payload.bytes.assign(data + object.offset, data + object.end);
        record.payloadObject = static_cast<int32_t>(asset->objects.size());
        record.active = true;
        asset->objects.push_back(std::move(payload));
        objectCursor = object.end;
    }
    return true;
}

bool parseEstEffectFile(const char* path, EffectAsset* asset)
{
    if (path == NULL || asset == NULL)
        return false;
    FILE* file = fopen(path, "rb");
    if (file == NULL)
        return false;
    if (fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return false;
    }
    const long fileLength = ftell(file);
    if (fileLength <= 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return false;
    }
    std::vector<uint8_t> data(static_cast<size_t>(fileLength));
    const bool read = fread(data.data(), 1, data.size(), file) == data.size();
    fclose(file);
    return read && parseEstEffectStream(data.data(), data.size(), asset);
}
