#ifndef GAME_COMBAT_GAUGE_H
#define GAME_COMBAT_GAUGE_H

#include "game/game.h"

inline constexpr float COMBAT_GAUGE_RECOVERY_INACTIVE = 1000.0f;
inline constexpr float COMBAT_ATTACK_GAUGE_RECOVERY_SECONDS = 10.0f;
inline constexpr float COMBAT_BODY_GAUGE_RECOVERY_SECONDS = 3.0f;
inline constexpr float COMBAT_TIRED_GAUGE_RECOVERY_SECONDS = 0.5f;
inline constexpr float COMBAT_GAUGE_MODIFIER_MINIMUM = 0.5f;
inline constexpr float COMBAT_GAUGE_MODIFIER_STEP = 0.1f;
inline constexpr int COMBAT_ATTACK_GAUGE_WEAR_INTERVAL = 30;
inline constexpr int COMBAT_BODY_GAUGE_WEAR_INTERVAL = 50;

void initializeCombatGaugeRuntime(GameSlotRecord& slot);
void updateCombatGaugeRecovery(GameSlotRecord& slot, float scaledSeconds);
void applyCombatGaugeModifier(GameSlotRecord& slot);
void recordCombatTiredCost(GameSlotRecord& slot, int cost);
void recordCombatAttackGaugeDrain(GameSlotRecord& slot, int gauge, int drain);
void recordCombatBodyGaugeDamage(GameSlotRecord& slot, int region, int damage);

#endif
