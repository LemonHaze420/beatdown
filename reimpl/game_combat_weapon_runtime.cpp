#include "game.h"
#include "stage.h"
#include "game_internal.h"
#include "game_bits.h"
#include "stage_weapon_grants.h"
#include "weapon_combat_tables.h"
#include "character_records.h"
#include <math.h>

void Game::setCombatWeaponCallbacks(const GameCombatWeaponCallbacks& callbacks)
{
    combatWeaponCallbacks = callbacks;
}

FstageWeaponActorState Game::combatWeaponActorState(const GameSlotRecord& slot) const
{
    FstageWeaponActorState actor;
    actor.slotIndex = slot.slotIndex;
    actor.battleSide = slot.battleSide;
    actor.actionFlags = slot.actorActionFlags;
    actor.heldWeapon = slot.heldWorldWeapon;
    actor.targetedWeapon = slot.targetedWorldWeapon;
    actor.linkedHeldWeapon = slot.boundWorldWeapon;
    actor.position = { slot.position.x, slot.position.y, slot.position.z };
    if((slot.combatBoneMask & 1u) != 0)
    {
        const Vec3& at = slot.combatBones[0].at;
        actor.forward = { at.x, at.y, at.z };
    }
    if(combatWeaponCallbacks.frames != nullptr)
        combatWeaponCallbacks.frames(combatWeaponCallbacks.userData, slot.slotIndex,
                                      actor.attachmentBones);
    return actor;
}

int16_t Game::ensureCombatWorldWeapon(int16_t worldId)
{
    if(combatItemScene == nullptr || worldId < 0 || worldId >= FSTAGE_WEAPON_CAPACITY)
        return -1;
    const FstageWeaponInstance& world = combatItemScene->stageWeapons.instances[worldId];
    if(!world.active || world.managerId < 0 || world.managerId >= FSTAGE_WEAPON_CAPACITY)
        return -1;
    rw::Clump* clump = combatItemScene->stageWeapons.records[world.managerId].clump;
    if(clump == nullptr || clump->getFrame() == nullptr)
        return -1;
    GameCombatWeaponBinding& binding = combatWeaponBindings[worldId];
    const WeaponInstance* instance = weaponManager.instance(binding.managerId);
    if(binding.clump == clump && binding.objectId == world.selector &&
       instance != nullptr && instance->lifecycle != WEAPON_LIFECYCLE_FREE)
        return binding.managerId;
    if(binding.managerId >= 0)
        weaponManager.release(binding.managerId);
    binding = {};
    clump->getFrame()->updateObjects();
    const rw::Matrix& matrix = *clump->getFrame()->getLTM();
    WeaponPose pose = {};
    pose.x = matrix.pos.x;
    pose.y = matrix.pos.y;
    pose.z = matrix.pos.z;
    pose.yaw = atan2f(matrix.at.x, matrix.at.z) * 57.29577951308232f;
    const int16_t id = weaponManager.spawn(world.selector, pose);
    if(id < 0)
        return -1;
    binding.managerId = id;
    binding.objectId = world.selector;
    binding.clump = clump;
    return id;
}

int Game::selectCombatWorldWeapon(GameSlotRecord& slot)
{
    if(combatItemScene == nullptr || (slot.actorActionFlags & 0x4000u) != 0)
        return -1;
    FstageWeaponActorState actor = combatWeaponActorState(slot);
    const int16_t candidate = queryFstageWeaponCandidate(combatItemScene, &actor, false);
    slot.targetedWorldWeapon = candidate;
    if(candidate < 0)
        return -1;
    const FstageWeaponInstance& world = combatItemScene->stageWeapons.instances[candidate];
    const WeaponDefinition* definition = weaponManager.definition(world.selector);
    const int action = definition != nullptr ? weaponPickupAction(definition->behaviorId) : 0;
    if(action <= 0)
        return -1;
    const int move = game_detail::actorMotionIdFromAction(slot, action);
    if(move <= 0)
        return -1;
    const int16_t managerId = ensureCombatWorldWeapon(candidate);
    if(managerId < 0 || !weaponManager.pickup(managerId, slot.slotIndex))
        return -1;

    if(queryFstageWeaponCandidate(combatItemScene, &actor, true) != candidate)
    {
        weaponManager.drop(managerId, weaponManager.instance(managerId)->pose);
        return -1;
    }
    slot.heldWorldWeapon = actor.heldWeapon;
    slot.targetedWorldWeapon = actor.targetedWeapon;
    slot.selectedWorldItemIndex = -1;
    return move;
}

void Game::refreshCombatWeaponAttachments()
{
    if(combatItemScene == nullptr)
        return;
    for(int index = 0; index < GAME_COMBAT_SLOT_COUNT; ++index)
    {
        GameSlotRecord* actor = mutableCombatSlotByIndex(static_cast<uint8_t>(index));
        if(actor != nullptr && actor->defaultWeaponAttachmentPending)
            prepareDefaultCombatWeapon(*actor);
    }
    for(int worldId = 0; worldId < FSTAGE_WEAPON_CAPACITY; ++worldId)
    {
        GameCombatWeaponBinding& binding = combatWeaponBindings[worldId];
        if(binding.managerId < 0)
            continue;
        FstageWeaponInstance& world = combatItemScene->stageWeapons.instances[worldId];
        if(!world.active || world.managerId < 0 || world.managerId >= FSTAGE_WEAPON_CAPACITY ||
           combatItemScene->stageWeapons.records[world.managerId].clump != binding.clump)
        {
            weaponManager.release(binding.managerId);
            binding = {};
            continue;
        }
        GameSlotRecord* slot = world.ownerSlot >= 0 ?
            mutableCombatSlotByIndex(static_cast<uint8_t>(world.ownerSlot)) : nullptr;
        if(slot == nullptr || (world.flags & FSTAGE_WEAPON_ATTACHED) == 0 ||
           (slot->actorActionFlags & 0x4000u) == 0)
            continue;
        FstageWeaponActorState actor = combatWeaponActorState(*slot);
        const WeaponDefinition* definition = weaponManager.definition(world.selector);
        if(definition != nullptr)
            switchFstageWeaponHand(combatItemScene, static_cast<int16_t>(worldId), &actor,
                                   world.attachmentSlot, *definition);
    }
}

bool Game::forceDropCombatWeapon(GameSlotRecord& slot, bool gated)
{
    if(combatItemScene == nullptr || slot.heldWorldWeapon < 0 ||
       slot.heldWorldWeapon >= FSTAGE_WEAPON_CAPACITY ||
       (slot.combatBoneMask & (1u << 21)) == 0)
        return false;
    const int16_t worldId = slot.heldWorldWeapon;
    const GameCombatWeaponBinding& binding = combatWeaponBindings[worldId];
    const WeaponInstance* held = weaponManager.instance(binding.managerId);
    const WeaponDefinition* definition = held != nullptr ?
        weaponManager.definition(held->objectId) : nullptr;
    if(definition == nullptr)
        return false;
    FstageWeaponActorState actor = combatWeaponActorState(slot);
    const Vec3& at = slot.combatBones[21].at;
    const rw::V3d forward = { at.x, at.y, at.z };
    const bool dropped = gated ?
        tryForceDropFstageWeapon(combatItemScene, worldId, &actor, *definition, forward) :
        forceDropFstageWeapon(combatItemScene, worldId, &actor, forward);
    if(!dropped)
        return false;
    slot.heldWorldWeapon = actor.heldWeapon;
    slot.boundWorldWeapon = actor.linkedHeldWeapon;
    slot.actorActionFlags = actor.actionFlags;
    dropCombatWeaponManager(worldId);
    return true;
}

bool Game::tryCombatDamageWeaponDrop(GameSlotRecord& slot)
{
    if(combatItemScene == nullptr || (slot.actorActionFlags & 0x4000u) == 0 ||
       slot.heldWorldWeapon < 0 || slot.heldWorldWeapon >= FSTAGE_WEAPON_CAPACITY)
        return false;
    const FstageWeaponInstance& world = combatItemScene->stageWeapons.instances[slot.heldWorldWeapon];
    const WeaponDefinition* definition = weaponManager.definition(world.selector);
    if(!world.active || definition == nullptr)
        return false;

    const uint32_t roll = nextRandom();
    const unsigned index = (definition->rawWords[31] >> 16) & 0xffu;
    const CharacterRecord* defaults = characterRecordForId(
        static_cast<uint16_t>(slot.resourceProfileId));
    if(index >= WEAPON_DAMAGE_DISARM_THRESHOLD_COUNT || defaults == nullptr)
        return false;
    int modifier = 0;
    if((persistent.story.gameStateFlags & 2u) == 0 && slot.nativeCharacterId == 0)
        modifier = static_cast<int8_t>(primarySaveImage.payload.coreGameStateBlock[
            GAME_SAVE_CORE_PRIMARY_COMBAT_MODIFIER_OFFSET]);
    const int threshold = WEAPON_DAMAGE_DISARM_THRESHOLDS[index] +
        defaults->weaponDropModifier - modifier * 10;

    return static_cast<int>(roll % 100u) < threshold &&
        forceDropCombatWeapon(slot, false);
}

void Game::dropCombatWeaponManager(int16_t worldId)
{
    if(worldId < 0 || worldId >= WEAPON_MANAGER_CAPACITY)
        return;
    const GameCombatWeaponBinding& binding = combatWeaponBindings[worldId];
    const WeaponInstance* held = weaponManager.instance(binding.managerId);
    if(held == nullptr || binding.clump == nullptr || binding.clump->getFrame() == nullptr)
        return;
    binding.clump->getFrame()->updateObjects();
    const rw::V3d& position = binding.clump->getFrame()->getLTM()->pos;
    WeaponPose pose = held->pose;
    pose.x = position.x;
    pose.y = position.y;
    pose.z = position.z;
    weaponManager.drop(binding.managerId, pose);
}

void Game::behaviourWeaponCallback(void* userData, int opcode, int16_t value)
{
    BehaviourCombatContext* context = static_cast<BehaviourCombatContext*>(userData);
    if(context != nullptr && context->owner != nullptr && context->actor != nullptr)
        context->owner->updateCombatWeaponBehaviour(*context->actor, opcode, value);
}

void Game::updateCombatWeaponBehaviour(GameSlotRecord& slot, int opcode, int16_t value)
{
    if(opcode == MOVE_BEHAVIOUR_WEAPON_ACTION && value == 3)
    {
        if((slot.actorActionFlags & 0x4000u) != 0)
        {
            forceDropCombatWeapon(slot, false);
            return;
        }
        GameSlotRecord* linked = mutableCombatSlotByIndex(slot.linkedSlotIndex);
        if(linked == nullptr || (linked->actorActionFlags & 0x4000u) == 0 ||
           linked->heldWorldWeapon < 0 || linked->heldWorldWeapon >= FSTAGE_WEAPON_CAPACITY ||
           combatItemScene == nullptr)
            return;
        const int16_t transferred = linked->heldWorldWeapon;
        const int16_t managerId = ensureCombatWorldWeapon(transferred);
        const WeaponInstance* held = weaponManager.instance(managerId);
        const WeaponDefinition* definition = held != nullptr ?
            weaponManager.definition(held->objectId) : nullptr;
        FstageWeaponActorState previous = combatWeaponActorState(*linked);

        if(definition == nullptr || !tossFstageWeapon(combatItemScene, transferred,
                &previous, static_cast<uint8_t>(definition->behaviorId)))
            return;
        linked->heldWorldWeapon = previous.heldWeapon;
        linked->boundWorldWeapon = previous.linkedHeldWeapon;
        linked->actorActionFlags = previous.actionFlags;
        dropCombatWeaponManager(transferred);
        slot.heldWorldWeapon = transferred;
        if(!weaponManager.pickup(managerId, slot.slotIndex))
            return;
        updateCombatWeaponBehaviour(slot, MOVE_BEHAVIOUR_WEAPON_ACTION, 1);
        return;
    }
    if(combatItemScene == nullptr || slot.heldWorldWeapon < 0 ||
       slot.heldWorldWeapon >= FSTAGE_WEAPON_CAPACITY)
        return;
    const int16_t worldId = slot.heldWorldWeapon;
    const int16_t managerId = ensureCombatWorldWeapon(worldId);
    const WeaponInstance* held = weaponManager.instance(managerId);
    const WeaponDefinition* definition = held != nullptr ?
        weaponManager.definition(held->objectId) : nullptr;
    if(definition == nullptr)
        return;
    FstageWeaponActorState actor = combatWeaponActorState(slot);
    bool attached = false;
    if(opcode == MOVE_BEHAVIOUR_WEAPON_ACTION)
    {
        if(value == 1)
            attached = attachFstageWeapon(combatItemScene, worldId, &actor, 0, *definition);
        else if(value == 5)
        {
            forceDropCombatWeapon(slot, false);
            return;
        }
        else if(value == 2)
        {
            const GameSlotRecord* target = combatSlotByIndex(slot.linkedSlotIndex);
            if(target != nullptr)
            {
                const rw::V3d position = { target->position.x, target->position.y,
                                          target->position.z };
                if(throwFstageWeapon(combatItemScene, worldId, &actor, position))
                    slot.boundWorldWeapon = actor.linkedHeldWeapon;
            }
        }
        else if(value == 4)
        {
            returnCombatOwnedWeapon(slot);
            return;
        }
        else
        {

            if(tossFstageWeapon(combatItemScene, worldId, &actor,
                                 static_cast<uint8_t>(definition->behaviorId)))
            {
                slot.heldWorldWeapon = actor.heldWeapon;
                slot.boundWorldWeapon = actor.linkedHeldWeapon;
                slot.actorActionFlags = actor.actionFlags;
                dropCombatWeaponManager(worldId);
            }
        }
    }
    else if(opcode == MOVE_BEHAVIOUR_WEAPON_VISIBLE)
        setFstageWeaponVisible(combatItemScene, worldId, value != 0);
    else if(opcode == MOVE_BEHAVIOUR_WEAPON_HAND)
        attached = switchFstageWeaponHand(combatItemScene, worldId, &actor, value, *definition);
    else if(opcode == MOVE_BEHAVIOUR_WEAPON_SNAPSHOT)
        snapshotFstageWeaponPose(combatItemScene, worldId, &actor);
    else if(opcode == MOVE_BEHAVIOUR_WEAPON_ALIGN &&
            (slot.actorActionFlags & 0x4000u) == 0 && (slot.combatBoneMask & 1u) != 0)
    {

        rw::Frame* frame = combatWeaponBindings[worldId].clump->getFrame();
        const FstageWeaponInstance& world = combatItemScene->stageWeapons.instances[worldId];
        rw::Matrix flat;
        const rw::Matrix& old = frame->matrix;
        const rw::V3d axis = { 0, 1, 0 };
        const float yaw = atan2f(old.at.x, old.at.z) * 57.29577951308232f;
        flat.rotate(&axis, yaw, rw::COMBINEREPLACE);
        flat.pos = { old.pos.x, world.groundY, old.pos.z };
        frame->transform(&flat, rw::COMBINEREPLACE);
        const CombatBoneTransform& root = slot.combatBones[0];
        const float x = game_detail::floatFromBits(definition->rawWords[25]);
        const float z = game_detail::floatFromBits(definition->rawWords[27]);
        const float rotation = game_detail::floatFromBits(definition->rawWords[28]);
        const rw::V3d translation = {
            root.position.x + x * root.right.x + z * root.at.x - flat.pos.x, 0,
            root.position.z + x * root.right.z + z * root.at.z - flat.pos.z };
        float delta = atan2f(root.at.x, root.at.z) * 57.29577951308232f + rotation - yaw;
        if(delta > 180) delta -= 360;
        else if(delta < -180) delta += 360;
        frame->translate(&translation, rw::COMBINEPOSTCONCAT);
        frame->rotate(&axis, delta, rw::COMBINEPRECONCAT);
    }
    if(attached)
    {
        slot.heldWorldWeapon = actor.heldWeapon;
        slot.actorActionFlags = actor.actionFlags;
        slot.defaultWeaponAttachmentPending = false;
        const FstageWeaponInstance& world = combatItemScene->stageWeapons.instances[worldId];
        if(world.originalMoveSlot >= 0)
            for(int index = 0; index < 2; ++index)
                if(slot.ownedWorldWeapons[index] == worldId)
                    slot.moveSlotActive[index] = 0;
        const int hand = combatItemScene->stageWeapons.instances[worldId].attachmentSlot;
        WeaponPose pose = {};
        pose.x = game_detail::floatFromBits(definition->rawWords[2 + hand * 3]);
        pose.y = game_detail::floatFromBits(definition->rawWords[3 + hand * 3]);
        pose.z = game_detail::floatFromBits(definition->rawWords[4 + hand * 3]);
        pose.pitch = game_detail::floatFromBits(definition->rawWords[8 + hand * 3]);
        pose.yaw = game_detail::floatFromBits(definition->rawWords[9 + hand * 3]);
        pose.roll = game_detail::floatFromBits(definition->rawWords[10 + hand * 3]);
        weaponManager.attach(managerId, slot.slotIndex, hand == 0 ? 14 : 19, pose);
    }
}
