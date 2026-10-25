#include "game.h"
#include "menu_inventory.h"
#include "scene_task.h"
#include "character_records.h"
#include "combat_styles.h"
#include "versus_tables.h"
#include "xaf_runtime.h"
#include "move_tables.h"
#include "action_state_handlers.h"
#include "options_menu.h"
#include "camera.h"
#include "sound_cues.h"
#include "stage.h"
#include "hud_advice.h"
#include "../formats/MENU_LAYOUT/pause_results_layout.h"
#include "../formats/BATTLE_ROSTER/battle_rosters.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "game_internal.h"

using namespace game_detail;

Game g_game;

GameInputState::GameInputState()
{
    reset();
}

void
GameInputState::reset(void)
{
    currentButtons = 0;
    previousButtons = 0;
    pressedButtons = 0;
    releasedButtons = 0;
    repeatedButtons = 0;
    memset(buttonHeldSeconds, 0, sizeof(buttonHeldSeconds));
    memset(lastChangedFrame, 0, sizeof(lastChangedFrame));
    repeatCounter = 0;
}

void
GameInputState::update(float deltaSeconds, uint32_t buttons,
                       uint32_t globalFrameCounter, uint32_t elapsedFrames)
{
    if(deltaSeconds < 0.0f)
        deltaSeconds = 0.0f;

    previousButtons = currentButtons;
    currentButtons = buttons;
    const uint32_t changed = currentButtons ^ previousButtons;
    pressedButtons = currentButtons & changed;
    releasedButtons = previousButtons & changed;

    for(int i = 0; i < GAME_INPUT_TRACKED_BUTTONS; ++i)
    {
        if((changed & (1u << i)) != 0)
            lastChangedFrame[i] = globalFrameCounter;
        if((currentButtons & (1u << i)) != 0)
            buttonHeldSeconds[i] += deltaSeconds;
        else
            buttonHeldSeconds[i] = 0.0f;
    }

    repeatedButtons = 0;
    if(repeatCounter == 0)
    {

        if(currentButtons != 0 &&
           (previousButtons == 0 || elapsedFrames != 0))
        {
            repeatedButtons = currentButtons;
            repeatCounter = 1;
        }
    }
    else if(previousButtons == currentButtons)
    {
        if(elapsedFrames != 0)
        {
            const uint32_t previousCounter = repeatCounter;
            const uint64_t advancedCounter =
                static_cast<uint64_t>(repeatCounter) + elapsedFrames;
            if(advancedCounter > 20)
            {
                repeatCounter = 20;
                const uint32_t firstEligibleOffset = 21 - previousCounter;
                const uint32_t firstFrame =
                    globalFrameCounter >= elapsedFrames ?
                    globalFrameCounter - elapsedFrames + firstEligibleOffset :
                    firstEligibleOffset;
                const uint32_t firstRepeatFrame =
                    (firstFrame + 3u) & ~3u;
                if(firstRepeatFrame <= globalFrameCounter)
                    repeatedButtons = currentButtons;
            }
            else
                repeatCounter = static_cast<uint32_t>(advancedCounter);
        }
    }
    else
        repeatCounter = 0;
}

GameInteraction::GameInteraction()
    : kind(GAME_INTERACTION_NONE), actorSlot(-1), selectorId(0),
      npcPositionRowIndex(-1), promptKind(0),
      range(GAME_INTERACTION_OBJECT_RANGE), focusSeconds(0.0f), used(0)
{
    position.x = position.y = position.z = 0.0f;
}

GameStageNavigation::GameStageNavigation()
    : groundHeight(NULL), groundColumn(NULL), groundContact(NULL), combatSurface(NULL),
      combatSurfaceContext(NULL), nearestFloor(NULL), context(NULL),
      collisionContext(NULL), minX(0.0f), maxX(0.0f), minZ(0.0f),
      maxZ(0.0f), spawnX(0.0f), spawnY(0.0f), spawnZ(0.0f),
      spawnHeadingDegrees(0.0f), valid(false)
{
}

Game::Game()
    : soundEventHead(0),
      soundEventCount(0),
      estEventHead(0),
      estEventCount(0),
      combatContactHead(0),
      combatContactCount(0),
      behaviourEffectSuppressionMask(0),
      pendingDamageCameraSettle(0.0f),
      combatDamageEnabled(false),
      camdatTimeScale(1.0f),
      pendingMoveCameraSettle(0.0f),
      previousCamdatVariant(0),
      pendingCamdatRecord(-1),
      pendingCamdatExclusive(false),
      pendingCamdatActorSlot(0),
      gameFlowTask(NULL),
      currentScreen(GAME_SCREEN_LOADING),
      optionsReturnScreen(GAME_SCREEN_MAIN_MENU),
      currentMode(GAME_MODE_STORY),
      matchSetupFlagValue(false),
      selectedVersusBattleType(VERSUS_PLAYER_VS_CPU),
      stageBriefingPresentationComplete(false),
      currentTitleAttractMode(TITLE_ATTRACT_VIDEO),
      titleTransitionSeconds(TITLE_TRANSITION_SECONDS),
      currentTitleAttractMovieIndex(-1),
      preTitleMoviePlayed(false),
      preTitleMovieComplete(false),
      activeSummaryIndex(0),
      primarySaveLoaded(false),
      savedFieldResumePending(false),
      pendingHostVm(NULL),
      eventWaitingForMessage(false),
      pendingEventFunction(EVENT_CALLFUNC_NONE),
      scaledFrameDeltaSeconds(0.0f),
      currentBattleOutcome(BATTLE_OUTCOME_IN_PROGRESS),
      battleTimeScale(BATTLE_TIME_SCALE_NORMAL),
      battleFeedbackModeValue(0),
      battleFinisherPhase(BATTLE_FINISHER_INACTIVE),
      battleFinisherElapsedSeconds(0.0f),
      battleFinisherAttackerSlot(0),
      battleFinisherTargetSlot(0),
      battleFinisherSelection{},
      battleFeedbackRawSecondsRemaining(0.0f),
      battleEndSequenceActive(false),
      battleEndSequenceElapsedSeconds(0.0f),
      pendingBattleOutcome(BATTLE_OUTCOME_IN_PROGRESS),
      secondaryProfileLoaded(false),
      shopReturnScreen(GAME_SCREEN_TITLE),
      currentShopMode(GAME_SHOP_BUY),
      currentShopResult(GAME_SHOP_RESULT_NONE),
      shopCategoryValue(-1),
      shopSelectionValue(0),
      advicePendingValue(false),
      activeOpponentCount(1),
      targetOpponentIndex(0),
      battleElapsedSeconds(0.0f),
      combatInputChargeSeconds(0.0f),
      combatInputLastCommand(-1),
      combatInputLastActionIndex(MOVE_ACTION_INDEX_RESOLVED),
      combatInputLastMoveId(-1),
      battleComboBonus(0),
      lastBattleExperience(0),
      lastBattleLevelsGained(0),
      battleSupplementalExperience(0),
      storyEncounterCompletionPending(false),
      arenaRadius(80.0f),
      combatItemScene(NULL),
      frameCounter(0),
      combatRepeatCount(0),
      combatRepeatDelaySeconds(0.0f),
      heldInput(0),
      pressedInput(0),
      releasedInput(0),
      secondHeldInput(0),
      secondPressedInput(0),
      secondReleasedInput(0),
      playerMovementX(0.0f),
      playerMovementZ(0.0f),
      playerLookX(0.0f),
      playerLookZ(0.0f),
      secondPlayerMovementX(0.0f),
      secondPlayerMovementZ(0.0f),
      secondPlayerLookX(0.0f),
      secondPlayerLookZ(0.0f),
      movementCameraPosition(),
      movementCameraRightX(1.0f),
      movementCameraRightZ(0.0f),
      movementCameraForwardX(0.0f),
      movementCameraForwardZ(1.0f),
      requestedSceneEntry(NULL),
      requestedSceneCompleted(false),
      pendingBattleSpawnValid(false),
      pendingBattleSpawn(),
      pendingBattleSpawnHeading(0.0f),
      menuSelection(0),
      selectedVideoMode(0),
      videoTimingModeValue(0),
      videoTestSeconds(0.0f),
      nominalFrameRate(50),
      videoHalfFrameSeconds(0.01f),
      selectedLanguage(1),
      selectedCharacter(0),
      selectedVersusSide(0),
      versusHumanSideValue(0),
      selectedPauseItem(0),
      pauseMoveListOpen(false),
      pauseMoveListFirst(0),
      selectedInventoryItem(0),
      inventoryUse(),
      temporarySlowActive(false),
      temporarySlowElapsed(0.0f),
      temporarySlowMode(0),
      temporarySlowBattleSerial(0),
      trackedOpponentCondition(false),
      inventoryConditionGameStateBitOne(false),
      selectedOption(0),
      selectedOptionsPage(GAME_OPTIONS_PAGE_MAIN),
      optionsStaging(),
      optionsPageBaselines(),
      movieSelections(),
      movieBaselines(),
      optionsStagingActive(false),
      optionsDefaultConfirmation(false),
      optionsDefaultSelection(0),
      selectedMovieScene(0),
      selectedMovieCharacterScene(0),
      vibrationPreview(),
      selectedStage(0),
      focusedInteractionSlot(-1),
      stageInteractionCount(0),
      focusedInteractionIndex(-1),
      lastInteractionSelectorId(0),
      hasSave(false),
      creditsPlayed(false)
{
    memset(&persistent, 0, sizeof(persistent));
    initializeGameSaveImage(primarySaveImage);
    memset(&secondarySaveImage, 0, sizeof(secondarySaveImage));
    weaponManager.releaseAll();
    for(GameCombatWeaponBinding& binding : combatWeaponBindings)
        binding = {};
    pendingWeaponStates.clear();
    weaponDataAvailable = false;
    weaponLoadDiagnostic.clear();
    initializeStoryState(persistent.story);
    persistent.hudRadar = battleHudDefaultRadarState();
    resetInventoryUseState();
    titleAttractReset(currentTitleAttractState, false);
    currentCreditsSequence = {};
    currentCreditsSequence.phase = CREDITS_PHASE_FINISHED;
    selectedVersusCharacters[0] = 0;
    selectedVersusCharacters[1] = 1;
    storyCharacterSelect = {};
    storyCharacterSelect.phase = STORY_CHARACTER_SELECT_LOADING;
    storyCharacterSelect.outgoingSelection = -1;
    storyCharacterSelect.incomingPortraitX = 287.0f;
    storyCharacterSelect.incomingPortraitScale = 1.0f;
    storyCharacterSelect.incomingPortraitAlpha = 0xff;
    storyCharacterSelect.outgoingPortraitX = 287.0f;
    storyCharacterSelect.statsX = 68.0f;
    storyCharacterSelect.leftArrowX = -101.0f;
    storyCharacterSelect.rightArrowX = 130.0f;
    storyCharacterSelect.primaryAlpha = 0xff;
    storyCharacterSelect.portraitAlpha = 0xff;
    storyCharacterSelect.portraitScale = 1.0f;
    storyCharacterSelectReady = false;
    currentIntroCard = {};
    currentIntroCard.state = INTRO_CARD_TASK_INACTIVE;
    EventVmCallbacks callbacks = {};
    callbacks.command = handleEventCommand;
    callbacks.callFunction = handleEventCallFunction;
    callbacks.inventoryCount = handleEventInventoryCount;
    callbacks.addInventory = handleEventInventoryAdd;
    callbacks.removeInventory = handleEventInventoryRemove;
    callbacks.userData = this;
    eventVm.initialize(&persistent.story, callbacks);
    EventVmCallbacks messageCallbacks = {};
    messageCallbacks.command = handleMessageCommand;
    messageCallbacks.callFunction = handleMessageCallFunction;
    messageCallbacks.inventoryCount = handleEventInventoryCount;
    messageCallbacks.addInventory = handleEventInventoryAdd;
    messageCallbacks.removeInventory = handleEventInventoryRemove;
    messageCallbacks.userData = this;
    messageVm.initialize(&persistent.story, messageCallbacks);
    memset(&enemy, 0, sizeof(enemy));
    memset(additionalEnemies, 0, sizeof(additionalEnemies));
    memset(connectedControllers, 0, sizeof(connectedControllers));
    memset(rumble, 0, sizeof(rumble));
    stageAiResourceLoaded = false;
    stageAiDiagnosticText.clear();
    stageAiLastDecision = {};
    stageAiDecisionValid = false;
    clearStageActorRuntime();
    resetBehaviourPresentation();
    resetBattleResultController();
}

Game::~Game() = default;

void
Game::initialize(void)
{
    combatItemScene = NULL;
    weaponDataAvailable = false;

    std::vector<uint8_t> camdatBytes;
    if(readXafEntry("raw/XAF/STAGE.XAF", "CAMDAT.BIN", camdatBytes))
        camdatDatabase.parse(camdatBytes.data(), camdatBytes.size());
    else
        camdatDatabase.clear();
    loadWeaponDataFromXaf("raw/XAF/FSTAGE.XAF", "WEAPON.OL");
    weaponManager.releaseAll();
    pendingWeaponStates.clear();
    initializeTaskSystem();
    resetSceneTasks();
    battleResources = {};
    combatMotionGenerationCounter = 0;
    memset(&persistent, 0, sizeof(persistent));
    initializeGameSaveImage(primarySaveImage);
    activeSummaryIndex = 0;
    primarySaveLoaded = false;
    savedFieldResumePending = false;
    initializePersistentRecords();
    initializeStoryState(persistent.story);
    persistent.hudRadar = battleHudDefaultRadarState();
    EventVmCallbacks callbacks = {};
    callbacks.command = handleEventCommand;
    callbacks.callFunction = handleEventCallFunction;
    callbacks.inventoryCount = handleEventInventoryCount;
    callbacks.addInventory = handleEventInventoryAdd;
    callbacks.removeInventory = handleEventInventoryRemove;
    callbacks.userData = this;
    eventVm.initialize(&persistent.story, callbacks);
    EventVmCallbacks messageCallbacks = {};
    messageCallbacks.command = handleMessageCommand;
    messageCallbacks.callFunction = handleMessageCallFunction;
    messageCallbacks.inventoryCount = handleEventInventoryCount;
    messageCallbacks.addInventory = handleEventInventoryAdd;
    messageCallbacks.removeInventory = handleEventInventoryRemove;
    messageCallbacks.userData = this;
    messageVm.initialize(&persistent.story, messageCallbacks);
    pendingHostVm = NULL;
    eventWaitingForMessage = false;
    pendingEventFunction = EVENT_CALLFUNC_NONE;
    persistent.options.musicVolume = 15;
    persistent.options.effectsVolume = 15;
    persistent.options.vibration = 0;
    persistent.options.language = 1;
    persistent.options.difficulty = 2;
    persistent.options.subtitles = 1;
    persistent.options.mapDisplay = 1;
    persistent.options.gaugeDisplay = 1;
    persistent.options.bloodMale = 1;
    persistent.options.bloodFemale = 1;
    persistent.options.bruisingMale = 1;
    persistent.options.bruisingFemale = 1;
    persistent.options.controllerTypeP1 = 0;
    persistent.options.controllerTypeP2 = 0;
    persistent.options.vibrationP2 = 0;
    persistent.options.cameraXType = 0;
    persistent.options.cameraYType = 0;
    persistent.options.cameraFollow = 1;
    persistent.options.screenAdjustX = 0;
    persistent.options.screenAdjustY = 0;
    persistent.options.brightness = 10;
    persistent.options.audioOutput = 0;
    persistent.options.videoTimingMode = 0;
    initializeGameSlot(persistent.player, CHARACTER_RAVEN, 0);
    initializeGameSlot(enemy, CHARACTER_LOLA, 1);

    currentScreen = GAME_SCREEN_LANGUAGE_SELECT;
    optionsReturnScreen = GAME_SCREEN_MAIN_MENU;
    currentMode = GAME_MODE_STORY;
    matchSetupFlagValue = false;
    selectedVersusBattleType = VERSUS_PLAYER_VS_CPU;
    stageBriefingPresentationComplete = false;
    currentTitleAttractMode = TITLE_ATTRACT_VIDEO;
    titleTransitionSeconds = TITLE_TRANSITION_SECONDS;
    currentTitleAttractMovieIndex = -1;
    preTitleMoviePlayed = false;
    preTitleMovieComplete = false;
    titleAttractReset(currentTitleAttractState, false);
    currentIntroCard = {};
    currentIntroCard.state = INTRO_CARD_TASK_INACTIVE;
    scaledFrameDeltaSeconds = 0.0f;
    battleTimeScale = BATTLE_TIME_SCALE_NORMAL;
    clearSecondaryProfile();
    currentBattleOutcome = BATTLE_OUTCOME_IN_PROGRESS;
    battleElapsedSeconds = 0.0f;
    activeOpponentCount = 1;
    targetOpponentIndex = 0;
    resetBattleResultController();
    battleSupplementalExperience = 0;
    for(int i = 0; i < BATTLE_REWARD_ROSTER_SIZE; ++i)
    {
        battleRewardOpponentIds[i] = -1;
        battleScoreMultipliers[i] = -1;
    }
    memset(battleRewardOpponentActive, 0,
           sizeof(battleRewardOpponentActive));
    robberyAwardPresentationAmount = 0;
    robberyAwardPresentationPending = false;
    robberyAwardLargeSideEffectPending = false;
    memset(additionalEnemies, 0, sizeof(additionalEnemies));
    arenaRadius = 80.0f;
    frameCounter = 0;
    seedGameRandom(random, frameCounter);
    heldInput = 0;
    pressedInput = 0;
    releasedInput = 0;
    combatRepeatCount = 0;
    combatRepeatDelaySeconds = 0.0f;
    secondHeldInput = 0;
    secondPressedInput = 0;
    secondReleasedInput = 0;
    playerMovementX = 0.0f;
    playerMovementZ = 0.0f;
    playerLookX = 0.0f;
    playerLookZ = 0.0f;
    secondPlayerMovementX = 0.0f;
    secondPlayerMovementZ = 0.0f;
    secondPlayerLookX = 0.0f;
    secondPlayerLookZ = 0.0f;
    movementCameraPosition = {};
    movementCameraRightX = 1.0f;
    movementCameraRightZ = 0.0f;
    movementCameraForwardX = 0.0f;
    movementCameraForwardZ = 1.0f;
    requestedSceneEntry = NULL;
    requestedSceneCompleted = false;
    pendingBattleSpawnValid = false;
    pendingBattleSpawn = {};
    pendingBattleSpawnHeading = 0.0f;
    menuSelection = 0;
    shopReturnScreen = GAME_SCREEN_TITLE;
    currentShopMode = GAME_SHOP_BUY;
    currentShopResult = GAME_SHOP_RESULT_NONE;
    shopCategoryValue = -1;
    shopSelectionValue = 0;
    shopItemIds.clear();
    advicePendingValue = false;
    selectedVideoMode = 0;
    videoTimingModeValue = 0;
    videoTestSeconds = 0.0f;
    nominalFrameRate = 50;
    videoHalfFrameSeconds = 0.01f;
    selectedLanguage = persistent.options.language - 1;
    selectedCharacter = 0;
    selectedVersusCharacters[0] = 0;
    selectedVersusCharacters[1] = 1;
    selectedVersusSide = 0;
    versusHumanSideValue = 0;
    selectedPauseItem = 0;
    pauseMoveListOpen = false;
    pauseMoveListFirst = 0;
    selectedInventoryItem = 0;
    resetInventoryUseState();
    inventoryEquipmentReady = false;
    temporarySlowActive = false;
    temporarySlowElapsed = 0.0f;
    temporarySlowMode = 0;
    temporarySlowBattleSerial = 0;
    trackedOpponentCondition = false;
    inventoryConditionGameStateBitOne = false;
    selectedStage = 0;
    selectedOption = 0;
    selectedOptionsPage = GAME_OPTIONS_PAGE_MAIN;
    focusedInteractionSlot = -1;
    clearStageInteractions();
    memset(&optionsStaging, 0, sizeof(optionsStaging));
    memset(optionsPageBaselines, 0, sizeof(optionsPageBaselines));
    for(int i = 0; i < 5; ++i)
    {
        movieSelections[i] = 1;
        movieBaselines[i] = 1;
    }
    optionsStagingActive = false;
    optionsDefaultConfirmation = false;
    optionsDefaultSelection = 0;
    selectedMovieScene = 0;
    selectedMovieCharacterScene = 0;
    vibrationPreview[0] = false;
    vibrationPreview[1] = false;
    resetBehaviourPresentation();
    hasSave = false;
    creditsPlayed = false;
    currentCreditsSequence = {};
    currentCreditsSequence.phase = CREDITS_PHASE_FINISHED;
    memset(connectedControllers, 0, sizeof(connectedControllers));
    memset(rumble, 0, sizeof(rumble));
    clearStageActorRuntime();
    stageAiResourceLoaded = false;
    stageAiDiagnosticText.clear();
    stageAiLastDecision = {};
    stageAiDecisionValid = false;

    gameFlowTask = createUpdateTask(updateBootLanguageSelection, this, 1, 3);
}

void
Game::update(float deltaSeconds, uint32_t heldButtons, uint32_t pressedButtons)
{
    update(deltaSeconds, heldButtons, pressedButtons, 0, 0);
}

void
Game::update(float deltaSeconds,
             uint32_t firstHeldButtons, uint32_t firstPressedButtons,
             uint32_t secondHeldButtons, uint32_t secondPressedButtons)
{
    if(deltaSeconds < 0.0f)
        deltaSeconds = 0.0f;
    const float sceneDeltaSeconds = deltaSeconds;
    battleController.rawDeltaSeconds = sceneDeltaSeconds;
    advanceTemporarySlowCondition(deltaSeconds);

    if(battleFinisherPhase == BATTLE_FINISHER_FEEDBACK_RECOVERY)
        updateBattleFinisher(0.0f, sceneDeltaSeconds);

    deltaSeconds = scaleFrameDelta(deltaSeconds);

    ++frameCounter;

    scaledFrameDeltaSeconds = deltaSeconds;
    releasedInput = heldInput & ~(firstHeldButtons | firstPressedButtons);
    secondReleasedInput = secondHeldInput & ~(secondHeldButtons | secondPressedButtons);
    heldInput = firstHeldButtons;
    pressedInput = firstPressedButtons;
    secondHeldInput = secondHeldButtons;
    secondPressedInput = secondPressedButtons;

    if((pressedInput & GAME_INPUT_QUIT) != 0)
        handleQuit();
    else if(currentScreen != GAME_SCREEN_EXIT)
    {
        if(pendingEventFunction != EVENT_CALLFUNC_NONE &&
           (pressedInput & (GAME_INPUT_CONFIRM | GAME_INPUT_CANCEL)) != 0)
        {
            EventVm* waitingVm = pendingHostVm;
            if(waitingVm != NULL)
            {
                waitingVm->setMailbox(0,
                    (pressedInput & GAME_INPUT_CONFIRM) != 0 ? 1u : 0u);
                waitingVm->resumeHostCall();
            }
            pendingEventFunction = EVENT_CALLFUNC_NONE;
            pendingHostVm = NULL;
        }

        if(messageVm.running())
        {
            messageVm.update(deltaSeconds, eventInputButtons(pressedInput),
                             eventInputButtons(heldInput));
            synchronizeEventState(NULL);
        }
        if(eventWaitingForMessage && !messageVm.running())
        {
            eventWaitingForMessage = false;
            eventVm.resumeHostCall();
        }
        if(eventVm.running() && !eventWaitingForMessage)
        {
            eventVm.update(deltaSeconds, eventInputButtons(pressedInput),
                           eventInputButtons(heldInput));
            synchronizeEventState(NULL);
        }

        const bool eventRunning = eventVm.running() || messageVm.running();
        const uint32_t eventPressed = pressedInput;
        const uint32_t eventHeld = heldInput;
        if(eventRunning && hudAdvicePanelActive())
        {

            const uint32_t adviceNavigation =
                GAME_INPUT_UP | GAME_INPUT_DOWN |
                GAME_INPUT_LEFT | GAME_INPUT_RIGHT;
            pressedInput &= adviceNavigation;
        }
        else if(eventRunning && persistent.story.playerStopped)
            pressedInput = 0;
        if(eventRunning && persistent.story.playerStopped)
            heldInput = 0;

        updateSceneSequence(sceneDeltaSeconds, deltaSeconds);
        updateStageAiGenerators(deltaSeconds);
        advanceActorActions(deltaSeconds);
        runUpdateTasks();

        advanceSceneCharacterSelectOverlayFrame();
        pressedInput = eventPressed;
        heldInput = eventHeld;
    }

    if(currentScreen == GAME_SCREEN_GAMEPLAY ||
       currentScreen == GAME_SCREEN_PAUSE)
        updateRumble(deltaSeconds);
    else
        cancelAllRumble();
    pressedInput = 0;
    secondPressedInput = 0;
}

void
Game::setControllerConnected(int playerIndex, bool connected)
{
    if(playerIndex < 0 || playerIndex >= 2)
        return;
    connectedControllers[playerIndex] = connected;
    if(!connected)
    {
        cancelRumble(playerIndex);
        if(playerIndex == 0)
        {
            setPlayerMovementAxes(0.0f, 0.0f);
            setPlayerLookAxes(0.0f, 0.0f);
        }
        else
        {
            setSecondPlayerMovementAxes(0.0f, 0.0f);
            setSecondPlayerLookAxes(0.0f, 0.0f);
        }
    }
}

void
Game::setPlayerMovementAxes(float x, float z)
{
    playerMovementX = clampNormalizedAxis(x);
    playerMovementZ = clampNormalizedAxis(z);
}

void
Game::setMovementCameraPosition(float x, float y, float z)
{
    movementCameraPosition = { x, y, z };
}

void
Game::setMovementCameraAxes(float rightX, float rightZ,
                            float forwardX, float forwardZ)
{
    const float rightLength = sqrtf(rightX * rightX + rightZ * rightZ);
    const float forwardLength =
        sqrtf(forwardX * forwardX + forwardZ * forwardZ);
    if(rightLength <= 0.0001f || forwardLength <= 0.0001f)
    {
        movementCameraRightX = 1.0f;
        movementCameraRightZ = 0.0f;
        movementCameraForwardX = 0.0f;
        movementCameraForwardZ = 1.0f;
        return;
    }
    movementCameraRightX = rightX / rightLength;
    movementCameraRightZ = rightZ / rightLength;
    movementCameraForwardX = forwardX / forwardLength;
    movementCameraForwardZ = forwardZ / forwardLength;
}

const char*
Game::pendingSceneEntry(void) const
{
    return requestedSceneEntry;
}

void
Game::requestScene(const char* entryName)
{
    requestedSceneEntry = entryName;
    requestedSceneCompleted = false;
}

void
Game::completePendingScene(void)
{
    if(requestedSceneEntry == NULL)
        return;
    requestedSceneEntry = NULL;
    requestedSceneCompleted = true;

    if(currentMode == GAME_MODE_STORY &&
       persistent.story.stageTransition.queuedEventId == 0x4c)
    {
        advanceStoryProgressionPhase(persistent.story);
        StoryStageTransition& transition =
            persistent.story.stageTransition;
        transition.actionFlags = 0;
        transition.queuedEventId = 0xffff;
        transition.lifecycle = STORY_TRANSITION_IDLE;
    }
}

void
Game::setPendingBattleSpawn(float x, float y, float z,
                            float headingDegrees)
{
    pendingBattleSpawn.x = x;
    pendingBattleSpawn.y = y;
    pendingBattleSpawn.z = z;
    pendingBattleSpawnHeading = headingDegrees;
    pendingBattleSpawnValid = true;
}

void
Game::cameraRelativeAxes(float localX, float localZ,
                         float& worldX, float& worldZ) const
{
    worldX = movementCameraRightX * localX +
             movementCameraForwardX * localZ;
    worldZ = movementCameraRightZ * localX +
             movementCameraForwardZ * localZ;
}

void
Game::setPlayerLookAxes(float x, float z)
{
    playerLookX = clampNormalizedAxis(x);
    playerLookZ = clampNormalizedAxis(z);
}

void
Game::setSecondPlayerMovementAxes(float x, float z)
{
    secondPlayerMovementX = clampNormalizedAxis(x);
    secondPlayerMovementZ = clampNormalizedAxis(z);
}

void
Game::setSecondPlayerLookAxes(float x, float z)
{
    secondPlayerLookX = clampNormalizedAxis(x);
    secondPlayerLookZ = clampNormalizedAxis(z);
}

void
Game::requestRumble(int playerIndex, float durationSeconds,
                    float lowMotor, float highMotor, uint8_t priority)
{
    if(playerIndex < 0 || playerIndex >= 2)
        return;
    if(durationSeconds <= 0.0f)
    {
        cancelRumble(playerIndex);
        return;
    }
    GameRumbleState& state = rumble[playerIndex];
    if(state.active && priority < state.priority)
        return;
    if(lowMotor < 0.0f)
        lowMotor = 0.0f;
    else if(lowMotor > 1.0f)
        lowMotor = 1.0f;
    if(highMotor < 0.0f)
        highMotor = 0.0f;
    else if(highMotor > 1.0f)
        highMotor = 1.0f;
    state.remainingSeconds = durationSeconds;
    state.lowMotor = lowMotor;
    state.highMotor = highMotor;
    state.priority = priority;
    const bool vibrationEnabled = playerIndex == 0 ?
        persistent.options.vibration != 0 :
        persistent.options.vibrationP2 != 0;
    state.active = vibrationEnabled && connectedControllers[playerIndex];
}

void
Game::cancelRumble(int playerIndex)
{
    if(playerIndex < 0 || playerIndex >= 2)
        return;
    memset(&rumble[playerIndex], 0, sizeof(rumble[playerIndex]));
}

void
Game::cancelAllRumble(void)
{
    cancelRumble(0);
    cancelRumble(1);
}

void
Game::updateRumble(float deltaSeconds)
{
    for(int i = 0; i < 2; ++i)
    {
        const bool vibrationEnabled = i == 0 ?
            persistent.options.vibration != 0 :
            persistent.options.vibrationP2 != 0;
        if(!vibrationEnabled || !connectedControllers[i])
        {
            cancelRumble(i);
            continue;
        }
        if(!rumble[i].active)
            continue;
        rumble[i].remainingSeconds -= deltaSeconds;
        if(rumble[i].remainingSeconds <= 0.0f)
            cancelRumble(i);
    }
}

void
Game::switchGameFlowTaskHandler(UpdateTaskCallback* callback, uint8_t initialState)
{
    gameFlowTask->updateCallback = callback;
    gameFlowTask->state = initialState;
    gameFlowTask->reserved08 = 0;
}

void
Game::enterTitleScreenFlow(void)
{
    currentScreen = GAME_SCREEN_TITLE;
    menuSelection = 0;
    currentTitleAttractMode = TITLE_ATTRACT_INTERACTIVE;
    titleAttractReset(currentTitleAttractState, false);
    switchGameFlowTaskHandler(updateTitleScreenFlow, 0x1a);
}

uint32_t
Game::nextRandom(void)
{
    return nextGameRandom(random);
}

uint32_t Game::currentTick(void) const { return frameCounter; }

const char*
Game::localizedText(uint16_t textId, int16_t* byteLength) const
{
    return gameData.localizedText(textId, byteLength);
}

bool
Game::loadTransientText(GameTextTable table, bool owned)
{
    return gameData.loadTransientText(table, owned);
}

const GameRumbleState&
Game::rumbleState(int playerIndex) const
{
    if(playerIndex != 1)
        playerIndex = 0;
    return rumble[playerIndex];
}

const GameData& Game::data(void) const { return gameData; }

WeaponManager& Game::weapons(void) { return weaponManager; }

const WeaponManager& Game::weapons(void) const { return weaponManager; }

int Game::resultSelection(void) const { return battleResult.selectedIndex; }

const BattleResultController&
Game::battleResultController(void) const
{
    return battleResult;
}

bool Game::resultSelectorVisible(void) const
{
    return battleResult.selectorVisible;
}

bool Game::resultConfirmationActive(void) const
{
    return battleResult.confirmationActive;
}

const EventVm& Game::eventScript(void) const
{
    return messageVm.running() ? messageVm : eventVm;
}

EventCallFunctionId Game::pendingEventCall(void) const
{
    return pendingEventFunction;
}

const GameRuntimeState& Game::runtimeState(void) const { return persistent; }

GameRuntimeState& Game::runtimeState(void) { return persistent; }

const GameSaveImage& Game::gameSaveImage(void) const
{
    return primarySaveImage;
}

GameSaveImage& Game::gameSaveImage(void) { return primarySaveImage; }

const GameSlotRecord& Game::opponent(void) const { return targetOpponent(); }

int Game::opponentCount(void) const { return activeOpponentCount; }

const GameSlotRecord&
Game::opponent(int index) const
{
    if(index <= 0 || index >= activeOpponentCount)
        return enemy;
    return additionalEnemies[index - 1];
}

int Game::interactionFocusSlot(void) const { return focusedInteractionSlot; }

int Game::interactionFocus(void) const { return focusedInteractionIndex; }

const GameInteraction* Game::focusedInteraction(void) const
{
    if(focusedInteractionIndex < 0 ||
       focusedInteractionIndex >= stageInteractionCount)
        return NULL;
    return &stageInteractions[focusedInteractionIndex];
}

int16_t Game::lastInteractionSelector(void) const
{
    return lastInteractionSelectorId;
}
