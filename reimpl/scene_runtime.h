#ifndef SCENE_RUNTIME_H
#define SCENE_RUNTIME_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

enum class SceneTrackKind : uint8_t
{
    Camera = 0,
    Light = 1,
    Model = 2,
    Motion = 3,
    Sound = 4,
    Music = 5,
    Player = 6,
    PlayerTask = 7,
    Caption = 8,
    Volume = 9,
    Morph = 10,
    Voice = 11,
    Vibration = 12,
    Effect = 13
};

enum class SceneLanguage : uint8_t
{
    Japanese = 0,
    English = 1,
    French = 2,
    German = 3,
    Spanish = 4,
    Italian = 5
};

enum class SceneStatus : uint8_t
{
    Empty,
    Loaded,
    Playing,
    Completed,
    Skipped,
    Failed
};

struct SceneVec3
{
    float x;
    float y;
    float z;

    SceneVec3()
        : x(0.0f), y(0.0f), z(0.0f)
    {
    }

    SceneVec3(float xValue, float yValue, float zValue)
        : x(xValue), y(yValue), z(zValue)
    {
    }
};

struct SceneMatrix
{
    SceneVec3 position;
    SceneVec3 right;
    SceneVec3 up;
    SceneVec3 at;

    SceneMatrix()
        : position(), right(1.0f, 0.0f, 0.0f),
          up(0.0f, 1.0f, 0.0f), at(0.0f, 0.0f, 1.0f)
    {
    }
};

struct SceneCaption
{
    SceneLanguage language;
    std::string path;

    SceneCaption()
        : language(SceneLanguage::Japanese), path()
    {
    }
};

struct SceneClip
{
    std::string name;
    bool hasTime;
    bool hasDtime;
    std::string resourceName;
    float startTime;
    float endTime;
    float dtimeStart;
    float dtimeEnd;

    bool hasOffsetPosition;
    SceneVec3 offsetPosition;
    bool hasOffsetRotation;
    SceneVec3 offsetRotation;
    uint8_t offsetRotationCount;

    bool hasMatrixStart;
    SceneMatrix matrixStart;
    bool hasMatrixEnd;
    SceneMatrix matrixEnd;
    bool hasViewAngle;
    float viewAngleStart;
    float viewAngleEnd;
    bool hasClipPlanes;
    float nearClip;
    float farClip;
    bool hasModelCheck;
    int32_t modelCheck;
    bool hasTarget;
    int32_t targetId;
    std::string targetName;
    bool hasCurve;
    int32_t curve;
    bool hasAcceleration;
    int32_t acceleration;
    bool hasDeceleration;
    int32_t deceleration;

    bool hasColor;
    SceneVec3 color;
    bool hasPosition;
    SceneVec3 position;
    bool hasRotation;
    float rotationStart;
    float rotationEnd;
    bool hasRadius;
    float radius;
    bool hasAngle;
    float angle;

    bool hasSound;
    std::array<int32_t, 4> sound;
    bool hasMusic;
    std::array<int32_t, 3> music;
    bool hasSwitch;
    std::array<int32_t, 2> switchValues;
    bool hasVoice;
    std::array<int32_t, 2> voice;
    bool hasLip;
    int32_t lip;
    float lipDuration;
    bool hasInterpolate;
    int32_t interpolate;
    bool hasNoStageCheckSwitch;
    int32_t noStageCheckSwitch;
    bool hasOffsetSwitch;
    int32_t offsetSwitch;
    bool hasCaption;
    int32_t caption;
    bool hasVolume;
    int32_t volumeStart;
    int32_t volumeEnd;
    bool hasMorph;
    std::array<int32_t, 3> morph;
    bool hasVibrationDirection;
    int32_t vibrationDirection;
    bool hasVibrationAmplify;
    float vibrationAmplify;
    bool hasVibrationFrequency;
    float vibrationFrequency;
    bool hasVibrationAcceleration;
    int32_t vibrationAcceleration;
    bool hasVibrationDeceleration;
    int32_t vibrationDeceleration;
    bool hasBlur;
    int32_t blur;
    bool hasTimeScale;
    float timeScale;

    std::vector<struct SceneTrack> children;

    SceneClip();
};

struct SceneTrack
{
    SceneTrackKind kind;
    bool enabled;
    std::string name;
    std::vector<SceneClip> clips;
    std::vector<SceneTrack> children;

    SceneTrack()
        : kind(SceneTrackKind::Camera), enabled(true), name(), clips(), children()
    {
    }
};

struct SceneDefinition
{
    std::string console;
    std::string version;
    int32_t stageId;
    std::string stageName;
    bool hasStageLight;
    bool stageLight;
    int32_t motionId;
    std::string motionResource;
    std::string soundbank;
    std::string fadeText;
    std::vector<SceneCaption> captions;
    std::vector<SceneTrack> tracks;
    float duration;

    SceneDefinition()
        : console(), version(), stageId(-1), stageName(),
          hasStageLight(false), stageLight(true), motionId(-1),
          motionResource(), soundbank(), fadeText(), captions(), tracks(),
          duration(0.0f)
    {
    }
};

enum class SceneModelTargetKind : uint8_t
{
    None,
    ActorBone,
    ModelFrame
};

struct SceneModelTarget
{
    SceneModelTargetKind kind;
    const SceneTrack* track;
    const SceneClip* clip;
    const std::string* actorTrackName;

    int32_t matrixIndex;

    SceneModelTarget()
        : kind(SceneModelTargetKind::None), track(nullptr), clip(nullptr),
          actorTrackName(nullptr), matrixIndex(-1)
    {
    }
};

bool resolveSceneModelTarget(const SceneDefinition& scene,
                             const SceneClip& modelClip,
                             SceneModelTarget& target);

SceneMatrix composeSceneModelBoneTransform(const SceneMatrix& boneMatrix,
                                           const SceneVec3& offset,
                                           const SceneVec3& rotationDegrees);

struct ScenePtaskRecord
{
    uint16_t moveId;
    std::string authoringPath;
    std::vector<int16_t> commandWords;
    std::vector<int16_t> behaviourWords;
    std::vector<uint8_t> attributeBytes;
    std::vector<uint8_t> moveBehaviourBytes;
};

struct ScenePtaskPool
{
    std::vector<ScenePtaskRecord> records;
};

struct ScenePtaskMotion
{
    int16_t motionId;
    int16_t blendFrames;
    int16_t maxDurationFrames;
    int16_t transitionFrames;
};

struct ScenePtaskMotionProgram
{
    std::vector<ScenePtaskMotion> motions;

    int loopStart;

    ScenePtaskMotionProgram()
        : motions(), loopStart(-1)
    {
    }
};

bool loadScenePtaskPool(const char* archivePath, const char* sceneEntry,
                        ScenePtaskPool& pool);
const ScenePtaskRecord* findScenePtaskRecord(const ScenePtaskPool& pool,
                                             int moveId);

bool buildScenePtaskMotionProgram(const ScenePtaskPool& pool, int moveId,
                                  ScenePtaskMotionProgram& program);

struct SceneCameraState
{
    SceneMatrix matrix;
    SceneVec3 eye;
    SceneVec3 target;
    float fieldOfView;
    float nearClip;
    float farClip;
    int32_t targetId;
    const char* targetName;
    const char* resourceName;

    SceneCameraState()
        : matrix(), eye(), target(), fieldOfView(60.0f), nearClip(0.0f),
          farClip(0.0f), targetId(0), targetName(NULL), resourceName(NULL)
    {
    }
};

struct SceneActorState
{
    SceneTrackKind kind;
    const char* trackName;
    const char* resourceName;
    const char* motionName;
    SceneVec3 position;
    SceneVec3 rotation;
    int32_t targetId;
    const char* targetName;
    bool visible;

    bool noStageCheck;

    bool applyPlacement;
    int32_t morphAttachment;
    int32_t morphStartTarget;
    int32_t morphEndTarget;
    float morphAmount;
    float clipElapsed;
    float clipDuration;

    float sampleTime;

    SceneActorState()
        : kind(SceneTrackKind::Model), trackName(NULL), resourceName(NULL),
          motionName(NULL), position(), rotation(), targetId(0),
          targetName(NULL), visible(true), noStageCheck(false),
          applyPlacement(true), morphAttachment(0), morphStartTarget(0),
          morphEndTarget(0), morphAmount(0.0f), clipElapsed(0.0f),
          clipDuration(0.0f), sampleTime(0.0f)
    {
    }
};

struct SceneActorPlacementState
{
    SceneVec3 logicalPosition;
    float heading;
    float renderedRootY;
    bool placed;
    bool noStageCheck;

    SceneActorPlacementState()
        : logicalPosition(), heading(0.0f), renderedRootY(0.0f),
          placed(false), noStageCheck(false)
    {
    }
};
struct SceneActorRootMotionState
{
    SceneVec3 logicalPosition;
    SceneVec3 startLogicalPosition;
    SceneVec3 startRootWorldPosition;
    SceneVec3 terminalRootWorldPosition;
    bool active;
    bool committed;

    SceneActorRootMotionState()
        : logicalPosition(), startLogicalPosition(),
          startRootWorldPosition(), terminalRootWorldPosition(),
          active(false), committed(false)
    {
    }
};

struct SceneMotionPlaybackState
{
    int index = 0;
    float startSeconds = 0.0f;
    float clipSeconds = -1.0f;
    uint8_t sequenceGeneration = 0;
};

bool advanceSceneMotionPlayback(SceneMotionPlaybackState& state,
                                 float elapsedSeconds, int motionCount,
                                 int loopStart);

void beginSceneActorRootMotion(SceneActorRootMotionState& retained,
                               const SceneVec3& logicalPosition,
                               const SceneVec3& rootWorldPosition);
bool commitSceneActorRootMotion(SceneActorRootMotionState& retained,
                                const SceneVec3& rootWorldPosition);

using SceneGroundHeightCallback = bool (*)(void* userData,
                                           float x,
                                           float z,
                                           float ceiling,
                                           float* height);

bool retainSceneActorPlacement(SceneActorPlacementState& retained,
                               const SceneActorState& state);

bool resolveSceneActorRootY(SceneActorPlacementState& retained,
                            float currentRootY,
                            float scaledDeltaSeconds,
                            SceneGroundHeightCallback groundHeight,
                            void* userData,
                            float* rootY);

struct SceneLightState
{
    const char* trackName;
    const char* resourceName;
    SceneVec3 color;
    SceneVec3 position;
    float rotationStart;
    float rotationEnd;
    float radius;
    float angle;
    bool visible;

    int32_t index;

    SceneLightState()
        : trackName(NULL), resourceName(NULL), color(1.0f, 1.0f, 1.0f),
          position(), rotationStart(0.0f), rotationEnd(0.0f), radius(0.0f),
          angle(0.0f), visible(true), index(-1)
    {
    }
};

enum class SceneAudioKind : uint8_t
{
    Sound,
    Music,
    Voice,
    Vibration
};

struct SceneAudioEvent
{
    SceneAudioKind kind;
    bool active;
    const char* trackName;
    const char* resourceName;
    std::array<int32_t, 4> values;
    float volume;
    bool continuous;
    bool volumeControl;
    int32_t lip;
    float lipRate;
    float clipElapsed;
    int32_t vibrationDirection;
    float vibrationAmplify;
    float vibrationFrequency;
    int32_t vibrationAcceleration;
    int32_t vibrationDeceleration;

    SceneAudioEvent()
        : kind(SceneAudioKind::Sound), active(false), trackName(NULL),
          resourceName(NULL), values{0, 0, 0, 0}, volume(1.0f),
          continuous(false), volumeControl(false), lip(0), lipRate(0.0f),
          clipElapsed(0.0f), vibrationDirection(0),
          vibrationAmplify(0.0f), vibrationFrequency(0.0f),
          vibrationAcceleration(0), vibrationDeceleration(0)
    {
    }
};

struct SceneCaptionEvent
{
    bool active;
    int32_t caption;
    SceneLanguage language;
    const char* path;
    const char* trackName;

    SceneCaptionEvent()
        : active(false), caption(0), language(SceneLanguage::Japanese),
          path(NULL), trackName(NULL)
    {
    }
};

struct SceneEffectState
{
    bool active;
    const char* trackName;
    int32_t blur;
    float timeScale;
    float volumeStart;
    float volumeEnd;

    SceneEffectState()
        : active(false), trackName(NULL), blur(0), timeScale(1.0f),
          volumeStart(1.0f), volumeEnd(1.0f)
    {
    }
};

using SceneResourceHandle = uint32_t;
using SceneAcquireResourceCallback = bool (*)(void* userData,
                                               SceneTrackKind kind,
                                               const char* resourceName,
                                               SceneResourceHandle* handle);
using SceneReleaseResourceCallback = void (*)(void* userData,
                                               SceneTrackKind kind,
                                               const char* resourceName,
                                               SceneResourceHandle handle);
using SceneApplyCameraCallback = void (*)(void* userData,
                                           const SceneCameraState& state);
using SceneApplyActorCallback = void (*)(void* userData,
                                          const SceneActorState& state);
using SceneApplyLightCallback = void (*)(void* userData,
                                          const SceneLightState& state);
using SceneApplyMotionCallback = void (*)(void* userData,
                                           const SceneActorState& state);
using SceneAudioCallback = void (*)(void* userData,
                                     const SceneAudioEvent& event);
using SceneCaptionCallback = void (*)(void* userData,
                                       const SceneCaptionEvent& event);
using SceneApplyEffectCallback = void (*)(void* userData,
                                           const SceneEffectState& state);
using SceneRestoreCallback = void (*)(void* userData,
                                       bool skipped,
                                       bool failed);

struct ScenePresentationBindings
{
    void* userData;
    SceneLanguage language;
    SceneAcquireResourceCallback acquireResource;
    SceneReleaseResourceCallback releaseResource;
    SceneApplyCameraCallback applyCamera;
    SceneApplyActorCallback applyActor;
    SceneApplyLightCallback applyLight;
    SceneApplyMotionCallback applyMotion;
    SceneAudioCallback audio;
    SceneCaptionCallback caption;
    SceneApplyEffectCallback effect;
    SceneRestoreCallback restore;

    ScenePresentationBindings()
        : userData(NULL), language(SceneLanguage::Japanese),
          acquireResource(NULL), releaseResource(NULL), applyCamera(NULL),
          applyActor(NULL), applyLight(NULL), applyMotion(NULL), audio(NULL),
          caption(NULL), effect(NULL), restore(NULL)
    {
    }
};

struct SceneUpdateResult
{
    SceneStatus status;
    float time;
    bool advanced;
    bool skipped;
    bool finished;

    SceneUpdateResult()
        : status(SceneStatus::Empty), time(0.0f), advanced(false),
          skipped(false), finished(false)
    {
    }
};

bool parseScene(const uint8_t* data,
                std::size_t size,
                SceneDefinition& scene,
                std::string* error = NULL);
bool parseScene(const std::vector<uint8_t>& data,
                SceneDefinition& scene,
                std::string* error = NULL);
bool loadSceneFromXaf(const char* archivePath,
                      const char* entryName,
                      SceneDefinition& scene,
                      std::string* error = NULL);
bool loadSceneFromXaf(const std::string& archivePath,
                      const std::string& entryName,
                      SceneDefinition& scene,
                      std::string* error = NULL);

class SceneRuntime
{
public:
    SceneRuntime();
    ~SceneRuntime();

    SceneRuntime(const SceneRuntime&) = delete;
    SceneRuntime& operator=(const SceneRuntime&) = delete;

    bool load(const SceneDefinition& scene,
              const ScenePresentationBindings& bindings,
              std::string* error = NULL);
    bool load(const uint8_t* data,
              std::size_t size,
              const ScenePresentationBindings& bindings,
              std::string* error = NULL);
    bool load(const std::vector<uint8_t>& data,
              const ScenePresentationBindings& bindings,
              std::string* error = NULL);
    bool loadFromXaf(const char* archivePath,
                     const char* entryName,
                     const ScenePresentationBindings& bindings,
                     std::string* error = NULL);

    bool start();
    SceneUpdateResult update(float scaledDeltaSeconds,
                             bool skipRequested = false);
    void requestSkip();
    void stop();

    SceneStatus status() const;
    float time() const;
    float duration() const;
    const SceneDefinition* definition() const;

private:
    struct RuntimeClip;
    struct RuntimeClipState;
    struct ResourceState;

    SceneDefinition scene_;
    ScenePresentationBindings bindings_;
    std::vector<RuntimeClip> clips_;
    std::vector<RuntimeClipState> clipStates_;
    std::vector<ResourceState> resources_;
    SceneStatus status_;
    float time_;
    bool skipRequested_;
    bool restored_;

    bool acquireResources(std::string* error);
    void releaseResources();
    int32_t lightTrackIndex(const SceneTrack* track) const;
    void flattenTracks(const std::vector<SceneTrack>& tracks,
                       const SceneTrack* parent,
                       const std::string* inheritedName);
    void startClip(std::size_t index);
    void stopClip(std::size_t index);
    void applyAtTime(float time);
    void applyClip(std::size_t index, float time);
    void restore(bool skipped, bool failed);
};

#endif
