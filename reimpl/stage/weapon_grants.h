#ifndef STAGE_WEAPON_GRANTS_H
#define STAGE_WEAPON_GRANTS_H

#include "stage/stage.h"

const uint8_t FSTAGE_WEAPON_GRANT_ACTOR_COUNT = 13;
const uint8_t FSTAGE_WEAPON_GRANT_MOVE_SLOT_COUNT = 2;

int16_t grantFstageWeapon(FstageScene* scene,
                          const WeaponDefinition& definition,
                          uint8_t ownerSlot, uint8_t originalMoveSlot);

bool setFstageWeaponVisible(FstageScene* scene, int16_t weaponId, bool visible);

bool resetFstageWeaponAttachment(FstageScene* scene, int16_t weaponId,
                                 FstageWeaponActorState* actor,
                                 const WeaponDefinition& definition,
                                 const rw::V3d& bone21Forward);

bool returnOwnedFstageWeapon(FstageScene* scene, int16_t weaponId,
                             FstageWeaponActorState* actor,
                             uint8_t* returnedMoveSlot = NULL);

#endif
