#include "game/battle/finisher.h"
#include "game/equipped_moves.h"

int battleFinisherRelation(int attackerMotionVariant, int targetMotionVariant)
{
    const int difference = targetMotionVariant - attackerMotionVariant;
    switch(difference)
    {
    case 2: return 4;
    case 1: return 3;
    case -1: return 2;
    case -2: return 1;
    default: return 0;
    }
}

BattleFinisherSelection selectBattleFinisherVariant(
    const BattleFinisherSelectionInput& input, GameRandom& random)
{
    const bool lowHealth = input.targetHealthRatio <= 0.2f;
    const bool highHealth = input.targetHealthRatio >= 0.8f;
    BattleFinisherSelection result = {};
    result.variant = input.attackerGender == 1 && input.targetGender == 1 ? 8 :
        lowHealth ? 6 : highHealth ? 7 : 2;
    if(input.armed)
        return result;

    bool selected = false;
    for(int index = 0; index < 2; ++index)
    {
        if(input.moveSlotActive[index] == 0)
            continue;
        const PersistentMoveSlot& slot = input.moveSlots[index];
        int variant = 0;
        uint8_t usesMoveSlot = 0;
        if(slot.type == 2 && slot.id >= 0 &&
           slot.id < static_cast<int>(sizeof(game_detail::EQUIPPED_MOVE_CATEGORIES)))
        {
            switch(game_detail::EQUIPPED_MOVE_CATEGORIES[slot.id])
            {
            case 6: variant = slot.id == 0x23 || lowHealth ? 13 : 5; break;
            case 10: variant = 12; break;
            case 3: variant = slot.id == 0x0d ? 11 : highHealth ? 10 : 4; break;
            case 13:
            case 7:
            case 2: variant = highHealth ? 15 : 3; break;
            case 1: variant = highHealth ? 14 : 9; break;
            default: break;
            }
            usesMoveSlot = variant != 0;
        }
        else if(slot.type == 1 && slot.id == 3)
            variant = 9;
        if(variant == 0)
            continue;

        if(!selected || (nextGameRandom(random) & 1u) != 0)
        {
            result.variant = variant;
            result.usesMoveSlot = usesMoveSlot;
            result.moveSlotIndex = static_cast<uint8_t>(index);
        }
        selected = true;
    }
    return result;
}
