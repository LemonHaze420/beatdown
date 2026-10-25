#ifndef GAME_BATTLE_SELECTION_H
#define GAME_BATTLE_SELECTION_H

#include "game/battle/sequence.h"
#include "game/random.h"
#include "game/save.h"

struct BattleActionSelectionInput
{
    bool surfaceContact = false;
    bool armed = false;
    PersistentMoveSlot moveSlots[2] = {};
    uint8_t moveSlotActive[2] = {};
};

int selectBattleAction(BattleSequenceRuntime& runtime,
    const BattleActionSelectionInput& input, GameRandom& random);

struct BattleOutcomePrideInput
{
    int reactionType = 0;
    int phase = 0;
    int outcome = 0;
    bool usesWeapon = false;
    int8_t characterModifiers[2] = {};
    int weaponClass = 0;
};

int battleOutcomePrideDamage(const BattleOutcomePrideInput& input,
                             GameRandom& random);

#endif
