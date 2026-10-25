#include "game_combat_gauge.h"

void initializeCombatGaugeRuntime(GameSlotRecord& slot)
{
    slot.tiredGauge = 0;
    slot.tiredGaugeWear = 0;
    slot.tiredGaugeRecoverySeconds = 0.0f;
    slot.pendingGaugeModifier = 0;
    for(float& modifier : slot.combatGaugeModifiers)
        modifier = 1.0f;
    for(float& seconds : slot.attackGaugeRecoverySeconds)
        seconds = COMBAT_GAUGE_RECOVERY_INACTIVE;
    for(float& seconds : slot.bodyGaugeRecoverySeconds)
        seconds = COMBAT_GAUGE_RECOVERY_INACTIVE;
}

void recordCombatTiredCost(GameSlotRecord& slot, int cost)
{

    slot.tiredGauge = static_cast<int16_t>(slot.tiredGauge + cost);
    slot.tiredGaugeWear = static_cast<int16_t>(slot.tiredGaugeWear + cost);
    const int total = slot.tiredGaugeWear;
    const int maximum = slot.tiredGaugeMaximum;
    if(maximum != 0 && total / maximum != (total - cost) / maximum)
    {
        float& modifier = slot.combatGaugeModifiers[11];
        modifier -= COMBAT_GAUGE_MODIFIER_STEP;
        if(modifier < COMBAT_GAUGE_MODIFIER_MINIMUM)
            modifier = COMBAT_GAUGE_MODIFIER_MINIMUM;
    }
}

void recordCombatAttackGaugeDrain(GameSlotRecord& slot, int gauge, int drain)
{

    const int total = slot.attackGaugeDamage[gauge];
    if(total / COMBAT_ATTACK_GAUGE_WEAR_INTERVAL !=
       (total - drain) / COMBAT_ATTACK_GAUGE_WEAR_INTERVAL)
    {
        float& modifier = slot.combatGaugeModifiers[gauge + 1];
        modifier -= COMBAT_GAUGE_MODIFIER_STEP;
        if(modifier < COMBAT_GAUGE_MODIFIER_MINIMUM)
            modifier = COMBAT_GAUGE_MODIFIER_MINIMUM;
    }
    if(slot.attackGaugeRecoverySeconds[gauge] >= 100.0f)
        slot.attackGaugeRecoverySeconds[gauge] = 0.0f;
}

void recordCombatBodyGaugeDamage(GameSlotRecord& slot, int region, int damage)
{

    const int total = slot.bodyPartDamage[region];
    if(total / COMBAT_BODY_GAUGE_WEAR_INTERVAL !=
       (total - damage) / COMBAT_BODY_GAUGE_WEAR_INTERVAL)
    {
        float& modifier = slot.combatGaugeModifiers[region + 7];
        modifier -= COMBAT_GAUGE_MODIFIER_STEP;
        if(modifier < COMBAT_GAUGE_MODIFIER_MINIMUM)
            modifier = COMBAT_GAUGE_MODIFIER_MINIMUM;
    }
    slot.bodyGaugeRecoverySeconds[region] = 0.0f;
}

void updateCombatGaugeRecovery(GameSlotRecord& slot, float scaledSeconds)
{

    if(scaledSeconds <= 0.0f || (slot.flags & 1u) == 0)
        return;
    slot.actionElapsedSeconds += scaledSeconds;
    if(slot.actionElapsedSeconds > 100.0f)
        slot.actionElapsedSeconds = 100.0f;
    for(int i = 0; i < COMBAT_ATTACK_GAUGE_COUNT; ++i)
    {
        float& seconds = slot.attackGaugeRecoverySeconds[i];
        if(seconds >= 100.0f)
            continue;
        seconds += scaledSeconds;
        if(seconds > COMBAT_ATTACK_GAUGE_RECOVERY_SECONDS)
        {
            slot.attackGauges[i] = static_cast<int16_t>(
                slot.attackGaugeMaximums[i] * slot.combatGaugeModifiers[i + 1]);
            seconds = COMBAT_GAUGE_RECOVERY_INACTIVE;
        }
    }
    for(int i = 0; i < 4; ++i)
    {
        float& seconds = slot.bodyGaugeRecoverySeconds[i];
        if(seconds >= 100.0f)
            continue;
        seconds += scaledSeconds;
        if(seconds > COMBAT_BODY_GAUGE_RECOVERY_SECONDS)
        {
            slot.bodyPartHealth[i] = static_cast<int16_t>(
                slot.auxiliaryGaugeMaximums[i] * slot.combatGaugeModifiers[i + 7]);
            seconds = COMBAT_GAUGE_RECOVERY_INACTIVE;
        }
    }
    slot.tiredGaugeRecoverySeconds += scaledSeconds;
    if(slot.tiredGaugeRecoverySeconds > COMBAT_TIRED_GAUGE_RECOVERY_SECONDS)
    {
        const int16_t minimum = slot.tiredGaugeMaximum - static_cast<int16_t>(
            slot.tiredGaugeMaximum * slot.combatGaugeModifiers[11]);
        if(slot.tiredGauge > minimum)
            --slot.tiredGauge;
        else
            slot.tiredGauge = minimum;
        slot.tiredGaugeRecoverySeconds = 0.0f;
    }
}

void applyCombatGaugeModifier(GameSlotRecord& slot)
{

    const int index = slot.pendingGaugeModifier;
    slot.pendingGaugeModifier = 0;
    if(index < 0 || index >= 12)
        return;
    float& modifier = slot.combatGaugeModifiers[index];
    modifier -= COMBAT_GAUGE_MODIFIER_STEP;
    if(modifier < COMBAT_GAUGE_MODIFIER_MINIMUM)
        modifier = COMBAT_GAUGE_MODIFIER_MINIMUM;
    if(index >= 1 && index <= 6)
        slot.attackGauges[index - 1] = static_cast<int16_t>(
            slot.attackGaugeMaximums[index - 1] * modifier);
    else if(index >= 7 && index <= 10)
        slot.bodyPartHealth[index - 7] = static_cast<int16_t>(
            slot.auxiliaryGaugeMaximums[index - 7] * modifier);
    else if(index == 11)
        slot.tiredGauge = slot.tiredGaugeMaximum - static_cast<int16_t>(
            slot.tiredGaugeMaximum * modifier);
}
