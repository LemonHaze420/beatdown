#ifndef SCENE_TASK_H
#define SCENE_TASK_H
#include <stdint.h>

enum SceneTaskOperation
{
    SCENE_TASK_OPERATION_NONE = -1,
    SCENE_TASK_OPERATION_RELOAD_MAIN_CHARACTER = 1,
    SCENE_TASK_OPERATION_EVENT_SCRIPT = 2,
    SCENE_TASK_OPERATION_SHOP_SELL_FLOW = 3,
    SCENE_TASK_OPERATION_UNESTABLISHED = 0x7fffffff
};

const uint16_t SCENE_CHARACTER_SELECT_CONFIRM_BLOCK_FLAG = 0x0848;

enum SceneCharacterSelectState
{
    SCENE_CHARACTER_SELECT_CLEAR = 0,
    SCENE_CHARACTER_SELECT_IDLE = 1,
    SCENE_CHARACTER_SELECT_FLASH = 2,
    SCENE_CHARACTER_SELECT_FLASH_COMPLETE = 3,
    SCENE_CHARACTER_SELECT_FADE_IN_BEGIN = 4,
    SCENE_CHARACTER_SELECT_FADE_IN = 5,
    SCENE_CHARACTER_SELECT_FADE_OUT_BEGIN = 6,
    SCENE_CHARACTER_SELECT_FADE_OUT = 7,
    SCENE_CHARACTER_SELECT_ALTERNATE_BEGIN = 8,
    SCENE_CHARACTER_SELECT_ALTERNATE = 9
};

void resetSceneTasks(void);

void updateSceneTasks(void);

bool sceneTaskFinished(int subOp);

SceneTaskOperation sceneTaskOperation(void);
bool sceneTaskPending(void);
bool sceneTaskFailed(void);

int sceneTaskScriptEntryIndex(void);
int sceneTaskScriptPageId(void);

void completeSceneTask(void);

int sceneEventMode(void);
bool sceneEventInputLocked(void);

int sceneCharacterSelectCommand(void);
SceneCharacterSelectState sceneCharacterSelectState(void);
int sceneCharacterSelectFadeStep(void);
void setSceneCharacterSelectFadeStep(int alphaStep);
void setSceneCharacterSelectOverlayColor(uint8_t red, uint8_t green, uint8_t blue);
bool sceneCharacterSelectOverlayColor(uint8_t* red, uint8_t* green, uint8_t* blue);
int sceneCharacterSelectAlpha(void);
bool sceneCharacterSelectActive(void);
bool sceneCharacterSelectAlternateGeometry(void);
bool sceneCharacterSelectReady(int target);
void setSceneCharacterSelectCommand(int command, bool confirmBlocked = false);
void completeSceneCharacterSelectFadeIn(void);
void advanceSceneCharacterSelectOverlayFrame(void);
bool takeSceneCharacterCaptureRequest(void);

bool takeSceneEventFlowStartRequest(void);

bool takeSceneAmbientStopRequest(int* categoryMask);

#endif
