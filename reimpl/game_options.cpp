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

int optionValueFrom(const GameOptions& options, OptionStorage storage)
{
    switch(storage)
    {
    case OPTION_STORAGE_DIFFICULTY:       return options.difficulty;
    case OPTION_STORAGE_SUBTITLES:        return options.subtitles;
    case OPTION_STORAGE_MAP:              return options.mapDisplay;
    case OPTION_STORAGE_GAUGE:            return options.gaugeDisplay;
    case OPTION_STORAGE_BLOOD_MALE:       return options.bloodMale;
    case OPTION_STORAGE_BLOOD_FEMALE:     return options.bloodFemale;
    case OPTION_STORAGE_BRUISING_MALE:    return options.bruisingMale;
    case OPTION_STORAGE_BRUISING_FEMALE:  return options.bruisingFemale;
    case OPTION_STORAGE_AUDIO_OUTPUT:     return options.audioOutput;
    case OPTION_STORAGE_BGM_LEVEL:        return options.musicVolume;
    case OPTION_STORAGE_SE_LEVEL:         return options.effectsVolume;
    case OPTION_STORAGE_CONTROLLER_P1:    return options.controllerTypeP1;
    case OPTION_STORAGE_VIBRATION_P1:     return options.vibration;
    case OPTION_STORAGE_CONTROLLER_P2:    return options.controllerTypeP2;
    case OPTION_STORAGE_VIBRATION_P2:     return options.vibrationP2;
    case OPTION_STORAGE_CAMERA_X:         return options.cameraXType;
    case OPTION_STORAGE_CAMERA_Y:         return options.cameraYType;
    case OPTION_STORAGE_CAMERA_FOLLOW:    return options.cameraFollow;
    case OPTION_STORAGE_SCREEN_X:         return options.screenAdjustX;
    case OPTION_STORAGE_SCREEN_Y:         return options.screenAdjustY;
    default:                              return 0;
    }
}

void setOptionValueIn(GameOptions& options, OptionStorage storage, int value)
{
    switch(storage)
    {
    case OPTION_STORAGE_DIFFICULTY: options.difficulty = static_cast<uint8_t>(value); break;
    case OPTION_STORAGE_SUBTITLES: options.subtitles = static_cast<uint8_t>(value); break;
    case OPTION_STORAGE_MAP: options.mapDisplay = static_cast<uint8_t>(value); break;
    case OPTION_STORAGE_GAUGE: options.gaugeDisplay = static_cast<uint8_t>(value); break;
    case OPTION_STORAGE_BLOOD_MALE: options.bloodMale = static_cast<uint8_t>(value); break;
    case OPTION_STORAGE_BLOOD_FEMALE: options.bloodFemale = static_cast<uint8_t>(value); break;
    case OPTION_STORAGE_BRUISING_MALE: options.bruisingMale = static_cast<uint8_t>(value); break;
    case OPTION_STORAGE_BRUISING_FEMALE: options.bruisingFemale = static_cast<uint8_t>(value); break;
    case OPTION_STORAGE_AUDIO_OUTPUT: options.audioOutput = static_cast<uint8_t>(value); break;
    case OPTION_STORAGE_BGM_LEVEL: options.musicVolume = static_cast<uint8_t>(value); break;
    case OPTION_STORAGE_SE_LEVEL: options.effectsVolume = static_cast<uint8_t>(value); break;
    case OPTION_STORAGE_CONTROLLER_P1: options.controllerTypeP1 = static_cast<uint8_t>(value); break;
    case OPTION_STORAGE_VIBRATION_P1: options.vibration = static_cast<uint8_t>(value); break;
    case OPTION_STORAGE_CONTROLLER_P2: options.controllerTypeP2 = static_cast<uint8_t>(value); break;
    case OPTION_STORAGE_VIBRATION_P2: options.vibrationP2 = static_cast<uint8_t>(value); break;
    case OPTION_STORAGE_CAMERA_X: options.cameraXType = static_cast<uint8_t>(value); break;
    case OPTION_STORAGE_CAMERA_Y: options.cameraYType = static_cast<uint8_t>(value); break;
    case OPTION_STORAGE_CAMERA_FOLLOW: options.cameraFollow = static_cast<uint8_t>(value); break;
    case OPTION_STORAGE_SCREEN_X: options.screenAdjustX = static_cast<int16_t>(value); break;
    case OPTION_STORAGE_SCREEN_Y: options.screenAdjustY = static_cast<int16_t>(value); break;
    default: break;
    }
}

OptionAction openerAction(GameOptionsPage page)
{
    switch(page)
    {
    case GAME_OPTIONS_PAGE_AUDIO:        return OPTION_ACTION_AUDIO;
    case GAME_OPTIONS_PAGE_CONTROL:      return OPTION_ACTION_CONTROL;
    case GAME_OPTIONS_PAGE_SCREEN:       return OPTION_ACTION_SCREEN;
    case GAME_OPTIONS_PAGE_BRIGHTNESS:   return OPTION_ACTION_BRIGHTNESS;
    case GAME_OPTIONS_PAGE_MOVIE_VIEWER: return OPTION_ACTION_MOVIE_VIEWER;
    default:                             return OPTION_ACTION_EXIT_PAGE;
    }
}

GameOptionsPage actionPage(OptionAction action)
{
    switch(action)
    {
    case OPTION_ACTION_AUDIO:        return GAME_OPTIONS_PAGE_AUDIO;
    case OPTION_ACTION_CONTROL:      return GAME_OPTIONS_PAGE_CONTROL;
    case OPTION_ACTION_SCREEN:       return GAME_OPTIONS_PAGE_SCREEN;
    case OPTION_ACTION_BRIGHTNESS:   return GAME_OPTIONS_PAGE_BRIGHTNESS;
    case OPTION_ACTION_MOVIE_VIEWER: return GAME_OPTIONS_PAGE_MOVIE_VIEWER;
    default:                         return GAME_OPTIONS_PAGE_MAIN;
    }
}

}

void
Game::updateOptionsMenuTask(UpdateTask* task)
{
    Game& game = gameFromTask(task);
    if(task->state == 1)
    {
        game.beginOptionsMenu();
        task->state = 4;
    }

    const bool cancel = (game.pressedInput & GAME_INPUT_CANCEL) != 0;
    const bool confirm = (game.pressedInput & GAME_INPUT_CONFIRM) != 0;
    int direction = 0;
    if((game.pressedInput & GAME_INPUT_LEFT) != 0)
        direction = -1;
    else if((game.pressedInput & GAME_INPUT_RIGHT) != 0)
        direction = 1;

    if(game.optionsDefaultConfirmation)
    {
        if(cancel)
        {
            game.optionsDefaultConfirmation = false;
            return;
        }
        if(direction != 0)
        {
            game.optionsDefaultSelection ^= 1;
            return;
        }
        if(confirm)
        {
            if(game.optionsDefaultSelection != 0)
            {
                game.resetOptionsToDefaults(game.selectedOptionsPage);
                game.previewOptionsPage(game.selectedOptionsPage);
            }
            game.optionsDefaultConfirmation = false;
        }
        return;
    }

    if(cancel)
    {
        game.restoreOptionsPage(game.selectedOptionsPage);
        game.previewOptionsPage(game.selectedOptionsPage);
        if(game.selectedOptionsPage == GAME_OPTIONS_PAGE_MAIN)
        {
            game.leaveOptionsMenu(false);
            return;
        }
        const OptionAction opener = openerAction(game.selectedOptionsPage);
        game.selectedOptionsPage = GAME_OPTIONS_PAGE_MAIN;
        game.selectedOption = 0;
        const OptionPage& mainPage = OPTION_PAGES[GAME_OPTIONS_PAGE_MAIN];
        for(int i = 0; i < mainPage.entryCount; ++i)
            if(mainPage.entries[i].action == opener)
            {
                game.selectedOption = i;
                break;
            }
        return;
    }

    if((game.pressedInput & (GAME_INPUT_UP | GAME_INPUT_DOWN)) != 0)
    {
        const int count = game.optionsItemCount();
        const int step =
            (game.pressedInput & GAME_INPUT_UP) != 0 ? -1 : 1;
        for(int i = 0; i < count; ++i)
        {
            game.selectedOption =
                wrapIndex(game.selectedOption + step, count);
            if(game.optionsItemVisible(game.selectedOption))
                break;
        }
        return;
    }

    if(!confirm && direction == 0)
        return;
    if(!game.optionsItemVisible(game.selectedOption))
        return;

    const OptionPage& page =
        OPTION_PAGES[static_cast<int>(game.selectedOptionsPage)];
    const OptionEntry& entry = page.entries[game.selectedOption];
    if(direction != 0)
    {
        if(optionEntryHasValue(entry))
        {
            const int value = optionAdjustedValue(
                entry, game.stagedOptionValue(entry), direction);
            game.setStagedOptionValue(entry, value);
            game.previewOptionsPage(game.selectedOptionsPage);
        }
        return;
    }

    switch(entry.action)
    {
    case OPTION_ACTION_AUDIO:
    case OPTION_ACTION_CONTROL:
    case OPTION_ACTION_SCREEN:
    case OPTION_ACTION_BRIGHTNESS:
    case OPTION_ACTION_MOVIE_VIEWER:
        game.snapshotOptionsPage(game.selectedOptionsPage);
        game.selectedOptionsPage = actionPage(entry.action);
        game.selectedOption = 0;
        return;
    case OPTION_ACTION_DEFAULT:
        game.snapshotOptionsPage(game.selectedOptionsPage);
        game.optionsDefaultSelection = 0;
        game.optionsDefaultConfirmation = true;
        return;
    case OPTION_ACTION_EXIT_PAGE:
        if(game.selectedOptionsPage == GAME_OPTIONS_PAGE_BRIGHTNESS)
            return;
        game.snapshotOptionsPage(game.selectedOptionsPage);
        {
            const OptionAction opener = openerAction(game.selectedOptionsPage);
            game.selectedOptionsPage = GAME_OPTIONS_PAGE_MAIN;
            game.selectedOption = 0;
            const OptionPage& mainPage = OPTION_PAGES[GAME_OPTIONS_PAGE_MAIN];
            for(int i = 0; i < mainPage.entryCount; ++i)
                if(mainPage.entries[i].action == opener)
                {
                    game.selectedOption = i;
                    break;
                }
        }
        return;
    case OPTION_ACTION_COMMIT_EXIT:
        game.snapshotOptionsPage(game.selectedOptionsPage);
        game.leaveOptionsMenu(true);
        return;
    case OPTION_ACTION_PLAY_MOVIE:
        game.selectedMovieScene =
            optionMovieSceneId(entry, game.stagedOptionValue(entry));
        game.selectedMovieCharacterScene =
            entry.movieSceneIds != NULL ? entry.movieSceneIds[0] : 0;
        return;
    default:
        return;
    }
}

void
Game::beginOptionsMenu(void)
{
    optionsStaging = persistent.options;
    for(int i = 0; i < 5; ++i)
        movieSelections[i] = 1;
    optionsStagingActive = true;
    optionsDefaultConfirmation = false;
    optionsDefaultSelection = 0;
    selectedMovieScene = 0;
    selectedMovieCharacterScene = 0;
    vibrationPreview[0] = false;
    vibrationPreview[1] = false;
    for(int page = 0; page < OPTION_PAGE_COUNT; ++page)
        snapshotOptionsPage(static_cast<GameOptionsPage>(page));
}

void
Game::leaveOptionsMenu(bool commit)
{
    if(commit)
        commitOptionsMenu();
    optionsStagingActive = false;
    optionsDefaultConfirmation = false;
    currentScreen = optionsReturnScreen;
    selectedOptionsPage = GAME_OPTIONS_PAGE_MAIN;
    selectedOption = 0;
    if(optionsReturnScreen == GAME_SCREEN_PAUSE)
        switchGameFlowTaskHandler(updatePauseFlow, 3);
    else
        switchGameFlowTaskHandler(updateTitleScreenFlow, 0x0c);
}

void
Game::snapshotOptionsPage(GameOptionsPage page)
{
    optionsPageBaselines[static_cast<int>(page)] = optionsStaging;
    if(page == GAME_OPTIONS_PAGE_MOVIE_VIEWER)
        memcpy(movieBaselines, movieSelections, sizeof(movieSelections));
}

void
Game::restoreOptionsPage(GameOptionsPage page)
{
    const OptionPage& descriptor = OPTION_PAGES[static_cast<int>(page)];
    const GameOptions& baseline = optionsPageBaselines[static_cast<int>(page)];
    for(int i = 0; i < descriptor.entryCount; ++i)
    {
        const OptionEntry& entry = descriptor.entries[i];
        if(entry.action == OPTION_ACTION_VALUE)
            setOptionValueIn(optionsStaging, entry.storage,
                optionValueFrom(baseline, entry.storage));
    }
    if(page == GAME_OPTIONS_PAGE_MOVIE_VIEWER)
        memcpy(movieSelections, movieBaselines, sizeof(movieSelections));
}

void
Game::commitOptionsMenu(void)
{
    for(int page = GAME_OPTIONS_PAGE_MAIN;
        page <= GAME_OPTIONS_PAGE_SCREEN; ++page)
    {
        const OptionPage& descriptor = OPTION_PAGES[page];
        for(int i = 0; i < descriptor.entryCount; ++i)
        {
            const OptionEntry& entry = descriptor.entries[i];
            if(entry.action == OPTION_ACTION_VALUE)
                setOptionValueIn(persistent.options, entry.storage,
                    optionValueFrom(optionsStaging, entry.storage));
        }
    }
    persistent.options.brightness = 10;
}

void
Game::resetOptionsToDefaults(GameOptionsPage page)
{
    const OptionPage& descriptor = OPTION_PAGES[static_cast<int>(page)];
    for(int i = 0; i < descriptor.entryCount; ++i)
    {
        const OptionEntry& entry = descriptor.entries[i];
        if(optionEntryHasValue(entry))
            setStagedOptionValue(entry, entry.defaultValue);
    }
}

void
Game::previewOptionsPage(GameOptionsPage page)
{
    if(page == GAME_OPTIONS_PAGE_AUDIO)
    {
        persistent.options.audioOutput = optionsStaging.audioOutput;
        persistent.options.musicVolume = optionsStaging.musicVolume;
        persistent.options.effectsVolume = optionsStaging.effectsVolume;
    }
    else if(page == GAME_OPTIONS_PAGE_SCREEN)
    {
        persistent.options.screenAdjustX = optionsStaging.screenAdjustX;
        persistent.options.screenAdjustY = optionsStaging.screenAdjustY;
    }
}

int
Game::stagedOptionValue(const OptionEntry& entry) const
{
    if(entry.storage >= OPTION_STORAGE_MOVIE_RAVEN &&
       entry.storage <= OPTION_STORAGE_MOVIE_AARON)
        return movieSelections[
            static_cast<int>(entry.storage - OPTION_STORAGE_MOVIE_RAVEN)];
    return optionValueFrom(optionsStaging, entry.storage);
}

void
Game::setStagedOptionValue(const OptionEntry& entry, int value)
{
    if(entry.storage >= OPTION_STORAGE_MOVIE_RAVEN &&
       entry.storage <= OPTION_STORAGE_MOVIE_AARON)
    {
        movieSelections[
            static_cast<int>(entry.storage - OPTION_STORAGE_MOVIE_RAVEN)] =
                static_cast<int8_t>(value);
        return;
    }
    setOptionValueIn(optionsStaging, entry.storage, value);
    if(entry.storage == OPTION_STORAGE_VIBRATION_P1 && value != 0)
        vibrationPreview[0] = true;
    else if(entry.storage == OPTION_STORAGE_VIBRATION_P2 && value != 0)
        vibrationPreview[1] = true;
}

int Game::optionsSelection(void) const { return selectedOption; }

const GameOptions& Game::options(void) const
{
    return optionsStagingActive ? optionsStaging : persistent.options;
}

bool Game::saveAvailable(void) const { return hasSave; }

GameOptionsPage Game::optionsPage(void) const
{
    return selectedOptionsPage;
}

int
Game::optionsItemCount(void) const
{

    const int page = static_cast<int>(selectedOptionsPage);
    if(page < 0 || page >= OPTION_PAGE_COUNT)
        return 0;
    return OPTION_PAGES[page].entryCount;
}

bool
Game::optionsOpenedFromPause(void) const
{
    return optionsReturnScreen == GAME_SCREEN_PAUSE;
}

bool
Game::optionsItemEnabled(int index) const
{
    return optionsItemVisible(index);
}

bool
Game::optionsItemVisible(int index) const
{
    if(index < 0 || index >= optionsItemCount())
        return false;
    const OptionEntry& entry =
        OPTION_PAGES[static_cast<int>(selectedOptionsPage)].entries[index];
    if(!optionEntryVisible(entry, optionsOpenedFromPause()))
        return false;
    if(selectedOptionsPage == GAME_OPTIONS_PAGE_MAIN &&
       entry.action == OPTION_ACTION_MOVIE_VIEWER &&
       (persistent.runtimeFlags & GAME_RUNTIME_STORY_COMPLETE) == 0)
        return false;
    return true;
}

int
Game::optionValue(const OptionEntry& entry) const
{
    return stagedOptionValue(entry);
}

bool
Game::optionsDefaultConfirmationActive(void) const
{
    return optionsDefaultConfirmation;
}

int
Game::optionsDefaultConfirmationSelection(void) const
{
    return optionsDefaultSelection;
}

uint16_t
Game::optionsMovieSceneId(void) const
{
    return selectedMovieScene;
}

uint16_t
Game::optionsMovieCharacterSceneId(void) const
{
    return selectedMovieCharacterScene;
}

bool
Game::optionsVibrationPreview(int playerIndex) const
{
    return playerIndex >= 0 && playerIndex < 2 &&
           vibrationPreview[playerIndex];
}

bool
Game::controllerConnected(int playerIndex) const
{
    return playerIndex >= 0 && playerIndex < 2 &&
           connectedControllers[playerIndex];
}

bool
Game::mainMenuItemEnabled(int index) const
{

    return index >= 0 && index < TITLE_MENU_ENTRY_COUNT &&
           TITLE_MENU_RECORDS[index].enabled;
}

bool
Game::pauseMenuItemEnabled(int index) const
{
    const int rowCount =
        currentMode == GAME_MODE_STORY ?
        PAUSE_ROW_COUNT : BATTLE_PAUSE_ROW_COUNT;
    return index >= 0 && index < rowCount;
}

bool
Game::inventoryItemEnabled(int index) const
{
    InventoryMenuSelection selection = {};
    return getInventoryMenuSelection(*this, index, selection) &&
        selection.quantity != 0;
}
