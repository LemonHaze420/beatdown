#include "scene_task_internal.h"

#include "game.h"
#include "scene_sequences.h"
#include "xaf_runtime.h"

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace
{

const char* SCRIPT_ARCHIVE_PATH = "raw/XAF/SCRIPT.XAF";
const int UNOBSERVED_VALUE = -0x40000000;

SceneTaskRuntime g_sceneTasks;
std::vector<XafEntryInfo> g_scriptEntries;
bool g_scriptEntriesListed;
bool g_sceneTasksInitialized;

void
ensureSceneTasksInitialized(void)
{
    if(!g_sceneTasksInitialized)
        resetSceneTasks();
}

bool
isStallingOpcode(int opcode)
{
    return opcode == SCENE_SEQUENCE_END ||
        opcode == SCENE_SEQUENCE_WAIT_FRAMES ||
        opcode == SCENE_SEQUENCE_WAIT_PROMPT ||
        opcode == SCENE_SEQUENCE_WAIT_SCENE_TASK ||
        opcode == SCENE_SEQUENCE_WAIT_EVENT_SOUND ||
        opcode == SCENE_SEQUENCE_PAUSE;
}

bool
sequencePrefixContains(int sequenceId, int wantedOpcode)
{
    if(sequenceId <= 0 || sequenceId >= SCENE_SEQUENCE_COUNT)
        return false;

    const SceneSequenceProgram& program =
        SCENE_SEQUENCE_PROGRAMS[sequenceId];
    int cursor = program.firstWord;
    const int end = cursor + program.wordCount;
    while(cursor < end)
    {
        const int opcode = SCENE_SEQUENCE_WORDS[cursor] & 0xffff;
        if(opcode == wantedOpcode)
            return true;
        if(isStallingOpcode(opcode))
            return false;
        const int words = sceneSequenceInstructionWords(
            &SCENE_SEQUENCE_WORDS[cursor], end - cursor);
        if(words <= 0)
            return false;
        cursor += words;
    }
    return false;
}

void
applyCharacterSelectCommand(int command, bool confirmBlocked)
{
    g_sceneTasks.characterCommand = command;
    switch(command)
    {
    case 0:
        g_sceneTasks.characterState = SCENE_CHARACTER_SELECT_CLEAR;
        g_sceneTasks.characterActive = false;
        break;
    case 1:
        g_sceneTasks.characterCaptureRequested = true;
        break;
    case 2:
        g_sceneTasks.characterState = SCENE_CHARACTER_SELECT_FLASH;
        break;
    case 3:
        if(g_sceneTasks.characterState !=
               SCENE_CHARACTER_SELECT_FADE_IN_BEGIN &&
           g_sceneTasks.characterState != SCENE_CHARACTER_SELECT_FADE_IN)
        {
            g_sceneTasks.characterState =
                SCENE_CHARACTER_SELECT_FADE_IN_BEGIN;
            g_sceneTasks.characterFadeStep = 6;
        }
        break;
    case 4:

        if(!confirmBlocked)
        {
            g_sceneTasks.characterState =
                SCENE_CHARACTER_SELECT_FADE_OUT_BEGIN;
            g_sceneTasks.characterFadeStep = 6;
        }
        break;
    case 5:
        g_sceneTasks.characterState = SCENE_CHARACTER_SELECT_FADE_OUT;
        g_sceneTasks.characterFadeStep = 6;
        break;
    case 6:
        g_sceneTasks.characterState =
            SCENE_CHARACTER_SELECT_ALTERNATE_BEGIN;
        break;
    default:

        break;
    }
}

SceneTaskOperation
operationForSubOp(int subOp)
{
    switch(subOp)
    {
    case 1:
        return SCENE_TASK_OPERATION_RELOAD_MAIN_CHARACTER;
    case 2:
        return SCENE_TASK_OPERATION_EVENT_SCRIPT;
    case 3:
        return SCENE_TASK_OPERATION_SHOP_SELL_FLOW;
    default:
        return SCENE_TASK_OPERATION_UNESTABLISHED;
    }
}

void
beginSceneTask(int subOp)
{
    const SceneSequencer& sequence = g_game.sceneSequence();
    g_sceneTasks.operation = operationForSubOp(subOp);
    g_sceneTasks.pending = true;
    g_sceneTasks.failed = false;
    g_sceneTasks.started = false;
    g_sceneTasks.externallyComplete = false;
    g_sceneTasks.scriptEntryIndex = sequence.sceneTaskArchive();
    g_sceneTasks.scriptPageId = sequence.sceneTaskEntry();
}

bool
startEventScriptTask(void)
{
    if(!g_scriptEntriesListed)
    {
        if(!listXafEntries(SCRIPT_ARCHIVE_PATH, g_scriptEntries))
            return false;
        g_scriptEntriesListed = true;
    }

    if(g_sceneTasks.scriptEntryIndex < 0 ||
       static_cast<size_t>(g_sceneTasks.scriptEntryIndex) >=
           g_scriptEntries.size())
        return false;

    uint16_t page = static_cast<uint16_t>(g_sceneTasks.scriptPageId);

    if(g_game.options().language == EVENT_LANGUAGE_ENGLISH)
        page = static_cast<uint16_t>(page + 1u);

    const std::string& entry =
        g_scriptEntries[static_cast<size_t>(g_sceneTasks.scriptEntryIndex)].name;
    return g_game.startEventScript(SCRIPT_ARCHIVE_PATH, entry.c_str(), page);
}

bool
finishCurrentTask(void)
{
    g_sceneTasks.pending = false;
    g_sceneTasks.started = false;
    g_sceneTasks.externallyComplete = false;
    return true;
}

}

void
resetSceneTasks(void)
{
    g_sceneTasks.operation = SCENE_TASK_OPERATION_NONE;
    g_sceneTasks.pending = false;
    g_sceneTasks.failed = false;
    g_sceneTasks.started = false;
    g_sceneTasks.externallyComplete = false;
    g_sceneTasks.scriptEntryIndex = -1;
    g_sceneTasks.scriptPageId = -1;

    g_sceneTasks.sequenceId = -1;
    g_sceneTasks.sequenceWasRunning = false;
    g_sceneTasks.observedEventMode = UNOBSERVED_VALUE;
    g_sceneTasks.observedCharacterCommand = UNOBSERVED_VALUE;
    g_sceneTasks.observedClearMask = UNOBSERVED_VALUE;
    g_sceneTasks.observedEventFlow = false;

    g_sceneTasks.eventMode = -1;
    g_sceneTasks.characterCommand = -1;
    g_sceneTasks.characterState = SCENE_CHARACTER_SELECT_CLEAR;
    g_sceneTasks.characterFadeStep = 0;
    g_sceneTasks.characterAlpha = 0;
    g_sceneTasks.characterRed = g_sceneTasks.characterGreen = g_sceneTasks.characterBlue = 0;
    g_sceneTasks.characterColorSet = false;
    g_sceneTasks.characterActive = false;
    g_sceneTasks.characterAlternateGeometry = false;
    g_sceneTasks.characterCaptureRequested = false;
    g_sceneTasks.eventFlowStartRequested = false;
    g_sceneTasks.ambientStopRequested = false;
    g_sceneTasks.ambientStopMask = 0;
    g_sceneTasksInitialized = true;
}

void
updateSceneTasks(void)
{
    ensureSceneTasksInitialized();
    const SceneSequencer& sequence = g_game.sceneSequence();
    const bool running = sequence.running();
    const bool newSequence = running &&
        (!g_sceneTasks.sequenceWasRunning ||
         g_sceneTasks.sequenceId != sequence.sequenceId());
    if(newSequence)
        g_sceneTasks.sequenceId = sequence.sequenceId();

    const int mode = sequence.eventMode();
    if(mode != g_sceneTasks.observedEventMode)
    {
        g_sceneTasks.observedEventMode = mode;
        g_sceneTasks.eventMode = mode;
    }

    const int characterCommand = sequence.characterSelectState();
    if(characterCommand != g_sceneTasks.observedCharacterCommand)
    {
        g_sceneTasks.observedCharacterCommand = characterCommand;
        if(characterCommand >= 0)
        {
            applyCharacterSelectCommand(
                characterCommand,
                getStorySaveFlag(
                    g_game.runtimeState().story,
                    SCENE_CHARACTER_SELECT_CONFIRM_BLOCK_FLAG));
        }
    }

    const bool prefixStartsEventFlow = newSequence &&
        sequencePrefixContains(sequence.sequenceId(),
                               SCENE_SEQUENCE_START_EVENT_FLOW);
    const bool eventFlow = sequence.eventFlowStarted();
    if(eventFlow &&
       (!g_sceneTasks.observedEventFlow || prefixStartsEventFlow))
        g_sceneTasks.eventFlowStartRequested = true;
    g_sceneTasks.observedEventFlow = eventFlow;

    const int clearMask = sequence.clearedSceneEffect();
    const bool prefixClearsAmbient = newSequence &&
        sequencePrefixContains(sequence.sequenceId(),
                               SCENE_SEQUENCE_CLEAR_SCENE_EFFECT);
    if(clearMask >= 0 &&
       (clearMask != g_sceneTasks.observedClearMask || prefixClearsAmbient))
    {

        g_sceneTasks.ambientStopMask = clearMask;
        g_sceneTasks.ambientStopRequested = true;
    }
    g_sceneTasks.observedClearMask = clearMask;
    g_sceneTasks.sequenceWasRunning = running;
}

bool
sceneTaskFinished(int subOp)
{
    ensureSceneTasksInitialized();
    const SceneSequencer& sequence = g_game.sceneSequence();
    const int requestedSubOp = sequence.sceneTaskActive() ?
        sequence.sceneTaskSubOp() : subOp;
    if(!g_sceneTasks.pending ||
       g_sceneTasks.operation != operationForSubOp(requestedSubOp))
        beginSceneTask(requestedSubOp);

    switch(g_sceneTasks.operation)
    {
    case SCENE_TASK_OPERATION_RELOAD_MAIN_CHARACTER:

        return g_sceneTasks.externallyComplete ? finishCurrentTask() : false;

    case SCENE_TASK_OPERATION_EVENT_SCRIPT:
        if(!g_sceneTasks.started)
        {
            g_sceneTasks.failed = false;
            g_sceneTasks.started = true;
            if(!startEventScriptTask())
            {
                g_sceneTasks.failed = true;
                g_sceneTasks.started = false;
                return false;
            }
        }
        return !g_game.eventScript().running() ? finishCurrentTask() : false;

    case SCENE_TASK_OPERATION_SHOP_SELL_FLOW:
        if(!g_sceneTasks.started)
        {

            g_sceneTasks.failed = false;
            g_sceneTasks.started = true;
            if(!g_game.openShop(-1))
            {
                g_sceneTasks.failed = true;
                g_sceneTasks.started = false;
                return false;
            }
            g_game.setShopMode(GAME_SHOP_SELL);
        }
        return !g_game.shopOpen() ? finishCurrentTask() : false;

    case SCENE_TASK_OPERATION_UNESTABLISHED:

        return finishCurrentTask();

    case SCENE_TASK_OPERATION_NONE:
        break;
    }
    return true;
}

SceneTaskOperation sceneTaskOperation(void) { return g_sceneTasks.operation; }
bool sceneTaskPending(void) { return g_sceneTasks.pending; }
bool sceneTaskFailed(void) { return g_sceneTasks.failed; }
int sceneTaskScriptEntryIndex(void) { return g_sceneTasks.scriptEntryIndex; }
int sceneTaskScriptPageId(void) { return g_sceneTasks.scriptPageId; }

void
completeSceneTask(void)
{
    if(g_sceneTasks.operation ==
       SCENE_TASK_OPERATION_RELOAD_MAIN_CHARACTER)
        g_sceneTasks.externallyComplete = true;
}

int sceneEventMode(void) { return g_sceneTasks.eventMode; }
bool sceneEventInputLocked(void) { return g_sceneTasks.eventMode == 0; }
int sceneCharacterSelectCommand(void)
{
    return g_sceneTasks.characterCommand;
}
SceneCharacterSelectState sceneCharacterSelectState(void)
{
    return g_sceneTasks.characterState;
}
int sceneCharacterSelectFadeStep(void)
{
    return g_sceneTasks.characterFadeStep;
}
int sceneCharacterSelectAlpha(void)
{
    return g_sceneTasks.characterAlpha;
}
bool sceneCharacterSelectActive(void)
{
    return g_sceneTasks.characterActive;
}
bool sceneCharacterSelectAlternateGeometry(void)
{
    return g_sceneTasks.characterAlternateGeometry;
}
bool sceneCharacterSelectReady(int target)
{
    return target >= 0 && target < 2 &&
        g_sceneTasks.characterAlpha == (target == 0 ? 0 : 0x7f);
}

void
setSceneCharacterSelectCommand(int command, bool confirmBlocked)
{
    ensureSceneTasksInitialized();
    applyCharacterSelectCommand(command, confirmBlocked);
}

void
completeSceneCharacterSelectFadeIn(void)
{
    ensureSceneTasksInitialized();
    g_sceneTasks.characterAlpha = 0x7f;
    g_sceneTasks.characterActive = true;
    g_sceneTasks.characterAlternateGeometry = false;
    g_sceneTasks.characterState = SCENE_CHARACTER_SELECT_FADE_IN;
}

void
advanceSceneCharacterSelectOverlayFrame(void)
{
    ensureSceneTasksInitialized();
    switch(g_sceneTasks.characterState)
    {
    case SCENE_CHARACTER_SELECT_CLEAR:
        g_sceneTasks.characterAlpha = 0;
        g_sceneTasks.characterActive = false;
        g_sceneTasks.characterAlternateGeometry = false;
        g_sceneTasks.characterState = SCENE_CHARACTER_SELECT_IDLE;
        break;
    case SCENE_CHARACTER_SELECT_FLASH:

        g_sceneTasks.characterAlpha = 0x7f;
        g_sceneTasks.characterActive = true;
        g_sceneTasks.characterState =
            SCENE_CHARACTER_SELECT_FLASH_COMPLETE;
        g_sceneTasks.characterAlpha = 0;
        g_sceneTasks.characterActive = false;
        g_sceneTasks.characterState = SCENE_CHARACTER_SELECT_CLEAR;
        break;
    case SCENE_CHARACTER_SELECT_FADE_IN_BEGIN:
        g_sceneTasks.characterAlternateGeometry = false;
        g_sceneTasks.characterActive = true;
        g_sceneTasks.characterState = SCENE_CHARACTER_SELECT_FADE_IN;

    case SCENE_CHARACTER_SELECT_FADE_IN:
        g_sceneTasks.characterAlpha += g_sceneTasks.characterFadeStep;
        if(g_sceneTasks.characterAlpha >= 0x7f)
            g_sceneTasks.characterAlpha = 0x7f;
        break;
    case SCENE_CHARACTER_SELECT_FADE_OUT_BEGIN:
        g_sceneTasks.characterAlternateGeometry = false;
        g_sceneTasks.characterActive = true;
        g_sceneTasks.characterState = SCENE_CHARACTER_SELECT_FADE_OUT;

    case SCENE_CHARACTER_SELECT_FADE_OUT:
        g_sceneTasks.characterAlpha -= g_sceneTasks.characterFadeStep;
        if(g_sceneTasks.characterAlpha <= 0)
        {
            g_sceneTasks.characterAlpha = 0;
            g_sceneTasks.characterActive = false;
            g_sceneTasks.characterState = SCENE_CHARACTER_SELECT_CLEAR;
        }
        break;
    case SCENE_CHARACTER_SELECT_ALTERNATE_BEGIN:

        g_sceneTasks.characterAlpha = 0x7f;
        g_sceneTasks.characterActive = true;
        g_sceneTasks.characterAlternateGeometry = true;
        g_sceneTasks.characterState = SCENE_CHARACTER_SELECT_ALTERNATE;
        break;
    case SCENE_CHARACTER_SELECT_IDLE:
    case SCENE_CHARACTER_SELECT_FLASH_COMPLETE:
    case SCENE_CHARACTER_SELECT_ALTERNATE:
        break;
    }
}

bool
takeSceneCharacterCaptureRequest(void)
{
    const bool requested = g_sceneTasks.characterCaptureRequested;
    g_sceneTasks.characterCaptureRequested = false;
    return requested;
}

bool
takeSceneEventFlowStartRequest(void)
{
    const bool requested = g_sceneTasks.eventFlowStartRequested;
    g_sceneTasks.eventFlowStartRequested = false;
    return requested;
}

bool
takeSceneAmbientStopRequest(int* categoryMask)
{
    if(!g_sceneTasks.ambientStopRequested)
        return false;
    if(categoryMask != NULL)
        *categoryMask = g_sceneTasks.ambientStopMask;
    g_sceneTasks.ambientStopRequested = false;
    return true;
}

void setSceneCharacterSelectFadeStep(int alphaStep)
{
    ensureSceneTasksInitialized();
    g_sceneTasks.characterFadeStep = alphaStep;
}

void setSceneCharacterSelectOverlayColor(uint8_t red, uint8_t green, uint8_t blue)
{
    ensureSceneTasksInitialized();
    g_sceneTasks.characterRed = red;
    g_sceneTasks.characterGreen = green;
    g_sceneTasks.characterBlue = blue;
    g_sceneTasks.characterColorSet = true;
}

bool sceneCharacterSelectOverlayColor(uint8_t* red, uint8_t* green, uint8_t* blue)
{
    if(red != nullptr) *red = g_sceneTasks.characterRed;
    if(green != nullptr) *green = g_sceneTasks.characterGreen;
    if(blue != nullptr) *blue = g_sceneTasks.characterBlue;
    return g_sceneTasks.characterColorSet;
}
