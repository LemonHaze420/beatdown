#include "effect.h"
#include "formats/common.h"
#include "formats/RWS/rws.h"

#include "xaf_runtime.h"

#include <rw.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>

namespace
{

using namespace effect_detail;

bool isFinite(float value)
{
    return std::isfinite(value) != 0;
}

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
        RwChunkSpan chunk;
        if (!readRwChunk(data, length, cursor, chunk))
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
    RwChunkSpan outer;
    if (!readRwChunk(source, object.bytes.size(), 0, outer) || outer.id != RW_CHUNK_ATOMIC)
        return NULL;
    RwChunkSpan structure;
    if (!readRwChunk(source, object.bytes.size(), 12, structure) || structure.id != rw::ID_STRUCT ||
        structure.dataSize < 12)
        return NULL;

    const uint8_t* structureData = source + structure.offset + 12;
    const uint32_t frameIndex = read32(structureData);
    const size_t frameCount = frameIndex < 64 ? frameIndex + 1 : 64;
    if(frameIndex >= frameCount)
        return NULL;
    std::vector<rw::Frame*> frames(frameCount, NULL);
    *frame = rw::Frame::create();
    if(*frame == NULL)
        return NULL;
    frames[frameIndex] = *frame;

    std::vector<uint8_t> converted;
    if (structure.dataSize >= 16)
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
    if (object.bytes.size() < 24 || read32(object.bytes.data()) != RW_CHUNK_CUSTOM)
        return NULL;

    const uint8_t* data = object.bytes.data();
    const size_t length = object.bytes.size();
    size_t payloadOffset = 0;
    for(size_t cursor = 24; cursor + 12 <= length;)
    {
        RwChunkSpan chunk;
        if (!readRwChunk(data, length, cursor, chunk))
            break;
        if (chunk.id == RW_CHUNK_CUSTOM_PAYLOAD)
        {
            payloadOffset = chunk.offset;
            break;
        }
        cursor = chunk.end;
    }
    if(payloadOffset == 0)
        return NULL;

    RwChunkSpan payload;
    if (!readRwChunk(data, length, payloadOffset, payload))
        return NULL;
    const uint8_t* nested = data + payload.offset + 12;
    const size_t nestedLength = payload.end - payload.offset - 12;
    size_t clumpOffset = 0;
    if (!findNestedChunk(nested, nestedLength, RW_CHUNK_CLUMP, &clumpOffset))
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
    EffectMatrix matrix = {.right = {.x = 1.0f, .y = 0.0f, .z = 0.0f},
                           .up = {.x = 0.0f, .y = 1.0f, .z = 0.0f},
                           .at = {.x = 0.0f, .y = 0.0f, .z = 1.0f},
                           .pos = {.x = 0.0f, .y = 0.0f, .z = 0.0f}};
    return matrix;
}

EffectMatrix
effectYRotationMatrix(float degrees, const EffectVector3& position)
{
    const float radians = degrees * 0.01745329251994329577f;
    const float sine = sinf(radians);
    const float cosine = cosf(radians);
    EffectMatrix matrix = {.right = {.x = cosine, .y = 0.0f, .z = -sine},
                           .up = {.x = 0.0f, .y = 1.0f, .z = 0.0f},
                           .at = {.x = sine, .y = 0.0f, .z = cosine},
                           .pos = position};
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

namespace
{

uint32_t allocateHandle(EffectManagerState* state)
{
    if (state == NULL)
        return EFFECT_INVALID_HANDLE;
    uint32_t handle = state->nextHandle++;
    if(handle == 0 || handle == EFFECT_INVALID_HANDLE)
        handle = state->nextHandle++;
    return handle;
}

bool validRecord(const EffectManagerState* state, uint8_t recordIndex)
{
    return state != NULL && recordIndex != EFFECT_RECORD_SENTINEL &&
           static_cast<size_t>(recordIndex) < state->asset.records.size() && state->asset.records[recordIndex].active;
}

EffectInstance* findInstance(EffectManagerState* state, uint32_t handle)
{
    if (state == NULL || handle == EFFECT_INVALID_HANDLE)
        return NULL;
    for (size_t i = 0; i < state->pool.size(); ++i)
        if (state->pool[i].active && state->pool[i].handle == handle)
            return &state->pool[i];
    return NULL;
}

const EffectInstance* findInstance(const EffectManagerState* state, uint32_t handle)
{
    if (state == NULL || handle == EFFECT_INVALID_HANDLE)
        return NULL;
    for (size_t i = 0; i < state->pool.size(); ++i)
        if (state->pool[i].active && state->pool[i].handle == handle)
            return &state->pool[i];
    return NULL;
}

EffectEmitter* findEmitter(EffectManagerState* state, uint32_t handle)
{
    if (state == NULL || handle == EFFECT_INVALID_HANDLE)
        return NULL;
    for(size_t i = 0; i < EFFECT_EMITTER_CAPACITY; ++i)
        if (state->emitters[i].active && state->emitters[i].handle == handle)
            return &state->emitters[i];
    return NULL;
}

const EffectEmitter* findEmitter(const EffectManagerState* state, uint32_t handle)
{
    if (state == NULL || handle == EFFECT_INVALID_HANDLE)
        return NULL;
    for(size_t i = 0; i < EFFECT_EMITTER_CAPACITY; ++i)
        if (state->emitters[i].active && state->emitters[i].handle == handle)
            return &state->emitters[i];
    return NULL;
}

void resetInstance(EffectManagerState* state, EffectInstance* instance)
{
    if (state == NULL || instance == NULL)
        return;
    destroyRenderObject(&instance->renderObject, state->world);
    *instance = EffectInstance();
}

void resetEmitter(EffectEmitter* emitter)
{
    if(emitter != NULL)
        *emitter = EffectEmitter();
}

void releaseAllRenderObjects(EffectManagerState* state)
{
    if (state == NULL)
        return;
    for (size_t i = 0; i < state->pool.size(); ++i)
        resetInstance(state, &state->pool[i]);
    if (state->textureDictionary != NULL)
    {
        state->textureDictionary->destroy();
        state->textureDictionary = NULL;
    }
}

bool loadTextureDictionary(EffectManagerState* state)
{
    if (state == NULL || !state->initialized || state->world == NULL || state->asset.textureChunkOffset == SIZE_MAX)
        return false;
    RwChunkSpan chunk;
    if (!readRwChunk(state->asset.stream.data(), state->asset.stream.size(), state->asset.textureChunkOffset, chunk))
        return false;
    rw::TexDictionary* previous = rw::TexDictionary::getCurrent();
    rw::StreamMemory stream;
    stream.open(state->asset.stream.data() + chunk.offset, static_cast<uint32_t>(chunk.end - chunk.offset));
    if(!rw::findChunk(&stream, rw::ID_TEXDICTIONARY, NULL, NULL))
        return false;
    state->textureDictionary = rw::TexDictionary::streamRead(&stream);
    rw::TexDictionary::setCurrent(previous);
    return state->textureDictionary != NULL;
}

bool instantiateRenderObject(EffectManagerState* state, EffectInstance* instance)
{
    if (state == NULL || instance == NULL || !state->initialized || state->world == NULL ||
        !validRecord(state, instance->recordIndex))
        return false;
    const EffectRecord& record = state->asset.records[instance->recordIndex];
    rw::TexDictionary* previous = rw::TexDictionary::getCurrent();
    if (state->textureDictionary != NULL)
        rw::TexDictionary::setCurrent(state->textureDictionary);

    bool success = false;
    if(record.type == EffectRecordType::TextureFallback)
    {
        instance->renderObject.atomic =
            createTextureFallback(state->textureDictionary, record.textureName, &instance->renderObject.sourceFrame);
        instance->renderObject.objectFrame =
            instance->renderObject.sourceFrame;
        success = instance->renderObject.atomic != NULL;
        if(success)
        {
            applyAtomicRenderFlags(instance->renderObject.atomic, record.flags);
            state->world->addAtomic(instance->renderObject.atomic);
        }
    }
    else if (record.payloadObject >= 0 && static_cast<size_t>(record.payloadObject) < state->asset.objects.size())
    {
        const EffectObjectChunk& object = state->asset.objects[static_cast<size_t>(record.payloadObject)];
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
                state->world->addAtomic(instance->renderObject.atomic);
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
                    state->world->addClump(instance->renderObject.clump);
                }
            }
        }
    }

    rw::TexDictionary::setCurrent(previous);
    if(!success)
        destroyRenderObject(&instance->renderObject, state->world);
    return success;
}

void updateRenderObject(const EffectManagerState* state, EffectInstance* instance)
{
    if (state == NULL || instance == NULL || static_cast<size_t>(instance->recordIndex) >= state->asset.records.size())
        return;
    const EffectRecord& record = state->asset.records[instance->recordIndex];
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

EffectInstance* acquireInstance(EffectManagerState* state)
{
    if (state == NULL)
        return NULL;
    for (size_t i = 0; i < state->pool.size(); ++i)
        if (!state->pool[i].active)
            return &state->pool[i];
    return NULL;
}

bool sampleEmitterMatrix(EffectManagerState* state, EffectEmitter* emitter)
{
    if (state == NULL || emitter == NULL || emitter->provider == NULL)
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
        state->randomState = state->randomState * 1664525u + 1013904223u;
        const float degrees = static_cast<float>(state->randomState % 360u);
        emitter->world = effectYRotationMatrix(degrees, sampled.pos);
    }
    return true;
}

}

EffectManager::EffectManager() : state_()
{
}

EffectManager::~EffectManager()
{
    shutdown();
}

bool
EffectManager::initialize(rw::World* world)
{
    if (state_.world != world)
    {
        stopAll();
        releaseAllRenderObjects(&state_);
        state_.world = world;
    }
    state_.initialized = true;
    if (state_.loaded && state_.world != NULL && state_.textureDictionary == NULL)
        loadTextureDictionary(&state_);
    return true;
}

bool
EffectManager::load(const char* archivePath, const char* entryName)
{
    if (archivePath == NULL)
        return false;
    std::vector<uint8_t> data;
    if(entryName != NULL && readXafEntry(archivePath, entryName, data))
        return loadStream(data.data(), data.size());
    EffectAsset parsed;
    if(!parseEstEffectFile(archivePath, &parsed))
        return false;
    stopAll();
    releaseAllRenderObjects(&state_);
    state_.asset = std::move(parsed);
    state_.invalidDispatches = 0;
    state_.loaded = true;
    if (state_.initialized && state_.world != NULL)
        loadTextureDictionary(&state_);
    return state_.asset.unknownActiveRecords == 0;
}

bool
EffectManager::loadStream(const uint8_t* data, size_t length)
{
    EffectAsset parsed;
    if(!parseEstEffectStream(data, length, &parsed))
        return false;
    const bool resolved = parsed.unknownActiveRecords == 0;
    stopAll();
    releaseAllRenderObjects(&state_);
    state_.asset = std::move(parsed);
    state_.invalidDispatches = 0;
    state_.loaded = true;
    if (state_.initialized && state_.world != NULL)
        loadTextureDictionary(&state_);
    return resolved;
}

bool
EffectManager::loadStageResource(const char* stageArchivePath,
                                 uint32_t stageId)
{
    return loadStageEffectResource(stageArchivePath, stageId, &state_.stageResource);
}

void
EffectManager::shutdown()
{
    stopAll();
    releaseAllRenderObjects(&state_);
    state_.asset.clear();
    state_.stageResource.clear();
    state_.loaded = false;
    state_.initialized = false;
    state_.invalidDispatches = 0;
    state_.world = NULL;
}

bool
EffectManager::isInitialized() const
{
    return state_.initialized;
}

bool
EffectManager::isLoaded() const
{
    return state_.loaded;
}

const EffectAsset&
EffectManager::asset() const
{
    return state_.asset;
}

size_t
EffectManager::invalidDispatchCount() const
{
    return state_.invalidDispatches;
}

bool
EffectManager::hasStageResource() const
{
    return !state_.stageResource.packedBundle.empty();
}

const StageEffectResource&
EffectManager::stageResource() const
{
    return state_.stageResource;
}

void
EffectManager::setPaused(bool paused)
{
    state_.paused = paused;
}

bool
EffectManager::paused() const
{
    return state_.paused;
}

void
EffectManager::setTimeScale(float timeScale)
{
    if(!isFinite(timeScale))
        timeScale = 1.0f;
    state_.timeScale = timeScale <= 0.0f ? 0.0f : timeScale >= 4.0f ? 4.0f : timeScale;
}

float
EffectManager::timeScale() const
{
    return state_.timeScale;
}

uint32_t
EffectManager::dispatchEst(uint8_t recordIndex, const EffectMatrix& world,
                           int16_t tuningA, int16_t tuningB)
{
    if (!validRecord(&state_, recordIndex) || !effectMatrixIsFinite(world))
    {
        ++state_.invalidDispatches;
        return EFFECT_INVALID_HANDLE;
    }
    EffectInstance* instance = acquireInstance(&state_);
    if(instance == NULL)
        return EFFECT_INVALID_HANDLE;

    instance->active = true;
    instance->handle = allocateHandle(&state_);
    instance->recordIndex = recordIndex;
    instance->ageSeconds = 0.0f;
    instance->lifetimeSeconds = state_.asset.records[recordIndex].initialLifetimeSeconds;
    instance->tuningA = tuningA;
    instance->tuningB = tuningB;
    instance->world = world;
    instantiateRenderObject(&state_, instance);
    updateRenderObject(&state_, instance);
    return instance->handle;
}

uint32_t
EffectManager::startEmitter(uint8_t recordIndex, float intervalSeconds,
                            float lifetimeSeconds, uint8_t mode,
                            EffectMatrixProvider* provider, void* userData)
{
    if (!validRecord(&state_, recordIndex) || provider == NULL || !isFinite(intervalSeconds) ||
        intervalSeconds <= 0.0f || !isFinite(lifetimeSeconds) ||
        (mode != EFFECT_EMITTER_COPY_MATRIX && mode != EFFECT_EMITTER_POSITION_RANDOM_Y))
    {
        ++state_.invalidDispatches;
        return EFFECT_INVALID_HANDLE;
    }
    EffectEmitter& emitter = state_.emitters[state_.emitterCursor % EFFECT_EMITTER_CAPACITY];
    ++state_.emitterCursor;
    resetEmitter(&emitter);
    emitter.active = true;
    emitter.handle = allocateHandle(&state_);
    emitter.recordIndex = recordIndex;
    emitter.mode = mode;
    emitter.intervalSeconds = intervalSeconds;
    emitter.lifetimeSeconds = lifetimeSeconds;
    emitter.provider = provider;
    emitter.userData = userData;
    if (!sampleEmitterMatrix(&state_, &emitter))
    {
        resetEmitter(&emitter);
        ++state_.invalidDispatches;
        return EFFECT_INVALID_HANDLE;
    }
    return emitter.handle;
}

void
EffectManager::stop(uint32_t handle)
{
    EffectInstance* instance = findInstance(&state_, handle);
    if(instance != NULL)
        resetInstance(&state_, instance);
}

void
EffectManager::complete(uint32_t handle)
{
    EffectInstance* instance = findInstance(&state_, handle);
    if(instance != NULL)
        instance->controllerComplete = true;
}

void
EffectManager::stopEmitter(uint32_t handle)
{
    resetEmitter(findEmitter(&state_, handle));
}

void
EffectManager::stopAll()
{
    for (size_t i = 0; i < state_.pool.size(); ++i)
        if (state_.pool[i].active)
            resetInstance(&state_, &state_.pool[i]);
    for(size_t i = 0; i < EFFECT_EMITTER_CAPACITY; ++i)
        resetEmitter(&state_.emitters[i]);
}

void
EffectManager::update(float deltaSeconds)
{
    if (state_.paused || deltaSeconds <= 0.0f || !isFinite(deltaSeconds))
        return;
    const float delta = deltaSeconds * state_.timeScale;
    if(delta <= 0.0f)
        return;

    for(size_t i = 0; i < EFFECT_EMITTER_CAPACITY; ++i)
    {
        EffectEmitter& emitter = state_.emitters[i];
        if(!emitter.active)
            continue;
        emitter.ageSeconds += delta;
        emitter.timerSeconds += delta;
        const bool sampled = sampleEmitterMatrix(&state_, &emitter);
        if(sampled && emitter.timerSeconds >= emitter.intervalSeconds)
        {
            dispatchEst(emitter.recordIndex, emitter.world);
            emitter.timerSeconds = 0.0f;
        }
        if(emitter.lifetimeSeconds >= 0.0f &&
           emitter.ageSeconds >= emitter.lifetimeSeconds)
            resetEmitter(&emitter);
    }

    for (size_t i = 0; i < state_.pool.size(); ++i)
    {
        EffectInstance& instance = state_.pool[i];
        if(!instance.active)
            continue;
        instance.ageSeconds += delta;
        if(instance.controllerComplete ||
           (instance.lifetimeSeconds > 0.0f &&
            instance.ageSeconds >= instance.lifetimeSeconds))
        {
            resetInstance(&state_, &instance);
            continue;
        }
        updateRenderObject(&state_, &instance);
    }
}

void
EffectManager::render() const
{
    if (!state_.initialized || state_.world == NULL)
        return;
    for (size_t i = 0; i < state_.pool.size(); ++i)
    {
        const EffectInstance& instance = state_.pool[i];
        if(!instance.active)
            continue;
        if (instance.renderObject.atomic != NULL && instance.renderObject.atomic->world != state_.world)
            instance.renderObject.atomic->render();
        if (instance.renderObject.clump != NULL && instance.renderObject.clump->world != state_.world)
            instance.renderObject.clump->render();
    }
}

size_t
EffectManager::activeCount() const
{
    size_t count = 0;
    for (size_t i = 0; i < state_.pool.size(); ++i)
        if (state_.pool[i].active)
            ++count;
    return count;
}

size_t
EffectManager::activeEmitterCount() const
{
    size_t count = 0;
    for(size_t i = 0; i < EFFECT_EMITTER_CAPACITY; ++i)
        if (state_.emitters[i].active)
            ++count;
    return count;
}

size_t
EffectManager::poolCapacity() const
{
    return state_.pool.size();
}

bool
EffectManager::getInstance(uint32_t handle, EffectInstanceState* state) const
{
    if(state == NULL)
        return false;
    const EffectInstance* instance = findInstance(&state_, handle);
    if(instance == NULL)
        return false;
    state->active = instance->active;
    state->handle = instance->handle;
    state->recordIndex = instance->recordIndex;
    state->type = state_.asset.records[instance->recordIndex].type;
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
    const EffectEmitter* emitter = findEmitter(&state_, handle);
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
