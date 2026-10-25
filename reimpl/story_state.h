#ifndef STORY_STATE_H
#define STORY_STATE_H

#include "game/data.h"

#include <stdint.h>

const int STORY_SAVE_FLAG_COUNT = 8192;
const int STORY_STATUS_COUNT = 256;
const int STORY_REGISTER_COUNT = 256;
const int STORY_ITEM_COUNT = 4096;
const int STORY_ACTOR_COUNT = 256;
const int STORY_PARTY_COUNT = 4;
const int STORY_CHARACTER_COUNT = 200;
const int STORY_DRESS_SLOT_COUNT = 128;
const int STORY_NPC_SLOT_COUNT = 64;

const int STORY_NPC_HIDE_SLOT_COUNT = 6;

const int STORY_NPC_BEFORE_RECORD_COUNT = 7;

const int STORY_NPC_BASE_SELECTOR_COUNT = 600;
const int STORY_GENERATOR_COUNT = 5;

enum StoryProgressionPhase
{
    STORY_PROGRESSION_PHASE_BOOT = 0,
    STORY_PROGRESSION_PHASE_PROLOGUE,
    STORY_PROGRESSION_PHASE_RECRUITMENT,
    STORY_PROGRESSION_PHASE_CHAPTER_1,
    STORY_PROGRESSION_PHASE_CHAPTER_2,
    STORY_PROGRESSION_PHASE_INTERLUDE,
    STORY_PROGRESSION_PHASE_CHAPTER_3,
    STORY_PROGRESSION_PHASE_CHAPTER_4,
    STORY_PROGRESSION_PHASE_CHAPTER_5,
    STORY_PROGRESSION_PHASE_CHAPTER_6,
    STORY_PROGRESSION_PHASE_CHAPTER_7,
    STORY_PROGRESSION_PHASE_FINALE,
    STORY_PROGRESSION_PHASE_COUNT
};

enum StoryTransitionLifecycle
{
    STORY_TRANSITION_IDLE = 0,
    STORY_TRANSITION_EVENT_QUEUED,
    STORY_TRANSITION_EVENT_RUNNING,
    STORY_TRANSITION_POST_BATTLE,
    STORY_TRANSITION_FIELD_UNLOAD,
    STORY_TRANSITION_STAGE_LOAD,
    STORY_TRANSITION_FIELD_RESUME
};

enum StoryStageLoadState
{
    STORY_STAGE_LOAD_OL_WEAPON = 0,
    STORY_STAGE_LOAD_QUEUE_ST4,
    STORY_STAGE_LOAD_WAIT_ST4,
    STORY_STAGE_LOAD_INITIALIZE_ST4,
    STORY_STAGE_LOAD_DISPLAY,
    STORY_STAGE_LOAD_BEGIN_EFFECTS,
    STORY_STAGE_LOAD_WAIT_EFFECTS,
    STORY_STAGE_LOAD_BEGIN_SCENE_SOUND,
    STORY_STAGE_LOAD_WAIT_SCENE_SOUND,
    STORY_STAGE_LOAD_BEGIN_AUDIO,
    STORY_STAGE_LOAD_WAIT_AUDIO,
    STORY_STAGE_LOAD_FINALIZE_AUDIO,
    STORY_STAGE_LOAD_BIND_EFFECTS
};

enum StoryTransitionActionFlags
{
    STORY_TRANSITION_ACTION_QUEUED_ROUTE = 0x08,
    STORY_TRANSITION_ACTION_EXPLICIT_TRANSFORM = 0x10
};

struct StoryProgressionTask
{
    int16_t state;
    int16_t mode;
    int32_t work;
    float elapsedSeconds;
    uint8_t active;
};

struct StoryAuthoredTransform
{
    float headingDegrees;
    float position[3];
};

struct StoryStageTransition
{

    uint32_t actionFlags;
    uint16_t resultStatus;
    uint16_t queuedEventId;
    float headingDegrees;
    float arrival[3];
    uint16_t routeMode;
    uint16_t destinationStageId;

    uint16_t actorProfileId;

    uint16_t currentStageId;
    uint16_t sourceScriptId;
    int16_t arrivalSelector;
    StoryAuthoredTransform actorTransforms[2];
    float elapsedSeconds;
    uint16_t postBattleState;
    uint8_t lifecycle;
    uint8_t loadState;
    uint8_t authoredTransformCount;
    uint8_t controllerFlags;
    uint8_t postBattleFlagsApplied;
};

struct StoryHudTarget
{
    int16_t characterId;
    int16_t stageId;
};

struct StoryHudObjective
{
    StoryHudTarget primary;
    StoryHudTarget alternate;
    uint8_t useAlternate;
    uint8_t primaryActive;
    uint8_t alternateActive;
};

struct StoryHudRadarState
{
    StoryHudTarget relationshipTargets[2];

    StoryHudTarget specialRelationshipTargets[2];
    StoryHudObjective objective;
    uint8_t specialRelationshipMode;
    mutable uint8_t suppressRelationshipCellsOnce;
};

struct StoryProgressionRuntime
{
    StoryProgressionPhase phase;
    uint16_t recruitedCharacterCount;
    uint32_t controlFlags;
    uint32_t chapter3RowMask;
    uint32_t chapter5RowMask;
    uint32_t chapter6RowMask;
    uint32_t promptPhaseSeenMask;
    uint32_t adviceShownMask;
    uint16_t globalSlots[32];
    uint8_t targetLists[2][24];
    int16_t promptPhase;
    int16_t promptStep;
    int16_t messageId;
    int16_t messageStageId;
    int16_t messageTextId;
    int16_t objectiveStageId;
    int16_t chapter3TargetCount;
    int16_t chapter5ObjectiveCount;
    int16_t chapter6TargetCount;

    int16_t scratch005f3d28;
    int16_t selectedPartnerProfileId;

    int16_t scratch005f3d5c;
    int16_t scratch005f3d58;
    int16_t scratch005f3d60;
    int16_t dynamicMessageStageId;
    int16_t dynamicMessageId;
    int16_t lastPromptStageId;
    uint16_t fullHealIds[8];
    uint16_t messageHelper00395e30Count;
    uint16_t thunk001becd0Count;
    uint8_t fullHealCount;
    uint8_t fullHealResourceProfileMask;
    uint8_t deferred;
    uint8_t transitionMode;
    uint8_t messageActive;
    uint8_t scratch005f3d64;
    uint8_t finaleCleanupStarted;
    uint8_t manualCameraDisabled;
    uint8_t cameraResetRequested;
    uint8_t interactionStateBit0;
    uint8_t storyComplete;
    StoryProgressionTask battleFlowTask;
    StoryProgressionTask promptTask;
    StoryProgressionTask sequenceTask;
    StoryProgressionTask dialogueTask;
    StoryProgressionTask battleStartTask;
    StoryProgressionTask introFadeTask;
    StoryHudRadarState hudRadar;
};

struct StoryProgressionInput
{
    uint16_t currentStageId;
    uint16_t destinationStageId;
    uint16_t sceneId;
    int16_t encounterProfileId;
    uint16_t playerResourceProfileId;
    uint16_t playerCharacterId;
    uint16_t opponentCharacterId;
    uint16_t activeCharacterIds[11];
    int activeCharacterCount;
    float playerX;
    float playerZ;
    uint8_t encounterCompleted;
    uint8_t encounterStatus;
    uint8_t encounterRecruited;
    uint8_t updateTaskActive;
    uint8_t battleFlowBlocked;
    uint8_t interactionStateBit0;
    uint8_t countdownFinished;
    uint8_t fieldReady;
    uint8_t promptConfirmed;
};

struct StoryEncounterTransitionInput
{
    uint16_t opponentResourceProfileId;
    uint16_t playerResourceProfileId;
    uint16_t opponentCharacterId;
    uint16_t playerCharacterId;
};

enum StoryCharacterFlags
{
    STORY_CHARACTER_ENCOUNTERED = 1 << 0,
    STORY_CHARACTER_RECRUITED = 1 << 1
};

struct StoryCharacterState
{
    int16_t relationshipScore;
    uint16_t characterFlags;
    uint16_t profileRevealFlags;
    uint16_t battleStatusFlags;
    uint32_t abilityFlags;
    int32_t cash;
    uint8_t relationshipState;
    uint8_t gender;
    uint8_t persistentStatus;
    uint8_t reserved;
};

struct StoryDressEntry
{
    uint16_t itemId;
    uint16_t variant;
    uint16_t quantity;
    uint8_t active;
    uint8_t reserved;
};

struct StoryNpcBeforeRecord
{
    uint8_t stageId;
    uint8_t transitionMode;
    int16_t root;
};

struct StoryNpcSlot
{
    uint16_t actorId;
    uint16_t markId;
    int32_t parameter;
    int32_t option;
    uint8_t active;
    uint8_t reserved[3];
};

struct StoryGeneratorState
{
    float position[3];
    float headingDegrees;
    float timerSeconds;
    int16_t remainingCount;
    uint16_t stageId;
};

struct StoryCameraTransition
{
    float start[3];
    float end[3];
    int32_t durationFrames;
    uint8_t interpolate;
    uint8_t active;
    uint8_t reserved[2];
};

struct StoryActorState
{
    uint16_t actorId;
    uint16_t type;
    float position[3];
    float headingDegrees;
    uint16_t motionId;
    uint16_t targetActorId;
    uint16_t markId;
    int16_t currentHealth;
    float destination[3];
    float movementDurationFrames;
    uint8_t movementMode;
    uint8_t active;
    uint8_t visible;
    uint8_t inParty;
    uint8_t friendly;
    uint8_t defeated;
    uint8_t captured;
    uint8_t arrested;
    uint8_t busted;
};

struct StoryState
{
    uint32_t saveFlags[STORY_SAVE_FLAG_COUNT / 32];
    int32_t statuses[STORY_STATUS_COUNT];
    uint32_t registers[STORY_REGISTER_COUNT];
    uint32_t globalRegisters[STORY_REGISTER_COUNT];
    uint8_t registerTypes[STORY_REGISTER_COUNT];
    StoryCharacterState characters[STORY_CHARACTER_COUNT];
    StoryDressEntry dressInventory[STORY_DRESS_SLOT_COUNT];
    StoryActorState actors[STORY_ACTOR_COUNT];
    StoryNpcSlot npcSlots[STORY_NPC_SLOT_COUNT];
    StoryNpcBeforeRecord npcBeforeRecords[STORY_NPC_BEFORE_RECORD_COUNT];
    StoryGeneratorState generators[STORY_GENERATOR_COUNT];
    StoryCameraTransition cameraPosition;
    StoryCameraTransition cameraTarget;
    uint16_t party[STORY_PARTY_COUNT];
    uint16_t stageId;
    uint16_t eventStageId;
    uint16_t circumstanceId;
    uint16_t callerActorId;
    uint16_t battleId;

    uint16_t battleEventId;
    int16_t activeOpponentRosterIndex;
    uint16_t activeOpponentCharacterId;
    uint16_t subBattleId;
    uint16_t targetActorId;
    uint16_t eventStageEntityId;
    uint16_t removedActorId;
    uint16_t lastPunchedActorId;
    uint16_t lastPunchingActorId;
    uint16_t messageActorId;
    int32_t countdownFrames;
    int32_t crime;
    int32_t result;
    int32_t battleResultCode;
    int32_t eventTypeMask;
    int32_t outlawValues[4];
    uint32_t eventGeneration;
    uint32_t eventValue;
    int32_t money;
    int32_t pride;
    uint32_t messageControlFlags;
    uint32_t gameStateFlags;
    uint32_t actorActionControlFlags;
    uint32_t battleFlags;
    uint32_t activeOpponentSlotFlags;
    uint16_t scratch005f3c50;
    uint16_t scratch005f3c54;
    uint8_t scratch005f1964;
    uint8_t scratch005f2220;

    uint8_t scratch005f24bc;
    uint8_t scratch005f3d04;
    uint8_t scratch005f3d08;
    uint8_t scratch005f3d24;
    uint8_t scratch005f33f4;
    uint8_t scratch005f333c;
    uint8_t battleActive;
    uint8_t battlePlural;
    uint8_t gameOver;
    uint8_t playerStopped;
    uint8_t escapeEnabled;
    uint8_t cameraMode;
    uint8_t telephoneEnabled;
    uint8_t messageTurnEnabled;
    uint8_t growDisabled;
    uint8_t pickpocketMode;
    uint8_t resultDisplayActive;
    uint8_t stageTransitionPending;

    int32_t eventSelectorPrimary;
    int32_t eventSelectorSecondary;

    uint8_t npcBaseSelectorCountdown[STORY_NPC_BASE_SELECTOR_COUNT];

    uint32_t playerConditionFlags;

    int16_t recruitmentMode;

    uint16_t recruitmentRatings[3];

    int8_t activeStoryEventKinds[2];

    int16_t recruitmentPlayerResourceProfile;

    uint32_t phase11ModeFlags;

    uint8_t resetRecruitmentRoot;

    uint8_t encounterSubstate;

    uint8_t phase10Stage31Suppress;

    uint8_t battleContextFlowActive;

    uint8_t battleLetterboxFlowActive;

    uint16_t gameplayEventId;

    uint8_t storyOverlayGlobalCondition11;

    uint8_t storyOverlayRoot2Blocked;

    uint8_t storyOverlayRoot2Eligibility;
    uint8_t storyOverlayRoot1Eligibility;

    uint8_t storyOverlayState;

    uint32_t npcBattleRootDebounceTick;

    uint32_t storyStageEligibilityFlags;

    uint8_t storyOverlayScriptLoadActive;
    StoryProgressionRuntime progression;
    StoryStageTransition stageTransition;
    CharacterCustomization customization;
};

void initializeStoryState(StoryState& state);
bool validateStoryState(const StoryState& state);
bool getStorySaveFlag(const StoryState& state, uint16_t flagId);
void setStorySaveFlag(StoryState& state, uint16_t flagId, bool enabled);
StoryActorState* findStoryActor(StoryState& state, uint16_t actorId,
                                bool create);
const StoryActorState* findStoryActor(const StoryState& state,
                                      uint16_t actorId);
bool addStoryPartyMember(StoryState& state, uint16_t actorId, bool friendly);
bool removeStoryPartyMember(StoryState& state, uint16_t actorId);
void advanceStoryActor(StoryState& state, StoryActorState& actor,
                       float deltaSeconds);
void advanceStoryActors(StoryState& state, float deltaSeconds);

bool storySceneDefersProgression(uint16_t sceneId);
bool storyMoveStageUsesRegister(uint16_t scriptId);

void beginStoryEventSelection(StoryState& state, int eventId);

void endStoryEventSelection(StoryState& state, int eventId);

bool storyNpcBaseSelectorAvailable(const StoryState& state, int16_t selector);

struct PersistentGameRecord;

typedef PersistentGameRecord* (*StoryPersistentRecordQuery)(
    int16_t characterId, void* userData);

enum StoryNpcOverlayDecision
{
    STORY_NPC_OVERLAY_KEEP = 0,
    STORY_NPC_OVERLAY_COPY = 1,
    STORY_NPC_OVERLAY_CLEAR = 2
};

StoryNpcOverlayDecision storyNpcOverlayDecision(
    StoryState& state, int16_t selector, bool indexedPersistentValue6,
    bool suppressPostBattleOverlays, const int16_t activeRecordIds[3],
    StoryPersistentRecordQuery recordQuery, void* recordUserData,
    int16_t* resolvedSelector);

int storyEffectiveEventId(const StoryState& state);

int storyAlternateOverlayMode(const StoryState& state);

int storyTransitionModeValue(const StoryState& state);

int16_t storyNpcBeforeRoot(const StoryState& state, uint16_t currentStageId);

typedef uint32_t (*StoryRandomSource)(void* userData);

bool storyResolveOverlayState(StoryState& state,
                              StoryRandomSource randomSource,
                              void* randomUserData);

void storyResolveNpcBattleRoot(StoryState& state,
                               uint16_t currentStageId,
                               uint32_t tick,
                               bool activePartyBattleFlag,
                               bool indexedValue6Suppressor,
                               StoryRandomSource randomSource,
                               void* randomUserData,
                               int16_t* battleRoot,
                               bool* suppressPostBattleOverlays);

bool canRecruitStoryCharacter(const StoryState& state, uint16_t phase,
                              uint16_t characterId);

int16_t storyRecruitmentState(const StoryState& state, uint16_t phase,
                              uint16_t characterId);
void selectStoryStageArrival(uint16_t destinationStageId,
                             uint16_t sceneId,
                             float& headingDegrees,
                             float arrival[3]);
void queueStoryStageTransition(StoryState& state,
                               uint16_t destinationStageId,
                               uint16_t sourceScriptId,
                               int16_t arrivalSelector);
void targetListMan(StoryState& state, int listType, int index);
void setStoryProgressionMessage(StoryState& state, int messageId,
                                bool pauseBattleSlots);
void startStoryDialogueGate(StoryState& state, int mode,
                            const StoryProgressionInput& input);
void startStoryIntroFade(StoryState& state, int mode);
void refreshStoryProgressionFlags(StoryState& state, bool clearOnExit);
int refreshStoryProgressionPrompt(StoryState& state);
bool updateStoryProgressionFlow(StoryState& state,
                                const StoryProgressionInput& input);
void updateStoryProgressionTasks(StoryState& state,
                                 const StoryProgressionInput& input,
                                 float deltaSeconds);
void applyStoryChapter3BattleFlags(StoryState& state,
                                   uint16_t characterId, bool encounterMode);
void applyStoryChapter5BattleFlags(StoryState& state,
                                   uint16_t characterId, bool encounterMode);
void applyStoryChapter6BattleFlags(StoryState& state,
                                   uint16_t characterId, bool encounterMode);
void applyStoryPostBattleFlags(StoryState& state, uint16_t opponentCharacterId);
void processStoryEncounterCompletion(StoryState& state,
                                     uint16_t resourceProfileId,
                                     uint8_t persistentStatus,
                                     bool recruited);
int handleStoryCharacterAction(StoryState& state,
                               uint16_t resourceProfileId);
bool applyStoryEncounterTransition(
    StoryState& state, const StoryEncounterTransitionInput& input);
bool selectStoryRelationshipTarget(StoryState& state, uint16_t characterId,
                                   int16_t stageId, bool specialMode);
bool clearStoryRelationshipTarget(StoryState& state, uint16_t characterId,
                                  bool specialMode, bool completed);
const StoryHudTarget* storyRelationshipTargets(const StoryState& state);

void setStoryHudObjective(StoryState& state, int16_t characterId,
                          int16_t stageId, bool alternate);

void advanceStoryProgressionPhase(StoryState& state);
void finalizeStoryEncounterFlow(StoryState& state);

#endif
