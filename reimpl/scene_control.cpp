#include "scene_control.h"

#include "game.h"

namespace
{

struct SceneControlRuntime
{
    bool initialized;
    bool observedRelationshipReset;
    bool relationshipResetApplied;
    int relationshipResetChangeCount;
};

SceneControlRuntime g_sceneControl;

void
ensureSceneControlInitialized(void)
{
    if(!g_sceneControl.initialized)
        resetSceneControl();
}

}

uint32_t
sceneStageControlFlagBits(void)
{
    return g_game.sceneSequence().controlFlagBits();
}

bool
sceneStageControlFlagActive(SceneStageControlFlag flag)
{
    return (sceneStageControlFlagBits() & static_cast<uint32_t>(flag)) != 0;
}

int
applySceneRelationshipStateReset(StoryState& state)
{
    int changed = 0;
    for(int i = 0; i < STORY_CHARACTER_COUNT; ++i)
    {
        StoryCharacterState& character = state.characters[i];
        if(character.relationshipState == 2)
        {
            character.relationshipState = 0;
            ++changed;
        }
    }
    return changed;
}

void
resetSceneControl(void)
{
    g_sceneControl.initialized = true;
    g_sceneControl.observedRelationshipReset = false;
    g_sceneControl.relationshipResetApplied = false;
    g_sceneControl.relationshipResetChangeCount = 0;
}

void
updateSceneControl(void)
{
    ensureSceneControlInitialized();
    const bool requested = g_game.sceneSequence().relationshipStatesReset();
    if(requested && !g_sceneControl.observedRelationshipReset)
    {
        g_sceneControl.relationshipResetChangeCount =
            applySceneRelationshipStateReset(g_game.runtimeState().story);
        g_sceneControl.relationshipResetApplied = true;
    }
    g_sceneControl.observedRelationshipReset = requested;
}

bool
sceneRelationshipStateResetApplied(void)
{
    ensureSceneControlInitialized();
    return g_sceneControl.relationshipResetApplied;
}

int
sceneRelationshipStateResetChangeCount(void)
{
    ensureSceneControlInitialized();
    return g_sceneControl.relationshipResetChangeCount;
}
