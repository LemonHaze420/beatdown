#ifndef SCENE_VIEW_H
#define SCENE_VIEW_H

#include "camera/camera.h"

int sceneHudMode(void);
bool sceneHudVisible(void);
int sceneHudPrideSide(void);
bool sceneManualCamera(void);

bool sceneHudOverrideActive(void);

enum SceneHudScreenSide
{
    SCENE_HUD_LEFT = 0,
    SCENE_HUD_RIGHT = 1
};

bool sceneHudSideVisible(int screenSide);

void updateSceneManualCamera(float horizontalAxis, float verticalAxis,
                             float frameStep);
PlayerFollowFraming applySceneManualCamera(
    const PlayerFollowFraming& automaticFraming);

void drawSceneCharacterSelectOverlay(void);
void resetSceneView(void);

#endif
