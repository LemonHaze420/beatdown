#ifndef GAME_BATTLE_FLOW_H
#define GAME_BATTLE_FLOW_H

#include <stdint.h>

const int BATTLE_FLOW_ACTOR_COUNT = 13;
const float BATTLE_FLOW_DISTANCE_OUTCOME_DELAY = 1.0f;
const float BATTLE_FLOW_PLAYER_TARGET_DELAY = 0.4f;
const float BATTLE_FLOW_OPPONENT_TARGET_DELAY = 1.5f;

enum BattleFlowControllerState
{
    BATTLE_FLOW_CONTROLLER_ACTIVE = 0,
    BATTLE_FLOW_CONTROLLER_WAIT_SLOW_MOTION = 1,
    BATTLE_FLOW_CONTROLLER_WAIT_TARGET = 2,
    BATTLE_FLOW_CONTROLLER_STORY_DELAY = 3,
    BATTLE_FLOW_CONTROLLER_STORY_WAIT = 4,
    BATTLE_FLOW_CONTROLLER_SEQUENCE = 5,
    BATTLE_FLOW_CONTROLLER_RESOLVE = 6,
    BATTLE_FLOW_CONTROLLER_LETTERBOX = 9,
    BATTLE_FLOW_CONTROLLER_END_WAIT = 10,
    BATTLE_FLOW_CONTROLLER_CLEANUP = 11
};

enum BattleFlowContextFlag
{
    BATTLE_FLOW_CONTEXT_EXIT_ENABLED = 0x2,
    BATTLE_FLOW_CONTEXT_SEQUENCE_ACTIVE = 0x8,
    BATTLE_FLOW_CONTEXT_OUTCOME_REQUEST = 0x10,
    BATTLE_FLOW_CONTEXT_SEQUENCE_20 = 0x20,
    BATTLE_FLOW_CONTEXT_TARGET_40 = 0x40,
    BATTLE_FLOW_CONTEXT_TARGET_80 = 0x80,
    BATTLE_FLOW_CONTEXT_TARGET_PENDING = 0x100,
    BATTLE_FLOW_CONTEXT_END_200 = 0x200,
    BATTLE_FLOW_CONTEXT_END_400 = 0x400,
    BATTLE_FLOW_CONTEXT_TARGET_800 = 0x800,
    BATTLE_FLOW_CONTEXT_END_1000 = 0x1000,
    BATTLE_FLOW_CONTEXT_CLEANUP_READY = 0x2000
};

struct BattleFlowActorSnapshot
{
    bool valid = false;
    bool modelPresent = false;
    bool loadStateKnown = false;
    uint32_t flags = 0;
    uint8_t loadStateFlags = 0;
    uint32_t behaviorFlags = 0;
    uint32_t actorActionFlags = 0;
    uint32_t poseFlags = 0;
    int16_t health = 0;
    int16_t pride = 0;
    int nativeCharacterId = -1;
    int nativeSlotIndex = -1;
    int linkedSlot = -1;
    int weaponObjectId = -1;
    int profileCategory = -1;
    bool activeRoster = false;
    int battleGroup = 2;
    bool groundedKnown = false;
    bool grounded = false;
};

struct BattleFlowRuntime
{
    int state = BATTLE_FLOW_CONTROLLER_ACTIVE;
    uint32_t flags = 0;
    int participants[2] = { -1, -1 };
    int pendingCombatant = -1;
    int attacker = -1;
    int pendingActionSlot = -1;
    int pendingOutcomeSlot = -1;
    int targetRequestTarget = -1;
    int outcomeReturnState = BATTLE_FLOW_CONTROLLER_SEQUENCE;
    float outcomeTimerFrames = 0;
    uint32_t nativeSequenceFlags = 0;
    int transitionType = 0;
    int outcomeKind = 0;
    uint32_t field_0x05c = 0;
    int sequenceValue = -1;
    uint32_t sequenceOptionFlags = 0;
    int sequenceState = 0;
    float opponentDefeatDelaySeconds = 0;
    float targetDelaySeconds = 0;
    float frameAccumulator = 0;
    uint16_t frameCounter = 0;
};

struct BattleFlowFrameInput
{
    BattleFlowActorSnapshot actors[BATTLE_FLOW_ACTOR_COUNT];
    float scaledDeltaSeconds = 0;
    bool requestFlag0400 = false;
    bool targetRequestPending = false;
    bool storyProgressTaskActive = false;
    bool sixtyHzTimingMode = true;
    bool pauseGuardEnabled = true;
    uint32_t gameStateFlags = 0;
    bool interaction = false;
    int slowMotionEnabled = 0;

    bool opponentResolutionKnown = false;
    bool filteredOpponentsResolved = false;
    bool allOpponentsResolved = false;
    bool interactionAttackerRoutingKnown = false;

    bool outcomeRulesKnown = false;
    uint32_t primaryLinkedPersistentFlags = 0;
    int nativeBattleModeFlag = 0;
    int encounterStageId = 0;
    int stageId = 0;
};

struct BattleFlowOutcomeBlockInput
{
    uint32_t gameStateFlags = 0;
    int nativeGlobal_0x5f3f90 = 0;
    int encounterStageId = 0;
    uint32_t nativeGlobal_0x5f3dcc = 0;
    bool primaryAlive = false;
    uint32_t primaryBehaviorFlags = 0;
};

struct BattleFlowEffects
{
    bool waitingForInput = false;
    bool delegatedState = false;
    bool stopBattleCommand = false;
    bool requestSequenceTick = false;
    bool requestEndSequenceTick = false;
    bool startStoryProgressTask = false;
    bool clearPauseGuard = false;
    bool restorePauseGuard = false;
    bool resetBattleState = false;
    bool updateOpponentRangeState = false;
    bool startPostBattleScene = false;
    bool createStageEventAudioTask = false;
    bool stopEventSoundChannel0 = false;
    bool pauseCountdown = false;
    bool releaseParticipantAttachments = false;
    bool releaseAttackerAttachment = false;
    bool clearAttackerInput = false;
    bool setAttackerMotion40000 = false;
    int eventSoundCue = -1;
    uint16_t resetAttachmentMask = 0;
    uint16_t actorStatusSet2Mask = 0;
    int directActionSlot = -1;
    int directAction = 0;
    bool resetAllAttachments = false;
    bool clearSlowMotionRequest = false;
    bool phase3StoryDelayRequested = false;

    bool finisherRequested = false;
    bool playerFeedbackRecovery = false;
    bool setAttackerWeaponMotionFlags = false;
    uint16_t freezeActorMask = 0;
    uint16_t updatePrideGaugeMask = 0;
    uint16_t returnOwnedWeaponMask = 0;
    int outcomePhase = -1;
    int mappedActions[BATTLE_FLOW_ACTOR_COUNT] = {};
    int persistentStatusSlot = -1;
    int persistentStatus = 0;
    float scoreMultiplier = 0;

    int phase3DelayFrameCount = 0;
};

bool battleFlowCombatReady(const BattleFlowActorSnapshot& actor);
bool battleFlowOutcomeBlocked(const BattleFlowOutcomeBlockInput& input);
bool battleFlowExitRequested(const BattleFlowRuntime& runtime,
                             const BattleFlowFrameInput& input);
bool battleFlowCanComplete(const BattleFlowFrameInput& input);
bool battleFlowCanTriggerOpponentFollowup(const BattleFlowRuntime& runtime,
                                          const BattleFlowFrameInput& input,
                                          int attackerSlot, int targetSlot);
bool queueBattleFlowTarget(BattleFlowRuntime& runtime,
                           const BattleFlowFrameInput& input,
                           int targetSlot, bool updateAction);
BattleFlowEffects updateBattleFlow(BattleFlowRuntime& runtime,
                                   const BattleFlowFrameInput& input);

void completeBattleFlowSequenceTick(BattleFlowRuntime& runtime,
    const BattleFlowFrameInput& input, int sequenceResult, BattleFlowEffects& effects);
void completeBattleFlowEndSequenceTick(BattleFlowRuntime& runtime,
    const BattleFlowFrameInput& input, bool complete, BattleFlowEffects& effects);

#endif
