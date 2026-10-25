#ifndef EFFECT_H
#define EFFECT_H

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace rw
{
struct Atomic;
struct Clump;
struct Frame;
struct TexDictionary;
struct World;
}

enum class EffectRecordType : int32_t
{
    Absent = -1,
    Atomic = 0,
    Clump = 1,
    Custom = 2,
    TextureFallback = 4,
    Unknown = 0x7fffffff
};

struct EffectVector3
{
    float x;
    float y;
    float z;
};

struct EffectMatrix
{
    EffectVector3 right;
    EffectVector3 up;
    EffectVector3 at;
    EffectVector3 pos;
};

EffectMatrix effectIdentityMatrix(void);
EffectMatrix effectYRotationMatrix(float degrees,
                                   const EffectVector3& position);
bool effectMatrixIsFinite(const EffectMatrix& matrix);

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

bool resolveStageEffectResourceId(uint32_t resourceId,
                                  uint32_t* archiveEntryIndex);

bool loadStageEffectResource(const char* stageArchivePath, uint32_t stageId,
                             StageEffectResource* resource);

bool parseEstEffectStream(const uint8_t* data, size_t length,
                          EffectAsset* asset);
bool parseEstEffectFile(const char* path, EffectAsset* asset);

static const uint32_t EFFECT_INVALID_HANDLE = 0xffffffffu;
static const uint8_t EFFECT_RECORD_SENTINEL = 0xffu;
static const size_t EFFECT_ACTIVE_CAPACITY = 64;
static const size_t EFFECT_EMITTER_CAPACITY = 16;

typedef bool EffectMatrixProvider(void* userData, EffectMatrix* matrix);

enum EffectEmitterMode
{
    EFFECT_EMITTER_COPY_MATRIX = 1,
    EFFECT_EMITTER_POSITION_RANDOM_Y = 2
};

struct EffectInstanceState
{
    bool active;
    uint32_t handle;
    uint8_t recordIndex;
    EffectRecordType type;
    float ageSeconds;
    float lifetimeSeconds;
    int16_t tuningA;
    int16_t tuningB;
    EffectMatrix world;
};

struct EffectEmitterState
{
    bool active;
    uint32_t handle;
    uint8_t recordIndex;
    uint8_t mode;
    float ageSeconds;
    float timerSeconds;
    float intervalSeconds;
    float lifetimeSeconds;
    EffectMatrix world;
};

class EffectManager
{
public:
    struct Impl;

    EffectManager();
    ~EffectManager();

    bool initialize(rw::World* world = NULL);
    bool load(const char* archivePath, const char* entryName = "EFF001.EST");
    bool loadStream(const uint8_t* data, size_t length);
    void shutdown();
    bool loadStageResource(const char* stageArchivePath, uint32_t stageId);

    bool isInitialized() const;
    bool isLoaded() const;
    const EffectAsset& asset() const;
    size_t invalidDispatchCount() const;
    bool hasStageResource() const;
    const StageEffectResource& stageResource() const;

    void setPaused(bool paused);
    bool paused() const;
    void setTimeScale(float timeScale);
    float timeScale() const;

    uint32_t dispatchEst(uint8_t recordIndex, const EffectMatrix& world,
                         int16_t tuningA = 0, int16_t tuningB = 0);
    uint32_t startEmitter(uint8_t recordIndex, float intervalSeconds,
                          float lifetimeSeconds, uint8_t mode,
                          EffectMatrixProvider* provider, void* userData);
    void stop(uint32_t handle);
    void complete(uint32_t handle);
    void stopEmitter(uint32_t handle);
    void stopAll();

    void update(float deltaSeconds);
    void render() const;

    size_t activeCount() const;
    size_t activeEmitterCount() const;
    size_t poolCapacity() const;
    bool getInstance(uint32_t handle, EffectInstanceState* state) const;
    bool getEmitter(uint32_t handle, EffectEmitterState* state) const;
    Impl* implementation();
    const Impl* implementation() const;

private:
    Impl* impl_;
};

#endif
