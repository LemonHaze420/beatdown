#ifndef GAME_BATTLE_RESOURCES_H
#define GAME_BATTLE_RESOURCES_H

#include "game/battle/tables.h"
#include "move_tables.h"

typedef bool BattleResourceLoad(void* userData, int resourceId);
struct BattleResourceCallbacks
{
    BattleResourceLoad* load = nullptr;
    void* userData = nullptr;
};

struct BattleResourceRuntime
{
    uint32_t listAddress = 0;
    int cursor = 0;
    bool complete = false;
    uint8_t references[MOVE_ANIMATION_BUNDLE_COUNT] = {};
    uint32_t registrationOrder[MOVE_ANIMATION_BUNDLE_COUNT] = {};
    uint32_t nextRegistration = 0;
};

bool updateBattleResourceSequence(BattleResourceRuntime& runtime,
    bool advance, uint32_t listAddress, const BattleResourceCallbacks& callbacks);

int battleResourceForMotion(const BattleResourceRuntime& runtime, int motionId,
                            int* positionInBundle = nullptr);

#endif
