#include "game.h"
#include "item_data.h"
#include "move_tables.h"
#include "stage.h"

uint8_t Game::applyCombatInventoryItemEffect(GameSlotRecord& slot,
                                             int kind, int selector)
{
    if(kind != 1)
        return 0;
    const ItemUseEffect* effect = NULL;
    for(int i = 0; i < ITEM_MAN_RECORD_COUNT; ++i)
        if(ITEM_MAN_RECORDS[i].consumable &&
           ITEM_MAN_RECORDS[i].useEffect.effectCode == selector)
        {
            effect = &ITEM_MAN_RECORDS[i].useEffect;
            break;
        }
    if(effect == NULL)
        return 0;
    const uint16_t recordId =
        static_cast<uint16_t>(slot.persistentRecordId);
    const PersistentGameRecord* const record = characterRecord(recordId);
    const bool oddCompanion =
        effect->slowRule == ITEM_SLOW_UNLESS_RANDOM16_HEALS &&
        record != NULL && record->characterId != 0;
    const uint32_t random =
        effect->slowRule == ITEM_SLOW_NEVER || oddCompanion ?
            0 : nextRandom();
    return applyInventoryItemEffect(*effect, recordId, random, &slot,
                                    currentMode == GAME_MODE_STORY, false);
}

void Game::updateCombatItemUse(GameSlotRecord& slot, bool entered)
{

    const unsigned equipped = slot.selectedEquippedSlot;
    if(entered)
    {
        slot.selectedItemCommand = static_cast<int16_t>(
            combatWorldItemCommand(slot));
        if(slot.selectedItemCommand >= 0)
            removeFstageWorldItem(combatItemScene,
                                  slot.selectedWorldItemIndex);
        else if(equipped < 2)
        {
            const PersistentMoveSlot& selected = slot.moveSlots[equipped];
            for(int i = 1; i < MOVE_COMMAND_COUNT; ++i)
                if(MOVE_COMMANDS[i].kind == selected.type &&
                   MOVE_COMMANDS[i].selector == selected.id)
                {
                    slot.selectedItemCommand = static_cast<int16_t>(i);
                    break;
                }
        }
    }
    if((slot.animationCueFlags & 2u) != 0)
    {
        slot.animationCueFlags &= ~2u;
        if(slot.selectedWorldItemIndex >= 0)
        {
            const int command = slot.selectedItemCommand;
            if(command >= 0 && command < MOVE_COMMAND_COUNT)
                applyCombatInventoryItemEffect(
                    slot, MOVE_COMMANDS[command].kind,
                    MOVE_COMMANDS[command].selector);
        }
        else if(equipped < 2)
        {
            const PersistentMoveSlot& selected = slot.moveSlots[equipped];

            if(applyCombatInventoryItemEffect(slot, selected.type,
                                              selected.id) != 0)
            {
                slot.moveSlots[equipped].type = 0;
                slot.moveSlots[equipped].id = 0;
                slot.moveSlotActive[equipped] = 0;
                PersistentGameRecord* const record = characterRecord(
                    static_cast<uint16_t>(slot.persistentRecordId));
                if(currentMode == GAME_MODE_STORY && record != NULL)
                {
                    record->moveSlots[equipped].type = 0;
                    record->moveSlots[equipped].id = 0;
                }
            }
        }
    }
    if((slot.animationCueFlags & 4u) != 0)
    {
        slot.animationCueFlags &= ~4u;
        slot.actorStatusFlags &= ~8u;
    }
    if(slot.selectedWorldItemIndex >= 0)
    {
        slot.actorRuntimeFlags |= 4u;
        for(int i = 0; i < GAME_COMBAT_SLOT_COUNT; ++i)
        {
            GameSlotRecord* const cpu = mutableCombatSlotByIndex(i);
            if(cpu != NULL && cpu->playerIndex > 1)
                cpu->actorRuntimeFlags |= 0x40u;
        }
    }
}
