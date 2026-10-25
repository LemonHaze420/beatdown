#ifndef SCENE_CONTROL_H
#define SCENE_CONTROL_H

#include "story_state.h"

#include <stdint.h>

enum SceneStageControlFlag : uint32_t
{
    SCENE_STAGE_CONTROL_FLAG_BASE = 0x10000u,
    SCENE_STAGE_CONTROL_FLAG_SELECTOR_1 = 0x20000u,
    SCENE_STAGE_CONTROL_FLAG_SELECTOR_2 = 0x40000u,
    SCENE_STAGE_CONTROL_FLAG_SELECTOR_3 = 0x80000u,
    SCENE_STAGE_CONTROL_FLAG_SELECTOR_4 = 0x100000u
};

uint32_t sceneStageControlFlagBits(void);
bool sceneStageControlFlagActive(SceneStageControlFlag flag);

int applySceneRelationshipStateReset(StoryState& state);

void resetSceneControl(void);

void updateSceneControl(void);
bool sceneRelationshipStateResetApplied(void);
int sceneRelationshipStateResetChangeCount(void);

#endif
