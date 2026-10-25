#ifndef EFFECT_H
#define EFFECT_H

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "formats/EST/est.h"
#include "formats/STAGE_EFFECT_RESOURCE/stage_effect_resource.h"

namespace rw
{
struct Atomic;
struct Clump;
struct Frame;
struct TexDictionary;
struct World;
}

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

namespace effect_detail
{

struct EffectRenderObject
{
    rw::Atomic* atomic;
    rw::Clump* clump;

    rw::Frame* sourceFrame;
    rw::Frame* objectFrame;

    EffectRenderObject() : atomic(NULL), clump(NULL), sourceFrame(NULL), objectFrame(NULL)
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
        : active(false), controllerComplete(false), handle(EFFECT_INVALID_HANDLE), recordIndex(EFFECT_RECORD_SENTINEL),
          ageSeconds(0.0f), lifetimeSeconds(0.8f), tuningA(0), tuningB(0), world(effectIdentityMatrix()), renderObject()
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
        : active(false), handle(EFFECT_INVALID_HANDLE), recordIndex(EFFECT_RECORD_SENTINEL), mode(0), ageSeconds(0.0f),
          timerSeconds(0.0f), intervalSeconds(0.0f), lifetimeSeconds(0.0f), provider(NULL), userData(NULL),
          world(effectIdentityMatrix())
    {
    }
};

struct EffectManagerState
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

    EffectManagerState()
        : world(NULL), textureDictionary(NULL), initialized(false), loaded(false), paused(false), timeScale(1.0f),
          nextHandle(1), randomState(0x13579bdfu), invalidDispatches(0), emitterCursor(0), asset(), stageResource(),
          pool(EFFECT_ACTIVE_CAPACITY)
    {
    }
};

}

class EffectManager
{
  public:
    EffectManager();
    ~EffectManager();
    EffectManager(const EffectManager&) = delete;
    EffectManager& operator=(const EffectManager&) = delete;

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

  private:
    effect_detail::EffectManagerState state_;
};

#endif
