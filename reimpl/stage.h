#ifndef FSTAGE_H
#define FSTAGE_H
#include "rwworld.h"

#include <rw.h>
#include <stddef.h>
#include <stdint.h>
#include <vector>

struct WeaponDefinition;

struct FstagePlacement
{
    int32_t objectIndex;
    rw::V3d translation;
    float quaternion[4];
    rw::V3d minBounds;
    rw::V3d maxBounds;
    bool visible;
    bool registeredCollider;
    rw::Clump* clump;

    FstagePlacement();
};

struct FstageObjectBounds
{
    uint32_t objectIndex;
    rw::V3d minBounds;
    rw::V3d maxBounds;
    float radius;

    FstageObjectBounds();
};

const size_t FSTAGE_NPC_ROW_COUNT = 7;
const size_t FSTAGE_NPC_LANE_COUNT = 32;

struct FstageNpcPositionRow
{
    int16_t values[FSTAGE_NPC_LANE_COUNT];

    FstageNpcPositionRow();
    int16_t kind() const;
    int16_t selector() const;
    int16_t interactionSelector() const;
};

struct FstageSpawnTransform
{
    rw::V3d position;
    float headingDegrees;

    FstageSpawnTransform();
};

class FstageNpcPositionData
{
public:
    FstageNpcPositionData();

    bool load(const char* path);
    bool parse(const uint8_t* bytes, size_t length);
    bool selectRows(uint32_t tableIndex, uint32_t stageIndex,
                    std::vector<FstageNpcPositionRow>& rows) const;
    const std::vector<uint8_t>& bytes() const;

private:
    std::vector<uint8_t> source;
};

typedef bool (*FstageEventFlagQuery)(uint16_t eventFlag, void* userData);
typedef int16_t (*FstageRecruitmentStateQuery)(
    uint16_t phase, uint16_t characterId, void* userData);
typedef bool (*FstageNpcBaseSelectorQuery)(int16_t selector, void* userData);

enum FstageNpcOverlayDecision
{
    FSTAGE_NPC_OVERLAY_KEEP = 0,
    FSTAGE_NPC_OVERLAY_COPY = 1,
    FSTAGE_NPC_OVERLAY_CLEAR = 2
};

typedef FstageNpcOverlayDecision (*FstageNpcOverlayQuery)(
    const FstageNpcPositionRow& row, uint32_t rowIndex, bool battleLayer,
    int16_t* resolvedSelector, void* userData);

struct FstageNpcSelectionState
{
    uint16_t stageId;
    uint16_t storyPhase;
    int16_t battleRoot;
    int16_t partyRoots[2];
    int16_t alternateOverlayMode;
    int16_t recruitmentMode;
    int16_t actorResourceProfile;
    int16_t storyTransitionMode;
    uint32_t phase11ModeFlags;
    bool gameStateBit1;
    bool suppressPostBattleOverlays;
    bool suppressPartyOverlay;
    bool trackedOpponent;
    bool resetRecruitmentRoot;
    bool encounterSubstate;
    bool phase10Stage31Suppress;
    bool phase11Stage49Override;
    void* userData;
    FstageEventFlagQuery eventFlag;
    FstageRecruitmentStateQuery recruitmentState;
    FstageNpcBaseSelectorQuery baseSelectorAvailable;
    FstageNpcOverlayQuery overlayDecision;

    FstageNpcSelectionState();
};

const uint32_t FSTAGE_NPC_PHASE_COUNT = 12;
uint32_t fstageNpcRootForPhase(uint16_t phase);
uint32_t fstageNpcRootForSelection(const FstageNpcSelectionState& state);
bool selectFstageNpcPositionRecords(
    const FstageNpcPositionData& positions,
    const FstageNpcSelectionState& state,
    std::vector<FstageNpcPositionRow>& rows);

bool getFstagePlayerSpawn(const FstageNpcPositionData& positions,
                          uint32_t stageIndex,
                          FstageSpawnTransform* transform);
bool getFstagePlayerSpawn(const std::vector<FstageNpcPositionRow>& rows,
                          FstageSpawnTransform* transform);
bool getFstageSpawnTransform(const FstageNpcPositionRow& row,
                             FstageSpawnTransform* transform);
bool isFstageObjectSuppressed(uint16_t stageId, uint32_t objectIndex);

struct FstageNpcActorBinding
{
    int16_t actorSlot;
    int16_t npcPositionRowIndex;
    uint16_t characterId;
    rw::V3d position;
    bool active;

    FstageNpcActorBinding();
};

struct FstageNpcInteractionCandidate
{
    int16_t kind;
    int16_t rowIndex;
    int16_t actorSlot;
    int16_t interactionSelector;
    uint16_t characterId;
    rw::V3d position;
    float radius;
    float distanceSquared;

    FstageNpcInteractionCandidate();
};

float fstageCharacterInteractionRadius(uint16_t characterId);
int collectFstageNpcInteractionCandidates(
    const std::vector<FstageNpcPositionRow>& rows,
    const FstageNpcActorBinding* actors, int actorCount,
    const rw::V3d& playerPosition,
    FstageNpcInteractionCandidate* candidates, int capacity);

typedef rw::Clump* (*FstageObjectModelResolver)(
    int8_t kind, int8_t selector, void* userData);
typedef void (*FstageEventFlagSetter)(
    uint16_t eventFlag, void* userData);
typedef void (*FstageObjectSoundCallback)(
    uint8_t bankSlot, uint8_t cue, uint8_t volume, void* userData);
typedef void (*FstageObjectEffectCallback)(
    uint8_t recordIndex, const rw::Matrix& world,
    int16_t tuningA, int16_t tuningB, void* userData);
typedef void (*FstageWeaponActionCallback)(
    uint8_t group, uint8_t action, void* userData);

struct FstageObjectRuntimeHooks
{
    void* userData;
    FstageEventFlagSetter eventSet;
    FstageObjectSoundCallback sound;
    FstageObjectEffectCallback effect;
    FstageWeaponActionCallback weaponAction;

    FstageObjectRuntimeHooks();
};

struct FstageObjectSpawnState
{
    uint16_t storyPhase;
    uint8_t enabledKindMask;
    bool gameStateBit1;
    void* userData;
    FstageEventFlagQuery eventFlag;
    FstageObjectModelResolver modelResolver;
    FstageObjectRuntimeHooks runtime;

    FstageObjectSpawnState();
};

enum FstageObjectPhase : uint8_t
{
    FSTAGE_OBJECT_INERT = 0,
    FSTAGE_OBJECT_DESTRUCTIBLE = 1,
    FSTAGE_OBJECT_BREAK_ANIMATION = 2,
    FSTAGE_OBJECT_FADE = 3
};

struct FstageObjectInstance
{
    rw::Clump* clump;
    rw::HAnimHierarchy* hierarchy;
    RwWorldGeometry colliderGeometry;
    int16_t pool;
    int16_t eventFlag;
    int16_t health;
    int16_t acceptedHitCount;
    int16_t fadeCounter;
    uint16_t stageId;
    uint16_t recordIndex;
    uint16_t state;
    uint16_t definitionSubtype;
    uint32_t renderControl;
    uint32_t runtimeFlags;
    int8_t kind;
    int8_t sub;
    int8_t gateValue;
    int8_t gateAuxiliary;
    uint8_t alpha;
    FstageObjectPhase phase;
    bool registeredCollider;
    bool animationInitialized;

    FstageObjectInstance();
};

struct FstageObjectContact
{
    int16_t damage;
    rw::V3d center;
    float radius;
    rw::Matrix effectTransform;

    FstageObjectContact();
};

struct FstageWorldItem
{
    uint16_t recordIndex;
    int16_t pool;
    int8_t kind;
    int8_t selector;
    rw::V3d position;
};

struct FstageObjectInstallResult
{
    uint32_t authored;
    uint32_t eligible;
    uint32_t instantiated;
    uint32_t missingModel;

    FstageObjectInstallResult();
};

const size_t FSTAGE_WEAPON_CAPACITY = 48;
const int16_t FSTAGE_WEAPON_INVALID_ID = -1;
const int16_t FSTAGE_WEAPON_PICKUP_ACTION = 0x154;
const int16_t FSTAGE_WEAPON_HUD_PROMPT = 0x0d;

enum FstageWeaponManagerFlags : uint16_t
{
    FSTAGE_WEAPON_MANAGER_ACTIVE = 0x01,
    FSTAGE_WEAPON_MANAGER_OWNED = 0x02,
    FSTAGE_WEAPON_MANAGER_DRAW = 0x04,

    FSTAGE_WEAPON_MANAGER_RELEASE = 0x02,
    FSTAGE_WEAPON_MANAGER_UPDATE = 0x04,
    FSTAGE_WEAPON_MANAGER_SUPPRESS_ONCE = 0x08
};

enum FstageWeaponFlags : uint8_t
{
    FSTAGE_WEAPON_ATTACHED = 0x01,
    FSTAGE_WEAPON_NORMAL_TOSS = 0x02,
    FSTAGE_WEAPON_POST_BREAK = 0x04,
    FSTAGE_WEAPON_TARGET_THROW = 0x08,
    FSTAGE_WEAPON_HELD = 0x10,
    FSTAGE_WEAPON_RETURNED = 0x80
};

enum FstageWeaponMotionState : uint8_t
{
    FSTAGE_WEAPON_MOTION_IDLE = 1,
    FSTAGE_WEAPON_MOTION_TOSS = 10,
    FSTAGE_WEAPON_MOTION_BREAK_DROP = 20,
    FSTAGE_WEAPON_MOTION_THROW = 30,
    FSTAGE_WEAPON_MOTION_THROW_SETTLE = 31
};

struct FstageWeaponManagerRecord
{
    uint16_t selector;
    uint16_t flags;
    int16_t managerId;
    rw::Clump* clump;
    int16_t linkedManagerId;

    FstageWeaponManagerRecord();
};

struct FstageWeaponInstance
{
    uint16_t stageId;
    uint16_t recordIndex;
    uint16_t selector;
    int16_t primaryManagerId;
    int16_t managerId;
    int8_t ownerSlot;
    int8_t attachmentSlot;
    int8_t originalOwnerSlot;
    int8_t originalMoveSlot;
    uint8_t flags;
    FstageWeaponMotionState motionState;
    float groundY;
    rw::V3d velocity;
    rw::V3d angularVelocity;
    rw::Matrix previousWorld;
    bool active;
    bool broken;

    FstageWeaponInstance();
};

struct FstageWeaponActorState
{
    uint8_t slotIndex;
    uint8_t battleSide;
    uint32_t actionFlags;
    int16_t heldWeapon;
    int16_t targetedWeapon;
    int16_t linkedHeldWeapon;
    rw::V3d position;
    rw::V3d forward;
    rw::Frame* attachmentBones[2];

    FstageWeaponActorState();
};

struct FstageWeaponTossContact
{
    float fraction;
    rw::V3d normal;
};

struct FstageWeaponManager
{
    FstageWeaponManagerRecord records[FSTAGE_WEAPON_CAPACITY];
    FstageWeaponInstance instances[FSTAGE_WEAPON_CAPACITY];
    uint16_t managerCount;
    uint16_t weaponCount;

    FstageWeaponManager();
};

const size_t FSTAGE_INLINE_COLLIDER_CAPACITY = 9;
const size_t FSTAGE_CAMERA_COLLISION_SOURCE_CAPACITY =
    1 + FSTAGE_INLINE_COLLIDER_CAPACITY;
const uint8_t FSTAGE_CAMERA_COLLISION_MASK = 0x05;

struct FstageInteractionMarker
{
    rw::Atomic* broadphaseAtomic;
    rw::Atomic* collisionAtomic;
    RwWorldGeometry broadphaseGeometry;
    RwWorldGeometry collisionGeometry;
    int32_t consumedTextId;
    int16_t textId;
    int16_t markerKey;
    int16_t commandParameter;
    int16_t side;
    int16_t contextValue;

    FstageInteractionMarker();
};

enum FstageMarkerResolution
{
    FSTAGE_MARKER_NONE = 0,
    FSTAGE_MARKER_PROMPT = 1,
    FSTAGE_MARKER_CONSUMED_TEXT = 2,
    FSTAGE_MARKER_TRANSITION = 3
};

enum FstageMarkerButton
{

    FSTAGE_MARKER_BUTTON_NONE = 0
};

struct FstageMarkerQueryState
{
    uint16_t stageId;
    uint64_t enabledTextMask;
    void* userData;
    FstageEventFlagQuery eventFlag;

    FstageMarkerQueryState();
};

struct FstageMarkerPrompt
{
    FstageMarkerResolution resolution;
    FstageMarkerButton button;
    int16_t textId;
    int16_t commandParameter;
    int16_t side;
    int16_t contextValue;
    int32_t displayTextId;
    float screenX;
    float screenY;

    FstageMarkerPrompt();
};

struct FstageScene
{
    std::vector<uint8_t> data;
    rw::TexDictionary* textureDictionary;
    rw::Clump* clump;
    rw::Clump* worldClump;
    rw::Clump* sectionClumps[4];

    RwWorldGeometry groundGeometry;

    RwWorldGeometry combatSurfaceGeometry;
    RwWorldGeometry collisionGeometry;
    RwWorldGeometry cameraStaticCollisionGeometry;
    RwWorldGeometry worldGeometry;
    std::vector<rw::Clump*> clumps;
    std::vector<rw::Clump*> objectTemplates;
    std::vector<FstageObjectBounds> objectBounds;
    std::vector<FstagePlacement> placements;
    std::vector<FstageObjectInstance> stageObjects;
    FstageObjectRuntimeHooks objectHooks;
    FstageObjectModelResolver objectModelResolver;
    void* objectModelContext;
    FstageWeaponManager stageWeapons;
    std::vector<FstageInteractionMarker> interactionMarkers;
    std::vector<rw::Animation*> animations;
    rw::HAnimHierarchy* hierarchy;
    size_t activeAnimation;
    size_t objectAnimationBase;
    rw::World* world;
    rw::V3d center;
    rw::V3d minBounds;
    rw::V3d maxBounds;
    float radius;
    bool haveBounds;
    uint16_t profileStageId;
    bool collisionProfileReady;
    size_t authoredWorldVertexCount;
    size_t authoredWorldTriangleCount;

    FstageScene();

    bool hasBounds() const;
    const std::vector<FstageObjectBounds>& objects() const;
    const rw::V3d& boundsMinimum() const;
    const rw::V3d& boundsMaximum() const;
    const std::vector<FstagePlacement>& placementRecords() const;
    const RwWorldGeometry& section2Collision() const;
    const RwWorldGeometry& cameraStaticCollision() const;
    const std::vector<FstageObjectInstance>& objectInstances() const;
    const FstageWeaponManager& weaponManager() const;
    const std::vector<FstageInteractionMarker>& markerRecords() const;
    bool containsNavigationPosition(const rw::V3d& position,
                                    float margin = 0.0f) const;
    void clampNavigationPosition(rw::V3d* position,
                                  float margin = 0.0f) const;
};

bool buildFstageCollisionGeometry(FstageScene* scene, uint16_t stageId);
bool installFstageStageObjects(
    FstageScene* scene, uint16_t stageId,
    const FstageObjectSpawnState& state,
    FstageObjectInstallResult* result = NULL);
void clearFstageStageObjects(FstageScene* scene);
bool queryFstageWorldItem(const FstageScene* scene, int index,
                          FstageWorldItem* item);
int findFstageWorldItem(const FstageScene* scene,
                        const rw::V3d& position);
bool removeFstageWorldItem(FstageScene* scene, int index);

size_t feedFstageObjectContacts(
    FstageScene* scene, const FstageObjectContact* contacts, size_t count);
bool completeFstageObjectAnimation(
    FstageScene* scene, uint16_t stageId, uint16_t recordIndex);

int16_t queryFstageWeaponCandidate(
    FstageScene* scene, FstageWeaponActorState* actor, bool commit);
bool attachFstageWeapon(
    FstageScene* scene, int16_t weaponId,
    FstageWeaponActorState* actor, uint8_t attachmentSlot);

bool attachFstageWeapon(
    FstageScene* scene, int16_t weaponId,
    FstageWeaponActorState* actor, uint8_t attachmentSlot,
    const WeaponDefinition& definition);

bool switchFstageWeaponHand(
    FstageScene* scene, int16_t weaponId,
    FstageWeaponActorState* actor, int16_t hand,
    const WeaponDefinition& definition);

bool snapshotFstageWeaponPose(
    FstageScene* scene, int16_t weaponId,
    const FstageWeaponActorState* actor);

bool forceDropFstageWeapon(
    FstageScene* scene, int16_t weaponId,
    FstageWeaponActorState* actor, const rw::V3d& bone21Forward);

bool tryForceDropFstageWeapon(
    FstageScene* scene, int16_t weaponId,
    FstageWeaponActorState* actor, const WeaponDefinition& definition,
    const rw::V3d& bone21Forward);
bool dropFstageWeapon(
    FstageScene* scene, int16_t weaponId, FstageWeaponActorState* actor);
bool tossFstageWeapon(
    FstageScene* scene, int16_t weaponId,
    FstageWeaponActorState* actor, uint8_t weaponBehavior,
    const FstageWeaponTossContact* firstContact = NULL);
bool throwFstageWeapon(
    FstageScene* scene, int16_t weaponId,
    FstageWeaponActorState* actor, const rw::V3d& target);
bool breakFstageWeapon(
    FstageScene* scene, int16_t weaponId,
    FstageWeaponActorState* actor, uint32_t randomU31,
    int8_t characterTerm, int8_t modifier);
bool releaseFstageWeapon(FstageScene* scene, int16_t weaponId);

void buildFstageInteractionMarkers(FstageScene* scene);

size_t gatherFstageCameraCollisionGeometry(
    const FstageScene& scene, const RwWorldGeometry** geometry,
    size_t capacity);
FstageMarkerResolution resolveFstageInteractionMarker(
    const FstageScene& scene, const rw::V3d& playerPosition,
    const FstageMarkerQueryState& state, FstageMarkerPrompt* prompt);

const uint32_t FSTAGE_ST4_FIRST_ENTRY = 1;
const uint32_t FSTAGE_ST4_LAST_ENTRY = 54;
uint32_t st4EntryIndexForStage(uint16_t stageId, uint32_t variantSelector,
                               bool gameStateBit1, bool matchSetupFlag);

bool loadFstageSceneByIndex(const char* archivePath, uint32_t entryIndex,
                            rw::World* world, FstageScene* scene);

bool loadFstageSceneByIndexForStage(const char* archivePath,
                                    uint32_t entryIndex, uint16_t stageId,
                                    rw::World* world, FstageScene* scene);

bool loadFstageScene(const char* archivePath, const char* entryName,
                     rw::World* world, FstageScene* scene);
void tickFstageScene(FstageScene* scene, float deltaSeconds);
void freeFstageScene(FstageScene* scene);

#endif
