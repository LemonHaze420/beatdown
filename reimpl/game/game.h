#ifndef GAME_H
#define GAME_H

#include "event_vm.h"
#include "task.h"
#include "game/data.h"
#include "game/save.h"
#include "story_state.h"
#include "stage/ai.h"
#include "scene/sequence.h"
#include "move_input.h"
#include "game/cpu_input.h"
#include "game/locomotion.h"
#include "game/actor_state.h"
#include "game/combat/received_damage.h"
#include "game/battle/finisher.h"
#include "game/battle/controller.h"
#include "game/battle/end.h"
#include "game/battle/resources.h"
#include "move_chains.h"
#include "behaviour_stream.h"
#include "move_stream.h"
#include "camera/camdat.h"
#include "title_attract.h"
#include "credits.h"
#include "ui/hud/radar.h"
#include "options_menu.h"

#include "weapon.h"
#include "game/combat/weapons.h"
#include "versus_tables.h"
#include "game/random.h"
#include <string>
#include <stdint.h>
struct FstageNpcPositionRow;
struct FstageScene;

#include "game/vector.h"
#include "game/combat/projectiles.h"

enum CharacterId
{
    CHARACTER_RAVEN = 0x009c,
    CHARACTER_LOLA = 0x009b,
    CHARACTER_JASON_G = 0x009a,
    CHARACTER_GINA = 0x009d,
    CHARACTER_AARON = 0x0099,
    CHARACTER_MOBSTER = 0x0103
};

enum GameScreen
{
    GAME_SCREEN_LOADING,
    GAME_SCREEN_LANGUAGE_SELECT,
    GAME_SCREEN_VIDEO_MODE_SELECT,
    GAME_SCREEN_NO_SAVE_DATA,
    GAME_SCREEN_ATTRACT_MOVIE,
    GAME_SCREEN_TITLE,
    GAME_SCREEN_MAIN_MENU,
    GAME_SCREEN_VERSUS_TYPE_SELECT,
    GAME_SCREEN_CHARACTER_SELECT,
    GAME_SCREEN_STAGE_SELECT,
    GAME_SCREEN_BRIEFING,
    GAME_SCREEN_GAMEPLAY,
    GAME_SCREEN_PAUSE,
    GAME_SCREEN_INVENTORY,
    GAME_SCREEN_SHOP,
    GAME_SCREEN_OPTIONS,
    GAME_SCREEN_ADVICE,
    GAME_SCREEN_RESULTS,
    GAME_SCREEN_CREDITS,
    GAME_SCREEN_EXIT
};

enum GameMode
{
    GAME_MODE_STORY,
    GAME_MODE_VERSUS_NORMAL,
    GAME_MODE_VERSUS_CUSTOM
};

enum VersusBattleType
{
    VERSUS_PLAYER_VS_CPU = 0,
    VERSUS_PLAYER_VS_PLAYER = 1,
    VERSUS_CPU_VS_CPU = 2,
    VERSUS_BATTLE_TYPE_COUNT = 3
};

enum StoryCharacterSelectPhase
{
    STORY_CHARACTER_SELECT_LOADING,
    STORY_CHARACTER_SELECT_ACTIVE,
    STORY_CHARACTER_SELECT_MOVING,
    STORY_CHARACTER_SELECT_CONFIRMING,
    STORY_CHARACTER_SELECT_MENU_WAIT,
    STORY_CHARACTER_SELECT_CANCEL_WAIT
};

struct StoryCharacterSelectPresentation
{
    StoryCharacterSelectPhase phase;
    int outgoingSelection;
    int movementFrame;
    int confirmFrame;
    int menuCommand;
    float incomingPortraitX;
    float incomingPortraitScale;
    uint8_t incomingPortraitAlpha;
    float outgoingPortraitX;
    uint8_t outgoingPortraitAlpha;
    float statsX;
    float leftArrowX;
    float rightArrowX;
    uint8_t primaryAlpha;
    uint8_t portraitAlpha;
    float portraitScale;
};

enum IntroCardTaskState
{
    INTRO_CARD_TASK_INACTIVE = 0,
    INTRO_CARD_TASK_SETUP = 2,
    INTRO_CARD_TASK_FADE_IN = 3,
    INTRO_CARD_TASK_SOUND_GATE = 4,
    INTRO_CARD_TASK_HOLD = 5,
    INTRO_CARD_TASK_FADE_OUT = 6,
    INTRO_CARD_TASK_COMPLETE = 7
};

const uint8_t GAME_INTRO_CARD_MASK_ALPHA_MAX = 0x7f;

struct IntroCardPresentation
{
    IntroCardTaskState state;
    uint8_t recordIndex;
    uint8_t textAlpha;
    uint8_t maskAlpha;
    float holdSeconds;
};

const int GAME_STORY_STAGE_COUNT = 53;
const uint32_t GAME_RUNTIME_ADVICE_SHOWN = 1u << 1;

const int GAME_VERSUS_STAGE_COUNT = VERSUS_STAGE_COUNT;
const int GAME_VERSUS_CHARACTER_COUNT = VERSUS_CHARACTER_COUNT;
const int GAME_VIDEO_MODE_COUNT = 3;
const uint32_t GAME_RUNTIME_STORY_COMPLETE = 1u << 0;
const uint32_t GAME_MOTION_INHIBIT_HEALTH_WRITE = 1u << 13;

enum GameInputButton
{
    GAME_INPUT_UP      = 1 << 0,
    GAME_INPUT_DOWN    = 1 << 1,
    GAME_INPUT_LEFT    = 1 << 2,
    GAME_INPUT_RIGHT   = 1 << 3,
    GAME_INPUT_CONFIRM = 1 << 4,
    GAME_INPUT_CANCEL  = 1 << 5,
    GAME_INPUT_START   = 1 << 6,
    GAME_INPUT_PAUSE   = 1 << 7,
    GAME_INPUT_QUIT    = 1 << 8,
    GAME_INPUT_ATTACK  = 1 << 9,
    GAME_INPUT_GUARD   = 1 << 10,
    GAME_INPUT_HEAVY   = 1 << 11,
    GAME_INPUT_DODGE   = 1 << 12,
    GAME_INPUT_GRAB    = 1 << 13,
    GAME_INPUT_INTERACT = 1 << 14,
    GAME_INPUT_USE_FIRST = 1 << 15,
    GAME_INPUT_USE_SECOND = 1 << 16,

    GAME_INPUT_PUNCH = GAME_INPUT_ATTACK,
    GAME_INPUT_KICK = GAME_INPUT_HEAVY
};

const int GAME_INPUT_TRACKED_BUTTONS = 32;

struct GameInputState
{
    uint32_t currentButtons;
    uint32_t previousButtons;
    uint32_t pressedButtons;
    uint32_t releasedButtons;
    uint32_t repeatedButtons;
    float buttonHeldSeconds[GAME_INPUT_TRACKED_BUTTONS];
    uint32_t lastChangedFrame[GAME_INPUT_TRACKED_BUTTONS];
    uint32_t repeatCounter;

    GameInputState();
    void reset(void);
    void update(float deltaSeconds, uint32_t buttons,
                uint32_t globalFrameCounter, uint32_t elapsedFrames = 1);
};

struct GameRumbleState
{
    float remainingSeconds;
    float lowMotor;
    float highMotor;
    uint8_t priority;
    bool active;
};

enum CombatReaction
{
    COMBAT_REACTION_NONE = 0,
    COMBAT_REACTION_LIGHT = 1,
    COMBAT_REACTION_MEDIUM = 2,
    COMBAT_REACTION_HEAVY = 3,
    COMBAT_REACTION_DEFEAT = 4
};

enum CombatAction
{
    COMBAT_ACTION_IDLE,
    COMBAT_ACTION_MOVE,
    COMBAT_ACTION_LIGHT_ATTACK,
    COMBAT_ACTION_HEAVY_ATTACK,
    COMBAT_ACTION_GRAB,
    COMBAT_ACTION_GUARD,
    COMBAT_ACTION_DODGE,
    COMBAT_ACTION_STAGGER,
    COMBAT_ACTION_KNOCKDOWN,
    COMBAT_ACTION_DEFEATED,
    COMBAT_ACTION_VICTORY
};

const float BATTLE_TIME_SCALE_NORMAL = 1.0f;

const float BATTLE_TIME_SCALE_SPECIAL = 0.85f;

const float BATTLE_TIME_SCALE_FINISHER = 0.72f;

const float BATTLE_FINISHER_PRE_BLOW_SECONDS = 1.3f;
const float BATTLE_FINISHER_POST_BLOW_SECONDS = 7.0f;
const float BATTLE_FEEDBACK_MODE_2_SECONDS = 0.7f;

enum BattleFinisherPhase
{
    BATTLE_FINISHER_INACTIVE,
    BATTLE_FINISHER_PRE_BLOW,
    BATTLE_FINISHER_POST_BLOW,
    BATTLE_FINISHER_FEEDBACK_RECOVERY
};

enum BattleOutcome
{
    BATTLE_OUTCOME_IN_PROGRESS,
    BATTLE_OUTCOME_PLAYER_WON,
    BATTLE_OUTCOME_PLAYER_LOST
};
const int BATTLE_RESULT_SOUND_CHANNEL_COUNT = 3;
const float BATTLE_RESULT_READINESS_DELAY_SECONDS = 4.0f;
const float BATTLE_RESULT_INPUT_DELAY_SECONDS = 1.0f;
const uint8_t BATTLE_RESULT_TRANSITION_TYPE_COUNT = 11;

enum BattleResultControllerState
{
    BATTLE_RESULT_CONTROLLER_IDLE,
    BATTLE_RESULT_CONTROLLER_DISPATCH_TRANSITION,
    BATTLE_RESULT_CONTROLLER_POST_BATTLE_SCENE,
    BATTLE_RESULT_CONTROLLER_SELECTOR_READINESS,
    BATTLE_RESULT_CONTROLLER_SELECTOR_INPUT_DELAY,
    BATTLE_RESULT_CONTROLLER_SELECTOR_ACTIVE,
    BATTLE_RESULT_CONTROLLER_CONFIRMATION,
    BATTLE_RESULT_CONTROLLER_TERMINAL_DRAIN,
    BATTLE_RESULT_CONTROLLER_RETRY_CLEANUP,
    BATTLE_RESULT_CONTROLLER_RETRY_RELOAD,
    BATTLE_RESULT_CONTROLLER_STORY_HANDOFF,
    BATTLE_RESULT_CONTROLLER_COMPLETE
};

struct BattleResultController
{
    uint8_t transitionType;
    uint8_t commandIndex;
    uint8_t terminalCode;
    uint8_t actorIndices[2];
    uint8_t sideActiveCounts[2];
    int8_t sideThresholds[2];
    BattleResultControllerState state;
    float readinessSecondsRemaining;
    float inputSecondsRemaining;
    int selectedIndex;
    int confirmationSelection;
    bool selectorVisible;
    bool confirmationActive;
    bool flowReady;
    bool transitionConfigured;
    bool soundPending[BATTLE_RESULT_SOUND_CHANNEL_COUNT];
    bool rewardSuppressed;
    bool rewardDispatched;
    int32_t pendingMessageVoiceTrack;
};

const int MAX_GAME_OPPONENTS = 3;

const int GAME_COMBAT_SLOT_COUNT = 13;

struct CombatActionPairRecord
{
    int32_t selfMotionIndex;
    int32_t linkedMotionIndex;
    int16_t selfMotionId;
    int16_t linkedMotionId;

    int32_t selfStateIndex;
    int32_t linkedStateIndex;
    int16_t selfStateValue;
    int16_t linkedStateValue;
    uint8_t eligibleSelf;
    uint8_t eligibleLinked;

    bool inputPresentationValid;
    CombatAction inputPresentation;
};

const int BATTLE_EXPERIENCE_CAP = 100;
const int BATTLE_COMBO_BONUS_CAP = 10000;
const int BATTLE_MAX_LEVEL = 50;
const int BATTLE_REWARD_ROSTER_SIZE = 3;
const uint32_t BATTLE_REWARD_INTERACTION_FLAG = 0x0200u;

const float GAME_GROUND_STEP_UP = 16.0f;

const float GAME_ACTOR_GROUND_OFFSET = 10.6f;

const uint16_t GAME_FIRST_PLAYABLE_STAGE_ID = 12;

struct CombatDamageRecord
{
    uint8_t unknown_0x00[2];
    uint8_t channel_0x02;
    uint8_t channel_0x03;
    uint8_t channel_0x04;
    uint8_t channel_0x05;
    uint8_t unknown_0x06[4];
    uint8_t channel_0x0a;
    uint8_t channel_0x0b;
    uint8_t unknown_0x0c[6];
    uint8_t channel_0x12;
    uint8_t channel_0x13;
};

const int COMBAT_BONE_COUNT = 22;
const int COMBAT_ATTACK_GAUGE_COUNT = 6;
const int GAME_COMBAT_CONTACT_CAPACITY = 32;

struct CombatBoneTransform
{
    Vec3 right;
    Vec3 up;
    Vec3 at;
    Vec3 position;
};

struct CombatSphere
{
    Vec3 center;
    float radius;
    CombatBoneTransform transform;
};

struct GameCombatContact
{
    uint16_t damage;
    uint8_t attackerSlot;
    uint8_t victimSlot;
    int8_t channel;
    int8_t contactIndex;
    Vec3 center;
    float radius;
    CombatBoneTransform transform;
};

bool strictCombatSphereOverlap(const CombatSphere& first,
                               const CombatSphere& second);
uint16_t stagedCombatDamage(uint16_t baseDamage, int16_t weaponBonus,
                            float attackerPower, float victimMultiplier,
                            float contactFactor, bool exitScale);
int16_t combatPrideDamageAmount(int16_t blockPride, uint32_t randomValue,
                                uint8_t movesetPercent,
                                int16_t currentHealth, int16_t maxHealth,
                                int8_t primaryModifier);
uint32_t combatHitDirection(const Vec3& victimPosition,
                            float victimHeadingDegrees,
                            const Vec3& attackerPosition);
int combatReactionDirectionIndex(uint32_t direction);

struct GameOptions
{
    uint8_t musicVolume;
    uint8_t effectsVolume;
    uint8_t vibration;
    uint8_t language;
    uint8_t difficulty;
    uint8_t subtitles;
    uint8_t mapDisplay;
    uint8_t gaugeDisplay;
    uint8_t bloodMale;
    uint8_t bloodFemale;
    uint8_t bruisingMale;
    uint8_t bruisingFemale;
    uint8_t controllerTypeP1;
    uint8_t controllerTypeP2;
    uint8_t vibrationP2;
    uint8_t cameraXType;
    uint8_t cameraYType;
    uint8_t cameraFollow;
    int16_t screenAdjustX;
    int16_t screenAdjustY;
    uint8_t brightness;
    uint8_t audioOutput;
    uint8_t videoTimingMode;
};

enum GameOptionsPage
{

    GAME_OPTIONS_PAGE_MAIN = 0,
    GAME_OPTIONS_PAGE_AUDIO = 1,
    GAME_OPTIONS_PAGE_CONTROL = 2,
    GAME_OPTIONS_PAGE_SCREEN = 3,
    GAME_OPTIONS_PAGE_BRIGHTNESS = 4,
    GAME_OPTIONS_PAGE_MOVIE_VIEWER = 5
};

enum InventoryUsePhase
{
    INVENTORY_USE_LIST,
    INVENTORY_USE_ACTION,
    INVENTORY_USE_TARGET,
    INVENTORY_USE_CONFIRM,
    INVENTORY_USE_RESULT
};

struct InventoryUseState
{
    InventoryUsePhase phase;
    uint16_t packedEntryIndex;
    uint32_t packedEntry;
    uint16_t itemId;
    uint8_t variant;
    uint8_t actionClass;
    uint8_t actionSelection;
    uint8_t targetSelection;
    uint8_t targetCount;
    uint8_t confirmSelection;
    uint16_t targetCharacterIds[3];
    uint8_t effectCode;
    uint8_t resultPromptIndex;
    uint8_t resultTextCount;
    uint16_t resultTextIds[2];
    uint8_t resultGateFrames;
    bool consumed;
    bool newlySlowed;
    bool slowWasAlreadyActive;
};

enum GameShopMode
{
    GAME_SHOP_BUY,
    GAME_SHOP_SELL
};

enum GameShopResult
{
    GAME_SHOP_RESULT_NONE,
    GAME_SHOP_RESULT_SUCCESS,
    GAME_SHOP_RESULT_INSUFFICIENT_FUNDS,
    GAME_SHOP_RESULT_LOCKED,
    GAME_SHOP_RESULT_EMPTY,
    GAME_SHOP_RESULT_CANCELLED
};

enum GameInteractionKind
{
    GAME_INTERACTION_NONE = 0,
    GAME_INTERACTION_ACTOR = 1,
    GAME_INTERACTION_OBJECT = 3
};

const float GAME_INTERACTION_VERTICAL_LIMIT = 20.0f;
const float GAME_INTERACTION_OBJECT_RANGE = 10.0f;
const int GAME_INTERACTION_SLOT_COUNT = 7;

const int GAME_STAGE_OBJECT_CAPACITY = 8;
const int GAME_INTERACTION_CAPACITY =
    GAME_INTERACTION_SLOT_COUNT + GAME_STAGE_OBJECT_CAPACITY;

struct GameInteraction
{
    uint8_t kind;
    int8_t actorSlot;
    int16_t selectorId;
    int16_t npcPositionRowIndex;
    int16_t promptKind;
    Vec3 position;
    float range;
    float focusSeconds;
    uint8_t used;

    GameInteraction();
};

struct GameCombatSurfaceContact
{
    Vec3 point;
    Vec3 normal;
    uint16_t material;
};

struct GameStageNavigation
{
    bool (*groundHeight)(void* context, float x, float z, float ceiling,
                         float* height);

    bool (*groundColumn)(void* context, float x, float z, float referenceY,
                         float* height, uint16_t* material);
    bool (*groundContact)(void* context, float x, float z, float referenceY,
                          GameCombatSurfaceContact* contact);

    bool (*combatSurface)(void* context, const Vec3& centre, float radius,
                           GameCombatSurfaceContact* contact);
    void* combatSurfaceContext;

    bool (*nearestFloor)(void* context, float x, float z, float* outX,
                         float* outY, float* outZ);
    void* context;

    void* collisionContext;
    float minX;
    float maxX;
    float minZ;
    float maxZ;

    float spawnX;
    float spawnY;
    float spawnZ;
    float spawnHeadingDegrees;
    bool valid;

    GameStageNavigation();
};

const int32_t PARTICIPANT_SCORE_GUARD_HELD = 50;
const int32_t PARTICIPANT_SCORE_HIT_TAKEN = 100;
const int32_t PARTICIPANT_SCORE_INTIMIDATION = 200;

int32_t scaledParticipantBattleScore(int32_t damageDealt);

enum CombatBehaviourDecision : uint32_t
{
    COMBAT_BEHAVIOUR_HIT_BLOCK = 1u << 0,
    COMBAT_BEHAVIOUR_HIT_WINDOW = 1u << 1,
    COMBAT_BEHAVIOUR_APPLY_HIT = 1u << 2,
    COMBAT_BEHAVIOUR_QUEUE_MOVE = 1u << 3,
    COMBAT_BEHAVIOUR_SET_ACTION = 1u << 4,
    COMBAT_BEHAVIOUR_ACTION_FLAGS = 1u << 5,
    COMBAT_BEHAVIOUR_TARGET_FLAGS = 1u << 6,
    COMBAT_BEHAVIOUR_FLAGS = 1u << 7,
    COMBAT_BEHAVIOUR_TEST_TARGET = 1u << 8,
    COMBAT_BEHAVIOUR_RUN_ACTION = 1u << 9
};

int16_t resolveBattleResourceProfile(int16_t authoredCharacterId,
                                     int16_t persistentResourceId);

struct CombatMotionRuntime
{
    Vec3 offset;
    float headingDelta;
    uint8_t frames;
    float duration;
    int16_t turnBone;
    float turnDuration;
    float spacingDistance;
    float elapsed;
    float spacingDuration;
    float spacingElapsed;
};

struct CombatInputHoldRuntime
{
    uint8_t kind;
    int16_t followUpMove;
    uint8_t flags;
    uint16_t endFrame;
    uint16_t accumulatedPresses;
    uint16_t threshold;
    uint32_t previousCpuInput;
};

struct CombatCommandRuntime
{
    uint8_t bufferedCount;
    int16_t command;
    uint8_t actionCount;
    int16_t actions[MOVE_ACTION_CHAIN_CAPACITY];
    uint32_t previousPad;
    bool preserveInputFacing;
};

struct GameSlotRecord
{
    int16_t characterId;

    int16_t nativeCharacterId;
    bool actorModelPresent;
    bool actorLoadStateKnown;
    uint8_t actorLoadStateFlags;

    int16_t persistentRecordId;
    int16_t resourceProfileId;

    PersistentMoveSlot moveSlots[2];
    uint8_t moveSlotActive[2];

    uint8_t selectedEquippedSlot;
    int16_t selectedItemCommand;
    int32_t selectedWorldItemIndex;
    int16_t heldWorldWeapon;
    int16_t targetedWorldWeapon;
    int16_t ownedWorldWeapons[2];
    int16_t boundWorldWeapon;
    int16_t defaultBoundWorldWeapon;
    bool defaultWeaponAttachmentPending;
    int16_t currentHealth;
    int16_t maxHealth;
    int16_t damageGauge;
    int16_t damageGaugeMaximum;
    int16_t tiredGaugeMaximum;

    int16_t tiredGauge;
    int16_t tiredGaugeWear;
    int16_t pendingGaugeModifier;
    float combatGaugeModifiers[12];
    float attackGaugeRecoverySeconds[COMBAT_ATTACK_GAUGE_COUNT];
    float bodyGaugeRecoverySeconds[4];
    float tiredGaugeRecoverySeconds;
    int16_t auxiliaryGaugeMaximums[4];
    int16_t bodyPartHealth[4];
    int16_t bodyPartDamage[4];
    uint32_t flags;
    uint32_t motionControlFlags;
    uint32_t actorStatusFlags;

    uint32_t actorRuntimeFlags;

    uint32_t animationCueFlags;
    uint32_t actorUpdateFlags;
    uint32_t actorActionFlags;
    uint32_t actorTargetingFlags;

    uint32_t actorBehaviorFlags;

    int16_t selectedActionId;

    uint32_t poseStateWord;

    uint8_t facingRelation;
    uint8_t facingDirections;
    uint8_t facingState;
    uint8_t facingTarget;

    uint8_t combatRecoveryState;
    bool combatFacingValid;
    float actionElapsedSeconds;
    uint8_t slotIndex;
    uint8_t linkedSlotIndex;
    uint8_t playerIndex;
    uint8_t battleTeamOrder;
    uint8_t battleGroup;
    uint8_t battleSide;
    uint8_t battleActionState;
    ActorActionRuntime actorAction;
    float levelTerm;
    float dynamicAttackMultiplier;

    float attackPowerMultiplier;

    float field_0x439c;
    Vec3 position;

    float groundHeight;
    bool groundHeightValid;

    Vec3 groundNormal;
    bool groundNormalValid;

    uint8_t groundSurfaceKind;
    bool groundSurfaceKindValid;

    int16_t combatSurfaceId;
    GameCombatSurfaceContact combatSurfaceContact;
    bool combatSurfaceValid;
    float headingDegrees;
    CombatMotionRuntime combatMotion;
    CombatReaction reaction;
    uint16_t reactionAction;

    int16_t npcPositionRowIndex;
    float interactionRange;
    float interactionElapsed;
    Vec3 velocity;

    float playingAnimationFrame;
    float animationTimeScale;

    int32_t participantScore;

    int32_t guardCount;
    int32_t hitsTakenCount;
    CombatAction action;

    int16_t locomotionMoveId;
    CombatLocomotionRuntime locomotion;
    uint8_t comboStep;
    CombatCommandRuntime combatCommand;
    CpuCombatInputState cpuInput;

    int16_t recoveredMoveId;
    int16_t queuedMoveId;
    MoveMotion scheduledMotion;
    bool scheduledMotionValid;

    int16_t appliedMotionId;

    bool scheduledMotionComplete;
    uint32_t scheduledMotionGeneration;
    bool combatPosePublished;

    uint32_t behaviourDecisionMask;

    uint32_t behaviourDecisionHistoryMask;

    int8_t stateHandler;
    uint8_t statePhase;
    int32_t stateWork[2];
    CombatInputHoldRuntime inputHold;

    float combatRecoverySeconds;
    uint8_t combatRecoveryAttack;
    uint8_t combatRecoveryOther;

    int16_t spinPhase;
    float spinPresses;

    uint8_t activeHitBlock;
    uint16_t hitGeneration;

    int16_t hitWindowStartFrame;
    int16_t hitWindowEndFrame;
    uint32_t hitChannelMask;
    float hitWindowRadius;
    float contactRadiusScale;
    uint32_t combatBoneMask;
    CombatBoneTransform combatBones[COMBAT_BONE_COUNT];

    int16_t lastHitAttackerSlot;
    int16_t lastHitMoveId;
    uint16_t lastHitGeneration;
    uint8_t lastHitBlock;

    int16_t receivedDamage;
    CombatReceivedHitContext receivedContext;
    CombatReceivedContact receivedContact;
    bool pendingReceivedDamage;
    bool receivedDamageTaskActive;

    int16_t combatAnimationPressAccumulator;
    int16_t receivedPrideDamage;
    int16_t receivedContactIndex;
    int8_t receivedContactBone;
    uint8_t receivedChannel;
    uint16_t receivedFlagsA;
    uint16_t receivedFlagsB;
    uint32_t receivedFlagsC;
    uint8_t receivedReactionSet;
    bool receivedDirectionFlag;
    int16_t attackGauges[COMBAT_ATTACK_GAUGE_COUNT];
    int16_t attackGaugeMaximums[COMBAT_ATTACK_GAUGE_COUNT];
    int16_t attackGaugeDamage[COMBAT_ATTACK_GAUGE_COUNT];
    uint16_t attackGaugeHits[COMBAT_ATTACK_GAUGE_COUNT];

    uint32_t impactSelectorState;
    bool impactSelectorStateValid;
    int16_t prideGauge;
    int16_t maxPrideGauge;
};

const int GAME_STAGE_ACTOR_CAPACITY = 7;
const int GAME_STAGE_ACTOR_OVERFLOW_RECORD_COUNT = 10;

enum StageActorGeneratorState : uint8_t
{
    STAGE_ACTOR_GENERATOR_NONE,
    STAGE_ACTOR_GENERATOR_ALIVE,
    STAGE_ACTOR_GENERATOR_ELIGIBLE
};

struct StageActorRuntime
{
    GameSlotRecord slot;
    uint16_t authoredCharacterId;
    int16_t secondaryCharacterId;
    int16_t defaultMotionId;
    int16_t alternateMotionId;
    int16_t interactionSelector;
    int16_t activeMotionId;
    int16_t sourceRowIndex;
    uint8_t rowKind;
    uint8_t generatorState;
    bool occupied;
    bool visible;
    bool battleAdmitted;
    bool battleActive;
    bool primaryEncounter;
};

void setGameSlotSelectedAction(GameSlotRecord& slot, int selectedActionId,
                               bool restartStateTask = false);

bool queueGameSlotCommandFollowUp(GameSlotRecord& slot, int commandIndex);

Vec3 resolveCombatCameraSubjectPosition(
    const GameSlotRecord& slot, const Vec3& renderedRootPosition);
enum GameSoundEventKind
{
    GAME_SOUND_EVENT_SPATIAL,
    GAME_SOUND_EVENT_FOOTSTEP,
    GAME_SOUND_EVENT_EVENT_CUE
};

struct GameSoundEvent
{
    GameSoundEventKind kind;
    uint8_t bank;
    uint8_t cue;
    uint16_t surface;
    uint32_t actorFlags;
    uint32_t footstepRotationCounter;
    bool alternate;
    Vec3 position;
};

const int GAME_SOUND_EVENT_CAPACITY = 32;

enum GameEstPlacement
{
    GAME_EST_BONE_Y_ANGLE,
    GAME_EST_DIRECTION_BASIS,
    GAME_EST_ACTOR_POSITION
};

struct GameEstEvent
{
    uint8_t recordIndex;
    uint8_t actorSlot;
    uint8_t boneIndex;
    uint8_t placement;
    int16_t angleBase;
    bool addAnimationFrame;
    int16_t tuningA;
    int16_t tuningB;
    int16_t rawClassifierId;
    uint16_t flagsB;
    Vec3 direction;
    Vec3 position;
    bool periodic;
    uint8_t emitterMode;
    float intervalSeconds;
    float emitterLifetimeSeconds;
};

const int GAME_EST_EVENT_CAPACITY = 64;

uint8_t combatImpactBone(uint16_t flagsB, uint32_t selectorState,
                         bool selectorStateValid);
uint8_t combatPrimaryEffectClass(int16_t rawClassifierId);
uint8_t combatSecondaryEffectClass(int16_t rawClassifierId);
int combatImpactEffectRecords(int16_t rawClassifierId, uint8_t* records,
                              int capacity);
int combatPrimaryEffectRecords(int16_t rawClassifierId, uint8_t* records,
                               int capacity);
int combatSecondaryEffectRecords(int16_t rawClassifierId, uint8_t* records,
                                 int capacity);

struct GameRuntimeState
{
    uint16_t characterId;
    uint16_t stageId;
    uint32_t runtimeFlags;
    GameOptions options;
    StoryState story;
    GameSlotRecord player;
    BattleHudRadarState hudRadar;
};

uint8_t classifyCombatDamage(const CombatDamageRecord& damage);
void reduceGameSlotHealth(GameSlotRecord& slot, int16_t amount);
void applyHealthFloor(GameSlotRecord& slot, int16_t amount);
float getGameSlotHealthRatio(const GameSlotRecord& slot);
void applyCombatHealthDamage(GameSlotRecord& target, int16_t amount);
bool combatPhysicalStateAcceptsHit(const GameSlotRecord& attacker,
                                  const GameSlotRecord& target,
                                  const MoveHitBlock& block);

class Game
{
public:
    Game();
    ~Game();

    void initialize(void);
    void update(float deltaSeconds, uint32_t heldButtons, uint32_t pressedButtons);
    void update(float deltaSeconds,
                uint32_t firstHeldButtons, uint32_t firstPressedButtons,
                uint32_t secondHeldButtons, uint32_t secondPressedButtons);

    void beginSceneInspection(void);
    void setPlayerMovementAxes(float x, float z);
    void setMovementCameraPosition(float x, float y, float z);

    void setMovementCameraAxes(float rightX, float rightZ,
                               float forwardX, float forwardZ);

    const char* pendingSceneEntry(void) const;
    void completePendingScene(void);

    void completeStageBriefingPresentation(void);

    void setPendingBattleSpawn(float x, float y, float z,
                               float headingDegrees);
    bool openShop(int category = -1);
    void closeShop(void);
    bool shopOpen(void) const;
    GameShopMode shopMode(void) const;
    void setShopMode(GameShopMode mode);
    int shopCategory(void) const;
    void setShopCategory(int category);
    int shopSelection(void) const;
    void setShopSelection(int index);
    int shopItemCount(void) const;
    uint32_t shopItemId(int index) const;
    int32_t shopItemPrice(int index) const;
    uint16_t shopItemInventoryCount(int index) const;
    bool shopItemLocked(int index) const;
    GameShopResult shopResult(void) const;
    GameShopResult buyShopSelection(void);
    GameShopResult sellShopSelection(void);
    void openAdvice(void);
    bool advicePending(void) const;
    const char* adviceTitle(void) const;
    const char* adviceBody(void) const;
    void dismissAdvice(void);
    void setPlayerLookAxes(float x, float z);
    void setControllerConnected(int playerIndex, bool connected);
    void setSecondPlayerMovementAxes(float x, float z);
    void setSecondPlayerLookAxes(float x, float z);
    void requestRumble(int playerIndex, float durationSeconds,
                       float lowMotor, float highMotor, uint8_t priority);
    void cancelRumble(int playerIndex);
    const GameRumbleState& rumbleState(int playerIndex) const;

    bool save(const char* path);

    bool loadSecondaryProfile(const char* path);
    void clearSecondaryProfile(void);
    bool hasSecondaryProfile(void) const;
    const PersistentGameRecord* secondaryCharacterRecord(
        uint16_t characterId) const;
    bool load(const char* path);
    void setSaveAvailable(bool available);
    bool loadGameData(const char* archiveDirectory);
    void setArenaRadius(float radius);
    void setStageNavigation(const GameStageNavigation& navigation);
    void clearStageNavigation(void);
    void setCombatItemScene(FstageScene* scene);
    void setCombatWeaponCallbacks(const GameCombatWeaponCallbacks& callbacks);
    void setBattleResourceCallbacks(const BattleResourceCallbacks& callbacks);
    int battleMotionResource(int motionId, int* positionInBundle = nullptr) const;
    void refreshCombatWeaponAttachments();
    bool purchaseInventoryItem(uint32_t itemId);
    bool sellInventoryItem(uint32_t itemId);
    uint16_t inventoryCount(uint16_t itemId) const;
    const InventoryUseState& inventoryUseState(void) const;
    bool beginInventoryAction(uint16_t packedEntryIndex);
    bool moveInventoryActionCursor(int direction);
    bool confirmInventoryAction(void);
    bool cancelInventoryAction(void);
    bool inventoryActionRowEnabled(int row) const;
    uint8_t applyInventoryItemEffect(uint16_t itemId,
                                     uint16_t targetCharacterId,
                                     uint32_t randomValue);
    uint8_t applyInventoryItemEffect(const ItemUseEffect& effect,
                                     uint16_t targetCharacterId,
                                     uint32_t randomValue);
    bool temporarySlowConditionActive(void) const;
    float temporarySlowConditionElapsed(void) const;
    uint8_t temporarySlowConditionMode(void) const;
    void setInventoryConditionBlockers(bool trackedOpponent,
                                       bool gameStateBitOne);
    void advanceTemporarySlowCondition(float elapsedSeconds);
    bool startEventScript(const char* archivePath, const char* entryName,
                          uint16_t pageId = 0);
    bool startStoryEvent(uint16_t scriptId, uint16_t record = 0);
    void stopEventScript(void);

    void installStageSceneSequenceCaption(void);
    void installMemberChangeSceneSequenceCaption(void);
    void startSceneSequence(int sequenceId, int firstSlot, int secondSlot);
    bool setSceneEventSoundChannelActive(int channelIndex, bool active);
    void stopSceneSequence(void);
    const SceneSequencer& sceneSequence(void) const;
    int videoModeSelection(void) const;
    uint8_t videoTimingMode(void) const;
    int nominalFrameRateHz(void) const;
    float halfFrameSeconds(void) const;

    GameScreen screen(void) const;
    const CreditsSequenceState& creditsSequence(void) const;
    int resultSelection(void) const;
    const BattleResultController& battleResultController(void) const;
    bool resultSelectorVisible(void) const;
    bool resultConfirmationActive(void) const;
    static uint8_t battleResultTerminalCode(uint8_t commandIndex);
    bool configureBattleResultTransition(uint8_t transitionType,
                                         uint8_t commandIndex);
    void configureBattleResultActors(uint8_t firstActorIndex,
                                     uint8_t secondActorIndex);
    bool configureBattleResultSide(int side, uint8_t activeCount,
                                   int8_t threshold);
    void setBattleResultFlowReady(bool ready);
    bool setBattleResultSoundPending(int channel, bool pending);
    bool pollEventMessageVoice(int32_t* trackIndex);
    void setBattleRewardSuppressed(bool suppressed);

    void dispatchBattleExperienceRewards(void);
    GameMode mode(void) const;

    bool matchSetupFlag(void) const;
    uint8_t flowState(void) const;
    uint16_t selectedCharacterId(void) const;
    void setCharacterSelection(int index);
    void setStageSelection(int index);
    static int versusStageCount(void);
    static uint16_t versusStageId(int index);
    static int versusStageIndex(uint16_t stageId);
    static int versusCharacterCount(void);
    static uint16_t versusCharacterId(int index);
    static int versusCharacterIndex(uint16_t characterId);
    VersusBattleType versusBattleType(void) const;
    int versusBattleTypeSelection(void) const;
    bool versusBattleTypeItemEnabled(int index) const;
    int versusSelectingSide(void) const;
    int versusCharacterSelection(int side) const;
    uint16_t versusSelectedCharacterId(int side) const;
    bool versusCharacterItemEnabled(int side, int index) const;
    int versusHumanSide(void) const;
    bool versusSideActive(int side) const;
    bool versusSideReady(int side) const;
    int versusSideController(int side) const;
    uint16_t versusCharacterDisplayNameTextId(int index) const;
    const char* versusCharacterDisplayName(int index) const;
    const char* versusCharacterStyleName(int side) const;
    const StoryCharacterSelectPresentation&
        storyCharacterSelectPresentation(void) const;
    void setStoryCharacterSelectFlowReady(bool ready);
    bool storyCharacterSelectFlowReady(void) const;
    const IntroCardPresentation& introCardPresentation(void) const;
    uint16_t introCardTextId(void) const;
    bool introCardSequenceActive(void) const;
    const TitleAttractState& titleAttractState(void) const;
    TitleAttractMode titleAttractMode(void) const;
    int titleAttractMovieIndex(void) const;
    void completePreTitleMoviePlayback(void);
    int languageSelection(void) const;
    int mainMenuSelection(void) const;
    int characterSelection(void) const;
    int stageSelection(void) const;
    uint16_t selectedStageId(void) const;
    int pauseSelection(void) const;
    bool pauseMoveListActive(void) const;
    int pauseMoveListFirstVisible(void) const;
    int pauseMoveListStyle(void) const;
    int pauseMoveListVisibleEntries(uint8_t* output, int capacity) const;
    int inventorySelection(void) const;
    void setInventoryEquipmentPresentationReady(bool ready);
    bool inventoryEquipmentPresentationReady(void) const;
    int optionsSelection(void) const;
    const GameOptions& options(void) const;
    GameOptionsPage optionsPage(void) const;
    int optionsItemCount(void) const;

    bool optionsOpenedFromPause(void) const;
    bool optionsItemEnabled(int index) const;
    bool optionsItemVisible(int index) const;
    int optionValue(const OptionEntry& entry) const;
    bool optionsDefaultConfirmationActive(void) const;
    int optionsDefaultConfirmationSelection(void) const;
    uint16_t optionsMovieSceneId(void) const;
    uint16_t optionsMovieCharacterSceneId(void) const;
    bool optionsVibrationPreview(int playerIndex) const;
    bool saveAvailable(void) const;
    bool controllerConnected(int playerIndex) const;
    bool mainMenuItemEnabled(int index) const;
    bool pauseMenuItemEnabled(int index) const;
    bool inventoryItemEnabled(int index) const;
    const char* localizedText(uint16_t textId,
                              int16_t* byteLength = NULL) const;
    bool loadTransientText(GameTextTable table, bool owned);
    const GameData& data(void) const;
    const EventVm& eventScript(void) const;
    EventCallFunctionId pendingEventCall(void) const;
    const GameRuntimeState& runtimeState(void) const;
    GameRuntimeState& runtimeState(void);
    const GameSaveImage& gameSaveImage(void) const;
    GameSaveImage& gameSaveImage(void);
    const PersistentGameRecord* characterRecord(uint16_t characterId) const;
    PersistentGameRecord* characterRecord(uint16_t characterId);

    void initializePersistentRecord(PersistentGameRecord& record,
                                    uint16_t characterId);
    int16_t activeRecordId(size_t index) const;
    bool setActiveRecordId(size_t index, int16_t characterId);
    const OwnedInventoryBlock& ownedInventory(void) const;
    bool removeInventoryQuantity(uint16_t itemId, uint8_t variant,
                                 uint16_t quantity);
    const InventoryActionStateBlock& inventoryActionState(void) const;
    int16_t moveInventoryCount(size_t index) const;
    bool eventFlag(size_t index) const;
    void setEventFlag(size_t index, bool enabled);
    uint8_t scenarioState(size_t index) const;
    uint8_t scenarioCompletion(size_t index) const;
    bool setScenarioState(size_t index, uint8_t value);
    bool setScenarioCompletion(size_t index, uint8_t value);
    uint32_t indexedPersistentValue(size_t index) const;
    bool setIndexedPersistentValue(size_t index, uint32_t value);

    uint32_t nextRandom(void);

    uint32_t currentTick(void) const;
    uint8_t characterCallCooldown(size_t characterId) const;
    bool setCharacterCallCooldown(size_t characterId, uint8_t value);
    bool characterCustomization(uint16_t characterId,
                                CharacterCustomization& customization) const;
    bool commitCharacterCustomization(
        uint16_t characterId, const CharacterCustomization& customization);
    bool projectPersistentRecordToGameSlot(uint16_t characterId,
                                           GameSlotRecord& slot) const;
    bool projectGameSlotCombatToPersistentRecord(const GameSlotRecord& slot);
    const GameSlotRecord& opponent(void) const;
    int interactionFocusSlot(void) const;
    int interactionFocus(void) const;
    const GameInteraction* focusedInteraction(void) const;
    int16_t lastInteractionSelector(void) const;
    void setStageInteractions(const GameInteraction* items, int count);
    void clearStageInteractions(void);
    void setFstageNpcPositionRows(
        const std::vector<FstageNpcPositionRow>& rows);
    void setFstageNpcPositionRows(
        const std::vector<FstageNpcPositionRow>& rows,
        uint16_t liveStageId);
    int stageActorCount(void) const;
    int fstageNpcPositionRowCount(void) const;
    const FstageNpcPositionRow* fstageNpcPositionRow(int index) const;
    StageActorRuntime* stageActor(int index);
    const StageActorRuntime* stageActor(int index) const;
    StageActorRuntime* findStageActor(uint16_t authoredCharacterId);
    const StageActorRuntime* findStageActor(
        uint16_t authoredCharacterId) const;
    bool setStageActorSecondaryIdentity(int index, int16_t persistentRecordId);
    const PersistentGameRecord* stageActorPersistentRecord(int index) const;
    PersistentGameRecord* stageActorPersistentRecord(int index);
    int stageResourcePreloadCount(void) const;
    int16_t stageResourcePreload(int index) const;
    bool startStageActorEncounter(
        uint16_t characterId, uint16_t eventId, bool plural);
    const StageAiAsyncContext* stageAiContext(int playerIndex) const;
    void initializeStageAiBattleContexts(uint8_t battleMode = 0,
                                         bool forceProfile44Matchup = false);
    void teardownStageAiBattleContexts(void);
    void updateStageAiGenerators(float deltaSeconds);
    bool stageAiGeneratorsPending(void) const;
    StoryProgressionPhase storyProgressionPhase(void) const;
    void setStoryProgressionPhase(StoryProgressionPhase phase);
    uint16_t storyPromptPhase(void) const;
    void setStoryPromptPhase(uint16_t phase);

    void requestFstageMarkerTransition(uint16_t destinationStageId,
                                       int16_t arrivalSelector);
    bool storyStageLoadBoundaryPending(void) const;
    uint8_t storyStageLoadState(void) const;
    bool pollStoryStageLoadBoundary(bool resourceReady);
    bool pollSoundEvent(GameSoundEvent* event);
    bool pollEstEvent(GameEstEvent* event);
    void setEffectSuppressionMask(uint32_t mask);
    uint32_t effectSuppressionMask(void) const;
    bool consumeCombatDamageCameraSettle(float* damage);
    bool consumeCombatMoveCameraSettle(float* closeUpDurationFrames);
    WeaponManager& weapons(void);
    const CamdatDatabase& camdat(void) const;
    bool consumeCamdatCameraRequest(int* recordIndex, bool* exclusive,
                                    uint8_t* actorSlot);
    void applyCamdatActorActionFlag(uint8_t actorSlot, bool enabled);
    const WeaponManager& weapons(void) const;
    bool loadWeaponDataFromXaf(const char* archivePath,
                               const char* entryName);
    bool weaponDataLoaded(void) const;
    static void updateShopFlow(UpdateTask* task);
    const char* weaponDiagnostic(void) const;
    BattleOutcome battleOutcome(void) const;
    const BattleControllerRuntime& battleControllerState(void) const;
    const BattleEndRuntime& battleEndState(void) const;
    const BattleEndNoticeRuntime& battleEndNotice(void) const;

    void setCamdatTimeScale(float value);
    float timeScale(void) const;

    int battleFeedbackMode(void) const;
    float battleTime(void) const;
    bool stageAiLoaded(void) const;
    const char* stageAiDiagnostic(void) const;

    float scaleFrameDelta(float rawSeconds) const;
    int opponentCount(void) const;
    const GameSlotRecord& opponent(int index) const;

    const GameSlotRecord& boundTargetOf(const GameSlotRecord& actor) const;

    const GameSlotRecord* allyEngagedTarget(void) const;

    bool hasAlly(void) const;
    const GameSlotRecord& allySlot(void) const;

    int battleExperienceAwarded(void) const;
    int battleLevelsGained(void) const;
    int combatInputCommand(void) const;
    int combatInputActionIndex(void) const;
    int combatInputMoveId(void) const;
    int combatInputHitWindows(MoveHitWindow* windows, int capacity) const;
    int combatMoveForAction(const GameSlotRecord& actor,
                            int actionIndex) const;

    void setCombatModelState(uint8_t slotIndex, bool modelPresent,
                              uint8_t loadStateFlags);
    void setCombatPose(uint8_t slotIndex, float animationFrame,
                       bool animationComplete,
                       const CombatBoneTransform* transforms, int count);
    bool combatMotion(uint8_t slotIndex, MoveMotion* motion) const;
    bool pollCombatContact(GameCombatContact* contact);

    bool queueCombatAction(GameSlotRecord& attacker, GameSlotRecord& target,
                           uint8_t channel, int contactIndex);
    bool directCombatHit(GameSlotRecord& attacker, GameSlotRecord& target);
    uint32_t hitDirection(const GameSlotRecord& victim,
                          const GameSlotRecord& attacker) const;

    int16_t wieldedDamageBonus(const GameSlotRecord& attacker) const;
    bool guardStopsHit(const GameSlotRecord& target,
                       const GameSlotRecord& attacker) const;

    int16_t seededSlotPride(const GameSlotRecord& slot) const;

    int16_t initialiseSlotPride(GameSlotRecord& slot, CharacterId characterId);

    bool battleExitEligible(void) const;

    static int battleExperienceForOpponent(int opponentLevel, int playerLevel,
                                           int comboBonus);
    static int battleInteractionExperienceForOpponent(
        int opponentLevel, int playerLevel, int comboBonus);

    static int battleExperienceReward(const int* opponentLevels,
                                      int8_t* scoreMultipliers,
                                      int opponentCount, int playerLevel,
                                      int comboBonus, int supplementalReward,
                                      bool interactionScore);
    int scoreBattleInteractionKnockout(int opponentLevel);
    bool queueBattleScoreMultiplier(float multiplier);
    void resetBattleScoreMultipliers(void);
    int battleScoreMultiplier(int index) const;
    int battleRewardOpponentId(int index) const;
    int battleSupplementalExperienceAwarded(void) const;

    static int playerMaxHealthForProgression(int defaultMaximum,
                                             int growth,
                                             int level,
                                             int maximumBonus);
    static int companionMaxHealthForProgression(int defaultMaximum,
                                                int defaultLevel,
                                                int level,
                                                int maximumBonus);
    int recalculatePlayerMaxHealth(void);
    int recalculatePartyMaxHealth(uint16_t characterId);

    int applyRobberyCashAward(uint16_t victimCharacterId,
                              uint32_t randomValue);
    bool robberyPresentationPending(void) const;
    int robberyPresentationAmount(void) const;
    bool robberyLargeAmountSideEffectPending(void) const;
    void clearRobberyPresentation(void);

    int applyBattleExperienceTo(PersistentGameRecord* record, int reward);
    int applyBattleExperience(int reward);

    int applyPartyBattleExperience(uint16_t characterId, int reward);

    int partyLevel(uint16_t characterId) const;
    int partyExperience(uint16_t characterId) const;

    int slotCharacterLevel(const GameSlotRecord& slot) const;
    const CombatAiDecision& stageAiDecision(void) const;

    static const char* screenName(GameScreen screen);
    static const char* characterName(int index);

    static uint16_t playableCharacterId(int index);
    static const char* combatActionName(CombatAction action);

private:
    friend struct GameCombatPairTestAccess;
    friend struct GameBattleControllerTestAccess;
    friend struct GameCombatStateTestAccess;
    friend struct GameBattleRosterTestAccess;
    friend struct GamePhysicalStateTestAccess;
    friend struct GameCombatRecoveryTestAccess;
    friend struct GameCombatActionsTestAccess;
    friend struct GameVersusControlsTestAccess;
    friend struct GameCombatLinksTestAccess;
    friend struct GameCombatGetUpTestAccess;
    friend struct GameCombatReceivedDamageTestAccess;
    friend struct GameCombatMeleeReceivedTestAccess;
    friend struct GameCombatProjectileTestAccess;
    friend struct GameCombatWeaponTestAccess;
    friend struct GameActorStateTestAccess;
    friend struct GameCombatItemsTestAccess;
    friend struct GameCombatMotionTestAccess;
    friend struct GameCombatChainTestAccess;
    friend struct GameCombatContextTestAccess;

    static void updateGameFlowTask(UpdateTask* task);
    static void updateBootLanguageSelection(UpdateTask* task);
    static void updateVideoModeSelection(UpdateTask* task);
    static void updateNoSaveDataFlow(UpdateTask* task);
    static void updateAdviceFlow(UpdateTask* task);
    static void updateTitleScreenFlow(UpdateTask* task);
    static void updateVersusMatchSetupFlow(UpdateTask* task);
    static void handlePreGameplayTransition(UpdateTask* task);
    static void updateNewGameCharacterSelectFlow(UpdateTask* task);
    void beginIntroCardSequence(void);
    void updateIntroCardSequence(void);
    void completeIntroCardSequence(void);
    static void updateStageSelectionFlow(UpdateTask* task);
    static void updateStageBriefingFlow(UpdateTask* task);
    static void updateCreditsFlow(UpdateTask* task);
    static void updateGameplaySessionFlow(UpdateTask* task);
    static void updatePauseFlow(UpdateTask* task);
    static EventVmHostResult handleEventCommand(void* userData,
                                                const EventVmCommand& command);
    static EventVmHostResult handleEventCallFunction(
        void* userData, EventCallFunctionId function);
    static EventVmHostResult handleMessageCommand(
        void* userData, const EventVmCommand& command);
    static EventVmHostResult handleMessageCallFunction(
        void* userData, EventCallFunctionId function);
    static uint16_t handleEventInventoryCount(
        void* userData, uint16_t itemId, int variant);
    static bool handleEventInventoryAdd(
        void* userData, uint16_t itemId, uint16_t variant,
        uint16_t quantity);
    static bool handleEventInventoryRemove(
        void* userData, uint16_t itemId, uint16_t variant,
        uint16_t quantity);
    bool startEventMessage(uint16_t scriptId);
    uint32_t eventInputButtons(uint32_t buttons) const;
    void synchronizeEventState(const EventVmCommand* command);
    static void updateOptionsMenuTask(UpdateTask* task);

    static void updateInventoryFlow(UpdateTask* task);
    void rebuildShopItems(void);
    void switchGameFlowTaskHandler(UpdateTaskCallback* callback, uint8_t initialState);
    void enterTitleScreenFlow(void);
    void beginNewGame(void);
    void beginStageBriefing(void);
    void initializeBattle(bool preserveFieldEquipment = false);
    void updateStoryRuntime(float deltaSeconds);
    bool updateStoryStageTransition(float deltaSeconds);
    void initializeLoadedStoryField(void);
    void applyPendingStoryHeals(void);
    StoryProgressionInput storyProgressionInput(void) const;
    void updateBattle(float deltaSeconds);
    void updateCombatDistanceFields(void);
    void updateCombatFacingFields(void);
    void prepareCombatInputPolicy(GameSlotRecord& slot);
    int rewriteCombatInputCommand(GameSlotRecord& slot, int command);
    uint32_t combatHistoryCode(const GameSlotRecord& slot, uint32_t pad,
                               float worldX, float worldZ) const;
    void updateCombatMoveRuntime(GameSlotRecord& slot);
    void setCombatActorMotion(GameSlotRecord& slot, int moveId);
    static void moveTimeScaleCallback(void* userData, float rate);
    void updateCombatMotionRuntime(void);
    void resolveCombatMovementRequests(void);
    bool setCombatMovementTarget(GameSlotRecord& slot,
                                  const GameSlotRecord& target);
    bool setCombatMovementTarget(GameSlotRecord& slot,
        const GameSlotRecord& target, const Vec3& offset, float heading, int frames);
    bool setCombatPositionTarget(GameSlotRecord& slot);
    void updateAuthoredCombatMotion(GameSlotRecord& slot);
    void updateCombatMovement(GameSlotRecord& slot);
    void updateCombatInteractions(void);
    void resetCombatRuntime(void);
    void resetBehaviourPresentation(void);
    void drainCombatActionPairs(void);
    void queueCombatPair(GameSlotRecord& actor, int selfMove, int linkedMove);
    void updateCombatStatePhases(GameSlotRecord& slot, bool entered);
    void updateCombatLandingDamage(GameSlotRecord& slot);
    void updateCombatSpin(GameSlotRecord& slot);
    void updateCombatGetUp(GameSlotRecord& slot, bool entered);
    bool tryCombatGetUpWallMotion(GameSlotRecord& slot);
    bool tryCombatGetUpSurfacePair(GameSlotRecord& attacker,
                                    GameSlotRecord& victim);
    static void behaviourContactMovementCallback(void* userData,
                                                  int16_t offset,
                                                  int16_t heading,
                                                  int16_t frames);
    static void behaviourStatusFlagsCallback(void* userData,
                                              uint32_t mask, bool set);
    void initializeCombatRecovery(GameSlotRecord& slot);
    void updateCombatRecovery(GameSlotRecord& slot);
    int selectCombatRecoveryMove(const GameSlotRecord& slot) const;
    uint32_t combatRecoveryHeldInput(const GameSlotRecord& slot) const;
    void updateCombatItemUse(GameSlotRecord& slot, bool entered);
    int selectCombatWorldItem(GameSlotRecord& slot);
    int combatWorldItemCommand(const GameSlotRecord& slot) const;
    void updateCombatWorldItemPickup(GameSlotRecord& slot);
    uint8_t applyCombatInventoryItemEffect(GameSlotRecord& slot,
                                           int kind, int selector);
    uint8_t applyInventoryItemEffect(const ItemUseEffect& effect,
                                     uint16_t targetCharacterId,
                                     uint32_t randomValue,
                                     GameSlotRecord* live,
                                     bool persistHealth, bool persistPride);

    void resetBattleResultController(void);
    void updateBattleResultController(void);
    void startBattleResultSelector(void);
    void captureBattleRewardRoster(void);
    void scoreDefeatedBattleOpponents(void);
    int battleRewardCharacterLevel(int16_t characterId) const;
    void cleanupBattleResultSession(void);
    void reloadBattleForRetry(void);
    bool applyActorActionCommand(const EventVmCommand& command);
    void advanceActorActions(float deltaSeconds);
    void updatePlayerMovement(float deltaSeconds);
    void updateHumanCombatantInput(GameSlotRecord& actor, MoveInputHistory& history,
        uint32_t held, uint32_t pressed, float leftX, float leftZ,
        float rightX, float rightZ, float deltaSeconds);
    int updateCombatInput(GameSlotRecord& actor, MoveInputHistory& history,
        uint32_t pad, float worldX, float worldZ, float deltaSeconds,
        bool buttonPressed = false);
    int dispatchCombatInput(GameSlotRecord& actor, int command);
    bool combatGuardAllowed(const GameSlotRecord& actor) const;
    void updateCombatGuard(GameSlotRecord& actor, bool crouched);
    bool combatantActive(const GameSlotRecord& actor) const;
    int combatMovementContext(const GameSlotRecord& actor) const;
    void updateCombatLocomotion(GameSlotRecord& actor, uint32_t pad,
        float worldX, float worldZ, float magnitude);
    void updateHumanOpponentMovement(float deltaSeconds);
    void cameraRelativeAxes(float localX, float localZ,
                            float& worldX, float& worldZ) const;
    void requestScene(const char* entryName);
    void updateInteraction(void);
    void updateFstageInteraction(void);
    void bindNpcPositionRow(GameSlotRecord& slot);
    void enqueueSoundEvent(const GameSoundEvent& event);
    void enqueueEstEvent(const GameEstEvent& event);
    void enqueueCombatEstEvents(const GameSlotRecord& attacker,
                                const GameSlotRecord& victim,
                                const MoveHitBlock& block,
                                int16_t damage);
    void enqueueCombatContact(const GameCombatContact& contact);
    static void behaviourHitBlockCallback(void* userData, int blockIndex);
    static void behaviourHitWindowCallback(void* userData,
                                           int16_t centreFrame,
                                           int16_t framesBefore,
                                           int16_t framesAfter,
                                           uint32_t channelMask);
    static void behaviourApplyHitCallback(void* userData, int finisher);
    static void behaviourQueueMoveCallback(void* userData, int moveId);
    static void behaviourSetActionCallback(void* userData, int action);
    static void behaviourActionFlagsCallback(void* userData, uint32_t mask,
                                             bool set);
    static void behaviourCueFlagsCallback(void* userData, uint32_t mask,
                                          bool set);
    static void behaviourTargetFlagsCallback(void* userData, uint32_t mask,
                                             bool set);
    static void behaviourSetFlagsCallback(void* userData, uint32_t value);
    static void behaviourImpactStateCallback(void* userData, uint32_t value);
    static void behaviourRecoveryStateCallback(void* userData, uint32_t value);
    static void behaviourMovementCallback(void* userData, int16_t x,
                                          int16_t y, int16_t z,
                                          int16_t heading, uint8_t frames);
    static void behaviourTurnDurationCallback(void* userData, int frames);
    static void behaviourSpacingCorrectionCallback(void* userData, int frames);
    static void behaviourInputHoldCallback(void* userData, int opcode,
                                           int16_t value);
    static bool behaviourTestTargetFlagsCallback(void* userData,
                                                 uint32_t mask,
                                                 bool expectSet);
    static void behaviourRunActionCallback(void* userData);
    static void behaviourEffectCallback(void* userData,
                                        int16_t recordIndex,
                                        int16_t boneIndex,
                                        int16_t angleBase,
                                        int16_t tuningA,
                                        int16_t tuningB);
    static void behaviourSoundCallback(void* userData, int kind,
                                       int first, int second);
    static void behaviourCameraSettleCallback(
        void* userData, int closeUpDurationFrames);
    static void behaviourCameraCallback(void* userData,
                                        int highDescriptor,
                                        int middleDescriptor,
                                        int lowDescriptor,
                                        bool exclusive);
    static void movePlayMotionCallback(void* userData, int motionId,
                                       int blendFrames,
                                       int maxDurationFrames,
                                       int transitionFrames);
    static int moveCallCallback(void* userData, int argument);
    static void moveSetActionCallback(void* userData, int moveId);
    static bool moveComboGateCallback(void* userData, int argument);
    static void moveComboArmCallback(void* userData);
    static uint32_t moveRandomCallback(void* userData);
    void hydrateStoryCharacters(void);
    void initializePersistentRecords(void);
    bool applyBattlePersistentStats(GameSlotRecord& slot,
                                    uint16_t authoredCharacterId,
                                    uint16_t persistentRecordId,
                                    bool activeParty) const;
    void projectPersistentStateToRuntime(void);
    void projectRuntimeStateToPersistent(GameSaveImage& image) const;
    void refreshCurrentSummary(GameSaveImage& image) const;
    void synchronizeBattleStoryState(void);

    void runCombatStateTask(GameSlotRecord& slot);
    void applyPendingCombatReceivedDamage(GameSlotRecord& slot);
    void recordCombatProjectileHit(GameSlotRecord& victim,
                                   const CombatReceivedHitContext& context,
                                   int16_t damage);
    void initializeCombatProjectileGroups();
    void resetCombatProjectileState();
    void resetCombatInteractionFlags();
    void updateCombatProjectiles();
    void fireCombatProjectile(GameSlotRecord& actor);
    static void behaviourFireProjectileCallback(void* userData);
    void updateCombatReceivedDamage(GameSlotRecord& slot, bool entered);
    bool tryCombatGroundReaction(GameSlotRecord& slot);
    void publishCombatGroundNormal(GameSlotRecord& slot);
    void updateCombatSurfaceMovement(GameSlotRecord& slot);
    void accumulateCombatReactionRate(GameSlotRecord& slot);
    const uint8_t* combatTrainingLevels(const GameSlotRecord& slot) const;
    MovePersonalActionContext combatPersonalActionContext(
        const GameSlotRecord& slot) const;
    void recordCombatInputCommand(GameSlotRecord& slot, int command,
                                   bool fresh);
    int combatInputMove(GameSlotRecord& slot, int command,
                        bool facingTarget, float chargeSeconds);
    int combatWeaponInputMove(const GameSlotRecord& slot, int command,
                              bool& handled) const;
    void prepareCombatWeaponInput(GameSlotRecord& slot, int command, int move);
    int selectCombatWorldWeapon(GameSlotRecord& slot);
    int16_t ensureCombatWorldWeapon(int16_t worldId);
    FstageWeaponActorState combatWeaponActorState(const GameSlotRecord& slot) const;
    void updateCombatWeaponBehaviour(GameSlotRecord& slot, int opcode, int16_t value);
    bool forceDropCombatWeapon(GameSlotRecord& slot, bool gated);
    bool tryCombatDamageWeaponDrop(GameSlotRecord& slot);
    void resetCombatWeaponAttachments();
    void initializeCombatOwnedWeapons();
    void prepareDefaultCombatWeapon(GameSlotRecord& slot);
    int combatOwnedWeaponInputMove(const GameSlotRecord& slot, int command) const;
    void prepareCombatOwnedWeaponInput(GameSlotRecord& slot, int command, int move);
    void returnCombatOwnedWeapon(GameSlotRecord& slot);
    void dropCombatWeaponManager(int16_t worldId);
    static void behaviourWeaponCallback(void* userData, int opcode, int16_t value);
    void setCombatReactionPair(GameSlotRecord& slot, int selfAction, int linkedAction);
    bool consumeCombatInputChain(GameSlotRecord& slot, int argument);
    int prepareCombatInputAction(
        GameSlotRecord& slot, int command, int move = -1);
    static void moveClearBehaviourWaitCallback(void* userData);

    void configureCombatInputHold(GameSlotRecord& slot, int opcode, int16_t value);
    uint32_t sampleCombatHeldInput(const GameSlotRecord& slot) const;
    uint32_t sampleCombatRecoveryInput(GameSlotRecord& slot);
    void updateCombatInputHold(GameSlotRecord& slot, uint32_t pressed);
    void startLinkedCombatantAction(GameSlotRecord& slot);
    int combatStanceCategory(const GameSlotRecord& slot) const;
    int stanceMotion(const GameSlotRecord& slot, int actionIndex) const;
    int selectCombatantStanceAction(const GameSlotRecord& slot) const;
    void updateCombatantStance(GameSlotRecord& slot);
    void applyInitialCombatantStance(GameSlotRecord& slot);
    GameSlotRecord& targetOpponent(void);
    void resetInventoryUseState(void);
    void populateInventoryTargets(void);
    void setInventoryResultPrompt(int32_t selector, bool newlySlowed,
                                  bool slowWasAlreadyActive);
    bool tryApplyTemporarySlowCondition(void);
    void clearTemporarySlowCondition(void);
    const GameSlotRecord& targetOpponent(void) const;
    void updateTargetOpponent(void);
    bool allOpponentsDefeated(void) const;
    void startAction(GameSlotRecord& slot, CombatAction action,
                     bool selectNativeAction = true);
    void updateEnemyAI(GameSlotRecord& opponent, GameSlotRecord& target,
                       float deltaSeconds);
    bool updateStageAiOpponent(GameSlotRecord& opponent,
                               GameSlotRecord& target, float deltaSeconds);
    void loadStageAiResources(void);
    void clearStageActorRuntime(void);
    void preloadStageActor(const FstageNpcPositionRow& row,
                           int rowIndex, int actorOrdinal);
    void synchronizeStageActorsFromStory(void);
    bool stageActorIsActiveRoster(uint16_t characterId) const;
    StageAiAsyncContext* mutableStageAiContext(int playerIndex);
    void startCombatMove(GameSlotRecord& slot, int moveId,
                         CombatAction presentationAction);
    int combatStyle(const GameSlotRecord& slot) const;
    float combatContactFactor(const GameSlotRecord& victim,
                              const GameSlotRecord& attacker,
                              uint8_t* reactionSet,
                              bool* directionFlag) const;
    void applyCombatReaction(GameSlotRecord& victim,
                             const GameSlotRecord& attacker,
                             const MoveHitBlock& block,
                             uint32_t direction);
    void applyGrabOutcome(GameSlotRecord& attacker, GameSlotRecord& victim,
                          uint32_t direction);
    CombatSphere combatSphere(const GameSlotRecord& slot, bool attacker,
                              int descriptor) const;
    bool combatChannelContact(const GameSlotRecord& attacker,
                              const GameSlotRecord& victim,
                              int channel, int* contactIndex) const;
    int attackGaugeForChannel(int channel) const;
    void drainAttackGauge(GameSlotRecord& attacker, int channel,
                          int damage);
    void applyPrideDamage(GameSlotRecord& victim,
                          const GameSlotRecord& attacker, int16_t amount);

    GameSlotRecord* allyTarget(void);

    const GameSlotRecord* combatSlotByIndex(uint8_t index) const;

    GameSlotRecord& boundTarget(const GameSlotRecord& actor);

    void syncCombatTargetsFromStory(void);

    void spawnBattleAlly(void);
    GameSlotRecord* mutableCombatSlotByIndex(uint8_t index);
    void separateFighters(GameSlotRecord& opponent);
    void clampFighterToArena(GameSlotRecord& slot);
    void groundFighter(GameSlotRecord& slot);
    void moveFighter(GameSlotRecord& slot, float deltaX, float deltaZ);
    bool placeFighter(GameSlotRecord& slot, float x, float z);
    void initializeBattleController(void);
    void updateBattleController(float scaledSeconds);
    BattleFlowFrameInput battleFlowInput(void) const;
    void applyBattleFlowEffects(const BattleFlowEffects& effects);
    void beginBattleOutcomePhase(int phase);
    bool queueBattleSequenceActorTarget(const GameSlotRecord& actor);
    bool prepareBattlePresentation(bool confirmationOnly = false);
    bool updateBattlePresentation(void);
    int selectBattleSequenceAction(void);
    int battleSequencePrideDamage(void);
    bool updateBattleCinematic(void);
    bool updateBattleSpecial(void);
    void queueBattleDialogue(GameSlotRecord& actor, int event);
    void repositionBattlePair(GameSlotRecord& first, GameSlotRecord& second);
    static int dispatchBattleSequence(void* userData, BattleSequenceRuntime& runtime,
                                      const BattleSequenceCommand& command);
    int consumeBattleSequenceCommand(BattleSequenceRuntime& runtime,
                                     const BattleSequenceCommand& command);
    void requestBattleCompletion(BattleOutcome outcome);
    void finishBattle(BattleOutcome outcome);
    bool shouldRunBattleFinisher(const GameSlotRecord& attacker,
                                 const GameSlotRecord& target) const;
    void startBattleFinisher(GameSlotRecord& attacker,
                             GameSlotRecord& target);
    void updateBattleFinisher(float scaledSeconds, float rawSeconds);
    void updateBattleEndSequence(float scaledSeconds);
    int selectBattleFinisherVariant(const GameSlotRecord& attacker,
                                    const GameSlotRecord& target);
    int battleRelation(const GameSlotRecord& attacker,
                       const GameSlotRecord& target) const;
    int battleFinisherMove(int variant,
                           const GameSlotRecord& attacker,
                           const GameSlotRecord& target) const;
    void updateRumble(float deltaSeconds);
    void cancelAllRumble(void);
    void beginOptionsMenu(void);
    void leaveOptionsMenu(bool commit);
    void snapshotOptionsPage(GameOptionsPage page);
    void restoreOptionsPage(GameOptionsPage page);
    void commitOptionsMenu(void);
    void resetOptionsToDefaults(GameOptionsPage page);
    void previewOptionsPage(GameOptionsPage page);
    int stagedOptionValue(const OptionEntry& entry) const;
    void setStagedOptionValue(const OptionEntry& entry, int value);
    void handleQuit(void);

    struct BehaviourCombatContext
    {
        Game* owner;
        GameSlotRecord* actor;
        uint32_t footstepRotationCounter;
    };

    std::vector<FstageNpcPositionRow> fstageNpcPositionRows;
    BehaviourStreamRunner behaviourRunners[
        GAME_COMBAT_SLOT_COUNT];
    MoveStreamRunner moveStreamRunners[GAME_COMBAT_SLOT_COUNT];
    BehaviourCombatContext behaviourContexts[
        GAME_COMBAT_SLOT_COUNT];
    int16_t behaviourMoveIds[GAME_COMBAT_SLOT_COUNT];
    CombatAction behaviourActions[GAME_COMBAT_SLOT_COUNT];
    CombatActionPairRecord combatActionPairs[GAME_COMBAT_SLOT_COUNT];
    GameSoundEvent soundEvents[GAME_SOUND_EVENT_CAPACITY];
    int soundEventHead;
    int soundEventCount;
    GameEstEvent estEvents[GAME_EST_EVENT_CAPACITY];
    int estEventHead;
    int estEventCount;
    GameCombatContact combatContacts[GAME_COMBAT_CONTACT_CAPACITY];
    int combatContactHead;
    int combatContactCount;
    uint32_t behaviourEffectSuppressionMask;
    float pendingDamageCameraSettle;

    bool combatDamageEnabled;
    CombatProjectilePool combatProjectiles;
    int32_t combatProjectileGroups[GAME_COMBAT_SLOT_COUNT];
    bool combatProjectileGroupsReady;
    float camdatTimeScale;
    float pendingMoveCameraSettle;
    CamdatDatabase camdatDatabase;
    uint16_t previousCamdatVariant;
    int pendingCamdatRecord;
    bool pendingCamdatExclusive;
    uint8_t pendingCamdatActorSlot;

    bool weaponDataAvailable;
    std::string weaponLoadDiagnostic;
    UpdateTask* gameFlowTask;
    GameScreen currentScreen;
    WeaponManager weaponManager;
    GameCombatWeaponBinding combatWeaponBindings[WEAPON_MANAGER_CAPACITY];
    GameCombatWeaponCallbacks combatWeaponCallbacks;
    BattleResourceCallbacks battleResourceCallbacks;
    BattleResourceRuntime battleResources;
    uint32_t combatMotionGenerationCounter = 0;
    std::vector<WeaponPersistentState> pendingWeaponStates;
    GameScreen optionsReturnScreen;
    GameMode currentMode;
    bool matchSetupFlagValue;
    VersusBattleType selectedVersusBattleType;
    bool stageBriefingPresentationComplete;
    TitleAttractState currentTitleAttractState;
    TitleAttractMode currentTitleAttractMode;
    float titleTransitionSeconds;
    int currentTitleAttractMovieIndex;
    bool preTitleMoviePlayed;
    bool preTitleMovieComplete;
    CreditsSequenceState currentCreditsSequence;
    GameRuntimeState persistent;
    GameSaveImage primarySaveImage;
    size_t activeSummaryIndex;
    bool primarySaveLoaded;
    bool savedFieldResumePending;
    GameData gameData;
    EventVm eventVm;
    EventVm messageVm;
    EventVm* pendingHostVm;
    bool eventWaitingForMessage;
    EventCallFunctionId pendingEventFunction;
    GameSlotRecord enemy;
    StageActorRuntime stageActors[GAME_STAGE_ACTOR_CAPACITY];
    PersistentGameRecord
        stageActorOverflowRecords[GAME_STAGE_ACTOR_OVERFLOW_RECORD_COUNT];
    StageAiAsyncContext stageAiContexts[STAGE_AI_CONTEXT_COUNT];
    MoveInputHistory stageAiInputHistories[STAGE_AI_CONTEXT_COUNT];
    int16_t stageResourcePreloads[GAME_STAGE_ACTOR_CAPACITY];
    int stageActorOccupiedCount;
    int stageResourcePreloadCountValue;
    uint16_t stageActorStageId;
    bool stageAiResourceLoaded;
    std::string stageAiDiagnosticText;
    CombatAiDecision stageAiLastDecision;
    bool stageAiDecisionValid;
    SceneSequencer sceneSequencer;
    bool sceneEventSoundChannelActive[
        SCENE_SEQUENCE_EVENT_SOUND_CHANNEL_COUNT] = {};
    GameSlotRecord* sceneSlot(int index);
    void updateSceneSequence(float sceneDeltaSeconds,
                             float scaledCaptionDeltaSeconds);
    void applySceneSequence(void);
    static bool sceneSequenceTaskFinished(void* userData, int subOp);
    static bool sceneSequenceEventSoundActive(void* userData,
                                              int channelIndex);
    static bool sceneSequencePromptFinished(void* userData);
    static int sceneSequenceActorGender(void* userData, int actorSlot);
    static bool sceneSequenceActorTransform(void* userData, int actorSlot,
                                            float* position, float* heading);
    static int sceneSequenceResolveMove(void* userData, int actorSlot,
                                        int moveId);
    float scaledFrameDeltaSeconds;
    BattleOutcome currentBattleOutcome;
    BattleControllerRuntime battleController;
    float battleTimeScale;
    int battleFeedbackModeValue;
    BattleFinisherPhase battleFinisherPhase;
    float battleFinisherElapsedSeconds;
    uint8_t battleFinisherAttackerSlot;
    uint8_t battleFinisherTargetSlot;
    BattleFinisherSelection battleFinisherSelection;
    float battleFeedbackRawSecondsRemaining;
    bool battleEndSequenceActive;
    float battleEndSequenceElapsedSeconds;
    BattleEndRuntime battleEndRuntime;
    BattleEndNoticeRuntime battleEndNoticeRuntime;
    BattleOutcome pendingBattleOutcome;
    GameSaveImage secondarySaveImage;
    bool secondaryProfileLoaded;
    GameScreen shopReturnScreen;
    GameShopMode currentShopMode;
    GameShopResult currentShopResult;
    int shopCategoryValue;
    int shopSelectionValue;
    std::vector<uint32_t> shopItemIds;
    bool advicePendingValue;
    GameSlotRecord additionalEnemies[MAX_GAME_OPPONENTS - 1];
    int activeOpponentCount;

    GameSlotRecord ally;
    bool allyActive;
    int targetOpponentIndex;
    float battleElapsedSeconds;

    MoveInputHistory combatInputHistory;
    float combatInputChargeSeconds;
    int combatInputLastCommand;
    int combatInputLastActionIndex;
    int combatInputLastMoveId;

    int battleComboBonus;
    int lastBattleExperience;
    int lastBattleLevelsGained;
    int16_t battleSupplementalExperience;
    int16_t battleRewardOpponentIds[BATTLE_REWARD_ROSTER_SIZE];
    int8_t battleScoreMultipliers[BATTLE_REWARD_ROSTER_SIZE];
    bool battleRewardOpponentActive[MAX_GAME_OPPONENTS];
    int robberyAwardPresentationAmount;
    bool robberyAwardPresentationPending;
    bool robberyAwardLargeSideEffectPending;
    bool storyEncounterCompletionPending;
    BattleResultController battleResult;
    float arenaRadius;
    GameStageNavigation stageNavigation;
    FstageScene* combatItemScene;

    GameRandom random;
    uint32_t frameCounter;

    uint8_t combatInputHoldSuccessCount = 0;
    uint32_t combatRepeatCount;
    float combatRepeatDelaySeconds;
    uint32_t heldInput;
    uint32_t pressedInput;
    uint32_t releasedInput;
    uint32_t secondHeldInput;
    uint32_t secondPressedInput;
    uint32_t secondReleasedInput;
    float playerMovementX;
    float playerMovementZ;
    float playerLookX;
    float playerLookZ;
    float secondPlayerMovementX;
    float secondPlayerMovementZ;
    float secondPlayerLookX;
    float secondPlayerLookZ;
    Vec3 movementCameraPosition;
    float movementCameraRightX;
    float movementCameraRightZ;
    float movementCameraForwardX;
    float movementCameraForwardZ;
    const char* requestedSceneEntry;
    bool requestedSceneCompleted;
    bool pendingBattleSpawnValid;
    Vec3 pendingBattleSpawn;
    float pendingBattleSpawnHeading;
    int menuSelection;
    int selectedVideoMode;
    uint8_t videoTimingModeValue;
    float videoTestSeconds;
    int nominalFrameRate;
    float videoHalfFrameSeconds;
    int selectedLanguage;
    int selectedCharacter;
    int selectedStage;
    int selectedVersusCharacters[2];
    int selectedVersusSide;
    int versusHumanSideValue;
    bool versusActive[2];
    bool versusReady[2];
    uint8_t versusControllers[2];
    StoryCharacterSelectPresentation storyCharacterSelect;
    bool storyCharacterSelectReady;
    IntroCardPresentation currentIntroCard;
    int selectedPauseItem;
    bool pauseMoveListOpen;
    int pauseMoveListFirst;
    int selectedInventoryItem;
    InventoryUseState inventoryUse;
    bool inventoryEquipmentReady;
    bool temporarySlowActive;
    float temporarySlowElapsed;
    uint8_t temporarySlowMode;
    uint32_t temporarySlowBattleSerial;
    bool trackedOpponentCondition;
    bool inventoryConditionGameStateBitOne;
    int selectedOption;
    GameOptionsPage selectedOptionsPage;
    GameOptions optionsStaging;
    GameOptions optionsPageBaselines[OPTION_PAGE_COUNT];
    int8_t movieSelections[5];
    int8_t movieBaselines[5];
    bool optionsStagingActive;
    bool optionsDefaultConfirmation;
    int optionsDefaultSelection;
    uint16_t selectedMovieScene;
    uint16_t selectedMovieCharacterScene;
    bool vibrationPreview[2];
    int focusedInteractionSlot;
    GameInteraction stageInteractions[GAME_INTERACTION_CAPACITY];
    int stageInteractionCount;
    int focusedInteractionIndex;
    int16_t lastInteractionSelectorId;
    bool hasSave;
    bool creditsPlayed;
    bool connectedControllers[2];
    GameRumbleState rumble[2];
};

extern Game g_game;

#endif
