#ifndef GAME_BATTLE_SEQUENCE_H
#define GAME_BATTLE_SEQUENCE_H

#include "game_battle_finisher.h"
#include "game_battle_tables.h"

enum BattleSequenceOperation
{
    BATTLE_SEQUENCE_UPDATE_PAIR_MOTION,
    BATTLE_SEQUENCE_SET_ACTION,
    BATTLE_SEQUENCE_PREPARE_PRESENTATION,
    BATTLE_SEQUENCE_UPDATE_PRESENTATION,
    BATTLE_SEQUENCE_RESET_PRESENTATION,
    BATTLE_SEQUENCE_SELECT_ACTION,
    BATTLE_SEQUENCE_RESOURCE_START,
    BATTLE_SEQUENCE_RESOURCE_POLL,
    BATTLE_SEQUENCE_LETTERBOX,
    BATTLE_SEQUENCE_STOP_LETTERBOX,
    BATTLE_SEQUENCE_START_COUNTDOWN,
    BATTLE_SEQUENCE_PAUSE_COUNTDOWN,
    BATTLE_SEQUENCE_MARK_GAMEPLAY_ACTIVE,
    BATTLE_SEQUENCE_SET_FEEDBACK,
    BATTLE_SEQUENCE_SET_TIME_SCALE,
    BATTLE_SEQUENCE_SET_FADE,
    BATTLE_SEQUENCE_FLOW_READY,
    BATTLE_SEQUENCE_LOAD_FLAG_CLEARED,
    BATTLE_SEQUENCE_PARTY_LOAD_FLAG_CLEARED,
    BATTLE_SEQUENCE_EVENT_2BC_CLEARED,
    BATTLE_SEQUENCE_SET_STATUS,
    BATTLE_SEQUENCE_SET_CHARACTER_VALUE0,
    BATTLE_SEQUENCE_DIALOGUE,
    BATTLE_SEQUENCE_CAMDAT,
    BATTLE_SEQUENCE_RELEASE_ATTACHMENT,
    BATTLE_SEQUENCE_RETURN_WEAPON,
    BATTLE_SEQUENCE_RESET_ATTACHMENTS,
    BATTLE_SEQUENCE_WEAPON_VISIBLE,
    BATTLE_SEQUENCE_ATTACH_OWNED_WEAPON,
    BATTLE_SEQUENCE_REPOSITION,
    BATTLE_SEQUENCE_PRIDE_VISIBLE,
    BATTLE_SEQUENCE_SCORE_MULTIPLIER,
    BATTLE_SEQUENCE_ROBBERY,
    BATTLE_SEQUENCE_START_SCENE_TRANSITION,
    BATTLE_SEQUENCE_SCENE_TRANSITION_COMPLETE,
    BATTLE_SEQUENCE_STOP_SCENE_TRANSITION,
    BATTLE_SEQUENCE_INIT_CONFIRM_CHOICE,
    BATTLE_SEQUENCE_UPDATE_CHOICE,
    BATTLE_SEQUENCE_FINALIZE_CHOICE,
    BATTLE_SEQUENCE_SOUND_CHANNEL2_ACTIVE,
    BATTLE_SEQUENCE_STOP_SOUND_CHANNEL2,
    BATTLE_SEQUENCE_SET_OUTCOME_BYTE,
    BATTLE_SEQUENCE_COMPUTE_PRIDE_DAMAGE,
    BATTLE_SEQUENCE_REDUCE_TARGET_PRIDE,
    BATTLE_SEQUENCE_CAN_APPLY_DAMAGE,
    BATTLE_SEQUENCE_MAPPED_ACTION,
    BATTLE_SEQUENCE_SETUP_ACTION_PAIR,
    BATTLE_SEQUENCE_RESET_CAMERA,
    BATTLE_SEQUENCE_SET_CAMERA_MODE,
    BATTLE_SEQUENCE_RELEASE_PARTY_ACTOR,
    BATTLE_SEQUENCE_EVENT_MESSAGE_READY,
    BATTLE_SEQUENCE_SELECT_FINISHER,
    BATTLE_SEQUENCE_RECORD_FINISHER,
    BATTLE_SEQUENCE_RECORD_SPARED_TARGET,
    BATTLE_SEQUENCE_ACTOR_ACTION_FLAGS,
    BATTLE_SEQUENCE_REDUCE_HEALTH,
    BATTLE_SEQUENCE_GROUNDED,
    BATTLE_SEQUENCE_MOTION_FLAGS,
    BATTLE_SEQUENCE_SPECIAL_INIT,
    BATTLE_SEQUENCE_SPECIAL_UPDATE,
    BATTLE_SEQUENCE_CINEMATIC_INIT,
    BATTLE_SEQUENCE_CINEMATIC_UPDATE,
    BATTLE_SEQUENCE_CINEMATIC_DESTROY,
    BATTLE_SEQUENCE_OPERATION_COUNT
};

struct BattleSequenceCommand
{
    BattleSequenceOperation operation;
    int actorRole;
    int value;
    int value2;
    int value3;
    float seconds;
    uint32_t address;
};

struct BattleSequenceRuntime
{
    int state = 0;
    int battleMode = 0;
    int outcomeMode = 0;
    int pendingOutcomeMode = 0;
    int selectedActionType = 0;
    int pendingActionType = 0;
    int reactionType = 0;
    int pendingReactionType = 0;
    uint8_t actionVariant = 0;
    uint8_t pendingActionVariant = 0;
    uint8_t actionSelector = 0;
    uint32_t flags = 0;
    uint32_t flowFlags = 0;
    float elapsedSeconds = 0;
    float delaySeconds = 0;
    float timerSeconds = 0;
    float soundTimeoutSeconds = 0;
    int choiceResult = -1;
    uint32_t resources[4] = {};
    BattleFinisherSelection finisher = {};
    int finisherStep = 0;
    float finisherElapsed = 0;
    uint32_t finisherFlags = 0;
};

struct BattleSequenceInput
{
    float scaledDeltaSeconds = 0;
    int relation = 0;
    uint32_t attackerFlags = 0;
    uint32_t targetFlags = 0;
    int attackerSelectedAction = 0;
    int attackerNativeSlot = -1;
    bool targetHasPride = false;
    bool condition16 = false;
};

typedef int BattleSequenceDispatch(void* userData, BattleSequenceRuntime& runtime,
                                   const BattleSequenceCommand& command);
struct BattleSequenceCallbacks
{
    BattleSequenceDispatch* dispatch = nullptr;
    void* userData = nullptr;
};

int updateBattleSequence(BattleSequenceRuntime& runtime,
                         const BattleSequenceInput& input,
                         const BattleSequenceCallbacks& callbacks);
bool updateBattleFinisherSequence(BattleSequenceRuntime& runtime,
                                  const BattleSequenceInput& input,
                                  const BattleSequenceCallbacks& callbacks);

#endif
