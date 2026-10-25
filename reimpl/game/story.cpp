#include "game/game.h"
#include "ui/menu/inventory.h"
#include "scene/task.h"
#include "character_records.h"
#include "combat_styles.h"
#include "versus_tables.h"
#include "xaf_runtime.h"
#include "move_tables.h"
#include "action_state_handlers.h"
#include "options_menu.h"
#include "camera/camera.h"
#include "sound_cues.h"
#include "stage/stage.h"
#include "ui/hud/advice.h"
#include "formats/MENU_LAYOUT/pause_results_layout.h"
#include "formats/BATTLE_ROSTER/battle_rosters.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "game/internal.h"

using namespace game_detail;

void
Game::beginNewGame(void)
{
    weaponManager.releaseAll();
    pendingWeaponStates.clear();
    initializeGameSaveImage(primarySaveImage);
    activeSummaryIndex = 0;
    primarySaveLoaded = false;
    savedFieldResumePending = false;
    initializePersistentRecords();
    initializeStoryState(persistent.story);
    persistent.hudRadar = battleHudDefaultRadarState();
    requestedSceneEntry = NULL;
    requestedSceneCompleted = false;
    pendingBattleSpawnValid = false;
    persistent.characterId = currentMode == GAME_MODE_STORY ?
        static_cast<uint16_t>(PLAYABLE_CHARACTER_IDS[selectedCharacter]) :
        VERSUS_CHARACTER_IDS[selectedVersusCharacters[0]];
    persistent.stageId = currentMode == GAME_MODE_STORY ?
        GAME_FIRST_PLAYABLE_STAGE_ID : 0;
    selectedStage = 0;
    persistent.runtimeFlags = 0;
    initializeGameSlot(
        persistent.player, static_cast<CharacterId>(persistent.characterId), 0);
    persistent.story.stageId = persistent.stageId;
    GameSaveStageResumeState resume =
        gameSaveStageResumeState(primarySaveImage);
    resume.stageId = persistent.stageId;
    setGameSaveStageResumeState(primarySaveImage, resume);

    if(persistent.characterId < GAME_SAVE_RECORD_COUNT)
    {
        setActiveRecordId(0, static_cast<int16_t>(persistent.characterId));
        PersistentGameRecord& playerRecord =
            primarySaveImage.payload.persistentRecords[persistent.characterId];
        playerRecord.characterFlags |= 3u;
        playerRecord.profileRevealFlags = 0xffff;
        playerRecord.cash = 500;
        projectPersistentRecordToGameSlot(
            persistent.characterId, persistent.player);
        setSettingsPrimaryProfileCharacterRecruited(
            primarySaveImage.settings, persistent.characterId, true);
    }

    if(persistent.characterId == CHARACTER_RAVEN)
    {
        ++primarySaveImage.payload.moveInventoryCounts[5];
        ++primarySaveImage.payload.moveInventoryCounts[2];
    }
    else if(persistent.characterId == CHARACTER_GINA)
    {
        primarySaveImage.payload.moveInventoryCounts[1] += 2;
        primarySaveImage.payload.moveInventoryCounts[2] += 2;
    }

    hydrateStoryCharacters();
    persistent.story.stageId = persistent.stageId;
    activeOpponentCount = 0;
    synchronizeBattleStoryState();

    if(currentMode != GAME_MODE_STORY)
        switchGameFlowTaskHandler(updateStageSelectionFlow, 1);
    else
        switchGameFlowTaskHandler(handlePreGameplayTransition, 7);
}

void
Game::beginStageBriefing(void)
{
    stageBriefingPresentationComplete = currentMode == GAME_MODE_STORY;
    currentScreen = GAME_SCREEN_BRIEFING;
    switchGameFlowTaskHandler(updateStageBriefingFlow, 2);
}

void
Game::completeStageBriefingPresentation(void)
{
    if(currentScreen == GAME_SCREEN_BRIEFING)
        stageBriefingPresentationComplete = true;
}

StoryProgressionInput
Game::storyProgressionInput(void) const
{
    StoryProgressionInput input = {};
    input.currentStageId = persistent.story.stageId;
    input.destinationStageId = persistent.story.stageTransitionPending ?
        persistent.story.stageTransition.destinationStageId :
        persistent.story.stageId;
    input.sceneId = eventVm.eventId();
    input.encounterProfileId = -1;
    const int playerResource =
        characterRecordResourceId(
            static_cast<uint16_t>(persistent.player.characterId));
    input.playerResourceProfileId =
        playerResource >= 0 ? static_cast<uint16_t>(playerResource) : 0;
    input.playerCharacterId =
        static_cast<uint16_t>(persistent.player.characterId);
    input.opponentCharacterId = activeOpponentCount > 0 ?
        static_cast<uint16_t>(targetOpponent().characterId) : 0xffff;
    input.playerX = persistent.player.position.x;
    input.playerZ = persistent.player.position.z;
    input.updateTaskActive =
        persistent.story.progression.dialogueTask.active;
    input.battleFlowBlocked =
        sceneSequencer.running() || eventWaitingForMessage;
    input.interactionStateBit0 =
        persistent.story.progression.interactionStateBit0;
    input.countdownFinished = persistent.story.countdownFrames == 0;
    input.fieldReady = !sceneSequencer.running();
    input.promptConfirmed =
        (pressedInput & (GAME_INPUT_CONFIRM | GAME_INPUT_START)) != 0;

    input.activeCharacterIds[input.activeCharacterCount++] =
        static_cast<uint16_t>(persistent.player.characterId);
    if(allyActive && input.activeCharacterCount < 11)
    {
        input.activeCharacterIds[input.activeCharacterCount++] =
            static_cast<uint16_t>(ally.characterId);
    }
    for(int i = 0; i < activeOpponentCount &&
        input.activeCharacterCount < 11; ++i)
    {
        const GameSlotRecord& slot =
            i == 0 ? enemy : additionalEnemies[i - 1];
        input.activeCharacterIds[input.activeCharacterCount++] =
            static_cast<uint16_t>(slot.characterId);
    }
    return input;
}

void
Game::updateStoryRuntime(float deltaSeconds)
{
    StoryStageTransition& transition = persistent.story.stageTransition;
    if(transition.lifecycle == STORY_TRANSITION_EVENT_RUNNING &&
       !eventVm.running() && !persistent.story.stageTransitionPending &&
       requestedSceneEntry == NULL && !sceneSequencer.running())
    {
        transition.actionFlags = 0;
        transition.queuedEventId = 0xffff;
        transition.lifecycle = STORY_TRANSITION_IDLE;
    }
    StoryProgressionInput input = storyProgressionInput();
    if(storyEncounterCompletionPending &&
       transition.lifecycle != STORY_TRANSITION_POST_BATTLE)
    {
        const GameSlotRecord& defeated =
            targetOpponentIndex == 0 ? enemy :
                additionalEnemies[targetOpponentIndex - 1];
        const int resource = characterRecordResourceId(
            static_cast<uint16_t>(defeated.characterId));
        if(resource >= 0)
            input.encounterProfileId = static_cast<int16_t>(resource);
        input.encounterCompleted = input.encounterProfileId >= 0;
        input.opponentCharacterId =
            static_cast<uint16_t>(defeated.characterId);
        const uint16_t characterId =
            static_cast<uint16_t>(defeated.characterId);
        if(characterId < STORY_CHARACTER_COUNT)
        {
            const StoryCharacterState& character =
                persistent.story.characters[characterId];
            input.encounterStatus = character.persistentStatus;
            input.encounterRecruited =
                (character.characterFlags & STORY_CHARACTER_RECRUITED) != 0;
        }
        storyEncounterCompletionPending = false;
    }

    const bool storyFinished =
        updateStoryProgressionFlow(persistent.story, input);
    updateStoryProgressionTasks(
        persistent.story, input, deltaSeconds);
    StoryProgressionRuntime& runtime = persistent.story.progression;
    if(transition.lifecycle == STORY_TRANSITION_EVENT_QUEUED &&
       !eventVm.running() && !persistent.story.stageTransitionPending)
    {
        if(startStoryEvent(transition.queuedEventId, transition.routeMode))
            transition.lifecycle = STORY_TRANSITION_EVENT_RUNNING;
    }
    applyPendingStoryHeals();
    if(storyFinished)
    {
        finalizeStoryEncounterFlow(persistent.story);
        runtime.storyComplete = 1;
        persistent.runtimeFlags |= GAME_RUNTIME_STORY_COMPLETE;
        if(!creditsPlayed &&
           loadTransientText(GAME_TEXT_TABLE_STAFF, true))
        {

            creditsPlayed = true;
            resetCreditsSequence(
                currentCreditsSequence, persistent.options.language,
                nominalFrameRate == 60);
            currentScreen = GAME_SCREEN_CREDITS;
            transition.lifecycle = STORY_TRANSITION_IDLE;
            persistent.story.stageTransitionPending = 0;
            switchGameFlowTaskHandler(updateCreditsFlow, 1);
        }
    }
    const int promptGroup = refreshStoryProgressionPrompt(persistent.story);
    if(promptGroup >= 0)
        hudAdviceShowGroup(promptGroup);
    projectRuntimeStateToPersistent(primarySaveImage);
}

void
Game::applyPendingStoryHeals(void)
{
    StoryProgressionRuntime& runtime = persistent.story.progression;
    for(uint8_t request = 0; request < runtime.fullHealCount; ++request)
    {
        const uint16_t id = runtime.fullHealIds[request];
        const bool resourceProfile =
            (runtime.fullHealResourceProfileMask & (1u << request)) != 0;
        GameSlotRecord* slots[MAX_GAME_OPPONENTS + 2] =
        {
            &persistent.player, allyActive ? &ally : NULL, &enemy,
            &additionalEnemies[0], &additionalEnemies[1]
        };
        const int slotCount = activeOpponentCount + 2;
        for(int slotIndex = 0; slotIndex < slotCount; ++slotIndex)
        {
            GameSlotRecord* slot = slots[slotIndex];
            if(slot == NULL)
                continue;
            const uint16_t characterId =
                static_cast<uint16_t>(slot->characterId);
            const int slotId = resourceProfile ?
                characterRecordResourceId(characterId) : characterId;
            if(!resourceProfile && id == 0)
            {
                if(slot != &persistent.player)
                    continue;
            }
            else if(slotId != id)
                continue;
            slot->currentHealth = slot->maxHealth;
        }
    }
    runtime.fullHealCount = 0;
    runtime.fullHealResourceProfileMask = 0;
}

bool
Game::updateStoryStageTransition(float deltaSeconds)
{
    StoryState& story = persistent.story;
    if(!story.stageTransitionPending)
        return false;

    StoryStageTransition& transition = story.stageTransition;
    switch(transition.lifecycle)
    {
    case STORY_TRANSITION_POST_BATTLE:

        if((transition.controllerFlags & 1u) == 0)
            return true;
        switch(transition.postBattleState)
        {
        case 0:
            if(persistent.player.maxHealth <= 0 ||
               persistent.player.slotIndex != 0)
                return true;
            setStorySaveFlag(story, 0x02bc, true);
            transition.elapsedSeconds = 0.0f;
            transition.postBattleState =
                (transition.controllerFlags & 2u) != 0 ? 0x32 : 3;
            return true;
        case 1:
            transition.elapsedSeconds += deltaSeconds;
            if(transition.elapsedSeconds >= 2.0f)
            {
                transition.elapsedSeconds = 0.0f;
                transition.postBattleState = 2;
            }
            return true;
        case 2:
            transition.postBattleState = 3;
            return true;
        case 3:
        {
            transition.elapsedSeconds += deltaSeconds;
            if(transition.elapsedSeconds >= 1.5f)
                transition.controllerFlags |= 8u;
            const float duration =
                (transition.controllerFlags & 2u) != 0 ? 1.6f : 2.8f;
            if(transition.elapsedSeconds >= duration)
            {
                transition.elapsedSeconds = 0.0f;
                transition.postBattleState = 4;
            }
            return true;
        }
        case 0x32:
            transition.elapsedSeconds += deltaSeconds;
            if(transition.elapsedSeconds >= 0.5f)
            {
                transition.elapsedSeconds = 0.0f;
                transition.postBattleState = 4;
            }
            return true;
        case 4:
            transition.postBattleState = 5;
            return true;
        case 5:
            if(!battleResult.flowReady)
                return true;
            transition.postBattleState = 100;
            return true;
        case 100:
            transition.postBattleState = 101;
            return true;
        case 101:
            transition.postBattleState = 200;
            return true;
        case 200:
            transition.lifecycle = STORY_TRANSITION_IDLE;
            story.stageTransitionPending = 0;
            story.resultDisplayActive = 0;
            currentScreen = GAME_SCREEN_GAMEPLAY;
            return true;
        default:
            transition.postBattleState = 0;
            return true;
        }
    case STORY_TRANSITION_FIELD_UNLOAD:
        currentScreen = GAME_SCREEN_LOADING;
        finalizeStoryEncounterFlow(story);
        refreshStoryProgressionFlags(story, false);
        transition.loadState = STORY_STAGE_LOAD_OL_WEAPON;
        transition.lifecycle = STORY_TRANSITION_STAGE_LOAD;
        return true;
    case STORY_TRANSITION_STAGE_LOAD:
    case STORY_TRANSITION_FIELD_RESUME:

        return true;
    default:
        story.stageTransitionPending = 0;
        return false;
    }
}

void
Game::updateCreditsFlow(UpdateTask* task)
{
    Game& game = gameFromTask(task);
    const bool r1Pressed =
        (game.pressedInput & GAME_INPUT_GUARD) != 0;
    updateCreditsSequence(game.currentCreditsSequence, true, r1Pressed);
    if(!creditsSequenceFinished(game.currentCreditsSequence))
        return;

    game.currentMode = GAME_MODE_STORY;
    game.selectedCharacter = 0;
    game.switchGameFlowTaskHandler(updateNewGameCharacterSelectFlow, 3);
}

void
Game::updateGameplaySessionFlow(UpdateTask* task)
{
    Game& game = gameFromTask(task);

    if(task->state == 1)
    {

        game.combatDamageEnabled = true;

        game.persistent.story.gameStateFlags |=
            game.currentMode == GAME_MODE_STORY ? 1u : 3u;
        task->state = 2;
    }

    if(game.currentMode == GAME_MODE_STORY)
    {
        hudAdviceUpdate(game.scaledFrameDeltaSeconds);
        if(hudAdvicePanelActive())
        {

            if((game.pressedInput &
                (GAME_INPUT_CONFIRM | GAME_INPUT_GUARD)) != 0)
                hudAdviceConfirm();
            return;
        }

        game.updateStoryRuntime(game.scaledFrameDeltaSeconds);
        if(game.updateStoryStageTransition(
               game.scaledFrameDeltaSeconds))
            return;
    }

    if(game.currentBattleOutcome == BATTLE_OUTCOME_IN_PROGRESS &&
       (game.pressedInput & (GAME_INPUT_PAUSE | GAME_INPUT_START)) != 0)
    {
        game.selectedPauseItem = 0;
        game.pauseMoveListOpen = false;
        game.pauseMoveListFirst = 0;
        game.currentScreen = GAME_SCREEN_PAUSE;
        game.switchGameFlowTaskHandler(updatePauseFlow, 1);
        return;
    }

    if(game.sceneSequencer.running())
        return;

    game.updateBattle(game.scaledFrameDeltaSeconds);
}
