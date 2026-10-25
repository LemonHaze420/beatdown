#ifndef GAME_BATTLE_FINISHER_H
#define GAME_BATTLE_FINISHER_H

#include "game_save.h"
#include "game_random.h"

struct BattleFinisherSelectionInput
{
    float targetHealthRatio;
    uint8_t attackerGender;
    uint8_t targetGender;
    bool armed;
    PersistentMoveSlot moveSlots[2];
    uint8_t moveSlotActive[2];
};

struct BattleFinisherSelection
{
    int variant;
    uint8_t usesMoveSlot;
    uint8_t moveSlotIndex;
};

BattleFinisherSelection selectBattleFinisherVariant(
    const BattleFinisherSelectionInput& input, GameRandom& random);

int battleFinisherRelation(int attackerMotionVariant, int targetMotionVariant);

#endif
