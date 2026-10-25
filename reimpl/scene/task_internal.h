#ifndef SCENE_TASK_INTERNAL_H
#define SCENE_TASK_INTERNAL_H

#include "scene/task.h"

struct SceneTaskRuntime
{
    SceneTaskOperation operation;
    bool pending;
    bool failed;
    bool started;
    bool externallyComplete;
    int scriptEntryIndex;
    int scriptPageId;

    int sequenceId;
    bool sequenceWasRunning;
    int observedEventMode;
    int observedCharacterCommand;
    int observedClearMask;
    bool observedEventFlow;

    int eventMode;
    int characterCommand;
    SceneCharacterSelectState characterState;
    int characterFadeStep;
    int characterAlpha;
    uint8_t characterRed;
    uint8_t characterGreen;
    uint8_t characterBlue;
    bool characterColorSet;
    bool characterActive;
    bool characterAlternateGeometry;
    bool characterCaptureRequested;
    bool eventFlowStartRequested;
    bool ambientStopRequested;
    int ambientStopMask;
};

#endif
