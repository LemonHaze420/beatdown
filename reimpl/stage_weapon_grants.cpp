#include "stage_weapon_grants.h"
#include "stage_weapon_internal.h"
#include "game_bits.h"
#include "weapon.h"

int16_t grantFstageWeapon(FstageScene* scene,
                          const WeaponDefinition& definition,
                          uint8_t ownerSlot, uint8_t originalMoveSlot)
{
    if(scene == NULL || ownerSlot >= FSTAGE_WEAPON_GRANT_ACTOR_COUNT ||
       originalMoveSlot >= FSTAGE_WEAPON_GRANT_MOVE_SLOT_COUNT ||
       definition.objectId >= 0x30 || definition.callbackAddress == 0)
        return FSTAGE_WEAPON_INVALID_ID;
    rw::Matrix identity;
    identity.setIdentity();
    const int16_t id = cloneFstageWeaponInstance(
        scene, definition.objectId, identity);
    if(id < 0)
        return id;
    FstageWeaponInstance& weapon = scene->stageWeapons.instances[id];
    weapon.originalOwnerSlot = static_cast<int8_t>(ownerSlot);
    weapon.originalMoveSlot = static_cast<int8_t>(originalMoveSlot);
    weapon.groundY = game_detail::floatFromBits(definition.rawWords[21]);
    weapon.flags = ownerSlot == 0 ? 0x20 : (ownerSlot == 1 ? 0x40 : 0);
    FstageWeaponManagerRecord& manager =
        scene->stageWeapons.records[weapon.managerId];
    manager.flags |= FSTAGE_WEAPON_MANAGER_OWNED;
    setFstageWeaponVisible(scene, id, false);
    return id;
}

bool resetFstageWeaponAttachment(FstageScene* scene, int16_t weaponId,
                                 FstageWeaponActorState* actor,
                                 const WeaponDefinition& definition,
                                 const rw::V3d& bone21Forward)
{
    rw::Frame* frame = findFstageWeaponFrame(scene, weaponId);
    if(frame == NULL || definition.objectId !=
            scene->stageWeapons.instances[weaponId].selector ||
       !forceDropFstageWeapon(scene, weaponId, actor, bone21Forward))
        return false;
    FstageWeaponInstance& weapon = scene->stageWeapons.instances[weaponId];
    rw::Matrix reset;
    reset.setIdentity();
    reset.pos = frame->matrix.pos;
    reset.pos.y = game_detail::floatFromBits(definition.rawWords[21]);
    frame->transform(&reset, rw::COMBINEREPLACE);
    weapon.flags &= ~FSTAGE_WEAPON_POST_BREAK;

    weapon.motionState = FSTAGE_WEAPON_MOTION_IDLE;
    setFstageWeaponVisible(scene, weaponId, false);
    return true;
}

bool returnOwnedFstageWeapon(FstageScene* scene, int16_t weaponId,
                             FstageWeaponActorState* actor,
                             uint8_t* returnedMoveSlot)
{
    if(scene == NULL || actor == NULL || weaponId < 0 ||
       weaponId >= FSTAGE_WEAPON_CAPACITY || actor->heldWeapon != weaponId ||
       (actor->actionFlags & 0x4000u) == 0)
        return false;
    FstageWeaponInstance& weapon = scene->stageWeapons.instances[weaponId];
    rw::Frame* frame = findFstageWeaponFrame(scene, weaponId);
    if(frame == NULL || weapon.originalOwnerSlot != actor->slotIndex ||
       weapon.originalMoveSlot < 0 ||
       weapon.originalMoveSlot >= FSTAGE_WEAPON_GRANT_MOVE_SLOT_COUNT)
        return false;
    setFstageWeaponVisible(scene, weaponId, false);
    const rw::Matrix current = detachFstageWeaponFrame(frame);
    weapon.velocity.x = (current.pos.x - weapon.previousWorld.pos.x) * 0.25f;
    weapon.velocity.y = 0.5f;
    weapon.velocity.z = (current.pos.z - weapon.previousWorld.pos.z) * 0.25f;
    fstageWeaponAngularDelta(current, weapon.previousWorld, weapon.angularVelocity);
    weapon.flags = (weapon.flags & ~FSTAGE_WEAPON_ATTACHED) | FSTAGE_WEAPON_RETURNED;
    weapon.ownerSlot = -1;
    weapon.attachmentSlot = -1;
    actor->heldWeapon = FSTAGE_WEAPON_INVALID_ID;
    actor->actionFlags &= ~0x4000u;
    if(returnedMoveSlot != NULL)
        *returnedMoveSlot = static_cast<uint8_t>(weapon.originalMoveSlot);
    return true;
}
