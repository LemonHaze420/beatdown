#include "game/game.h"
#include "game/combat/input.h"
#include "game/internal.h"
#include "character_records.h"

int Game::combatWeaponInputMove(const GameSlotRecord& slot, int command,
                                 bool& handled) const
{
    handled = false;
    if(command < 0 || (slot.actorActionFlags & 0x4000u) == 0)
        return -1;
    const WeaponInstance* held = weaponManager.equippedBy(slot.slotIndex);
    const WeaponDefinition* definition = held != nullptr ?
        weaponManager.definition(held->objectId) : nullptr;
    if(definition == nullptr || definition->rawWords[1] == 0 ||
       definition->rawWords[1] == 0xbu)
        return -1;

    const int style = combatStyle(slot);
    const int category = style >= 0 && style < MOVE_CHARACTER_COUNT ?
        MOVE_CHARACTER_RECORDS[style].personalMovesetId : 0;
    const int selector = combatWeaponChainSelector(
        definition->behaviorId, category, held->objectId);
    if(selector > 0)
    {

        if(!weaponManager.canUse(held->managerId) &&
           (definition->rawWords[30] & 2u) == 0)
        {
            handled = true;
            return stanceMotion(slot, weaponEmptyAction(definition->behaviorId));
        }
        const int move = moveInitialActionChain(MOVE_WEAPON_CHAIN_FAMILIES[selector],
            command, combatPersonalActionContext(slot));
        if(move != 0)
        {
            handled = true;
            return move;
        }
    }
    int move;
    if(weaponInputFallback(definition->behaviorId, command, move))
    {
        handled = true;
        return move;
    }
    return -1;
}

void Game::setCombatReactionPair(GameSlotRecord& actor, int selfAction,
                                  int linkedAction)
{
    if(actor.slotIndex >= GAME_COMBAT_SLOT_COUNT)
        return;
    CombatActionPairRecord& pair = combatActionPairs[actor.slotIndex];
    const GameSlotRecord* linked = combatSlotByIndex(actor.linkedSlotIndex);
    pair.selfStateIndex = selfAction;
    pair.linkedStateIndex = linkedAction;
    pair.selfStateValue = static_cast<int16_t>(game_detail::actorMotionIdFromAction(actor, selfAction));
    pair.linkedStateValue = linked != nullptr ?
        static_cast<int16_t>(game_detail::actorMotionIdFromAction(*linked, linkedAction)) : 0;
}

void Game::prepareCombatWeaponInput(GameSlotRecord& slot, int command, int move)
{
    bool handled;
    if(move <= 0 || combatWeaponInputMove(slot, command, handled) != move || !handled)
        return;
    const WeaponInstance* held = weaponManager.equippedBy(slot.slotIndex);
    const WeaponDefinition* definition = held != nullptr ?
        weaponManager.definition(held->objectId) : nullptr;
    if(definition == nullptr || weaponManager.canUse(held->managerId) ||
       (definition->rawWords[30] & 2u) != 0)
        return;

    setCombatReactionPair(slot, 1, 0);
    slot.queuedMoveId = combatActionPairs[slot.slotIndex].selfStateValue;
}
