#include "game.h"
#include "game_internal.h"
#include "stage.h"
#include "stage_weapon_grants.h"
#include "character_records.h"

void Game::resetCombatWeaponAttachments()
{
    if(combatItemScene == nullptr)
        return;
    for(int index = 0; index < GAME_COMBAT_SLOT_COUNT; ++index)
    {
        GameSlotRecord* slot = mutableCombatSlotByIndex(static_cast<uint8_t>(index));
        if(slot == nullptr || (slot->flags & 1u) == 0 ||
           (slot->actorActionFlags & 0x4000u) == 0 ||
           (slot->combatBoneMask & (1u << 21)) == 0 ||
           slot->heldWorldWeapon < 0 || slot->heldWorldWeapon >= FSTAGE_WEAPON_CAPACITY)
            continue;
        const int16_t worldId = slot->heldWorldWeapon;
        const FstageWeaponInstance& world = combatItemScene->stageWeapons.instances[worldId];
        const WeaponDefinition* definition = weaponManager.definition(world.selector);
        if(definition == nullptr)
            continue;
        FstageWeaponActorState actor = combatWeaponActorState(*slot);
        const Vec3& at = slot->combatBones[21].at;
        const rw::V3d forward = { at.x, at.y, at.z };
        if(resetFstageWeaponAttachment(combatItemScene, worldId, &actor,
                                       *definition, forward))
        {
            slot->heldWorldWeapon = actor.heldWeapon;
            slot->boundWorldWeapon = actor.linkedHeldWeapon;
            slot->actorActionFlags = actor.actionFlags;
            dropCombatWeaponManager(worldId);
        }
    }
}

void Game::initializeCombatOwnedWeapons()
{
    if(combatItemScene == nullptr || combatItemScene->profileStageId != persistent.stageId)
        return;

    for(int worldId = 0; worldId < FSTAGE_WEAPON_CAPACITY; ++worldId)
    {
        const FstageWeaponInstance& world = combatItemScene->stageWeapons.instances[worldId];
        if(!world.active || world.originalMoveSlot < 0)
            continue;
        GameCombatWeaponBinding& binding = combatWeaponBindings[worldId];
        if(binding.managerId >= 0)
            weaponManager.release(binding.managerId);
        binding = {};
        releaseFstageWeapon(combatItemScene, static_cast<int16_t>(worldId));
    }
    for(int index = 0; index < GAME_COMBAT_SLOT_COUNT; ++index)
    {
        GameSlotRecord* slot = mutableCombatSlotByIndex(static_cast<uint8_t>(index));
        if(slot == nullptr || (slot->flags & 1u) == 0)
            continue;
        slot->ownedWorldWeapons[0] = slot->ownedWorldWeapons[1] = -1;
        slot->boundWorldWeapon = slot->defaultBoundWorldWeapon = -1;
        slot->defaultWeaponAttachmentPending = false;
        for(int moveSlot = 0; moveSlot < 2; ++moveSlot)
        {
            const PersistentMoveSlot& equipped = slot->moveSlots[moveSlot];
            if(equipped.type != 2 || equipped.id < 0 || equipped.id >= 48)
                continue;
            const WeaponDefinition* definition = weaponManager.definition(
                static_cast<uint16_t>(equipped.id));
            if(definition == nullptr)
                continue;
            const int16_t worldId = grantFstageWeapon(combatItemScene, *definition,
                slot->slotIndex, static_cast<uint8_t>(moveSlot));
            if(worldId < 0)
                continue;
            if(ensureCombatWorldWeapon(worldId) < 0)
            {
                releaseFstageWeapon(combatItemScene, worldId);
                continue;
            }
            slot->ownedWorldWeapons[moveSlot] = worldId;

            slot->boundWorldWeapon = slot->defaultBoundWorldWeapon = worldId;
        }

        if(currentMode == GAME_MODE_STORY || slot->slotIndex > 1)
            continue;
        slot->heldWorldWeapon = slot->boundWorldWeapon;
        int profile = slot->resourceProfileId;
        if(profile == 0x86 || profile == 0x88 || profile == 0x89 ||
           profile == 0x8e || profile == 0x91 || profile == 0x92)
        {
            const PersistentGameRecord* record = characterRecord(
                static_cast<uint16_t>(profile));
            if(record != nullptr)
                profile = record->characterId;
        }
        if(profile == 0x21a || profile == 0x219 || profile == 0x218 ||
           profile == 0x217 || profile == 0xb9 || profile == 0x49)
            profile = 0xb9;
        slot->defaultWeaponAttachmentPending = slot->heldWorldWeapon >= 0 &&
            (profile == 0x4a || profile == 0x39 || profile == 0xb9 || profile == 0x3f);
        prepareDefaultCombatWeapon(*slot);
    }
}

void Game::prepareDefaultCombatWeapon(GameSlotRecord& slot)
{
    if(!slot.defaultWeaponAttachmentPending || combatItemScene == nullptr)
        return;
    const FstageWeaponActorState actor = combatWeaponActorState(slot);
    if(actor.attachmentBones[0] == nullptr)
        return;
    const int16_t worldId = slot.boundWorldWeapon;
    const int16_t managerId = ensureCombatWorldWeapon(worldId);
    if(managerId < 0 || !weaponManager.pickup(managerId, slot.slotIndex))
        return;
    slot.heldWorldWeapon = worldId;
    updateCombatWeaponBehaviour(slot, MOVE_BEHAVIOUR_WEAPON_ACTION, 1);
    if((slot.actorActionFlags & 0x4000u) != 0)
    {
        setFstageWeaponVisible(combatItemScene, worldId, true);
        slot.defaultWeaponAttachmentPending = false;
    }
}

int Game::combatOwnedWeaponInputMove(const GameSlotRecord& slot, int command) const
{
    if(command != 0x4c && command != 0x4d)
        return -1;
    const int selected = command - 0x4c;
    if((persistent.story.gameStateFlags & 2u) != 0 &&
       (persistent.stageId == 0x33 || persistent.stageId == 0x21))
        return -1;
    if((slot.actorActionFlags & 0x4000u) != 0)
    {
        if(combatItemScene == nullptr || slot.heldWorldWeapon < 0 ||
           slot.heldWorldWeapon >= FSTAGE_WEAPON_CAPACITY)
            return -1;
        const FstageWeaponInstance& world =
            combatItemScene->stageWeapons.instances[slot.heldWorldWeapon];
        const WeaponDefinition* definition = weaponManager.definition(world.selector);
        const uint8_t ownFlag = slot.slotIndex == 0 ? 0x20 :
            slot.slotIndex == 1 ? 0x40 : 0;
        if(definition == nullptr || (definition->rawWords[30] & 0x20u) == 0 ||
           ownFlag == 0 || (world.flags & ownFlag) == 0)
            return -1;
        return game_detail::actorMotionIdFromAction(slot, 441);
    }
    const PersistentMoveSlot& equipped = slot.moveSlots[selected];
    if(equipped.type == 1)
        return equipped.id != 0 ? moveEquippedTypeOne(equipped.id) : -1;
    if(equipped.type != 2 || equipped.id == -1 || slot.moveSlotActive[selected] == 0 ||
       (persistent.story.gameStateFlags & 1u) == 0 ||
       slot.ownedWorldWeapons[selected] < 0)
        return -1;
    return 396;
}

void Game::prepareCombatOwnedWeaponInput(GameSlotRecord& slot, int command, int move)
{
    if(command != 0x4c && command != 0x4d)
        return;
    slot.selectedEquippedSlot = static_cast<uint8_t>(command - 0x4c);
    if(move <= 0)
        return;
    if((slot.actorActionFlags & 0x4000u) != 0)
    {
        if(move == game_detail::actorMotionIdFromAction(slot, 441))
        {
            setCombatReactionPair(slot, 1, 0);
            slot.queuedMoveId = combatActionPairs[slot.slotIndex].selfStateValue;
        }
        return;
    }
    slot.selectedWorldItemIndex = -1;
    if(slot.moveSlots[slot.selectedEquippedSlot].type == 2)
    {
        const int16_t worldId = slot.ownedWorldWeapons[slot.selectedEquippedSlot];
        const int16_t managerId = ensureCombatWorldWeapon(worldId);
        if(managerId >= 0 && weaponManager.pickup(managerId, slot.slotIndex))
            slot.heldWorldWeapon = worldId;
    }
}

void Game::returnCombatOwnedWeapon(GameSlotRecord& slot)
{
    const int16_t worldId = slot.heldWorldWeapon;

    setFstageWeaponVisible(combatItemScene, worldId, false);
    FstageWeaponActorState actor = combatWeaponActorState(slot);
    uint8_t returnedMoveSlot = 0xff;
    if(!returnOwnedFstageWeapon(combatItemScene, worldId, &actor, &returnedMoveSlot))
        return;
    if(returnedMoveSlot < 2)
        slot.moveSlotActive[returnedMoveSlot] = 1;
    slot.heldWorldWeapon = actor.heldWeapon;
    slot.actorActionFlags = actor.actionFlags;
    dropCombatWeaponManager(worldId);
}
