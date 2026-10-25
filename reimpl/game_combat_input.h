#ifndef GAME_COMBAT_INPUT_H
#define GAME_COMBAT_INPUT_H

#include "game.h"

inline constexpr int COMBAT_MOVE_DELAYED_POSE_HANDLER = 16;

int combatInputTrigger(uint32_t current, uint32_t previous);
bool combatInputScanAllowed(const GameSlotRecord& actor);

int combatWeaponChainSelector(int behavior, int category, int objectId);

void publishCombatFacing(GameSlotRecord& actor, const Vec3& target,
                          const Vec3& camera);
bool combatTargetInteractable(const GameSlotRecord& actor);

bool combatGroundedState(const GameSlotRecord& actor, bool& grounded);
uint32_t combatDirectionCode(uint32_t gameButtons);

uint32_t combatButtonCode(uint32_t gameButtons);
uint32_t combatMovementRelation(const GameSlotRecord& actor,
                                const GameSlotRecord& target,
                                float worldX, float worldZ);
bool readMovementInput(float rawX, float rawZ, uint32_t buttons,
                       float& x, float& z, float& magnitude);

#endif
