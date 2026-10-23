#ifndef MOVE_TABLES_H
#define MOVE_TABLES_H
#include <stdint.h>

const int MOVE_RECORD_COUNT = 2001;
const int MOVE_HANDLER_COUNT = 37;
const int MOVE_CHARACTER_COUNT = 51;
const int MOVE_ARCHETYPE_COUNT = 3;

const int MOVE_REMAP_COUNT = 552;
const int MOVE_REMAP_LIMIT = 0x225;
const int MOVE_HIT_BLOCK_COUNT = 3;

const int MOVE_LIST_STYLE_COUNT = 36;
const int MOVE_LIST_GATE_COUNT = 49;
const int MOVE_PERSONAL_ACTION_COUNT = 1023;
const int MOVE_PERSONAL_CONDITION_COUNT = 17;
const int MOVE_LIST_REQUIREMENT_COUNT = 8;
const int MOVE_LIST_VISIBLE_ROW_COUNT = 5;

enum MoveBehaviorFlag
{

    MOVE_BEHAVIOR_HIT_BLOCKS = 0x2,

    MOVE_BEHAVIOR_LINKED = 0x4
};

enum MoveHitBlockFlag
{
    MOVE_HIT_BLOCK_PART_LOW = 0x40,
    MOVE_HIT_BLOCK_PART_HIGH = 0x80,
    MOVE_HIT_BLOCK_SIDE_LOW = 0x400,
    MOVE_HIT_BLOCK_SIDE_HIGH = 0x800
};

struct MoveHitBlock
{
    uint16_t damage;
    uint8_t prideDamage;

    uint16_t flagsA;

    uint16_t flagsB;

    uint32_t flagsC;
};

enum MoveHandler
{
    MOVE_HANDLER_NONE = 0,

    MOVE_HANDLER_RESTORE = 1,

    MOVE_HANDLER_ATTACK = 2,

    MOVE_HANDLER_DECISION = 3,

    MOVE_HANDLER_DAMAGE = 7
};
struct MoveRecord
{
    uint32_t behaviorFlags;
    int16_t tiredCost;
    MoveHitBlock hitBlocks[MOVE_HIT_BLOCK_COUNT];
    int16_t handler;
    int32_t handlerParam;
    int16_t linkedMoveId;
    int16_t alternateLinkedMoveId;
    int16_t field_0xC;
    int16_t field_0xE;

    uint32_t field_0x38;

    uint32_t attributeAddress;
};

struct MoveCharacterRecord
{
    uint8_t archetype;
    uint8_t locomotionVariant;
    uint8_t personalMovesetId;
    uint16_t firstAnimationResource;
    uint8_t animationResourceCount;
};
struct MoveListGate
{
    uint8_t entryIndex;
    bool predicatePass;
    uint8_t requirements[MOVE_LIST_REQUIREMENT_COUNT];
};
struct MovePersonalCondition
{
    int16_t opcode;
    int16_t argument;
};
struct MovePersonalAction
{
    int16_t actionId;
    int16_t moveId;
    uint8_t requirements[MOVE_LIST_REQUIREMENT_COUNT];
    uint16_t firstCondition;
    uint8_t conditionCount;
};
struct MoveListStyle
{
    uint16_t commandTextFirst;
    uint16_t nameTextFirst;
    uint8_t entryCount;
    uint16_t firstGate;
    uint8_t gateCount;
    uint16_t firstAction;
    uint16_t actionCount;
};

const int MOVE_CHARACTER_ACTION_IDLE = 2;

const int MOVE_LOCOMOTION_MODE_NORMAL = 0;
const int MOVE_LOCOMOTION_MODE_COUNT = 13;
const int MOVE_LOCOMOTION_VARIANT_COUNT = 3;
const int MOVE_LOCOMOTION_DIRECTION_COUNT = 8;

const int16_t MOVE_LOCOMOTION_TARGET_RUN = 0x003;
const int16_t MOVE_LOCOMOTION_FREE_RUN = 0x23a;
const int16_t MOVE_LOCOMOTION_FREE_WALK = 0x249;
const int16_t MOVE_LOCOMOTION_FREE_WALK_FEMALE = 0x25a;
struct MoveLocomotion
{

    int16_t moveId;
    float frameDistance;
};

const int MOVE_REACTION_SET_COUNT = 2;
const int MOVE_REACTION_COUNT = 22;
const int MOVE_REACTION_DIRECTIONS = 4;
const int MOVE_DEFEAT_REACTION_COUNT = 3;
const int MOVE_BEHIND_REACTION_COUNT = 2;
struct MoveReaction
{
    int16_t moveId;
    int16_t headingDelta;
};
extern const MoveReaction MOVE_REACTIONS[MOVE_REACTION_SET_COUNT]
                                        [MOVE_REACTION_COUNT]
                                        [MOVE_REACTION_DIRECTIONS];
extern const MoveReaction MOVE_DEFEAT_REACTIONS[MOVE_DEFEAT_REACTION_COUNT]
                                               [MOVE_REACTION_DIRECTIONS];
extern const MoveReaction MOVE_BEHIND_REACTIONS[MOVE_BEHIND_REACTION_COUNT]
                                               [MOVE_REACTION_DIRECTIONS];

const MoveReaction* moveReaction(int set, int reaction, int direction);

int moveHitDamage(const MoveHitBlock& block, float attackerPower,
                  float victimMultiplier, float situational);

int moveGuardDamage(int appliedDamage);

int moveGuardGauge(int partSelector);

enum MoveStreamOpcode
{

    MOVE_STREAM_PLAY_MOTION = 0x00,

    MOVE_STREAM_END = 0x01,

    MOVE_STREAM_LOOP_BEGIN = 0x02,
    MOVE_STREAM_LOOP_END = 0x03,
    MOVE_STREAM_CLEAR_PROGRESS = 0x04,

    MOVE_STREAM_SET_TIME_SCALE = 0x05,

    MOVE_STREAM_CALL = 0x06,

    MOVE_STREAM_SET_ACTION = 0x07,
    MOVE_STREAM_NOP = 0x08,
    MOVE_STREAM_SKIP_3 = 0x09,
    MOVE_STREAM_EFFECT = 0x13,
    MOVE_STREAM_COMBO_ARM = 0x15,
    MOVE_STREAM_COMBO_GATE = 0x16,

    MOVE_STREAM_RANDOM_CALL = 0x34
};
struct MoveStream
{
    int firstWord;
    int wordCount;
};
extern const int16_t MOVE_STREAM_WORDS[];
extern const MoveStream MOVE_STREAMS[MOVE_RECORD_COUNT];

int moveStreamInstructionWords(const int16_t* words, int available);

enum MoveBehaviourOpcode
{

    MOVE_BEHAVIOUR_PARK = 0x01,

    MOVE_BEHAVIOUR_LOOP_BEGIN = 0x02,
    MOVE_BEHAVIOUR_LOOP_END = 0x03,

    MOVE_BEHAVIOUR_SET_TIME_SCALE = 0x05,

    MOVE_BEHAVIOUR_WAIT_FRAMES = 0x0a,

    MOVE_BEHAVIOUR_PLAY_SOUND = 0x0b,

    MOVE_BEHAVIOUR_SOUND_KIND_COUNT = 9,

    MOVE_BEHAVIOUR_EFFECT = 0x0d,

    MOVE_BEHAVIOUR_EFFECT_LIST = 0x12,
    MOVE_BEHAVIOUR_ARM_HOLD_COMBO = 0x15,
    MOVE_BEHAVIOUR_COMBO_GATE = 0x16,

    MOVE_BEHAVIOUR_MOVEMENT_TARGET = 0x17,

    MOVE_BEHAVIOUR_CONTACT_MOVEMENT = 0x18,
    MOVE_BEHAVIOUR_TURN_DURATION = 0x1a,
    MOVE_BEHAVIOUR_SPACING_CORRECTION = 0x1b,

    MOVE_BEHAVIOUR_CAMERA = 0x1c,

    MOVE_BEHAVIOUR_HOLD_END_FRAME = 0x1e,
    MOVE_BEHAVIOUR_HOLD_FOLLOWUP = 0x1f,
    MOVE_BEHAVIOUR_HOLD_THRESHOLD = 0x20,
    MOVE_BEHAVIOUR_HOLD_KIND = 0x21,

    MOVE_BEHAVIOUR_ARM_HIT_WINDOW = 0x24,

    MOVE_BEHAVIOUR_TARGET_FLAG_20 = 0x26,
    MOVE_BEHAVIOUR_WEAPON_ACTION = 0x27,
    MOVE_BEHAVIOUR_WEAPON_SNAPSHOT = 0x28,
    MOVE_BEHAVIOUR_WEAPON_HAND = 0x29,
    MOVE_BEHAVIOUR_WEAPON_ALIGN = 0x2a,

    MOVE_BEHAVIOUR_APPLY_HIT = 0x2b,

    MOVE_BEHAVIOUR_TARGET_FLAGS = 0x2d,

    MOVE_BEHAVIOUR_ACTION_FLAGS = 0x2e,

    MOVE_BEHAVIOUR_SET_FLAGS = 0x2f,

    MOVE_BEHAVIOUR_SET_HIT_BLOCK = 0x30,

    MOVE_BEHAVIOUR_QUEUE_IF_FLAG = 0x32,

    MOVE_BEHAVIOUR_QUEUE_MOVE = 0x33,

    MOVE_BEHAVIOUR_SET_IMPACT_STATE = 0x35,

    MOVE_BEHAVIOUR_SET_RECOVERY_STATE = 0x31,

    MOVE_BEHAVIOUR_RUN_ACTION = 0x36,

    MOVE_BEHAVIOUR_SET_ACTION = 0x3b,

    MOVE_BEHAVIOUR_WEAPON_VISIBLE = 0x3e,

    MOVE_BEHAVIOUR_WAIT_UNITS = 0x43,

    MOVE_BEHAVIOUR_WAIT_MS = 0x44,

    MOVE_BEHAVIOUR_CONTACT_RESPONSE = 0x45,

    MOVE_BEHAVIOUR_CUE_FLAG = 0x46,

    MOVE_BEHAVIOUR_FIRE_PROJECTILE = 0x4a
};
extern const int16_t MOVE_BEHAVIOUR_WORDS[];
extern const MoveStream MOVE_BEHAVIOURS[MOVE_RECORD_COUNT];

int moveBehaviourInstructionWords(const int16_t* words, int available);

struct MoveHitWindow
{
    int16_t centreFrame;
    int16_t startFrame;
    int16_t endFrame;

    uint32_t channelMask;
};
const int MOVE_HIT_CHANNEL_COUNT = 14;

int moveHitWindows(int moveId, MoveHitWindow* out, int capacity);

bool moveHitWindowActive(const MoveHitWindow& window, int animationFrame);

const uint32_t MOVE_ACTOR_INITIAL_BEHAVIOUR = 0x1;
const uint32_t MOVE_ACTOR_DRIVES_HITS = 0x4;
const uint32_t MOVE_ACTOR_TAKES_TRANSITIONS = 0x400;
const uint32_t MOVE_ACTION_HIT_PENDING = 0x10000;
const uint32_t MOVE_TARGET_HIT_TRANSITION = 0x200000;

bool moveHitTransitionApplies(uint32_t attackerBehaviourFlags,
                              uint32_t victimBehaviourFlags);

enum MoveInputStepBit
{

    MOVE_INPUT_BUTTON_0 = 0x00001,
    MOVE_INPUT_BUTTON_1 = 0x00002,
    MOVE_INPUT_BUTTON_2 = 0x00004,
    MOVE_INPUT_BUTTON_3 = 0x00008,
    MOVE_INPUT_BUTTON_4 = 0x00010,
    MOVE_INPUT_BUTTON_5 = 0x00020,
    MOVE_INPUT_BUTTON_6 = 0x00040,
    MOVE_INPUT_BUTTON_7 = 0x00080,
    MOVE_INPUT_BUTTON_8 = 0x00100,

    MOVE_INPUT_DIR_CURRENT = 0x00200,
    MOVE_INPUT_DIR_MIRRORED = 0x00400,
    MOVE_INPUT_DIR_LEFT = 0x00800,
    MOVE_INPUT_DIR_RIGHT = 0x01000,
    MOVE_INPUT_DIR_SWAP_LR = 0x02000,
    MOVE_INPUT_DIR_SWAP_UD = 0x04000,

    MOVE_INPUT_ON_RELEASE = 0x08000,

    MOVE_INPUT_WHILE_HELD = 0x10000,

    MOVE_INPUT_ANY_DIRECTION = 0x20000,
    MOVE_INPUT_RIGHT_STICK_UP = 0x40000,
    MOVE_INPUT_RIGHT_STICK_DOWN = 0x80000,
    MOVE_INPUT_RIGHT_STICK_RIGHT = 0x100000,
    MOVE_INPUT_RIGHT_STICK_LEFT = 0x200000,
    MOVE_INPUT_RIGHT_STICK_ANY = 0x400000
};
struct MoveInputCommand
{

    int32_t windowMilliseconds;

    uint32_t actorBehaviour;
    uint32_t linkedBehaviour;
    uint32_t actorField0x4390;
    uint32_t linkedField0x4390;
    uint32_t actorTargeting;
    uint32_t linkedTargeting;

    int32_t firstStep;
    int32_t stepCount;
};
const int MOVE_INPUT_COMMAND_COUNT = 83;

const int MOVE_INPUT_COMMAND_GUARD_HELD = 75;

const int MOVE_ATTACK_POWER_CLASS_COUNT = 8;
const int MOVE_DIFFICULTY_POWER_COUNT = 4;
extern const float MOVE_ATTACK_POWER_CLASSES[MOVE_ATTACK_POWER_CLASS_COUNT];
extern const float MOVE_DIFFICULTY_POWER[MOVE_DIFFICULTY_POWER_COUNT];

float moveAttackPowerForClass(int attackPowerClass);
extern const uint32_t MOVE_INPUT_STEPS[];
extern const MoveInputCommand MOVE_INPUT_COMMANDS[MOVE_INPUT_COMMAND_COUNT];

bool moveInputCommandEligible(const MoveInputCommand& command,
                              uint32_t actorBehaviour, uint32_t linkedBehaviour,
                              uint32_t actorField0x4390,
                              uint32_t linkedField0x4390,
                              uint32_t actorTargeting,
                              uint32_t linkedTargeting);

const int MOVE_ACTION_INDEX_RESOLVED = -1;

const int MOVE_ACTION_INDEX_NEUTRAL = 0x69;
const int MOVE_ACTION_INDEX_FACING = 0x6a;
const int MOVE_ACTION_INDEX_FACING_ALT = 0x6b;
const int MOVE_ACTION_INDEX_FACING_STRONG = 0x6c;

const int MOVE_ACTION_INDEX_UNCHARGED = 0x1e;
const float MOVE_COMMAND_CHARGE_SECONDS = 0.1f;

struct MoveGrabActions
{
    int primary;
    int secondary;
};

const uint32_t MOVE_HIT_DIRECTION_FRONT = 0x4;
const uint32_t MOVE_HIT_DIRECTION_BACK = 0x8;
const uint32_t MOVE_HIT_DIRECTION_SIDE_A = 0x1;
const uint32_t MOVE_HIT_DIRECTION_SIDE_B = 0x2;

MoveGrabActions moveGrabActionsFor(uint32_t hitDirection);

int moveGrabMoveFor(uint32_t hitDirection);

struct MovePairedOutcome
{
    int16_t primaryMove;
    int16_t primaryLinkedMove;
    int16_t secondaryMove;
    int16_t secondaryLinkedMove;
};

MovePairedOutcome moveGrabPairedOutcome(int attackerStyle, int victimStyle,
                                        uint32_t hitDirection);

MovePairedOutcome moveWeaponGrabOutcome(uint8_t weaponCategory);

const uint16_t MOVE_HIT_GUARD_BREAK = 0x100;

bool moveIsGuardBreak(int moveId);

int moveCommandActionIndex(int command, bool facingTarget, float chargeSeconds);

int moveCommandFallbackAction(int command, bool facingTarget);

int moveCommandResolvedAction(int command, bool facingTarget,
                              float chargeSeconds);

struct MovePersonalActionContext
{
    const uint8_t* levels;
    int archetype;
    int playerIndex;
    int resourceProfileId;
    bool characterIdZero;
    bool secondaryGameState;
    bool storySpecial;
    bool interactionRecovery;
};

bool movePersonalActionEligible(
    const uint8_t requirements[MOVE_LIST_REQUIREMENT_COUNT],
    const MovePersonalCondition* conditions, int conditionCount,
    const MovePersonalActionContext& context);
int movePersonalActionForStyle(
    int combatStyleId, int action,
    const MovePersonalActionContext& context);

struct MoveMotion
{
    int16_t motionId;
    int16_t blendFrames;
    int16_t maxDurationFrames;
    int16_t transitionFrames;
};

int moveMotions(int moveId, MoveMotion* out, int capacity);

const int MOVE_COMMAND_COUNT = 114;
const int MOVE_COMMAND_MOVE_COUNT = 24;
const int MOVE_COMMAND_KIND_FOLLOWUP = 1;
struct MoveCommand
{
    int32_t kind;
    int16_t selector;
    int16_t param;
    int32_t cooldown;
};
extern const int16_t MOVE_COMMAND_MOVES[MOVE_COMMAND_MOVE_COUNT];
extern const MoveCommand MOVE_COMMANDS[MOVE_COMMAND_COUNT];

int moveCommandFollowUp(int commandIndex);

int moveDispatchCommand(int kind, int selector);

const int MOVE_ACTION_COUNT = 136;
extern const int16_t MOVE_DEFAULT_ACTIONS[MOVE_ACTION_COUNT];

extern const int16_t MOVE_ACTION_PARENTS[MOVE_ACTION_COUNT];

int moveActionParent(int actionId);

int moveActionResolveChain(int actionId);

int moveForAction(int actionId);

int moveEquippedTypeOne(int slotId);

float moveMotionDuration(const MoveMotion& motion, float clipSeconds);

float moveMotionTransition(const MoveMotion& motion, float clipSeconds);

const int MOVE_ANIMATION_BUNDLE_COUNT = 317;
struct MoveAnimationBundle
{
    int16_t xafEntry;
    int firstMotion;
    int motionCount;
};
extern const int16_t MOVE_BUNDLE_MOTIONS[];
extern const MoveAnimationBundle
    MOVE_ANIMATION_BUNDLES[MOVE_ANIMATION_BUNDLE_COUNT];

int moveMotionBundle(int motionId, int* positionInBundle);

int moveMotionBundleForCharacter(int characterIndex, int motionId,
                                 int* positionInBundle);
extern const MoveRecord MOVE_RECORDS[MOVE_RECORD_COUNT];
extern const int16_t MOVE_ARCHETYPE_REMAP[MOVE_ARCHETYPE_COUNT]
                                         [MOVE_REMAP_COUNT];
extern const MoveCharacterRecord MOVE_CHARACTER_RECORDS[MOVE_CHARACTER_COUNT];
extern const MoveLocomotion
    MOVE_LOCOMOTION[MOVE_LOCOMOTION_MODE_COUNT]
                   [MOVE_LOCOMOTION_VARIANT_COUNT]
                   [MOVE_LOCOMOTION_DIRECTION_COUNT];
extern const int16_t MOVE_CHARACTER_ANIMATION_RESOURCES[];
extern const MoveListGate MOVE_LIST_GATES[MOVE_LIST_GATE_COUNT];
extern const MovePersonalCondition
    MOVE_PERSONAL_CONDITIONS[MOVE_PERSONAL_CONDITION_COUNT];
extern const MovePersonalAction
    MOVE_PERSONAL_ACTIONS[MOVE_PERSONAL_ACTION_COUNT];
extern const MoveListStyle MOVE_LIST_STYLES[MOVE_LIST_STYLE_COUNT];

const MoveListStyle* moveListStyle(int personalMovesetId);

bool moveListEntryVisible(int personalMovesetId, int entryIndex,
                          const uint8_t levels[MOVE_LIST_REQUIREMENT_COUNT]);

int moveListVisibleEntries(
    int personalMovesetId,
    const uint8_t levels[MOVE_LIST_REQUIREMENT_COUNT],
    uint8_t* output, int capacity);

const MoveRecord* moveRecord(int moveId);

int resolveCharacterMove(int characterIndex, int moveId);

int moveLocomotionDirection(float relativeHeadingDegrees);

const MoveLocomotion* moveLocomotion(int mode, int variant, int direction);

const MoveLocomotion* moveLocomotionForCharacter(
    int characterIndex, int mode, int direction);

bool moveIsAttack(int moveId);

int movePairedAction(int moveId);

uint32_t moveTargetingBits(int moveId);
#endif
