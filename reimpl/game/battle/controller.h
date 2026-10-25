#ifndef GAME_BATTLE_CONTROLLER_H
#define GAME_BATTLE_CONTROLLER_H

#include "game/battle/flow.h"
#include "game/battle/sequence.h"
#include "game/battle/choice.h"

struct BattleSubsequenceRuntime
{
    int state = 0;
    uint32_t flags = 0;
    float elapsedSeconds = 0;
    float delaySeconds = 0;
    int variant = 0;
    uint32_t resources = 0;
};

struct BattleControllerRuntime
{
    bool active = false;
    bool pauseGuardEnabled = true;
    bool letterbox = false;
    bool prideVisible = true;
    bool choiceVisible = false;
    bool pairMotionUpdated = false;
    bool condition16 = false;
    bool sceneTransitionActive = false;
    bool sceneTransitionComplete = false;
    int sceneTransitionKind = -1;
    int sceneTransitionArguments[3] = {};
    int dialogueActor = -1;
    int dialogueEvent = -1;
    uint32_t dialogueGeneration = 0;
    int16_t sparedCount = 0;
    int16_t robberyCount = 0;
    int16_t finisherCount = 0;
    bool sequenceInitialized = false;
    bool targetRequestPending = false;
    int targetRequestSource = -1;
    bool eventMessageActive = false;
    int eventMessageText = 0;
    float eventMessageTimeout = 0;
    bool cinematicPromptVisible = false;
    int conditionOpponentCode = 0;
    int conditionOpponentCharacterId = -1;

    int nativeBattleMode = 0;
    uint32_t encounterFlags = 0;
    int nativeGlobal_0x5f3f90 = 0;
    int choiceGate_0x5f3dac = 0;
    int outcomeByte = 0;
    int pendingSoundStopMask = 0;
    int countdownState = -1;
    int cameraModeRequest = -1;
    float rawDeltaSeconds = 0;
    float feedbackRecoverySeconds = 0;
    BattleFlowRuntime flow;
    BattleSequenceRuntime sequence;
    BattleChoiceDescriptor choiceDescriptor;
    BattleChoiceRuntime choice;
    BattleSubsequenceRuntime special;
    BattleSubsequenceRuntime cinematic;
    int unresolvedOperation = -1;
};

#endif
