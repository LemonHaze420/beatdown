#ifndef SCENE_MASK_H
#define SCENE_MASK_H

enum SceneMaskMode
{
    SCENE_MASK_NONE = 0,
    SCENE_MASK_FULL_FRAME = 1,
    SCENE_MASK_INSET = 2,
    SCENE_MASK_HORIZONTAL_INSET = 3,
    SCENE_MASK_CYCLING = 4
};

int sceneMaskMode(void);

void setSceneMaskOffsetMode(bool enabled);

void setSceneMaskEffectMode(int mode, bool active);

void resetSceneMask(void);

void drawSceneMask(void);

#endif
