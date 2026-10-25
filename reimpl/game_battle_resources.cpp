#include "game_battle_resources.h"

bool updateBattleResourceSequence(BattleResourceRuntime& runtime,
    bool advance, uint32_t listAddress, const BattleResourceCallbacks& callbacks)
{
    if(!advance)
    {
        runtime.listAddress = listAddress;
        runtime.cursor = 0;
        runtime.complete = false;
        return false;
    }
    if(runtime.listAddress != listAddress)
        return false;
    if(runtime.complete)
        return true;
    const BattleResourceList* list = battleResourceList(listAddress);
    if(list == nullptr)
        return false;
    if(runtime.cursor >= list->count)
    {
        runtime.complete = true;
        return true;
    }
    const int resource = BATTLE_RESOURCE_IDS[list->firstId + runtime.cursor];
    if(resource < 0 || resource >= MOVE_ANIMATION_BUNDLE_COUNT)
        return false;
    const MoveAnimationBundle& bundle = MOVE_ANIMATION_BUNDLES[resource];
    if(runtime.references[resource] == 0 && bundle.xafEntry >= 0)
    {
        if(callbacks.load == nullptr || !callbacks.load(callbacks.userData, resource))
            return false;
        runtime.registrationOrder[resource] = ++runtime.nextRegistration;
    }
    ++runtime.references[resource];
    ++runtime.cursor;
    runtime.complete = runtime.cursor == list->count;
    return runtime.complete;
}

int battleResourceForMotion(const BattleResourceRuntime& runtime, int motionId,
                            int* positionInBundle)
{
    int result = -1;
    int position = -1;
    uint32_t order = 0;
    for(int id = 0; id < MOVE_ANIMATION_BUNDLE_COUNT; ++id)
    {
        if(runtime.references[id] == 0 || runtime.registrationOrder[id] <= order)
            continue;
        const MoveAnimationBundle& bundle = MOVE_ANIMATION_BUNDLES[id];
        for(int index = 0; index < bundle.motionCount; ++index)
            if(MOVE_BUNDLE_MOTIONS[bundle.firstMotion + index] == motionId)
            {
                result = id;
                position = index;
                order = runtime.registrationOrder[id];
                break;
            }
    }
    if(positionInBundle != nullptr)
        *positionInBundle = position;
    return result;
}
