#ifndef CAMERA_H
#define CAMERA_H

#include <stdint.h>
#include <stddef.h>

const float COMBAT_FRAMING_MIN_DISTANCE = 12.5f;
const float COMBAT_FRAMING_MAX_DISTANCE = 90.0f;
const float COMBAT_FRAMING_TARGET_HEIGHT = 10.0f;
const float COMBAT_FRAMING_EYE_BASE_HEIGHT = 2.0f;
const float COMBAT_FRAMING_EYE_HEIGHT_RATE = 0.1f;

struct CombatFraming
{
    float targetX;
    float targetY;
    float targetZ;
    float eyeX;
    float eyeY;
    float eyeZ;

    float distance;
};
struct BattleCameraSubject
{
    float x;
    float y;
    float z;
};

bool updateBattleCameraSubject(
    BattleCameraSubject& tracked,
    float actualX, float actualY, float actualZ);

CombatFraming computeCombatFraming(float ax, float az, float bx, float bz,
                                   float fallbackDirectionX,
                                   float fallbackDirectionZ,
                                   float fieldOfViewDegrees);

const float BATTLE_CAMERA_FRAMING_ANGLE_DEGREES = 19.5f;
const float BATTLE_CAMERA_SUBJECT_CLEARANCE = 5.0f;
const float BATTLE_CAMERA_MIN_DISTANCE = 15.0f;
const float BATTLE_CAMERA_MAX_DISTANCE = 75.0f;
const float BATTLE_CAMERA_EYE_HEIGHT = 1.5f;

CombatFraming computeBattleCameraFraming(
    float ax, float ay, float az, float bx, float by, float bz,
    float previousEyeX, float previousEyeZ, float maxDistance);

struct CameraFramingVector
{
    float x;
    float y;
    float z;
};

struct CamdatFramingBasis
{
    CameraFramingVector target;
    CameraFramingVector right;
    CameraFramingVector up;
    CameraFramingVector forward;
};

struct CamdatFramingParameters
{

    float distance;

    float pitchDegrees;
    float yawDegrees;
};

struct CamdatFraming
{
    CameraFramingVector target;
    CameraFramingVector eye;
};

CamdatFramingBasis computeCamdatPairFramingBasis(
    const CameraFramingVector& first,
    const CameraFramingVector& second,
    const CameraFramingVector& offset);

CamdatFraming computeCamdatOpcode1And4Framing(
    const CamdatFramingBasis& basis,
    const CamdatFramingParameters& parameters);

CamdatFraming computeCamdatOpcode2Framing(
    const CamdatFramingBasis& boneBasis,
    const CamdatFramingParameters& start,
    const CamdatFramingParameters& end,
    float elapsedFraction);

const float COMBAT_CAMERA_FIELD_0X439C_MINIMUM = 2.0f;
const float COMBAT_CAMERA_SETTLE_FRAME_RATE = 60.0f;
const float COMBAT_CAMERA_DEFAULT_BLEND_FRAMES = 15.0f;
const float COMBAT_CAMERA_DAMAGE_THRESHOLD = 14.0f;
const float COMBAT_CAMERA_DAMAGE_SETTLE_FRAMES = 5.0f;
const float COMBAT_CAMERA_DAMAGE_FOLLOWUP_FRAMES = 15.0f;
const float COMBAT_CAMERA_DAMAGE_EYE_SCALE = 0.7f;
const float COMBAT_CAMERA_MOVE_SETTLE_FRAMES = 1.5f;
const float COMBAT_CAMERA_MOVE_FOLLOWUP_FRAMES = 2.0f;
const float COMBAT_CAMERA_MOVE_EYE_SCALE = 0.6f;
const float COMBAT_CAMERA_MIN_ARM_DISTANCE = 42.0f;

struct CombatCameraState
{
    CombatFraming framing;
    float settleRemainingSeconds;
    float blendFrames;
    float initialBlendFrames;
    float followupBlendFrames;
    float specialRemainingSeconds;
    bool specialDistanceScale;
};

void resetCombatCameraState(CombatCameraState& state,
                            const CombatFraming& currentFraming);

float combatCameraBlendFraction(
    const CombatCameraState& state,
    float scaledFrameDeltaSeconds,
    float nominalFrameRateHz);

float computeCombatCameraField0x439c(float slotX, float slotZ,
                                    float linkedModelX, float linkedModelZ);

bool armCombatDamageCameraSettle(
    CombatCameraState& state, float damage,
    const CameraFramingVector& cameraForward,
    float currentCameraDistance);

void armCombatMoveCameraSettle(
    CombatCameraState& state, float closeUpDurationFrames,
    const CameraFramingVector& cameraForward,
    float currentCameraDistance);

bool updateCombatCameraState(
    CombatCameraState& state,
    float ax, float az, float bx, float bz,
    float fallbackDirectionX, float fallbackDirectionZ,
    float fieldOfViewDegrees,
    float field0x439c,
    float scaledFrameDeltaSeconds);

struct PlayerFollowFraming
{
    float centerX;
    float centerY;
    float centerZ;
    float targetX;
    float targetY;
    float targetZ;
    float eyeX;
    float eyeY;
    float eyeZ;
};

struct FieldCameraRegionRecord
{
    uint16_t behavior;
    uint16_t smoothingRow;
    float eyeX;
    float eyeY;
    float eyeZ;
    float minX;
    float maxX;
    float minZ;
    float maxZ;
    float maxY;
};

struct FieldCameraRegionState
{
    uint16_t stageId;
    int16_t recordIndex;
    const FieldCameraRegionRecord* table;
    uint16_t tableRecordCount;
    bool active;
    bool preserveEyeY;
    uint16_t smoothingRow;
    float targetDeadband;
    float eyeDeadband;
    float smoothingAlpha;
};

struct FieldCameraRegionInputs
{
    uint16_t stageId;
    CameraFramingVector player;
    CameraFramingVector currentEye;

    uint8_t groundSurface;
    int storyPhase;
    bool eventFlag031e;

    uint32_t sceneControlFlags;
};

bool resolveFieldCameraRegion(const FieldCameraRegionInputs& inputs,
                              FieldCameraRegionState& state);

const FieldCameraRegionRecord* fieldCameraRegionTable(
    uint16_t stageId, float playerZ, int storyPhase, bool eventFlag031e,
    uint16_t* recordCount);

PlayerFollowFraming computePlayerFollowFraming(
    float x, float y, float z, float headingDegrees, float distance,
    float targetX, float targetY, float targetZ);

const float MANUAL_CAMERA_DEFAULT_POLAR_DEGREES = 166.61f;
const float MANUAL_CAMERA_MIN_POLAR_DEGREES = 155.0f;
const float MANUAL_CAMERA_MAX_POLAR_DEGREES = 175.0f;
const float MANUAL_CAMERA_YAW_PER_FRAME = 2.0f;
const float MANUAL_CAMERA_POLAR_PER_FRAME = 1.4f;

struct ManualCameraControl
{

    float yawOffsetDegrees;
    float polarDegrees;
    bool verticalInputIdle;
    bool enabled;
};

void resetManualCameraControl(ManualCameraControl& control);

void updateManualCameraControl(ManualCameraControl& control, bool enabled,
                               float horizontalAxis, float verticalAxis,
                               float frameStep);

PlayerFollowFraming applyManualCameraControl(
    const ManualCameraControl& control,
    const PlayerFollowFraming& automaticFraming);

struct RwWorldGeometry;

struct CameraCollisionVector
{
    float x;
    float y;
    float z;
};

struct GameplayCameraCollisionState
{
    CameraCollisionVector lastClearEye;
    CameraCollisionVector trackedAnchor;
    uint8_t obstructionMode;
    uint8_t clearFramesRemaining;
    bool initialized;
};

const float GAMEPLAY_CAMERA_COLLISION_RADIUS = 7.5f;

void resetGameplayCameraCollision(GameplayCameraCollisionState& state);

bool resolveGameplayCameraCollision(
    GameplayCameraCollisionState& state,
    const RwWorldGeometry* const* geometry, size_t geometryCount,
    const CameraCollisionVector& trackedPosition,
    float trackedHeadingDegrees,
    const CameraCollisionVector& target,
    CameraCollisionVector& eye);

struct CameraPathSegment
{
    float eyeStart[3];
    float eyeEnd[3];
    float eyeDuration;
    uint16_t eyeStop;
    float targetStart[3];
    float targetEnd[3];
    float targetDuration;
    uint16_t targetStop;
};

struct CameraPathPlayer
{
    CameraPathSegment segment;
    float eyeT;
    float targetT;
    bool finished;
};

void cameraPathStart(CameraPathPlayer& player, const CameraPathSegment& segment);

bool cameraPathSample(CameraPathPlayer& player, float frameStep,
                      float* eyeX, float* eyeY, float* eyeZ,
                      float* targetX, float* targetY, float* targetZ);

extern const CameraPathSegment TITLE_CAMERA_PATH;

#endif
