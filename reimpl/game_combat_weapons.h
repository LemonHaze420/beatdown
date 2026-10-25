#ifndef GAME_COMBAT_WEAPONS_H
#define GAME_COMBAT_WEAPONS_H

#include "weapon.h"
#include <stdint.h>

namespace rw
{
struct Frame;
struct Clump;
}

struct FstageWeaponActorState;

struct GameCombatWeaponBinding
{
    int16_t managerId = -1;
    uint16_t objectId = 0;
    rw::Clump* clump = nullptr;
};

struct GameCombatWeaponCallbacks
{

    bool (*frames)(void* userData, uint8_t slotIndex, rw::Frame** frames) = nullptr;
    void* userData = nullptr;
};

#endif
