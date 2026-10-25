#include "game.h"
#include "game_combat_input.h"
#include "stage.h"
#include "move_tables.h"

void Game::setCombatItemScene(FstageScene* scene)
{
    for(GameCombatWeaponBinding& binding : combatWeaponBindings)
    {
        if(binding.managerId >= 0)
            weaponManager.release(binding.managerId);
        binding = {};
    }
    for(int index = 0; index < GAME_COMBAT_SLOT_COUNT; ++index)
    {
        GameSlotRecord* actor = mutableCombatSlotByIndex(static_cast<uint8_t>(index));
        if(actor == nullptr)
            continue;
        if(actor->heldWorldWeapon >= 0)
            actor->actorActionFlags &= ~0x4000u;
        actor->heldWorldWeapon = actor->targetedWorldWeapon = -1;
        actor->ownedWorldWeapons[0] = actor->ownedWorldWeapons[1] = -1;
        actor->boundWorldWeapon = actor->defaultBoundWorldWeapon = -1;
        actor->defaultWeaponAttachmentPending = false;
    }
    combatItemScene = scene;
    initializeCombatOwnedWeapons();
}

int Game::selectCombatWorldItem(GameSlotRecord& slot)
{
    if(combatItemScene == NULL ||
       combatItemScene->profileStageId != persistent.stageId ||
       slot.playerIndex >= 2 || (slot.actorActionFlags & 0x4000u) != 0)
        return -1;
    const GameSlotRecord* const target =
        mutableCombatSlotByIndex(slot.linkedSlotIndex);
    if(target != NULL)
    {
        const bool suppressedDeadTarget =
            (persistent.story.battleFlags & 0x200u) != 0 &&
            target->currentHealth <= 0;
        if(!suppressedDeadTarget &&
           (target->impactSelectorState & 4u) != 0 &&
           slot.field_0x439c <= 15.0f && !combatTargetInteractable(*target) &&
           (target->actorRuntimeFlags & 0xcu) == 0)
        {

            return -1;
        }
    }
    const rw::V3d position =
        { slot.position.x, slot.position.y, slot.position.z };
    const int index = findFstageWorldItem(combatItemScene, position);
    if(index < 0)
        return selectCombatWorldWeapon(slot);
    slot.selectedWorldItemIndex = index;
    return 1930;
}

int Game::combatWorldItemCommand(const GameSlotRecord& slot) const
{
    FstageWorldItem item;
    if(combatItemScene == NULL ||
       combatItemScene->profileStageId != persistent.stageId ||
       !queryFstageWorldItem(combatItemScene,
                             slot.selectedWorldItemIndex, &item))
        return -1;
    return moveDispatchCommand(item.kind, item.selector);
}

void Game::updateCombatWorldItemPickup(GameSlotRecord& slot)
{

    if((slot.animationCueFlags & 4u) == 0)
        return;
    slot.animationCueFlags &= ~4u;
    const int command = combatWorldItemCommand(slot);
    const int move = moveCommandFollowUp(command);
    if(move > 0)
        queueCombatPair(slot, move, 0);
}
