#include "game/battle/selection.h"
#include "game/equipped_moves.h"

int selectBattleAction(BattleSequenceRuntime& runtime,
    const BattleActionSelectionInput& input, GameRandom& random)
{
    int selected = input.surfaceContact ? 2 : 1;
    int fallback = selected;
    int reaction = 0;
    uint8_t variant = 0;
    uint8_t selector = 0;
    bool selectedSlot = false;
    if(!input.armed)
    {
        for(int index = 0; index < 2; ++index)
        {
            if(input.moveSlotActive[index] == 0)
                continue;
            const PersistentMoveSlot& slot = input.moveSlots[index];
            int type = 0;
            int slotReaction = 0;
            bool weapon = false;
            if(slot.type == 2 && slot.id >= 0 &&
               slot.id < static_cast<int>(sizeof(game_detail::EQUIPPED_MOVE_CATEGORIES)))
            {
                switch(game_detail::EQUIPPED_MOVE_CATEGORIES[slot.id])
                {
                case 10:
                case 3: type = 6; break;
                case 6: type = 5; break;
                case 1:
                case 2: type = 4; break;
                default: break;
                }
                slotReaction = 2;
                weapon = true;
            }
            else if(slot.type == 1)
            {
                if(slot.id == 4)
                {
                    type = 3;
                    slotReaction = 1;
                }
                else if(slot.id == 3)
                {
                    type = 4;
                    slotReaction = 2;
                }
            }
            if(type == 0)
                continue;
            if(!selectedSlot || (nextGameRandom(random) & 1u) != 0)
            {
                selected = type;
                fallback = 1;
                reaction = slotReaction;
                variant = weapon;
                selector = static_cast<uint8_t>(index);
            }
            selectedSlot = true;
        }
    }
    runtime.selectedActionType = selected;
    runtime.pendingActionType = fallback;
    runtime.reactionType = reaction;
    runtime.pendingReactionType = 0;
    runtime.actionVariant = variant;
    runtime.pendingActionVariant = 0;
    runtime.actionSelector = selector;
    return selected;
}

int battleOutcomePrideDamage(const BattleOutcomePrideInput& input,
                             GameRandom& random)
{
    int damage;
    if(input.reactionType == 2)
    {
        damage = -input.characterModifiers[1];
        if(input.phase == 0 && input.outcome != 3 && input.usesWeapon &&
           input.weaponClass >= 0 && input.weaponClass < BATTLE_WEAPON_PRIDE_CLASS_COUNT)
            damage += BATTLE_WEAPON_PRIDE_BONUSES[input.weaponClass];
    }
    else if(input.reactionType == 1)
        damage = -input.characterModifiers[0];
    else
        damage = static_cast<int>(nextGameRandom(random) % 10u) + 10;
    return damage < 0 ? 0 : damage;
}
