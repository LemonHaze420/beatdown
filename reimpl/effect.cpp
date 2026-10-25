#include "effect.h"
#include "formats/STAGE_EFFECT_RESOURCE/stage_effect_resource_table.h"

#include "xaf_runtime.h"

#include <rw.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>

namespace
{

const uint32_t CHUNK_TEXDICTIONARY = 0x16;
const uint32_t CHUNK_ATOMIC = 0x14;
const uint32_t CHUNK_CLUMP = 0x10;
const uint32_t CHUNK_EST_DIRECTORY = 0x79032003;
const uint32_t CHUNK_EST_CUSTOM = 0x78032010;
const uint32_t CHUNK_EST_CUSTOM_PAYLOAD = 0x78032020;
const uint32_t CHUNK_EST_CUSTOM_ANIMATIONS = 0x78032030;
const uint32_t CHUNK_ANIMATION = 0x1b;
const uint32_t EFFECT_FLAGS_ATOMIC_BRANCH = 0x04000000;
const uint32_t MAX_PACKED_BUNDLE_ENTRIES = 0x10000;

struct ChunkSpan
{
    size_t offset;
    size_t end;
    uint32_t id;
    uint32_t size;
    uint32_t libraryId;
};

uint32_t read32(const uint8_t* data)
{
    uint32_t value;
    memcpy(&value, data, sizeof(value));
    return value;
}

void write32(uint8_t* data, uint32_t value)
{
    memcpy(data, &value, sizeof(value));
}

int32_t readI32(const uint8_t* data)
{
    int32_t value;
    memcpy(&value, data, sizeof(value));
    return value;
}

float readFloat(const uint8_t* data)
{
    float value;
    memcpy(&value, data, sizeof(value));
    return value;
}

bool readChunk(const uint8_t* data, size_t length, size_t offset,
              ChunkSpan* chunk)
{
    if(chunk == NULL || offset > length || length - offset < 12)
        return false;
    const uint32_t size = read32(data + offset + 4);
    if(static_cast<uint64_t>(offset) + 12 + size > length)
        return false;
    chunk->offset = offset;
    chunk->end = offset + 12 + size;
    chunk->id = read32(data + offset);
    chunk->size = size;
    chunk->libraryId = read32(data + offset + 8);
    return true;
}

EffectRecordType recordType(uint32_t rawType)
{
    switch(rawType)
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
    switch(type)
    {
    case EffectRecordType::Atomic:
        return CHUNK_ATOMIC;
    case EffectRecordType::Clump:
        return CHUNK_CLUMP;
    case EffectRecordType::Custom:
        return CHUNK_EST_CUSTOM;
    default:
        return 0;
    }
}

bool hasName(const uint8_t* name)
{
    return name != NULL && name[0] != 0;
}

std::string readName(const uint8_t* name)
{
    if(!hasName(name))
        return std::string();
    size_t length = 0;
    while(length < 12 && name[length] != 0)
        length++;
    return std::string(reinterpret_cast<const char*>(name), length);
}

bool isFinite(float value)
{
    return std::isfinite(value) != 0;
}

struct EffectRenderObject
{
    rw::Atomic* atomic;
    rw::Clump* clump;

    rw::Frame* sourceFrame;
    rw::Frame* objectFrame;

    EffectRenderObject()
        : atomic(NULL), clump(NULL), sourceFrame(NULL), objectFrame(NULL)
    {
    }
};

struct EffectInstance
{
    bool active;
    bool controllerComplete;
    uint32_t handle;
    uint8_t recordIndex;
    float ageSeconds;
    float lifetimeSeconds;
    int16_t tuningA;
    int16_t tuningB;
    EffectMatrix world;
    EffectRenderObject renderObject;

    EffectInstance()
        : active(false), controllerComplete(false),
          handle(EFFECT_INVALID_HANDLE), recordIndex(EFFECT_RECORD_SENTINEL),
          ageSeconds(0.0f), lifetimeSeconds(0.8f), tuningA(0), tuningB(0),
          world(effectIdentityMatrix()), renderObject()
    {
    }
};

struct EffectEmitter
{
    bool active;
    uint32_t handle;
    uint8_t recordIndex;
    uint8_t mode;
    float ageSeconds;
    float timerSeconds;
    float intervalSeconds;
    float lifetimeSeconds;
    EffectMatrixProvider* provider;
    void* userData;
    EffectMatrix world;

    EffectEmitter()
        : active(false), handle(EFFECT_INVALID_HANDLE),
          recordIndex(EFFECT_RECORD_SENTINEL), mode(0), ageSeconds(0.0f),
          timerSeconds(0.0f), intervalSeconds(0.0f), lifetimeSeconds(0.0f),
          provider(NULL), userData(NULL), world(effectIdentityMatrix())
    {
    }
};

void setFrameMatrix(rw::Frame* frame, const EffectMatrix& world, float scale)
{
    if(frame == NULL)
        return;
    frame->matrix.right = { world.right.x * scale, world.right.y * scale,
                            world.right.z * scale };
    frame->matrix.up = { world.up.x * scale, world.up.y * scale,
                         world.up.z * scale };
    frame->matrix.at = { world.at.x * scale, world.at.y * scale,
                         world.at.z * scale };
    frame->matrix.pos = { world.pos.x, world.pos.y, world.pos.z };
    frame->matrix.optimize();
    frame->updateObjects();
}
void applyAtomicRenderFlags(rw::Atomic* atomic, uint32_t effectFlags)
{
    if(atomic == NULL)
        return;
    const uint32_t flags = atomic->getFlags();
    if((effectFlags & EFFECT_FLAGS_ATOMIC_BRANCH) == 0)
        atomic->setFlags(flags | 0x20);
    else
        atomic->setFlags(flags & ~0x20u);
}

void applyAtomicRenderFlags(rw::Clump* clump, uint32_t effectFlags)
{
    if(clump == NULL)
        return;
    FORLIST(link, clump->atomics)
    {
        applyAtomicRenderFlags(rw::Atomic::fromClump(link), effectFlags);
    }
}

bool findNestedChunk(const uint8_t* data, size_t length, uint32_t id,
                     size_t* offset)
{
    if(data == NULL || offset == NULL)
        return false;
    for(size_t cursor = 0; cursor + 12 <= length;)
    {
        ChunkSpan chunk;
        if(!readChunk(data, length, cursor, &chunk))
            return false;
        if(chunk.id == id)
        {
            *offset = cursor;
            return true;
        }
        cursor = chunk.end;
    }
    return false;
}

rw::Clump* readClumpChunk(const EffectObjectChunk& object)
{
    rw::StreamMemory stream;
    stream.open(const_cast<uint8_t*>(object.bytes.data()),
                static_cast<uint32_t>(object.bytes.size()));
    if(!rw::findChunk(&stream, rw::ID_CLUMP, NULL, NULL))
        return NULL;
    return rw::Clump::streamRead(&stream);
}

rw::Atomic* readAtomicChunk(const EffectObjectChunk& object, rw::Frame** frame)
{
    if(frame == NULL || object.bytes.size() < 40)
        return NULL;

    const uint8_t* source = object.bytes.data();
    ChunkSpan outer;
    if(!readChunk(source, object.bytes.size(), 0, &outer) ||
       outer.id != CHUNK_ATOMIC)
        return NULL;
    ChunkSpan structure;
    if(!readChunk(source, object.bytes.size(), 12, &structure) ||
       structure.id != rw::ID_STRUCT || structure.size < 12)
        return NULL;

    const uint8_t* structureData = source + structure.offset + 12;
    const uint32_t frameIndex = read32(structureData);
    const size_t frameCount = std::min<size_t>(frameIndex + 1, 64);
    if(frameIndex >= frameCount)
        return NULL;
    std::vector<rw::Frame*> frames(frameCount, NULL);
    *frame = rw::Frame::create();
    if(*frame == NULL)
        return NULL;
    frames[frameIndex] = *frame;

    std::vector<uint8_t> converted;
    if(structure.size >= 16)
    {
        const size_t suffixOffset = structure.end;
        converted.resize(object.bytes.size() - 4);
        memcpy(converted.data(), source, 12);
        write32(converted.data() + 4,
                static_cast<uint32_t>(converted.size() - 12));
        memcpy(converted.data() + 12, source + structure.offset, 12);
        write32(converted.data() + 16, 12);
        write32(converted.data() + 20, 0x00000300);
        memcpy(converted.data() + 24, structureData, 4);
        memcpy(converted.data() + 28, structureData + 4, 4);
        memcpy(converted.data() + 32, structureData + 8, 4);
        memcpy(converted.data() + 36, source + suffixOffset,
               object.bytes.size() - suffixOffset);
    }

    const uint8_t* streamData = converted.empty() ? source : converted.data();
    const size_t streamLength = converted.empty() ? object.bytes.size() :
                                                    converted.size();
    rw::StreamMemory stream;
    stream.open(const_cast<uint8_t*>(streamData),
                static_cast<uint32_t>(streamLength));
    if(!rw::findChunk(&stream, rw::ID_ATOMIC, NULL, NULL))
    {
        (*frame)->destroy();
        *frame = NULL;
        return NULL;
    }
    rw::FrameList_ frameList = {
        static_cast<int32_t>(frames.size()), frames.data()
    };
    rw::Atomic* atomic = rw::Atomic::streamReadClump(&stream, &frameList,
                                                     NULL);
    if(atomic == NULL)
    {
        (*frame)->destroy();
        *frame = NULL;
    }
    return atomic;
}

rw::Clump* readCustomClumpChunk(const EffectObjectChunk& object)
{
    if(object.bytes.size() < 24 || read32(object.bytes.data()) != CHUNK_EST_CUSTOM)
        return NULL;

    const uint8_t* data = object.bytes.data();
    const size_t length = object.bytes.size();
    size_t payloadOffset = 0;
    for(size_t cursor = 24; cursor + 12 <= length;)
    {
        ChunkSpan chunk;
        if(!readChunk(data, length, cursor, &chunk))
            break;
        if(chunk.id == CHUNK_EST_CUSTOM_PAYLOAD)
        {
            payloadOffset = chunk.offset;
            break;
        }
        cursor = chunk.end;
    }
    if(payloadOffset == 0)
        return NULL;

    const ChunkSpan payload = {
        payloadOffset,
        payloadOffset + 12 + read32(data + payloadOffset + 4),
        read32(data + payloadOffset),
        read32(data + payloadOffset + 4),
        read32(data + payloadOffset + 8)
    };
    const uint8_t* nested = data + payload.offset + 12;
    const size_t nestedLength = payload.end - payload.offset - 12;
    size_t clumpOffset = 0;
    if(!findNestedChunk(nested, nestedLength, CHUNK_CLUMP, &clumpOffset))
        return NULL;

    rw::StreamMemory stream;
    stream.open(const_cast<uint8_t*>(nested + clumpOffset),
                static_cast<uint32_t>(nestedLength - clumpOffset));
    if(!rw::findChunk(&stream, rw::ID_CLUMP, NULL, NULL))
        return NULL;
    return rw::Clump::streamRead(&stream);
}

rw::Atomic* createTextureFallback(rw::TexDictionary* dictionary,
                                  const std::string& name,
                                  rw::Frame** frame)
{
    if(dictionary == NULL || frame == NULL || name.empty())
        return NULL;
    rw::Texture* texture = dictionary->find(name.c_str());
    if(texture == NULL)
        return NULL;

    rw::Geometry* geometry = rw::Geometry::create(
        4, 2, rw::Geometry::TEXTURED | rw::Geometry::PRELIT);
    if(geometry == NULL)
        return NULL;
    geometry->morphTargets[0].vertices[0] = { -0.5f, -0.5f, 0.0f };
    geometry->morphTargets[0].vertices[1] = { 0.5f, -0.5f, 0.0f };
    geometry->morphTargets[0].vertices[2] = { 0.5f, 0.5f, 0.0f };
    geometry->morphTargets[0].vertices[3] = { -0.5f, 0.5f, 0.0f };
    for(int32_t i = 0; i < 4; i++)
    {
        geometry->colors[i] = rw::makeRGBA(255, 255, 255, 255);
        geometry->texCoords[0][i].u = (i == 0 || i == 3) ? 0.0f : 1.0f;
        geometry->texCoords[0][i].v = (i < 2) ? 1.0f : 0.0f;
    }
    geometry->triangles[0] = { { 0, 1, 2 }, 0 };
    geometry->triangles[1] = { { 0, 2, 3 }, 0 };
    geometry->morphTargets[0].boundingSphere.center = { 0.0f, 0.0f, 0.0f };
    geometry->morphTargets[0].boundingSphere.radius = 0.70710678f;
    rw::Material* material = rw::Material::create();
    if(material == NULL)
    {
        geometry->destroy();
        return NULL;
    }
    material->setTexture(texture);
    material->color = rw::makeRGBA(255, 255, 255, 255);
    geometry->matList.appendMaterial(material);
    material->destroy();
    geometry->buildMeshes();

    rw::Atomic* atomic = rw::Atomic::create();
    if(atomic == NULL)
    {
        geometry->destroy();
        return NULL;
    }
    atomic->setGeometry(geometry, 0);
    geometry->destroy();
    *frame = rw::Frame::create();
    if(*frame == NULL)
    {
        atomic->destroy();
        return NULL;
    }
    atomic->setFrame(*frame);
    return atomic;
}

void destroyRenderObject(EffectRenderObject* object, rw::World* world)
{
    if(object == NULL)
        return;

    if(object->atomic != NULL)
    {
        if(world != NULL && object->atomic->world == world)
            world->removeAtomic(object->atomic);
        object->atomic->destroy();
    }
    if(object->clump != NULL)
    {
        if(world != NULL && object->clump->world == world)
            world->removeClump(object->clump);
        object->clump->destroy();
    }
    if(object->sourceFrame != NULL)
        object->sourceFrame->destroy();
    object->atomic = NULL;
    object->clump = NULL;
    object->sourceFrame = NULL;
    object->objectFrame = NULL;
}

}

EffectMatrix
effectIdentityMatrix(void)
{
    EffectMatrix matrix = {
        { 1.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f }
    };
    return matrix;
}

EffectMatrix
effectYRotationMatrix(float degrees, const EffectVector3& position)
{
    const float radians = degrees * 0.01745329251994329577f;
    const float sine = sinf(radians);
    const float cosine = cosf(radians);
    EffectMatrix matrix = {
        { cosine, 0.0f, -sine },
        { 0.0f, 1.0f, 0.0f },
        { sine, 0.0f, cosine },
        position
    };
    return matrix;
}

bool
effectMatrixIsFinite(const EffectMatrix& matrix)
{
    return isFinite(matrix.right.x) && isFinite(matrix.right.y) &&
           isFinite(matrix.right.z) && isFinite(matrix.up.x) &&
           isFinite(matrix.up.y) && isFinite(matrix.up.z) &&
           isFinite(matrix.at.x) && isFinite(matrix.at.y) &&
           isFinite(matrix.at.z) && isFinite(matrix.pos.x) &&
           isFinite(matrix.pos.y) && isFinite(matrix.pos.z);
}

EffectAsset::EffectAsset()
    : stream(), records(), objects(), textureChunkOffset(SIZE_MAX),
      directoryChunkOffset(SIZE_MAX), unknownActiveRecords(0)
{
}

void
EffectAsset::clear()
{
    stream.clear();
    records.clear();
    objects.clear();
    textureChunkOffset = SIZE_MAX;
    directoryChunkOffset = SIZE_MAX;
    unknownActiveRecords = 0;
}

StageEffectResource::StageEffectResource()
    : stageId(0), resourceId(0), archiveEntryIndex(0),
      animationStreamCount(0), archiveEntryName(), packedBundle()
{
}

void
StageEffectResource::clear()
{
    stageId = 0;
    resourceId = 0;
    archiveEntryIndex = 0;
    animationStreamCount = 0;
    archiveEntryName.clear();
    packedBundle.clear();
}

bool
parseEstEffectStream(const uint8_t* data, size_t length, EffectAsset* asset)
{
    if(asset == NULL)
        return false;
    asset->clear();
    if(data == NULL || length < 12)
        return false;

    std::vector<ChunkSpan> chunks;
    for(size_t offset = 0; offset < length;)
    {
        ChunkSpan chunk;
        if(!readChunk(data, length, offset, &chunk))
            return false;
        chunks.push_back(chunk);
        if(chunk.id == CHUNK_TEXDICTIONARY && asset->textureChunkOffset == SIZE_MAX)
            asset->textureChunkOffset = chunk.offset;
        if(chunk.id == CHUNK_EST_DIRECTORY && asset->directoryChunkOffset == SIZE_MAX)
            asset->directoryChunkOffset = chunk.offset;
        offset = chunk.end;
    }
    if(asset->directoryChunkOffset == SIZE_MAX)
        return false;

    ChunkSpan directory;
    if(!readChunk(data, length, asset->directoryChunkOffset, &directory) ||
       directory.size < 4)
        return false;
    const uint32_t count = read32(data + directory.offset + 12);
    if(count > 0x10000u ||
       static_cast<uint64_t>(4) + static_cast<uint64_t>(count) * 0x1c >
           directory.size)
        return false;

    asset->stream.assign(data, data + length);
    asset->records.reserve(count);
    const uint8_t* recordData = data + directory.offset + 16;
    for(uint32_t i = 0; i < count; i++)
    {
        const uint8_t* source = recordData + i * 0x1c;
        const uint32_t rawType = read32(source);
        EffectRecord record;
        record.type = recordType(rawType);
        record.sourceType = record.type;
        record.flags = read32(source + 4);
        record.payloadReference = readI32(source + 8);
        record.defaultScale = readFloat(source + 24);
        if(record.defaultScale == 0.0f)
            record.defaultScale = 1.0f;
        if(!isFinite(record.defaultScale) || record.defaultScale < 0.0f)
            record.defaultScale = 1.0f;

        record.initialLifetimeSeconds = 0.8f;
        record.textureName = readName(source + 12);
        record.payloadObject = -1;
        record.active = false;
        record.fallback = false;
        asset->records.push_back(record);
    }

    size_t objectCursor = directory.end;
    for(size_t i = 0; i < asset->records.size(); i++)
    {
        EffectRecord& record = asset->records[i];
        if(record.sourceType == EffectRecordType::Absent)
        {
            if(!record.textureName.empty())
            {
                record.type = EffectRecordType::TextureFallback;
                record.fallback = true;
                record.active = true;
            }
            continue;
        }
        if(record.payloadReference < 0)
            continue;

        if(record.payloadReference < static_cast<int32_t>(i))
        {
            const EffectRecord& owner =
                asset->records[static_cast<size_t>(record.payloadReference)];
            if(owner.active && owner.payloadObject >= 0)
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
        if(record.payloadReference != static_cast<int32_t>(i))
        {
            record.type = EffectRecordType::Unknown;
            asset->unknownActiveRecords++;
            continue;
        }

        ChunkSpan object;
        if(!readChunk(data, length, objectCursor, &object))
            return false;
        const uint32_t expected = expectedChunkId(record.sourceType);
        if(expected == 0 || object.id != expected)
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

bool
parseEstEffectFile(const char* path, EffectAsset* asset)
{
    if(path == NULL || asset == NULL)
        return false;
    FILE* file = fopen(path, "rb");
    if(file == NULL)
        return false;
    if(fseek(file, 0, SEEK_END) != 0)
    {
        fclose(file);
        return false;
    }
    const long fileLength = ftell(file);
    if(fileLength <= 0 || fseek(file, 0, SEEK_SET) != 0)
    {
        fclose(file);
        return false;
    }
    std::vector<uint8_t> data(static_cast<size_t>(fileLength));
    const bool read = fread(data.data(), 1, data.size(), file) == data.size();
    fclose(file);
    return read && parseEstEffectStream(data.data(), data.size(), asset);
}

bool
resolveStageEffectResourceId(uint32_t resourceId,
                             uint32_t* archiveEntryIndex)
{
    if(archiveEntryIndex == NULL ||
       resourceId < STAGE_EFFECT_FIRST_RESOURCE_ID)
        return false;
    const uint32_t row = resourceId - STAGE_EFFECT_FIRST_RESOURCE_ID;
    if(row >= STAGE_EFFECT_TABLE_ROW_COUNT)
        return false;

    const int16_t entry = STAGE_EFFECT_ARCHIVE_ENTRY_BY_RESOURCE[row];
    if(entry < 0)
        return false;
    *archiveEntryIndex = static_cast<uint32_t>(entry);
    return true;
}

bool
loadStageEffectResource(const char* stageArchivePath, uint32_t stageId,
                        StageEffectResource* resource)
{
    if(resource == NULL)
        return false;
    resource->clear();
    if(stageArchivePath == NULL || stageId >= STAGE_EFFECT_STAGE_COUNT)
        return false;

    const uint32_t resourceId = STAGE_EFFECT_FIRST_RESOURCE_ID + stageId;
    uint32_t archiveEntryIndex;
    if(!resolveStageEffectResourceId(resourceId, &archiveEntryIndex))
        return false;
    std::vector<XafEntryInfo> entries;
    if(!listXafEntries(stageArchivePath, entries) ||
       archiveEntryIndex >= entries.size())
        return false;

    std::vector<uint8_t> bundle;
    if(!readXafEntry(stageArchivePath, entries[archiveEntryIndex], bundle) ||
       bundle.size() < 8)
        return false;
    const uint32_t streamCount = read32(bundle.data());
    const uint64_t tableEnd = 4 + static_cast<uint64_t>(streamCount) * 4;
    if(streamCount == 0 || streamCount > MAX_PACKED_BUNDLE_ENTRIES ||
       tableEnd > bundle.size())
        return false;

    std::vector<uint8_t> stream;
    for(uint32_t i = 0; i < streamCount; i++)
    {
        if(!extractPackedBundleEntry(bundle, i, stream) ||
           stream.size() < 12 || read32(stream.data()) != CHUNK_ANIMATION ||
           static_cast<uint64_t>(read32(stream.data() + 4)) + 12 !=
               stream.size())
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

struct EffectManager::Impl
{
    rw::World* world;
    rw::TexDictionary* textureDictionary;
    bool initialized;
    bool loaded;
    bool paused;
    float timeScale;
    uint32_t nextHandle;
    uint32_t randomState;
    size_t invalidDispatches;
    size_t emitterCursor;
    EffectAsset asset;
    StageEffectResource stageResource;
    std::vector<EffectInstance> pool;
    EffectEmitter emitters[EFFECT_EMITTER_CAPACITY];

    Impl()
        : world(NULL), textureDictionary(NULL), initialized(false),
          loaded(false), paused(false), timeScale(1.0f), nextHandle(1),
          randomState(0x13579bdfu), invalidDispatches(0), emitterCursor(0),
          asset(), stageResource(), pool(EFFECT_ACTIVE_CAPACITY)
    {
    }
};

namespace
{

EffectManager::Impl* managerImpl(EffectManager* manager)
{
    return manager == NULL ? NULL : manager->implementation();
}

const EffectManager::Impl* managerImpl(const EffectManager* manager)
{
    return manager == NULL ? NULL : manager->implementation();
}

uint32_t allocateHandle(EffectManager::Impl* impl)
{
    if(impl == NULL)
        return EFFECT_INVALID_HANDLE;
    uint32_t handle = impl->nextHandle++;
    if(handle == 0 || handle == EFFECT_INVALID_HANDLE)
        handle = impl->nextHandle++;
    return handle;
}

bool validRecord(const EffectManager::Impl* impl, uint8_t recordIndex)
{
    return impl != NULL && recordIndex != EFFECT_RECORD_SENTINEL &&
           static_cast<size_t>(recordIndex) < impl->asset.records.size() &&
           impl->asset.records[recordIndex].active;
}

EffectInstance* findInstance(EffectManager::Impl* impl, uint32_t handle)
{
    if(impl == NULL || handle == EFFECT_INVALID_HANDLE)
        return NULL;
    for(size_t i = 0; i < impl->pool.size(); ++i)
        if(impl->pool[i].active && impl->pool[i].handle == handle)
            return &impl->pool[i];
    return NULL;
}

const EffectInstance* findInstance(const EffectManager::Impl* impl,
                                   uint32_t handle)
{
    if(impl == NULL || handle == EFFECT_INVALID_HANDLE)
        return NULL;
    for(size_t i = 0; i < impl->pool.size(); ++i)
        if(impl->pool[i].active && impl->pool[i].handle == handle)
            return &impl->pool[i];
    return NULL;
}

EffectEmitter* findEmitter(EffectManager::Impl* impl, uint32_t handle)
{
    if(impl == NULL || handle == EFFECT_INVALID_HANDLE)
        return NULL;
    for(size_t i = 0; i < EFFECT_EMITTER_CAPACITY; ++i)
        if(impl->emitters[i].active && impl->emitters[i].handle == handle)
            return &impl->emitters[i];
    return NULL;
}

const EffectEmitter* findEmitter(const EffectManager::Impl* impl,
                                 uint32_t handle)
{
    if(impl == NULL || handle == EFFECT_INVALID_HANDLE)
        return NULL;
    for(size_t i = 0; i < EFFECT_EMITTER_CAPACITY; ++i)
        if(impl->emitters[i].active && impl->emitters[i].handle == handle)
            return &impl->emitters[i];
    return NULL;
}

void resetInstance(EffectManager::Impl* impl, EffectInstance* instance)
{
    if(impl == NULL || instance == NULL)
        return;
    destroyRenderObject(&instance->renderObject, impl->world);
    *instance = EffectInstance();
}

void resetEmitter(EffectEmitter* emitter)
{
    if(emitter != NULL)
        *emitter = EffectEmitter();
}

void releaseAllRenderObjects(EffectManager::Impl* impl)
{
    if(impl == NULL)
        return;
    for(size_t i = 0; i < impl->pool.size(); ++i)
        resetInstance(impl, &impl->pool[i]);
    if(impl->textureDictionary != NULL)
    {
        impl->textureDictionary->destroy();
        impl->textureDictionary = NULL;
    }
}

bool loadTextureDictionary(EffectManager::Impl* impl)
{
    if(impl == NULL || !impl->initialized || impl->world == NULL ||
       impl->asset.textureChunkOffset == SIZE_MAX)
        return false;
    ChunkSpan chunk;
    if(!readChunk(impl->asset.stream.data(), impl->asset.stream.size(),
                  impl->asset.textureChunkOffset, &chunk))
        return false;
    rw::TexDictionary* previous = rw::TexDictionary::getCurrent();
    rw::StreamMemory stream;
    stream.open(impl->asset.stream.data() + chunk.offset,
                static_cast<uint32_t>(chunk.end - chunk.offset));
    if(!rw::findChunk(&stream, rw::ID_TEXDICTIONARY, NULL, NULL))
        return false;
    impl->textureDictionary = rw::TexDictionary::streamRead(&stream);
    rw::TexDictionary::setCurrent(previous);
    return impl->textureDictionary != NULL;
}

bool instantiateRenderObject(EffectManager::Impl* impl,
                             EffectInstance* instance)
{
    if(impl == NULL || instance == NULL || !impl->initialized ||
       impl->world == NULL || !validRecord(impl, instance->recordIndex))
        return false;
    const EffectRecord& record = impl->asset.records[instance->recordIndex];
    rw::TexDictionary* previous = rw::TexDictionary::getCurrent();
    if(impl->textureDictionary != NULL)
        rw::TexDictionary::setCurrent(impl->textureDictionary);

    bool success = false;
    if(record.type == EffectRecordType::TextureFallback)
    {
        instance->renderObject.atomic = createTextureFallback(
            impl->textureDictionary, record.textureName,
            &instance->renderObject.sourceFrame);
        instance->renderObject.objectFrame =
            instance->renderObject.sourceFrame;
        success = instance->renderObject.atomic != NULL;
        if(success)
        {
            applyAtomicRenderFlags(instance->renderObject.atomic, record.flags);
            impl->world->addAtomic(instance->renderObject.atomic);
        }
    }
    else if(record.payloadObject >= 0 &&
            static_cast<size_t>(record.payloadObject) <
                impl->asset.objects.size())
    {
        const EffectObjectChunk& object =
            impl->asset.objects[static_cast<size_t>(record.payloadObject)];
        if(record.type == EffectRecordType::Atomic)
        {
            instance->renderObject.atomic = readAtomicChunk(
                object, &instance->renderObject.sourceFrame);
            instance->renderObject.objectFrame =
                instance->renderObject.sourceFrame;
            success = instance->renderObject.atomic != NULL;
            if(success)
            {
                applyAtomicRenderFlags(instance->renderObject.atomic,
                                        record.flags);
                impl->world->addAtomic(instance->renderObject.atomic);
            }
        }
        else if(record.type == EffectRecordType::Clump ||
                record.type == EffectRecordType::Custom)
        {
            instance->renderObject.clump =
                record.type == EffectRecordType::Clump ?
                    readClumpChunk(object) : readCustomClumpChunk(object);
            if(instance->renderObject.clump != NULL)
            {
                instance->renderObject.objectFrame =
                    instance->renderObject.clump->getFrame();
                instance->renderObject.sourceFrame = rw::Frame::create();
                success = instance->renderObject.objectFrame != NULL &&
                          instance->renderObject.sourceFrame != NULL;
                if(success)
                {
                    applyAtomicRenderFlags(instance->renderObject.clump,
                                            record.flags);
                    impl->world->addClump(instance->renderObject.clump);
                }
            }
        }
    }

    rw::TexDictionary::setCurrent(previous);
    if(!success)
        destroyRenderObject(&instance->renderObject, impl->world);
    return success;
}

void updateRenderObject(const EffectManager::Impl* impl,
                        EffectInstance* instance)
{
    if(impl == NULL || instance == NULL ||
       static_cast<size_t>(instance->recordIndex) >= impl->asset.records.size())
        return;
    const EffectRecord& record = impl->asset.records[instance->recordIndex];
    setFrameMatrix(instance->renderObject.sourceFrame, instance->world,
                   record.defaultScale);
    if(instance->renderObject.objectFrame != NULL &&
       instance->renderObject.objectFrame !=
           instance->renderObject.sourceFrame &&
       instance->renderObject.sourceFrame != NULL)
    {
        instance->renderObject.objectFrame->matrix =
            instance->renderObject.sourceFrame->matrix;
        instance->renderObject.objectFrame->updateObjects();
    }
}

EffectInstance* acquireInstance(EffectManager::Impl* impl)
{
    if(impl == NULL)
        return NULL;
    for(size_t i = 0; i < impl->pool.size(); ++i)
        if(!impl->pool[i].active)
            return &impl->pool[i];
    return NULL;
}

bool sampleEmitterMatrix(EffectManager::Impl* impl, EffectEmitter* emitter)
{
    if(impl == NULL || emitter == NULL || emitter->provider == NULL)
        return false;
    EffectMatrix sampled;
    if(!emitter->provider(emitter->userData, &sampled) ||
       !effectMatrixIsFinite(sampled))
        return false;
    if(emitter->mode == EFFECT_EMITTER_COPY_MATRIX)
    {
        emitter->world = sampled;
    }
    else
    {
        impl->randomState = impl->randomState * 1664525u + 1013904223u;
        const float degrees =
            static_cast<float>(impl->randomState % 360u);
        emitter->world = effectYRotationMatrix(degrees, sampled.pos);
    }
    return true;
}

}

EffectManager::EffectManager()
    : impl_(new Impl())
{
}

EffectManager::Impl*
EffectManager::implementation()
{
    return impl_;
}

const EffectManager::Impl*
EffectManager::implementation() const
{
    return impl_;
}

EffectManager::~EffectManager()
{
    shutdown();
    delete impl_;
    impl_ = NULL;
}

bool
EffectManager::initialize(rw::World* world)
{
    Impl* impl = managerImpl(this);
    if(impl == NULL)
        return false;
    if(impl->world != world)
    {
        stopAll();
        releaseAllRenderObjects(impl);
        impl->world = world;
    }
    impl->initialized = true;
    if(impl->loaded && impl->world != NULL &&
       impl->textureDictionary == NULL)
        loadTextureDictionary(impl);
    return true;
}

bool
EffectManager::load(const char* archivePath, const char* entryName)
{
    Impl* impl = managerImpl(this);
    if(impl == NULL || archivePath == NULL)
        return false;
    std::vector<uint8_t> data;
    if(entryName != NULL && readXafEntry(archivePath, entryName, data))
        return loadStream(data.data(), data.size());
    EffectAsset parsed;
    if(!parseEstEffectFile(archivePath, &parsed))
        return false;
    stopAll();
    releaseAllRenderObjects(impl);
    impl->asset = std::move(parsed);
    impl->invalidDispatches = 0;
    impl->loaded = true;
    if(impl->initialized && impl->world != NULL)
        loadTextureDictionary(impl);
    return impl->asset.unknownActiveRecords == 0;
}

bool
EffectManager::loadStream(const uint8_t* data, size_t length)
{
    Impl* impl = managerImpl(this);
    if(impl == NULL)
        return false;
    EffectAsset parsed;
    if(!parseEstEffectStream(data, length, &parsed))
        return false;
    const bool resolved = parsed.unknownActiveRecords == 0;
    stopAll();
    releaseAllRenderObjects(impl);
    impl->asset = std::move(parsed);
    impl->invalidDispatches = 0;
    impl->loaded = true;
    if(impl->initialized && impl->world != NULL)
        loadTextureDictionary(impl);
    return resolved;
}

bool
EffectManager::loadStageResource(const char* stageArchivePath,
                                 uint32_t stageId)
{
    Impl* impl = managerImpl(this);
    if(impl == NULL)
        return false;
    return loadStageEffectResource(stageArchivePath, stageId,
                                   &impl->stageResource);
}

void
EffectManager::shutdown()
{
    Impl* impl = managerImpl(this);
    if(impl == NULL)
        return;
    stopAll();
    releaseAllRenderObjects(impl);
    impl->asset.clear();
    impl->stageResource.clear();
    impl->loaded = false;
    impl->initialized = false;
    impl->invalidDispatches = 0;
    impl->world = NULL;
}

bool
EffectManager::isInitialized() const
{
    const Impl* impl = managerImpl(this);
    return impl != NULL && impl->initialized;
}

bool
EffectManager::isLoaded() const
{
    const Impl* impl = managerImpl(this);
    return impl != NULL && impl->loaded;
}

const EffectAsset&
EffectManager::asset() const
{
    static const EffectAsset empty;
    const Impl* impl = managerImpl(this);
    return impl == NULL ? empty : impl->asset;
}

size_t
EffectManager::invalidDispatchCount() const
{
    const Impl* impl = managerImpl(this);
    return impl == NULL ? 0 : impl->invalidDispatches;
}

bool
EffectManager::hasStageResource() const
{
    const Impl* impl = managerImpl(this);
    return impl != NULL && !impl->stageResource.packedBundle.empty();
}

const StageEffectResource&
EffectManager::stageResource() const
{
    static const StageEffectResource empty;
    const Impl* impl = managerImpl(this);
    return impl == NULL ? empty : impl->stageResource;
}

void
EffectManager::setPaused(bool paused)
{
    Impl* impl = managerImpl(this);
    if(impl != NULL)
        impl->paused = paused;
}

bool
EffectManager::paused() const
{
    const Impl* impl = managerImpl(this);
    return impl != NULL && impl->paused;
}

void
EffectManager::setTimeScale(float timeScale)
{
    Impl* impl = managerImpl(this);
    if(impl == NULL)
        return;
    if(!isFinite(timeScale))
        timeScale = 1.0f;
    impl->timeScale = std::max(0.0f, std::min(4.0f, timeScale));
}

float
EffectManager::timeScale() const
{
    const Impl* impl = managerImpl(this);
    return impl == NULL ? 1.0f : impl->timeScale;
}

uint32_t
EffectManager::dispatchEst(uint8_t recordIndex, const EffectMatrix& world,
                           int16_t tuningA, int16_t tuningB)
{
    Impl* impl = managerImpl(this);
    if(!validRecord(impl, recordIndex) || !effectMatrixIsFinite(world))
    {
        if(impl != NULL)
            ++impl->invalidDispatches;
        return EFFECT_INVALID_HANDLE;
    }
    EffectInstance* instance = acquireInstance(impl);
    if(instance == NULL)
        return EFFECT_INVALID_HANDLE;

    instance->active = true;
    instance->handle = allocateHandle(impl);
    instance->recordIndex = recordIndex;
    instance->ageSeconds = 0.0f;
    instance->lifetimeSeconds =
        impl->asset.records[recordIndex].initialLifetimeSeconds;
    instance->tuningA = tuningA;
    instance->tuningB = tuningB;
    instance->world = world;
    instantiateRenderObject(impl, instance);
    updateRenderObject(impl, instance);
    return instance->handle;
}

uint32_t
EffectManager::startEmitter(uint8_t recordIndex, float intervalSeconds,
                            float lifetimeSeconds, uint8_t mode,
                            EffectMatrixProvider* provider, void* userData)
{
    Impl* impl = managerImpl(this);
    if(!validRecord(impl, recordIndex) || provider == NULL ||
       !isFinite(intervalSeconds) || intervalSeconds <= 0.0f ||
       !isFinite(lifetimeSeconds) ||
       (mode != EFFECT_EMITTER_COPY_MATRIX &&
        mode != EFFECT_EMITTER_POSITION_RANDOM_Y))
    {
        if(impl != NULL)
            ++impl->invalidDispatches;
        return EFFECT_INVALID_HANDLE;
    }
    EffectEmitter& emitter =
        impl->emitters[impl->emitterCursor % EFFECT_EMITTER_CAPACITY];
    ++impl->emitterCursor;
    resetEmitter(&emitter);
    emitter.active = true;
    emitter.handle = allocateHandle(impl);
    emitter.recordIndex = recordIndex;
    emitter.mode = mode;
    emitter.intervalSeconds = intervalSeconds;
    emitter.lifetimeSeconds = lifetimeSeconds;
    emitter.provider = provider;
    emitter.userData = userData;
    if(!sampleEmitterMatrix(impl, &emitter))
    {
        resetEmitter(&emitter);
        ++impl->invalidDispatches;
        return EFFECT_INVALID_HANDLE;
    }
    return emitter.handle;
}

void
EffectManager::stop(uint32_t handle)
{
    Impl* impl = managerImpl(this);
    EffectInstance* instance = findInstance(impl, handle);
    if(instance != NULL)
        resetInstance(impl, instance);
}

void
EffectManager::complete(uint32_t handle)
{
    EffectInstance* instance = findInstance(managerImpl(this), handle);
    if(instance != NULL)
        instance->controllerComplete = true;
}

void
EffectManager::stopEmitter(uint32_t handle)
{
    resetEmitter(findEmitter(managerImpl(this), handle));
}

void
EffectManager::stopAll()
{
    Impl* impl = managerImpl(this);
    if(impl == NULL)
        return;
    for(size_t i = 0; i < impl->pool.size(); ++i)
        if(impl->pool[i].active)
            resetInstance(impl, &impl->pool[i]);
    for(size_t i = 0; i < EFFECT_EMITTER_CAPACITY; ++i)
        resetEmitter(&impl->emitters[i]);
}

void
EffectManager::update(float deltaSeconds)
{
    Impl* impl = managerImpl(this);
    if(impl == NULL || impl->paused || deltaSeconds <= 0.0f ||
       !isFinite(deltaSeconds))
        return;
    const float delta = deltaSeconds * impl->timeScale;
    if(delta <= 0.0f)
        return;

    for(size_t i = 0; i < EFFECT_EMITTER_CAPACITY; ++i)
    {
        EffectEmitter& emitter = impl->emitters[i];
        if(!emitter.active)
            continue;
        emitter.ageSeconds += delta;
        emitter.timerSeconds += delta;
        const bool sampled = sampleEmitterMatrix(impl, &emitter);
        if(sampled && emitter.timerSeconds >= emitter.intervalSeconds)
        {
            dispatchEst(emitter.recordIndex, emitter.world);
            emitter.timerSeconds = 0.0f;
        }
        if(emitter.lifetimeSeconds >= 0.0f &&
           emitter.ageSeconds >= emitter.lifetimeSeconds)
            resetEmitter(&emitter);
    }

    for(size_t i = 0; i < impl->pool.size(); ++i)
    {
        EffectInstance& instance = impl->pool[i];
        if(!instance.active)
            continue;
        instance.ageSeconds += delta;
        if(instance.controllerComplete ||
           (instance.lifetimeSeconds > 0.0f &&
            instance.ageSeconds >= instance.lifetimeSeconds))
        {
            resetInstance(impl, &instance);
            continue;
        }
        updateRenderObject(impl, &instance);
    }
}

void
EffectManager::render() const
{
    const Impl* impl = managerImpl(this);
    if(impl == NULL || !impl->initialized || impl->world == NULL)
        return;
    for(size_t i = 0; i < impl->pool.size(); ++i)
    {
        const EffectInstance& instance = impl->pool[i];
        if(!instance.active)
            continue;
        if(instance.renderObject.atomic != NULL &&
           instance.renderObject.atomic->world != impl->world)
            instance.renderObject.atomic->render();
        if(instance.renderObject.clump != NULL &&
           instance.renderObject.clump->world != impl->world)
            instance.renderObject.clump->render();
    }
}

size_t
EffectManager::activeCount() const
{
    const Impl* impl = managerImpl(this);
    if(impl == NULL)
        return 0;
    size_t count = 0;
    for(size_t i = 0; i < impl->pool.size(); ++i)
        if(impl->pool[i].active)
            ++count;
    return count;
}

size_t
EffectManager::activeEmitterCount() const
{
    const Impl* impl = managerImpl(this);
    if(impl == NULL)
        return 0;
    size_t count = 0;
    for(size_t i = 0; i < EFFECT_EMITTER_CAPACITY; ++i)
        if(impl->emitters[i].active)
            ++count;
    return count;
}

size_t
EffectManager::poolCapacity() const
{
    const Impl* impl = managerImpl(this);
    return impl == NULL ? 0 : impl->pool.size();
}

bool
EffectManager::getInstance(uint32_t handle, EffectInstanceState* state) const
{
    if(state == NULL)
        return false;
    const Impl* impl = managerImpl(this);
    const EffectInstance* instance = findInstance(impl, handle);
    if(instance == NULL)
        return false;
    state->active = instance->active;
    state->handle = instance->handle;
    state->recordIndex = instance->recordIndex;
    state->type = impl->asset.records[instance->recordIndex].type;
    state->ageSeconds = instance->ageSeconds;
    state->lifetimeSeconds = instance->lifetimeSeconds;
    state->tuningA = instance->tuningA;
    state->tuningB = instance->tuningB;
    state->world = instance->world;
    return true;
}

bool
EffectManager::getEmitter(uint32_t handle, EffectEmitterState* state) const
{
    if(state == NULL)
        return false;
    const EffectEmitter* emitter = findEmitter(managerImpl(this), handle);
    if(emitter == NULL)
        return false;
    state->active = emitter->active;
    state->handle = emitter->handle;
    state->recordIndex = emitter->recordIndex;
    state->mode = emitter->mode;
    state->ageSeconds = emitter->ageSeconds;
    state->timerSeconds = emitter->timerSeconds;
    state->intervalSeconds = emitter->intervalSeconds;
    state->lifetimeSeconds = emitter->lifetimeSeconds;
    state->world = emitter->world;
    return true;
}
