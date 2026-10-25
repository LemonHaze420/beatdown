#include "game/battle/tables.h"

const BattleResourceList* battleResourceList(uint32_t address)
{
    for(int index = 0; index < BATTLE_RESOURCE_LIST_COUNT; ++index)
        if(BATTLE_RESOURCE_LISTS[index].address == address)
            return &BATTLE_RESOURCE_LISTS[index];
    return nullptr;
}
