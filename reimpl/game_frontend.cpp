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

namespace game_detail
{

uint16_t characterDatabaseIndex(CharacterId characterId)
{

    const int resource =
        characterRecordResourceId(static_cast<uint16_t>(characterId));
    return resource > 0 ? static_cast<uint16_t>(resource) : 0xffff;
}

bool validVersusCharacter(uint16_t characterId)
{
    for(int i = 0; i < GAME_VERSUS_CHARACTER_COUNT; ++i)
    {
        if(characterId == VERSUS_CHARACTER_IDS[i])
            return true;
    }
    return false;
}

bool validPlayableCharacter(uint16_t characterId)
{
    for(int i = 0; i < 5; ++i)
    {
        if(characterId == static_cast<uint16_t>(PLAYABLE_CHARACTER_IDS[i]))
            return true;
    }
    return validVersusCharacter(characterId);
}

int versusStageIndexForId(uint16_t stageId)
{
    for(int i = 0; i < GAME_VERSUS_STAGE_COUNT; ++i)
    {
        if(VERSUS_STAGE_IDS[i] == stageId)
            return i;
    }
    return -1;
}

Game& gameFromTask(UpdateTask* task)
{
    return *static_cast<Game*>(task->userData);
}

int wrapIndex(int value, int count)
{
    if(value < 0)
        return count - 1;
    if(value >= count)
        return 0;
    return value;
}

bool actionRowEnabled(uint8_t actionClass, int row)
{

    return actionClass < 5 && row >= 0 && row < 3 &&
        (GAME_FRONTEND_MASKS[actionClass] & (1u << row)) != 0;
}

}

void
Game::handleQuit(void)
{
    currentScreen = GAME_SCREEN_EXIT;
}

void
Game::updateGameFlowTask(UpdateTask* task)
{
    Game& game = gameFromTask(task);

    switch(task->state)
    {
    case 1:
        memset(task->work, 0, sizeof(task->work));
        task->state = 3;
        break;
    case 2:
        ++*reinterpret_cast<int32_t*>(task->work);
        break;
    case 3:
        task->state = 4;
        break;
    case 4:
        task->state = 8;
        break;
    case 5:
    case 6:
        task->state = 7;
        break;
    case 7:
        task->state = 0x0c;
        break;
    case 8:
        game.menuSelection = 0;
        task->state = 9;
        break;
    case 9:
        task->state = 10;
        break;
    case 10:
        task->state = 0x0b;
        break;
    case 0x0b:
        task->state = 5;
        break;
    case 0x0c:
        game.switchGameFlowTaskHandler(updateTitleScreenFlow, 1);
        break;
    default:
        break;
    }
}

void
Game::updateBootLanguageSelection(UpdateTask* task)
{
    Game& game = gameFromTask(task);

    switch(task->state)
    {
    case 1:
        game.selectedLanguage = game.persistent.options.language - 1;
        game.currentScreen = GAME_SCREEN_LANGUAGE_SELECT;
        task->state = 2;
        break;
    case 2:
        if((game.pressedInput & GAME_INPUT_UP) != 0)
            game.selectedLanguage = wrapIndex(game.selectedLanguage - 1, 5);
        else if((game.pressedInput & GAME_INPUT_DOWN) != 0)
            game.selectedLanguage = wrapIndex(game.selectedLanguage + 1, 5);

        if((game.pressedInput & GAME_INPUT_CONFIRM) != 0)
        {
            const uint8_t language =
                static_cast<uint8_t>(game.selectedLanguage + 1);
            if(game.gameData.reloadLocalizedText(language))
            {
                game.persistent.options.language = language;
                game.switchGameFlowTaskHandler(updateVideoModeSelection, 1);
            }
        }
        break;
    default:
        break;
    }
}

void
Game::updateVideoModeSelection(UpdateTask* task)
{
    Game& game = gameFromTask(task);
    switch(task->state)
    {
    case 1:
        game.selectedVideoMode = game.persistent.options.videoTimingMode < 2 ?
            game.persistent.options.videoTimingMode : 0;
        game.videoTestSeconds = 0.0f;
        game.currentScreen = GAME_SCREEN_VIDEO_MODE_SELECT;
        task->state = 2;
        break;
    case 2:
        if((game.pressedInput & GAME_INPUT_UP) != 0)
            game.selectedVideoMode =
                wrapIndex(game.selectedVideoMode - 1, GAME_VIDEO_MODE_COUNT);
        else if((game.pressedInput & GAME_INPUT_DOWN) != 0)
            game.selectedVideoMode =
                wrapIndex(game.selectedVideoMode + 1, GAME_VIDEO_MODE_COUNT);
        else if((game.pressedInput & GAME_INPUT_CONFIRM) != 0)
        {
            if(game.selectedVideoMode == 2)
            {
                game.videoTestSeconds = 5.0f;
                task->state = 3;
                break;
            }
            game.videoTimingModeValue =
                static_cast<uint8_t>(game.selectedVideoMode);
            game.persistent.options.videoTimingMode =
                game.videoTimingModeValue;
            game.nominalFrameRate = game.selectedVideoMode == 0 ? 50 : 60;
            game.videoHalfFrameSeconds =
                game.selectedVideoMode == 0 ? 0.01f : 0.008333334f;
            game.currentScreen = GAME_SCREEN_NO_SAVE_DATA;
            game.switchGameFlowTaskHandler(updateNoSaveDataFlow, 1);
        }
        break;
    case 3:
        game.videoTestSeconds -= game.scaledFrameDeltaSeconds;
        if(game.videoTestSeconds <= 0.0f)
            task->state = 2;
        break;
    default:
        break;
    }
}

void
Game::updateNoSaveDataFlow(UpdateTask* task)
{
    Game& game = gameFromTask(task);
    if(task->state == 1)
    {
        game.loadTransientText(GAME_TEXT_TABLE_MCARD, false);
        game.currentScreen = GAME_SCREEN_NO_SAVE_DATA;
        task->state = 2;
    }
    else if(task->state == 2 &&
            (game.pressedInput & (GAME_INPUT_CONFIRM | GAME_INPUT_START)) != 0)
        game.switchGameFlowTaskHandler(updateGameFlowTask, 1);
}

void
Game::updateTitleScreenFlow(UpdateTask* task)
{
    Game& game = gameFromTask(task);

    switch(task->state)
    {
    case 1:
        game.currentTitleAttractMode = TITLE_ATTRACT_VIDEO;
        task->state = 4;
        break;
    case 4:
        game.menuSelection = 0;
        game.currentScreen = GAME_SCREEN_LOADING;
        task->state = 5;
        break;
    case 5:
        task->state = 6;
        break;
    case 6:
        task->state = game.currentTitleAttractMode == TITLE_ATTRACT_INTERACTIVE ?
            0x18 : 0x17;
        break;
    case 7:
        game.currentScreen = GAME_SCREEN_TITLE;
        titleAttractTick(game.currentTitleAttractState,
                         game.currentTitleAttractMode, true, false);
        task->state = 8;
        break;
    case 8:
    {
        const bool startPressed =
            (game.pressedInput &
             (GAME_INPUT_START | GAME_INPUT_CONFIRM)) != 0;
        titleAttractTick(game.currentTitleAttractState,
                         game.currentTitleAttractMode, true, startPressed);
        if(game.currentTitleAttractState.inputActive)
        {
            task->state = 0x0b;
            break;
        }
        game.titleTransitionSeconds -= game.scaledFrameDeltaSeconds;
        if(game.titleTransitionSeconds <= 0.0f)
            task->state = 9;
        break;
    }
    case 9:
        task->state = 10;
        break;
    case 10:
        game.currentTitleAttractMode = TITLE_ATTRACT_VIDEO;
        task->state = 4;
        break;
    case 0x0b:
        game.currentScreen = GAME_SCREEN_MAIN_MENU;
        task->state = 0x0c;
        break;
    case 0x0c:

        titleAttractTick(game.currentTitleAttractState,
                         game.currentTitleAttractMode, true, false);
        if((game.pressedInput & (GAME_INPUT_UP | GAME_INPUT_DOWN)) != 0)
        {
            const int step =
                (game.pressedInput & GAME_INPUT_UP) != 0 ? -1 : 1;
            game.menuSelection =
                wrapIndex(game.menuSelection + step, TITLE_MENU_ENTRY_COUNT);
        }
        else if((game.pressedInput & GAME_INPUT_CANCEL) != 0)
        {
            game.currentTitleAttractMode = TITLE_ATTRACT_VIDEO;
            game.titleTransitionSeconds = 10.0f;
            titleAttractReset(game.currentTitleAttractState, true);
            game.currentScreen = GAME_SCREEN_TITLE;
            task->state = 7;
        }
        else if((game.pressedInput & GAME_INPUT_CONFIRM) != 0)
            task->state =
                game.menuSelection == TITLE_MENU_OPTIONS ? 0x0e : 0x0d;
        break;
    case 0x0d:
        task->state = 0x0e;
        break;
    case 0x0e:
        switch(game.menuSelection)
        {
        case TITLE_MENU_NEW_GAME:
            game.currentMode = GAME_MODE_STORY;
            game.selectedCharacter = 0;
            game.switchGameFlowTaskHandler(handlePreGameplayTransition, 1);
            break;
        case TITLE_MENU_LOAD_GAME:
            if(game.hasSave)
                game.beginStageBriefing();
            else
            {

                game.loadTransientText(GAME_TEXT_TABLE_MCARD, false);
                game.currentScreen = GAME_SCREEN_NO_SAVE_DATA;
                task->state = 0x0f;
            }
            break;
        case TITLE_MENU_VS_NORMAL:
        case TITLE_MENU_VS_CUSTOM:
            game.currentMode =
                game.menuSelection == TITLE_MENU_VS_NORMAL ?
                GAME_MODE_VERSUS_NORMAL : GAME_MODE_VERSUS_CUSTOM;
            game.selectedVersusBattleType = VERSUS_PLAYER_VS_CPU;
            game.switchGameFlowTaskHandler(updateVersusMatchSetupFlow, 1);
            break;
        case TITLE_MENU_OPTIONS:
            task->state = 0x11;
            break;
        default:
            task->state = 0x0c;
            break;
        }
        break;
    case 0x0f:
        if((game.pressedInput &
            (GAME_INPUT_CONFIRM | GAME_INPUT_CANCEL | GAME_INPUT_START)) != 0)
        {
            game.currentScreen = GAME_SCREEN_MAIN_MENU;
            task->state = 0x0c;
        }
        break;
    case 0x11:
        game.optionsReturnScreen = GAME_SCREEN_MAIN_MENU;
        game.selectedOption = 0;
        game.selectedOptionsPage = GAME_OPTIONS_PAGE_MAIN;
        game.currentScreen = GAME_SCREEN_OPTIONS;
        game.switchGameFlowTaskHandler(updateOptionsMenuTask, 1);
        break;
    case 0x16:
        task->state = 0x0b;
        break;
    case 0x17:

        if(game.currentScreen != GAME_SCREEN_ATTRACT_MOVIE)
        {
            game.currentTitleAttractMovieIndex =
                (game.currentTitleAttractMovieIndex + 1) %
                TITLE_ATTRACT_MOVIE_COUNT;
        }
        if(!game.preTitleMoviePlayed)
        {
            game.currentScreen = GAME_SCREEN_ATTRACT_MOVIE;
            if(!game.preTitleMovieComplete)
                break;
            game.preTitleMovieComplete = false;
            game.preTitleMoviePlayed = true;
        }
        task->state = 0x18;
        [[fallthrough]];
    case 0x18:
        game.titleTransitionSeconds = TITLE_TRANSITION_SECONDS;
        titleAttractReset(game.currentTitleAttractState, false);
        game.currentScreen = GAME_SCREEN_TITLE;
        task->state = 7;
        break;
    case 0x1a:
        game.currentScreen = GAME_SCREEN_LOADING;
        game.currentTitleAttractMode = TITLE_ATTRACT_INTERACTIVE;
        task->state = 5;
        break;
    default:
        break;
    }
}

void
Game::updateVersusMatchSetupFlow(UpdateTask* task)
{
    Game& game = gameFromTask(task);
    const uint32_t bothPressed =
        game.pressedInput | game.secondPressedInput;

    switch(task->state)
    {
    case 1:

        game.matchSetupFlagValue = true;
        game.currentScreen = GAME_SCREEN_VERSUS_TYPE_SELECT;
        task->state = 2;
        break;
    case 2:
        if((bothPressed & GAME_INPUT_UP) != 0)
            game.selectedVersusBattleType = static_cast<VersusBattleType>(
                wrapIndex(game.selectedVersusBattleType - 1,
                          VERSUS_BATTLE_TYPE_COUNT));
        else if((bothPressed & GAME_INPUT_DOWN) != 0)
            game.selectedVersusBattleType = static_cast<VersusBattleType>(
                wrapIndex(game.selectedVersusBattleType + 1,
                          VERSUS_BATTLE_TYPE_COUNT));
        else if((bothPressed & GAME_INPUT_CANCEL) != 0)
        {

            game.matchSetupFlagValue = false;
            game.enterTitleScreenFlow();
        }
        else if((bothPressed &
                 (GAME_INPUT_CONFIRM | GAME_INPUT_START)) != 0)
        {
            game.selectedVersusSide = 0;
            game.versusHumanSideValue = 0;
            game.selectedVersusCharacters[0] = 0;
            game.selectedVersusCharacters[1] = 1;
            game.versusControllers[0] = 0;
            game.versusControllers[1] =
                game.selectedVersusBattleType ==
                    VERSUS_PLAYER_VS_PLAYER ? 1 : 0;
            game.versusReady[0] = false;
            game.versusReady[1] = false;
            game.versusActive[0] = true;
            game.versusActive[1] =
                game.selectedVersusBattleType ==
                    VERSUS_PLAYER_VS_PLAYER;
            game.currentScreen = GAME_SCREEN_CHARACTER_SELECT;
            task->state = 3;
        }
        break;
    case 3:
    {
        bool stopAfterActivation = false;
        for(int side = 0; side < 2 && !stopAfterActivation; ++side)
        {
            if(!game.versusActive[side] || game.versusReady[side])
                continue;
            const uint32_t sidePressed =
                game.versusControllers[side] == 0 ?
                game.pressedInput : game.secondPressedInput;
            int& selection = game.selectedVersusCharacters[side];
            if((sidePressed & (GAME_INPUT_LEFT | GAME_INPUT_UP)) != 0)
                selection = wrapIndex(
                    selection - 1, GAME_VERSUS_CHARACTER_COUNT);
            else if((sidePressed &
                     (GAME_INPUT_RIGHT | GAME_INPUT_DOWN)) != 0)
                selection = wrapIndex(
                    selection + 1, GAME_VERSUS_CHARACTER_COUNT);
            else if((sidePressed & GAME_INPUT_CANCEL) != 0)
            {

                game.matchSetupFlagValue = false;
                game.versusActive[0] = false;
                game.versusActive[1] = false;
                game.versusReady[0] = false;
                game.versusReady[1] = false;
                game.currentScreen = GAME_SCREEN_VERSUS_TYPE_SELECT;
                task->state = 2;
                return;
            }
            else if((sidePressed &
                     (GAME_INPUT_CONFIRM | GAME_INPUT_START)) != 0)
            {
                game.versusReady[side] = true;
                game.versusActive[side] = false;
                if(side == 0 && !game.versusReady[1] &&
                   !game.versusActive[1])
                {
                    game.versusActive[1] = true;
                    game.selectedVersusSide = 1;
                    stopAfterActivation = true;
                }
            }
        }
        if(game.versusReady[0] && game.versusReady[1])
            game.beginNewGame();
        break;
    }
    case 5:
        game.currentScreen = GAME_SCREEN_VERSUS_TYPE_SELECT;
        task->state = 2;
        break;
    case 7:
        game.currentScreen = GAME_SCREEN_CHARACTER_SELECT;
        task->state = 3;
        break;
    default:
        break;
    }
}

void
Game::beginIntroCardSequence(void)
{
    currentIntroCard = {};
    currentIntroCard.state = INTRO_CARD_TASK_SETUP;
    currentIntroCard.maskAlpha = GAME_INTRO_CARD_MASK_ALPHA_MAX;
}

void
Game::updateIntroCardSequence(void)
{
    switch(currentIntroCard.state)
    {
    case INTRO_CARD_TASK_SETUP:
        currentIntroCard.textAlpha = 0;
        currentIntroCard.holdSeconds =
            INTRO_CARD_DESCRIPTOR_SECONDS -
            INTRO_CARD_FADE_ALLOWANCE_SECONDS;
        currentIntroCard.state = INTRO_CARD_TASK_FADE_IN;
        break;
    case INTRO_CARD_TASK_FADE_IN:
        if(scaledFrameDeltaSeconds != 0.0f)
        {
            const int alpha =
                currentIntroCard.textAlpha + INTRO_CARD_TEXT_FADE_STEP;
            if(alpha > INTRO_CARD_TEXT_FADE_THRESHOLD)
            {
                currentIntroCard.textAlpha = 0xff;
                currentIntroCard.state = INTRO_CARD_TASK_SOUND_GATE;
            }
            else
                currentIntroCard.textAlpha = static_cast<uint8_t>(alpha);
        }
        break;
    case INTRO_CARD_TASK_SOUND_GATE:
        currentIntroCard.state = INTRO_CARD_TASK_HOLD;
        break;
    case INTRO_CARD_TASK_HOLD:
        if(currentIntroCard.holdSeconds > 0.0f)
        {
            currentIntroCard.holdSeconds -= scaledFrameDeltaSeconds;
            if(currentIntroCard.holdSeconds < 0.0f)
                currentIntroCard.holdSeconds = 0.0f;
        }
        if(currentIntroCard.holdSeconds == 0.0f)
            currentIntroCard.state = INTRO_CARD_TASK_FADE_OUT;
        break;
    case INTRO_CARD_TASK_FADE_OUT:
        if(scaledFrameDeltaSeconds != 0.0f)
        {
            const int alpha =
                currentIntroCard.textAlpha - INTRO_CARD_TEXT_FADE_STEP;
            if(alpha < 0)
            {
                currentIntroCard.textAlpha = 0;
                ++currentIntroCard.recordIndex;
                currentIntroCard.state =
                    currentIntroCard.recordIndex < INTRO_CARD_RECORD_COUNT ?
                    INTRO_CARD_TASK_SETUP : INTRO_CARD_TASK_COMPLETE;
            }
            else
                currentIntroCard.textAlpha = static_cast<uint8_t>(alpha);
        }
        break;
    default:
        break;
    }
}

void
Game::completeIntroCardSequence(void)
{
    currentIntroCard = {};
    currentIntroCard.state = INTRO_CARD_TASK_INACTIVE;
}

void
Game::handlePreGameplayTransition(UpdateTask* task)
{
    Game& game = gameFromTask(task);

    switch(task->state)
    {
    case 1:

        game.currentScreen = GAME_SCREEN_LOADING;
        game.beginIntroCardSequence();
        task->state = 2;
        break;
    case 2:
        game.updateIntroCardSequence();
        if(game.currentIntroCard.maskAlpha <= INTRO_CARD_MASK_FADE_STEP)
        {
            game.currentIntroCard.maskAlpha = 0;
            task->state = 3;
        }
        else
        {
            game.currentIntroCard.maskAlpha -=
                INTRO_CARD_MASK_FADE_STEP;
        }
        break;
    case 3:

        if((game.pressedInput & GAME_INPUT_DODGE) != 0 ||
           game.currentIntroCard.state == INTRO_CARD_TASK_COMPLETE)
        {
            task->state = 4;
            break;
        }
        game.updateIntroCardSequence();
        break;
    case 4:
        game.updateIntroCardSequence();
        if(game.currentIntroCard.maskAlpha + INTRO_CARD_MASK_FADE_STEP >=
           GAME_INTRO_CARD_MASK_ALPHA_MAX)
        {
            game.currentIntroCard.maskAlpha =
                GAME_INTRO_CARD_MASK_ALPHA_MAX;
            task->state = 5;
        }
        else
        {
            game.currentIntroCard.maskAlpha +=
                INTRO_CARD_MASK_FADE_STEP;
        }
        break;
    case 5:
        game.completeIntroCardSequence();
        task->state = 6;
        break;
    case 6:
        game.currentScreen = GAME_SCREEN_LOADING;
        game.requestScene("SCENE_01.SCN");
        task->state = 0x0b;
        break;
    case 7:
        if(game.currentMode != GAME_MODE_STORY)
        {
            task->state = 9;
            break;
        }
        game.currentScreen = GAME_SCREEN_LOADING;
        game.requestScene("SCENE_74A.SCN");
        task->state = 8;
        break;
    case 8:
        if(game.currentMode == GAME_MODE_STORY &&
           !game.requestedSceneCompleted)
            break;
        game.requestedSceneCompleted = false;
        if(game.currentMode == GAME_MODE_STORY)
            game.requestScene("SCENE_75A.SCN");
        task->state = 9;
        break;
    case 9:
        if(game.currentMode == GAME_MODE_STORY &&
           !game.requestedSceneCompleted)
            break;
        game.requestedSceneCompleted = false;
        game.currentScreen = GAME_SCREEN_LOADING;
        game.switchGameFlowTaskHandler(updateStageBriefingFlow, 3);
        break;
    case 0x0b:
        if(!game.requestedSceneCompleted)
            break;
        game.requestedSceneCompleted = false;
        game.switchGameFlowTaskHandler(updateNewGameCharacterSelectFlow, 1);
        break;
    default:
        break;
    }
}

static int
storyCharacterSelectFrameCount(const Game& game, int authoredFrames)
{
    return game.nominalFrameRateHz() == 60 ?
        authoredFrames : authoredFrames * 5 / 6;
}

static float
storyCharacterSelectRatio(int frame, int frameCount)
{
    if(frame <= 0)
        return 0.0f;
    if(frame >= frameCount)
        return 1.0f;
    return static_cast<float>(frame) / static_cast<float>(frameCount);
}

static uint8_t
storyCharacterSelectAlpha(float value)
{
    if(value <= 0.0f)
        return 0;
    if(value >= 255.0f)
        return 0xff;
    return static_cast<uint8_t>(value + 0.5f);
}

static void
resetStoryCharacterSelectPresentation(
    StoryCharacterSelectPresentation& presentation)
{
    presentation = {};
    presentation.phase = STORY_CHARACTER_SELECT_LOADING;
    presentation.outgoingSelection = -1;
    presentation.incomingPortraitX = 287.0f;
    presentation.incomingPortraitScale = 1.0f;
    presentation.incomingPortraitAlpha = 0xff;
    presentation.outgoingPortraitX = 287.0f;
    presentation.statsX = 68.0f;
    presentation.leftArrowX = -101.0f;
    presentation.rightArrowX = 130.0f;
    presentation.primaryAlpha = 0xff;
    presentation.portraitAlpha = 0xff;
    presentation.portraitScale = 1.0f;
}

static void
beginStoryCharacterSelectMove(StoryCharacterSelectPresentation& presentation,
                              int outgoingSelection, bool towardNext)
{
    presentation.phase = STORY_CHARACTER_SELECT_MOVING;
    presentation.outgoingSelection = outgoingSelection;
    presentation.movementFrame = 0;
    presentation.incomingPortraitX = 287.0f;
    presentation.incomingPortraitScale = 1.5f;
    presentation.incomingPortraitAlpha = 0;
    presentation.outgoingPortraitX = 287.0f;
    presentation.outgoingPortraitAlpha = 0xff;
    presentation.statsX = towardNext ? 708.0f : -640.0f;
    presentation.leftArrowX = -94.0f;
    presentation.rightArrowX = 123.0f;
    presentation.portraitAlpha = 0;
    presentation.portraitScale = 1.5f;
}

static void
advanceStoryCharacterSelectMove(
    const Game& game, StoryCharacterSelectPresentation& presentation)
{
    ++presentation.movementFrame;
    const int shortFrames = storyCharacterSelectFrameCount(game, 6);
    const int longFrames = storyCharacterSelectFrameCount(game, 12);
    const float shortRatio =
        storyCharacterSelectRatio(presentation.movementFrame, shortFrames);
    const float longRatio =
        storyCharacterSelectRatio(presentation.movementFrame, longFrames);
    const bool towardNext = presentation.statsX > 68.0f;

    presentation.incomingPortraitScale = 1.5f - 0.5f * shortRatio;
    presentation.incomingPortraitAlpha =
        storyCharacterSelectAlpha(255.0f * shortRatio);
    presentation.outgoingPortraitX = 287.0f + 353.0f * longRatio;
    presentation.outgoingPortraitAlpha =
        storyCharacterSelectAlpha(255.0f * (1.0f - longRatio));
    presentation.statsX = towardNext ?
        708.0f - 640.0f * shortRatio :
        -640.0f + 708.0f * shortRatio;
    presentation.leftArrowX = -94.0f - 7.0f * longRatio;
    presentation.rightArrowX = 123.0f + 7.0f * longRatio;
    presentation.portraitAlpha = presentation.incomingPortraitAlpha;
    presentation.portraitScale = presentation.incomingPortraitScale;

    if(presentation.movementFrame >= longFrames)
    {
        presentation.phase = STORY_CHARACTER_SELECT_ACTIVE;
        presentation.outgoingSelection = -1;
        presentation.incomingPortraitScale = 1.0f;
        presentation.incomingPortraitAlpha = 0xff;
        presentation.outgoingPortraitAlpha = 0;
        presentation.statsX = 68.0f;
        presentation.leftArrowX = -101.0f;
        presentation.rightArrowX = 130.0f;
        presentation.portraitAlpha = 0xff;
        presentation.portraitScale = 1.0f;
    }
}

static void
beginStoryCharacterSelectConfirmation(
    StoryCharacterSelectPresentation& presentation)
{
    presentation.phase = STORY_CHARACTER_SELECT_CONFIRMING;
    presentation.confirmFrame = 0;
    presentation.primaryAlpha = 0xff;
    presentation.portraitAlpha = 0xff;
    presentation.portraitScale = 1.0f;
}

static void
advanceStoryCharacterSelectConfirmation(
    const Game& game, StoryCharacterSelectPresentation& presentation)
{
    ++presentation.confirmFrame;
    const float primaryRatio = storyCharacterSelectRatio(
        presentation.confirmFrame,
        storyCharacterSelectFrameCount(game, 45));
    const float portraitRatio = storyCharacterSelectRatio(
        presentation.confirmFrame,
        storyCharacterSelectFrameCount(game, 67));
    presentation.primaryAlpha =
        storyCharacterSelectAlpha(255.0f * (1.0f - primaryRatio));
    presentation.portraitAlpha =
        storyCharacterSelectAlpha(255.0f * (1.0f - portraitRatio));
    presentation.incomingPortraitAlpha = presentation.portraitAlpha;
    presentation.portraitScale = 1.0f + 0.2f * portraitRatio;
    presentation.incomingPortraitScale = presentation.portraitScale;
}

void
Game::updateNewGameCharacterSelectFlow(UpdateTask* task)
{
    Game& game = gameFromTask(task);
    StoryCharacterSelectPresentation& presentation =
        game.storyCharacterSelect;

    switch(task->state)
    {
    case 1:
    case 3:
        game.selectedCharacter = 0;
        game.storyCharacterSelectReady = false;
        resetStoryCharacterSelectPresentation(presentation);
        completeSceneCharacterSelectFadeIn();
        setSceneCharacterSelectCommand(
            4, getStorySaveFlag(
                game.persistent.story,
                SCENE_CHARACTER_SELECT_CONFIRM_BLOCK_FLAG));
        presentation.menuCommand = 4;
        task->state = 0x10;
        break;
    case 0x10:
        if(sceneCharacterSelectReady(0))
        {
            game.currentScreen = GAME_SCREEN_CHARACTER_SELECT;
            presentation.phase = STORY_CHARACTER_SELECT_ACTIVE;
            game.storyCharacterSelectReady = true;
            task->state = 0x11;
        }
        break;
    case 0x11:
        if(presentation.phase == STORY_CHARACTER_SELECT_MOVING)
        {
            advanceStoryCharacterSelectMove(game, presentation);
            break;
        }
        if((game.pressedInput & (GAME_INPUT_LEFT | GAME_INPUT_UP)) != 0)
        {

            const int outgoing = game.selectedCharacter;
            game.selectedCharacter = GAME_FRONTEND_PREVIOUS[game.selectedCharacter];
            beginStoryCharacterSelectMove(presentation, outgoing, false);
        }
        else if((game.pressedInput &
                 (GAME_INPUT_RIGHT | GAME_INPUT_DOWN)) != 0)
        {

            const int outgoing = game.selectedCharacter;
            game.selectedCharacter = GAME_FRONTEND_NEXT[game.selectedCharacter];
            beginStoryCharacterSelectMove(presentation, outgoing, true);
        }
        else if((game.pressedInput & GAME_INPUT_CANCEL) != 0)
            task->state = 0x15;
        else if((game.pressedInput & GAME_INPUT_CONFIRM) != 0)
        {
            beginStoryCharacterSelectConfirmation(presentation);
            task->state = 0x17;
        }
        break;
    case 0x15:
        setSceneCharacterSelectCommand(3);
        presentation.menuCommand = 3;
        presentation.phase = STORY_CHARACTER_SELECT_CANCEL_WAIT;
        game.storyCharacterSelectReady = false;
        task->state = 0x16;
        break;
    case 0x16:
        if(sceneCharacterSelectReady(1))
            game.enterTitleScreenFlow();
        break;
    case 0x17:
        advanceStoryCharacterSelectConfirmation(game, presentation);
        if(presentation.confirmFrame >=
           storyCharacterSelectFrameCount(game, 45))
        {
            *reinterpret_cast<int32_t*>(task->work) = 30;
            task->state = 0x18;
        }
        break;
    case 0x18:
        advanceStoryCharacterSelectConfirmation(game, presentation);
        if(--*reinterpret_cast<int32_t*>(task->work) < 1)
        {
            *reinterpret_cast<int32_t*>(task->work) = 0;
            setSceneCharacterSelectCommand(3);
            presentation.menuCommand = 3;
            presentation.phase = STORY_CHARACTER_SELECT_MENU_WAIT;
            task->state = 0x19;
        }
        break;
    case 0x19:
        advanceStoryCharacterSelectConfirmation(game, presentation);
        if(sceneCharacterSelectReady(1))
            task->state = 0x1a;
        break;
    case 0x1a:
        game.storyCharacterSelectReady = false;

        setSceneCharacterSelectCommand(4);
        presentation.menuCommand = 4;
        game.beginNewGame();
        break;
    default:
        break;
    }
}

void
Game::updateAdviceFlow(UpdateTask* task)
{
    Game& game = gameFromTask(task);
    if(task->state == 1)
    {
        game.currentScreen = GAME_SCREEN_ADVICE;
        task->state = 2;
    }
    else if(task->state == 2 &&
            (game.pressedInput & (GAME_INPUT_CONFIRM | GAME_INPUT_CANCEL |
                                  GAME_INPUT_START)) != 0)
        game.dismissAdvice();
}

void
Game::updateShopFlow(UpdateTask* task)
{
    Game& game = gameFromTask(task);
    if(task->state == 1)
    {
        game.rebuildShopItems();
        game.currentScreen = GAME_SCREEN_SHOP;
        task->state = 2;
        return;
    }
    if(task->state != 2)
        return;
    if((game.pressedInput & GAME_INPUT_UP) != 0)
        game.setShopSelection(game.shopSelectionValue - 1);
    else if((game.pressedInput & GAME_INPUT_DOWN) != 0)
        game.setShopSelection(game.shopSelectionValue + 1);
    else if((game.pressedInput & GAME_INPUT_LEFT) != 0 ||
            (game.pressedInput & GAME_INPUT_RIGHT) != 0)
        game.setShopMode(game.currentShopMode == GAME_SHOP_BUY ?
                         GAME_SHOP_SELL : GAME_SHOP_BUY);
    else if((game.pressedInput & GAME_INPUT_CANCEL) != 0)
        game.closeShop();
    else if((game.pressedInput & GAME_INPUT_CONFIRM) != 0)
    {
        if(game.currentShopMode == GAME_SHOP_BUY)
            game.buyShopSelection();
        else
            game.sellShopSelection();
    }
}

void
Game::updateStageSelectionFlow(UpdateTask* task)
{
    Game& game = gameFromTask(task);
    const uint32_t bothPressed =
        game.pressedInput | game.secondPressedInput;

    switch(task->state)
    {
    case 1:
        if(game.currentMode == GAME_MODE_STORY)
        {
            game.beginStageBriefing();
            break;
        }
        game.selectedStage = versusStageIndexForId(game.persistent.stageId);
        if(game.selectedStage < 0)
            game.selectedStage = 0;
        game.currentScreen = GAME_SCREEN_STAGE_SELECT;
        task->state = 2;
        break;
    case 2:
        if((bothPressed & GAME_INPUT_UP) != 0)
            game.selectedStage =
                wrapIndex(game.selectedStage - 1, GAME_VERSUS_STAGE_COUNT);
        else if((bothPressed & GAME_INPUT_DOWN) != 0)
            game.selectedStage =
                wrapIndex(game.selectedStage + 1, GAME_VERSUS_STAGE_COUNT);
        else if((bothPressed & GAME_INPUT_CANCEL) != 0)
        {

            game.selectedVersusSide = 1;
            game.currentScreen = GAME_SCREEN_CHARACTER_SELECT;
            game.switchGameFlowTaskHandler(updateVersusMatchSetupFlow, 3);
        }
        else if((bothPressed &
                 (GAME_INPUT_CONFIRM | GAME_INPUT_START)) != 0)
        {
            game.persistent.stageId = VERSUS_STAGE_IDS[game.selectedStage];
            game.persistent.story.stageId = game.persistent.stageId;
            game.beginStageBriefing();
        }
        break;
    default:
        break;
    }
}

void
Game::updateStageBriefingFlow(UpdateTask* task)
{
    Game& game = gameFromTask(task);
    const uint32_t bothPressed =
        game.pressedInput | game.secondPressedInput;
    switch(task->state)
    {
    case 2:
        if((bothPressed & GAME_INPUT_CANCEL) != 0)
        {
            if(game.currentMode == GAME_MODE_STORY)
                game.enterTitleScreenFlow();
            else
                game.switchGameFlowTaskHandler(updateStageSelectionFlow, 1);
            return;
        }
        if(game.stageBriefingPresentationComplete)
        {
            game.stageBriefingPresentationComplete = false;
            game.currentScreen = GAME_SCREEN_LOADING;
            task->state = 3;
            break;
        }
        if((bothPressed &
            (GAME_INPUT_CONFIRM | GAME_INPUT_START)) != 0)
        {
            game.currentScreen = GAME_SCREEN_LOADING;
            task->state = 3;
        }
        break;
    case 3:
        if(game.currentMode == GAME_MODE_STORY)
            game.initializeLoadedStoryField();
        else
            game.initializeBattle();

        game.installStageSceneSequenceCaption();
        game.sceneSequencer.setCaptionShadowEnabled(false);
        game.startSceneSequence(7, 0, 1);
        game.switchGameFlowTaskHandler(updateStageBriefingFlow, 4);
        break;
    case 4:

        if((bothPressed &
            (GAME_INPUT_CONFIRM | GAME_INPUT_START | GAME_INPUT_CANCEL)) != 0)
            game.stopSceneSequence();
        if(!game.sceneSequencer.complete())
            break;

        game.sceneSequencer.stopOverlayFade();

        task->state = 5;
        break;
    case 5:
        if(game.sceneSequencer.overlayActive())
            break;
        game.combatDamageEnabled = true;

        game.persistent.story.gameStateFlags |=
            game.currentMode == GAME_MODE_STORY ? 1u : 3u;
        game.switchGameFlowTaskHandler(updateGameplaySessionFlow, 2);
        break;
    default:
        break;
    }
}

void
Game::updatePauseFlow(UpdateTask* task)
{
    Game& game = gameFromTask(task);

    if(task->state == 1)
        task->state = 3;

    if(game.pauseMoveListOpen)
    {
        if((game.pressedInput &
            (GAME_INPUT_CANCEL | GAME_INPUT_PAUSE | GAME_INPUT_START)) != 0)
        {
            game.pauseMoveListOpen = false;
            game.pauseMoveListFirst = 0;
            return;
        }

        const int count = game.pauseMoveListVisibleEntries(NULL, 0);
        const int lastFirst = count > MOVE_LIST_VISIBLE_ROW_COUNT ?
            count - MOVE_LIST_VISIBLE_ROW_COUNT : 0;
        if((game.pressedInput & GAME_INPUT_UP) != 0 &&
           game.pauseMoveListFirst > 0)
            --game.pauseMoveListFirst;
        else if((game.pressedInput & GAME_INPUT_DOWN) != 0 &&
                game.pauseMoveListFirst < lastFirst)
            ++game.pauseMoveListFirst;
        return;
    }

    if((game.pressedInput & (GAME_INPUT_UP | GAME_INPUT_DOWN)) != 0)
    {
        const int step =
            (game.pressedInput & GAME_INPUT_UP) != 0 ? -1 : 1;
        const int rowCount =
            game.currentMode == GAME_MODE_STORY ?
            PAUSE_ROW_COUNT : BATTLE_PAUSE_ROW_COUNT;
        game.selectedPauseItem =
            wrapIndex(game.selectedPauseItem + step, rowCount);
    }
    else if((game.pressedInput &
             (GAME_INPUT_CANCEL | GAME_INPUT_PAUSE | GAME_INPUT_START)) != 0)
    {
        game.currentScreen = GAME_SCREEN_GAMEPLAY;
        game.switchGameFlowTaskHandler(updateGameplaySessionFlow, 2);
    }
    else if((game.pressedInput & GAME_INPUT_CONFIRM) != 0 &&
            game.pauseMenuItemEnabled(game.selectedPauseItem))
    {
        switch(game.selectedPauseItem)
        {
        case 0:
            game.currentScreen = GAME_SCREEN_GAMEPLAY;
            game.switchGameFlowTaskHandler(updateGameplaySessionFlow, 2);
            break;
        case 1:
            game.pauseMoveListOpen = true;
            game.pauseMoveListFirst = 0;
            break;
        case 2:
            if(game.currentMode == GAME_MODE_STORY)
                game.enterTitleScreenFlow();
            else
            {
                game.reloadBattleForRetry();
                game.switchGameFlowTaskHandler(
                    updateGameplaySessionFlow, 1);
            }
            break;
        case 3:
            game.cleanupBattleResultSession();
            game.currentScreen = GAME_SCREEN_VERSUS_TYPE_SELECT;
            game.switchGameFlowTaskHandler(updateVersusMatchSetupFlow, 5);
            break;
        case 4:
            game.cleanupBattleResultSession();
            game.currentScreen = GAME_SCREEN_CHARACTER_SELECT;
            game.switchGameFlowTaskHandler(updateVersusMatchSetupFlow, 7);
            break;
        case 5:
            game.handleQuit();
            break;
        }
    }
}

const CreditsSequenceState&
Game::creditsSequence(void) const
{
    return currentCreditsSequence;
}

GameMode Game::mode(void) const { return currentMode; }

bool Game::matchSetupFlag(void) const { return matchSetupFlagValue; }

uint8_t Game::flowState(void) const { return gameFlowTask == NULL ? 0 : gameFlowTask->state; }

int Game::languageSelection(void) const { return selectedLanguage; }

int Game::mainMenuSelection(void) const { return menuSelection; }

int Game::characterSelection(void) const
{
    return currentMode == GAME_MODE_STORY ?
        selectedCharacter : selectedVersusCharacters[selectedVersusSide];
}

uint16_t Game::selectedCharacterId(void) const
{
    if(currentMode != GAME_MODE_STORY)
        return versusSelectedCharacterId(selectedVersusSide);
    if(selectedCharacter < 0 || selectedCharacter >= 5)
        return CHARACTER_RAVEN;
    return static_cast<uint16_t>(PLAYABLE_CHARACTER_IDS[selectedCharacter]);
}

void Game::setCharacterSelection(int index)
{
    if(currentMode == GAME_MODE_STORY)
        selectedCharacter = wrapIndex(index, 5);
    else
        selectedVersusCharacters[selectedVersusSide] =
            wrapIndex(index, GAME_VERSUS_CHARACTER_COUNT);
}

void Game::setStageSelection(int index)
{
    if(currentMode != GAME_MODE_STORY)
        selectedStage = wrapIndex(index, GAME_VERSUS_STAGE_COUNT);
    else
        selectedStage = index < 0 ? 0 : index;
}

int Game::versusStageCount(void) { return GAME_VERSUS_STAGE_COUNT; }

uint16_t Game::versusStageId(int index)
{
    if(index < 0 || index >= GAME_VERSUS_STAGE_COUNT)
        return 0;
    return VERSUS_STAGE_IDS[index];
}

int Game::versusStageIndex(uint16_t stageId)
{
    return versusStageIndexForId(stageId);
}

int Game::versusCharacterCount(void) { return GAME_VERSUS_CHARACTER_COUNT; }

uint16_t Game::versusCharacterId(int index)
{
    if(index < 0 || index >= GAME_VERSUS_CHARACTER_COUNT)
        return 0;
    return VERSUS_CHARACTER_IDS[index];
}

int Game::versusCharacterIndex(uint16_t characterId)
{
    for(int i = 0; i < GAME_VERSUS_CHARACTER_COUNT; ++i)
        if(VERSUS_CHARACTER_IDS[i] == characterId)
            return i;
    return -1;
}

VersusBattleType Game::versusBattleType(void) const
{
    return selectedVersusBattleType;
}

int Game::versusBattleTypeSelection(void) const
{
    return static_cast<int>(selectedVersusBattleType);
}

bool Game::versusBattleTypeItemEnabled(int index) const
{

    return index >= 0 && index < VERSUS_BATTLE_TYPE_COUNT;
}

int Game::versusSelectingSide(void) const
{
    return selectedVersusSide;
}

int Game::versusCharacterSelection(int side) const
{
    return side >= 0 && side < 2 ? selectedVersusCharacters[side] : -1;
}

bool Game::versusSideActive(int side) const
{
    return side >= 0 && side < 2 && versusActive[side];
}

bool Game::versusSideReady(int side) const
{
    return side >= 0 && side < 2 && versusReady[side];
}

int Game::versusSideController(int side) const
{
    return side >= 0 && side < 2 ? versusControllers[side] : -1;
}

uint16_t Game::versusCharacterDisplayNameTextId(int index) const
{
    if(index < 0 || index >= GAME_VERSUS_CHARACTER_COUNT)
        return 0;
    const uint16_t characterId = VERSUS_CHARACTER_IDS[index];
    const PersistentGameRecord* record = characterRecord(characterId);

    const bool recordInitialized = record != NULL && record->level > 0;
    int16_t displayId = recordInitialized ?
        record->characterId : static_cast<int16_t>(characterId);
    const int16_t resource = recordInitialized ?
        record->resourceId :
        static_cast<int16_t>(characterRecordResourceId(characterId));
    switch(resource)
    {
    case 0x54: displayId = 0x4a; break;
    case 0x74: displayId = 0x4d; break;
    case 0x23: displayId = 0x4b; break;
    case 0x71:
    case 0x9a: displayId = 0x6c; break;
    case 0x73:
    case 0x9b: displayId = 0x6b; break;
    case 0x4e:
    case 0x99: displayId = 0x44; break;
    case 0x3f:
    case 0x9c: displayId = 0x35; break;
    case 0x43: displayId = 0x39; break;
    case 0x49: displayId = 0x3f; break;
    case 0x18:
    case 0x9d: displayId = 0x16; break;
    case 2: displayId = 0; break;
    default: break;
    }
    return displayId < 0 ? 0 :
        static_cast<uint16_t>(0x2af9 + displayId);
}

const char* Game::versusCharacterDisplayName(int index) const
{
    const char* name = localizedText(versusCharacterDisplayNameTextId(index));
    return name != NULL ? name : "";
}

const char* Game::versusCharacterStyleName(int side) const
{
    if(side < 0 || side >= 2)
        return COMBAT_STYLE_NAMES[COMBAT_STYLE_UNKNOWN];
    const uint16_t characterId =
        versusSelectedCharacterId(side);
    const PersistentGameRecord* record =
        side == 1 && currentMode == GAME_MODE_VERSUS_CUSTOM &&
        secondaryProfileLoaded ?
        secondaryCharacterRecord(characterId) : characterRecord(characterId);
    const CharacterRecord* defaults = characterRecordForId(characterId);
    const int style = record != NULL ? record->combatStyleId :
        (defaults != NULL ? defaults->combatStyleId : 0x33);
    return style == 0x33 || style == 0x34 || style >= 0x33 ?
        COMBAT_STYLE_NAMES[COMBAT_STYLE_UNKNOWN] :
        combatStyleName(style);
}

uint16_t Game::versusSelectedCharacterId(int side) const
{
    const int selection = versusCharacterSelection(side);
    return selection < 0 || selection >= GAME_VERSUS_CHARACTER_COUNT ?
        0 : VERSUS_CHARACTER_IDS[selection];
}

bool Game::versusCharacterItemEnabled(int side, int index) const
{
    return side >= 0 && side < 2 &&
        index >= 0 && index < GAME_VERSUS_CHARACTER_COUNT;
}

int Game::versusHumanSide(void) const
{
    return versusHumanSideValue;
}

const StoryCharacterSelectPresentation&
Game::storyCharacterSelectPresentation(void) const
{
    return storyCharacterSelect;
}

void Game::setStoryCharacterSelectFlowReady(bool ready)
{
    storyCharacterSelectReady = ready;
}

bool Game::storyCharacterSelectFlowReady(void) const
{
    return storyCharacterSelectReady;
}

const IntroCardPresentation& Game::introCardPresentation(void) const
{
    return currentIntroCard;
}

uint16_t Game::introCardTextId(void) const
{
    return currentIntroCard.recordIndex < INTRO_CARD_RECORD_COUNT ?
        INTRO_CARD_TEXT_IDS[currentIntroCard.recordIndex] : 0;
}

bool Game::introCardSequenceActive(void) const
{
    return currentIntroCard.state != INTRO_CARD_TASK_INACTIVE;
}

const TitleAttractState& Game::titleAttractState(void) const
{
    return currentTitleAttractState;
}

TitleAttractMode Game::titleAttractMode(void) const
{
    return currentTitleAttractMode;
}

int Game::titleAttractMovieIndex(void) const
{
    return currentTitleAttractMovieIndex;
}

void Game::completePreTitleMoviePlayback(void)
{
    if(currentScreen == GAME_SCREEN_ATTRACT_MOVIE &&
       gameFlowTask != NULL && gameFlowTask->state == 0x17)
        preTitleMovieComplete = true;
}

int Game::videoModeSelection(void) const { return selectedVideoMode; }

uint8_t Game::videoTimingMode(void) const { return videoTimingModeValue; }

int Game::nominalFrameRateHz(void) const { return nominalFrameRate; }

float Game::halfFrameSeconds(void) const { return videoHalfFrameSeconds; }

int Game::pauseSelection(void) const { return selectedPauseItem; }

bool Game::pauseMoveListActive(void) const { return pauseMoveListOpen; }

int Game::pauseMoveListFirstVisible(void) const
{
    return pauseMoveListFirst;
}

int Game::pauseMoveListStyle(void) const
{
    const int16_t characterId = activeRecordId(0);
    if(characterId < 0)
        return 0;
    const PersistentGameRecord* record =
        characterRecord(static_cast<uint16_t>(characterId));
    if(record == NULL || record->combatStyleId < 0 ||
       record->combatStyleId >= MOVE_CHARACTER_COUNT)
        return 0;
    return MOVE_CHARACTER_RECORDS[record->combatStyleId].personalMovesetId;
}

int Game::pauseMoveListVisibleEntries(uint8_t* output, int capacity) const
{
    return ::moveListVisibleEntries(
        pauseMoveListStyle(), combatTrainingLevels(persistent.player), output, capacity);
}

const char*
Game::screenName(GameScreen value)
{
    switch(value)
    {
    case GAME_SCREEN_LOADING:          return "loading";
    case GAME_SCREEN_LANGUAGE_SELECT:  return "language_select";
    case GAME_SCREEN_VIDEO_MODE_SELECT: return "video_mode_select";
    case GAME_SCREEN_NO_SAVE_DATA:      return "no_save_data";
    case GAME_SCREEN_ATTRACT_MOVIE:    return "attract_movie";
    case GAME_SCREEN_TITLE:            return "title";
    case GAME_SCREEN_MAIN_MENU:        return "main_menu";
    case GAME_SCREEN_VERSUS_TYPE_SELECT: return "versus_type_select";
    case GAME_SCREEN_CHARACTER_SELECT: return "character_select";
    case GAME_SCREEN_STAGE_SELECT:      return "stage_select";
    case GAME_SCREEN_BRIEFING:         return "briefing";
    case GAME_SCREEN_GAMEPLAY:         return "gameplay";
    case GAME_SCREEN_PAUSE:            return "pause";
    case GAME_SCREEN_INVENTORY:        return "inventory";
    case GAME_SCREEN_SHOP:             return "shop";
    case GAME_SCREEN_ADVICE:           return "advice";
    case GAME_SCREEN_OPTIONS:          return "options";
    case GAME_SCREEN_RESULTS:          return "results";
    case GAME_SCREEN_CREDITS:          return "credits";
    case GAME_SCREEN_EXIT:             return "exit";
    }
    return "unknown";
}

const char*
Game::characterName(int index)
{

    if(index < 0 || index >= 5)
        return "";

    const GameData* labels = &g_game.data();
    static GameData fallbackLabels;
    if(!labels->loaded())
    {
        if(!fallbackLabels.loaded() &&
           !fallbackLabels.load("raw/XAF") &&
           !fallbackLabels.load("../raw/XAF"))
            return "";
        labels = &fallbackLabels;
    }
    const char* text = labels->localizedText(GAME_FRONTEND_TEXT_IDS[index]);
    return text != NULL ? text : "";
}

uint16_t
Game::playableCharacterId(int index)
{
    if(index < 0 || index >= 5)
        return 0;
    return static_cast<uint16_t>(PLAYABLE_CHARACTER_IDS[index]);
}
