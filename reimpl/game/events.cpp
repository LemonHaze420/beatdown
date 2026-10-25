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

bool
Game::startEventScript(const char* archivePath, const char* entryName,
                       uint16_t pageId)
{
    messageVm.stop();
    eventWaitingForMessage = false;
    pendingHostVm = NULL;
    std::vector<uint8_t> data;
    if(!readXafEntry(archivePath, entryName, data))
        return false;

    projectPersistentStateToRuntime();
    persistent.story.stageId = persistent.stageId;

    EventVmCallbacks callbacks = {};
    callbacks.command = handleEventCommand;
    callbacks.callFunction = handleEventCallFunction;
    callbacks.inventoryCount = handleEventInventoryCount;
    callbacks.addInventory = handleEventInventoryAdd;
    callbacks.removeInventory = handleEventInventoryRemove;
    callbacks.userData = this;
    eventVm.initialize(&persistent.story, callbacks);

    uint8_t language = persistent.options.language;
    if(language >= EVENT_LANGUAGE_COUNT)
        language = EVENT_LANGUAGE_ENGLISH;
    if(!eventVm.load(data, static_cast<EventLanguage>(language)) ||
       !eventVm.start(pageId))
    {
        printf("ERROR: Could not start event '%s': %s\n", entryName,
               eventVm.error());
        return false;
    }
    pendingEventFunction = EVENT_CALLFUNC_NONE;
    return true;
}

bool
Game::startStoryEvent(uint16_t scriptId, uint16_t record)
{
    if(scriptId >= 148)
        return false;
    char entryName[16];
    snprintf(entryName, sizeof(entryName), "SMS_%03u.XAF",
             static_cast<unsigned>(scriptId));
    return startEventScript("raw/XAF/SCRIPT.XAF", entryName, record);
}

bool
Game::startEventMessage(uint16_t scriptId)
{
    std::vector<XafEntryInfo> entries;
    if(!listXafEntries("raw/XAF/SCRIPT.XAF", entries) ||
       scriptId >= entries.size() ||
       entries[scriptId].name.compare(0, 2, "S_") != 0)
        return false;
    const std::string& entryName = entries[scriptId].name;
    std::vector<uint8_t> data;
    if(!readXafEntry("raw/XAF/SCRIPT.XAF", entries[scriptId], data))
        return false;
    EventVmCallbacks callbacks = {};
    callbacks.command = handleMessageCommand;
    callbacks.callFunction = handleMessageCallFunction;
    callbacks.inventoryCount = handleEventInventoryCount;
    callbacks.addInventory = handleEventInventoryAdd;
    callbacks.removeInventory = handleEventInventoryRemove;
    callbacks.userData = this;
    messageVm.initialize(&persistent.story, callbacks);
    uint8_t language = persistent.options.language;
    if(language >= EVENT_LANGUAGE_COUNT)
        language = EVENT_LANGUAGE_ENGLISH;
    if(!messageVm.load(data, static_cast<EventLanguage>(language)) ||
       !messageVm.start(0))
    {
        printf("ERROR: Could not start event message '%s': %s\n",
               entryName.c_str(),
               messageVm.error());
        return false;
    }
    return true;
}

void
Game::stopEventScript(void)
{
    eventVm.stop();
    messageVm.stop();
    eventWaitingForMessage = false;
    pendingEventFunction = EVENT_CALLFUNC_NONE;
    pendingHostVm = NULL;
}

bool
Game::setSceneEventSoundChannelActive(int channelIndex, bool active)
{
    if(channelIndex < 0 ||
       channelIndex >= SCENE_SEQUENCE_EVENT_SOUND_CHANNEL_COUNT)
        return false;
    sceneEventSoundChannelActive[channelIndex] = active;
    return true;
}

bool
Game::sceneSequenceEventSoundActive(void* userData, int channelIndex)
{
    const Game& game = *static_cast<const Game*>(userData);
    if(channelIndex < 0 ||
       channelIndex >= SCENE_SEQUENCE_EVENT_SOUND_CHANNEL_COUNT)
        return false;

    return game.sceneEventSoundChannelActive[channelIndex];
}

bool
Game::sceneSequenceTaskFinished(void* userData, int subOp)
{

    (void)userData;
    return sceneTaskFinished(subOp);
}

bool
Game::sceneSequencePromptFinished(void* userData)
{
    const Game& game = *static_cast<const Game*>(userData);

    const uint32_t pressed = game.pressedInput | game.secondPressedInput;
    return (pressed &
            (GAME_INPUT_CONFIRM | GAME_INPUT_START | GAME_INPUT_CANCEL)) != 0;
}

int
Game::sceneSequenceActorGender(void* userData, int actorSlot)
{
    const Game& game = *static_cast<const Game*>(userData);

    if(actorSlot < 0 || actorSlot > 0xff)
        return 0;
    const GameSlotRecord* slot = game.combatSlotByIndex(
        static_cast<uint8_t>(actorSlot));
    if(slot == NULL)
        return 0;
    const PersistentGameRecord* record =
        game.characterRecord(
            static_cast<uint16_t>(slot->persistentRecordId));
    return record != NULL ? static_cast<int>(record->gender) : 0;
}

bool
Game::sceneSequenceActorTransform(void* userData, int actorSlot,
                                  float* position, float* heading)
{
    Game& game = *static_cast<Game*>(userData);
    const GameSlotRecord* slot = game.sceneSlot(actorSlot);
    if(slot == NULL)
        return false;
    position[0] = slot->position.x;
    position[1] = slot->position.y;
    position[2] = slot->position.z;
    *heading = slot->headingDegrees;
    return true;
}

int
Game::sceneSequenceResolveMove(void* userData, int actorSlot, int moveId)
{
    Game& game = *static_cast<Game*>(userData);
    const GameSlotRecord* slot = game.combatSlotByIndex(
        static_cast<uint8_t>(actorSlot));
    if(slot == NULL)
        return moveId;
    const PersistentGameRecord* record =
        game.characterRecord(
            static_cast<uint16_t>(slot->persistentRecordId));
    return resolveCharacterMove(
        record != NULL ? record->combatStyleId : 0, moveId);
}

GameSlotRecord*
Game::sceneSlot(int index)
{
    return index >= 0 && index <= 0xff ?
        mutableCombatSlotByIndex(static_cast<uint8_t>(index)) : NULL;
}

void
Game::installStageSceneSequenceCaption(void)
{
    SceneSequenceCaptionConfig caption;
    caption.holdSeconds = 1.5f;
    caption.loop = 0;
    caption.fadeStep = 8;
    caption.lines[0].x = 320.0f;
    caption.lines[0].y = 240.0f;
    caption.lines[0].color = 0xb4b4b4ffu;

    const uint16_t stageId = persistent.stageId;
    caption.lines[0].text =
        stageId < SCENE_SEQUENCE_STAGE_NAME_COUNT ?
            localizedText(SCENE_SEQUENCE_STAGE_NAME_TEXT_IDS[stageId]) : NULL;
    sceneSequencer.setCaptionConfig(caption);
}

void
Game::installMemberChangeSceneSequenceCaption(void)
{

    SceneSequenceCaptionConfig caption;
    caption.holdSeconds = 0.1f;
    caption.loop = 1;
    caption.fadeStep = 16;
    caption.lines[0].x = 320.0f;
    caption.lines[0].y = 452.0f;
    caption.lines[0].color = 0x80c0ffffu;
    caption.lines[0].text = localizedText(0x2956);
    sceneSequencer.setCaptionConfig(caption);
}

void
Game::startSceneSequence(int sequenceId, int firstSlot, int secondSlot)
{
    SceneSequenceCallbacks callbacks;
    callbacks.eventSoundActive = sceneSequenceEventSoundActive;
    callbacks.sceneTaskFinished = sceneSequenceTaskFinished;
    callbacks.promptFinished = sceneSequencePromptFinished;
    callbacks.actorGender = sceneSequenceActorGender;
    callbacks.actorTransform = sceneSequenceActorTransform;
    callbacks.resolveMove = sceneSequenceResolveMove;
    callbacks.userData = this;
    sceneSequencer.setCallbacks(callbacks);
    sceneSequencer.setActorSlots(firstSlot, secondSlot);
    sceneSequencer.setEscapeEnabled(persistent.story.escapeEnabled != 0);

    sceneSequencer.start(sequenceId);
}

void
Game::stopSceneSequence(void)
{
    sceneSequencer.stop();
}

const SceneSequencer&
Game::sceneSequence(void) const
{
    return sceneSequencer;
}

void
Game::updateSceneSequence(float sceneDeltaSeconds,
                          float scaledCaptionDeltaSeconds)
{
    const bool wasRunning = sceneSequencer.running();
    if(!wasRunning && !sceneSequencer.caption().activeTask &&
       !sceneSequencer.overlayFadeStopping())
        return;
    if(wasRunning)
        sceneSequencer.setEscapeEnabled(persistent.story.escapeEnabled != 0);
    sceneSequencer.update(sceneDeltaSeconds, scaledCaptionDeltaSeconds);
    if(wasRunning)
        applySceneSequence();
}

void
Game::applySceneSequence(void)
{
    for(int i = 0; i < SCENE_SEQUENCE_ACTOR_COUNT; ++i)
    {
        const SceneSequenceActor& source = sceneSequencer.actor(i);
        GameSlotRecord* slot = sceneSlot(sceneSequencer.actorSlot(i));
        if(slot == NULL)
            continue;
        if(source.placed)
        {
            slot->position.x = source.position[0];
            slot->position.y = source.position[1];
            slot->position.z = source.position[2];
            slot->headingDegrees = source.headingDegrees;
        }

        slot->motionControlFlags =
            (slot->motionControlFlags & ~3u) |
            (source.motionControlFlags & 3u);
    }

    const uint32_t flag = 0x40u;
    for(int i = 0; i < MAX_GAME_OPPONENTS - 1; ++i)
        additionalEnemies[i].actorActionFlags =
            sceneSequencer.extraActorFlag() ?
                additionalEnemies[i].actorActionFlags | flag :
                additionalEnemies[i].actorActionFlags & ~flag;
}

EventVmHostResult
Game::handleEventCommand(void* userData, const EventVmCommand& command)
{
    Game& game = *static_cast<Game*>(userData);
    game.synchronizeEventState(&command);
    if(command.opcode == EVENT_OP_EXF_MESSAGE ||
       command.opcode == EVENT_OP_EXF_CALL_TELPHONE)
    {
        const EventOperandTypeCode scriptOperand =
            command.opcode == EVENT_OP_EXF_MESSAGE ?
                EVENT_OPERAND_EXF_PARAMETER : EVENT_OPERAND_TEXT_SLOT;
        for(uint8_t i = 0; i < command.operandCount; ++i)
        {
            if(command.operands[i].type != scriptOperand)
                continue;
            if(!game.startEventMessage(
                   static_cast<uint16_t>(command.operands[i].bits)))
                return EVENT_VM_HOST_FAIL;
            game.eventWaitingForMessage = true;
            return EVENT_VM_HOST_WAIT;
        }
        return EVENT_VM_HOST_FAIL;
    }
    switch(command.opcode)
    {
    case EVENT_OP_EXF_GAMEOVER:
        if(game.currentScreen == GAME_SCREEN_GAMEPLAY)
            game.finishBattle(BATTLE_OUTCOME_PLAYER_LOST);
        break;
    case EVENT_OP_EXF_SET_BATTLE:
    case EVENT_OP_EXF_SET_BATTLE_PLURAL:
        game.startStageActorEncounter(
            game.persistent.story.battleId,
            game.persistent.story.battleEventId,
            game.persistent.story.battlePlural != 0);
        break;
    case EVENT_OP_EXF_EXIT_BATTLE:
        if(game.currentScreen == GAME_SCREEN_GAMEPLAY &&
           game.currentBattleOutcome == BATTLE_OUTCOME_IN_PROGRESS)
        {
            game.finishBattle(game.persistent.story.result > 0 ?
                BATTLE_OUTCOME_PLAYER_WON : BATTLE_OUTCOME_PLAYER_LOST);
        }
        break;
    case EVENT_OP_EXF_MOVE_STAGE:

        break;
    default:
        break;
    }
    return EVENT_VM_HOST_CONTINUE;
}

EventVmHostResult
Game::handleEventCallFunction(void* userData, EventCallFunctionId function)
{
    Game& game = *static_cast<Game*>(userData);
    if(function == EVENT_CALLFUNC_NONE)
        return EVENT_VM_HOST_CONTINUE;
    game.pendingEventFunction = function;
    game.pendingHostVm = &game.eventVm;
    return EVENT_VM_HOST_WAIT;
}

EventVmHostResult
Game::handleMessageCommand(void* userData, const EventVmCommand& command)
{
    Game& game = *static_cast<Game*>(userData);
    game.synchronizeEventState(&command);
    return EVENT_VM_HOST_CONTINUE;
}

EventVmHostResult
Game::handleMessageCallFunction(void* userData, EventCallFunctionId function)
{
    Game& game = *static_cast<Game*>(userData);
    if(function == EVENT_CALLFUNC_NONE)
        return EVENT_VM_HOST_CONTINUE;
    game.pendingEventFunction = function;
    game.pendingHostVm = &game.messageVm;
    return EVENT_VM_HOST_WAIT;
}

uint16_t
Game::handleEventInventoryCount(
    void* userData, uint16_t itemId, int variant)
{
    const Game& game = *static_cast<const Game*>(userData);
    return ownedInventoryQuantity(
        game.primarySaveImage.payload.itemInventoryBlock, itemId, variant);
}

bool
Game::handleEventInventoryAdd(
    void* userData, uint16_t itemId, uint16_t variant, uint16_t quantity)
{
    if(variant > 0x0fu)
        return false;
    Game& game = *static_cast<Game*>(userData);
    return addOwnedInventoryQuantity(
        game.primarySaveImage.payload.itemInventoryBlock, itemId,
        static_cast<uint8_t>(variant), quantity);
}

bool
Game::handleEventInventoryRemove(
    void* userData, uint16_t itemId, uint16_t variant, uint16_t quantity)
{
    if(variant > 0x0fu)
        return false;
    Game& game = *static_cast<Game*>(userData);
    return removeOwnedInventoryQuantity(
        game.primarySaveImage.payload.itemInventoryBlock, itemId,
        static_cast<uint8_t>(variant), quantity);
}

uint32_t
Game::eventInputButtons(uint32_t buttons) const
{
    uint32_t eventButtons = 0;
    if((buttons & GAME_INPUT_UP) != 0) eventButtons |= GAMEPAD_DPAD_UP;
    if((buttons & GAME_INPUT_DOWN) != 0) eventButtons |= GAMEPAD_DPAD_DOWN;
    if((buttons & GAME_INPUT_LEFT) != 0) eventButtons |= GAMEPAD_DPAD_LEFT;
    if((buttons & GAME_INPUT_RIGHT) != 0) eventButtons |= GAMEPAD_DPAD_RIGHT;
    if((buttons & GAME_INPUT_CONFIRM) != 0) eventButtons |= GAMEPAD_CROSS;
    if((buttons & GAME_INPUT_CANCEL) != 0) eventButtons |= GAMEPAD_CIRCLE;
    if((buttons & (GAME_INPUT_START | GAME_INPUT_PAUSE)) != 0)
        eventButtons |= GAMEPAD_START;
    if((buttons & GAME_INPUT_ATTACK) != 0) eventButtons |= GAMEPAD_SQUARE;
    if((buttons & GAME_INPUT_HEAVY) != 0) eventButtons |= GAMEPAD_TRIANGLE;
    if((buttons & GAME_INPUT_GRAB) != 0) eventButtons |= GAMEPAD_R1;
    if((buttons & GAME_INPUT_GUARD) != 0) eventButtons |= GAMEPAD_L1;
    if((buttons & GAME_INPUT_DODGE) != 0) eventButtons |= GAMEPAD_R2;
    return eventButtons;
}

void
Game::synchronizeStageActorsFromStory(void)
{
    for(int i = 0; i < GAME_STAGE_ACTOR_CAPACITY; ++i)
    {
        StageActorRuntime& runtime = stageActors[i];
        if(!runtime.occupied)
            continue;
        const StoryActorState* actor = findStoryActor(
            persistent.story, runtime.authoredCharacterId);
        if(actor == NULL)
            continue;
        runtime.slot.position = {
            actor->position[0], actor->position[1], actor->position[2]};
        runtime.slot.headingDegrees = actor->headingDegrees;
        runtime.activeMotionId = static_cast<int16_t>(actor->motionId);
        runtime.visible = actor->active != 0 && actor->visible != 0;
        runtime.slot.currentHealth =
            actor->active ? actor->currentHealth : 0;
    }
}

void
Game::synchronizeEventState(const EventVmCommand* command)
{
    if(command != NULL)
        applyActorActionCommand(*command);
    if(command == NULL)
        advanceStoryActors(persistent.story, scaledFrameDeltaSeconds);
    synchronizeStageActorsFromStory();
    const int16_t active = activeRecordId(0);
    if(active >= 0)
        persistent.story.characters[active].cash = persistent.story.money;
    const StoryActorState* player = findStoryActor(persistent.story, 0);
    if(player != NULL)
    {
        persistent.player.position.x = player->position[0];
        persistent.player.position.y = player->position[1];
        persistent.player.position.z = player->position[2];
        persistent.player.headingDegrees = player->headingDegrees;
        persistent.player.currentHealth = player->currentHealth;
    }
    for(int i = 0; i < activeOpponentCount; ++i)
    {
        GameSlotRecord& slot = i == 0 ?
            enemy : additionalEnemies[i - 1];
        const StoryActorState* actor = findStoryActor(persistent.story,
            static_cast<uint16_t>(slot.characterId));
        if(actor == NULL)
            continue;
        slot.position.x = actor->position[0];
        slot.position.y = actor->position[1];
        slot.position.z = actor->position[2];
        slot.headingDegrees = actor->headingDegrees;
        slot.currentHealth = actor->active ?
            actor->currentHealth : 0;
    }

    projectRuntimeStateToPersistent(primarySaveImage);
    if(command == NULL)
        return;
}
