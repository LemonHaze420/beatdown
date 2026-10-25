#include "game.h"
#include "menu_inventory.h"
#include "scene_task.h"
#include "character_records.h"
#include "combat_styles.h"
#include "versus_tables.h"
#include "xaf_runtime.h"
#include "move_tables.h"
#include "action_state_handlers.h"
#include "options_menu.h"
#include "camera.h"
#include "sound_cues.h"
#include "stage.h"
#include "hud_advice.h"
#include "../formats/MENU_LAYOUT/pause_results_layout.h"
#include "../formats/BATTLE_ROSTER/battle_rosters.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "game_internal.h"

using namespace game_detail;

int16_t
Game::seededSlotPride(const GameSlotRecord& slot) const
{
    if(slot.characterId >= 0)
    {
        const PersistentGameRecord* record =
            characterRecord(static_cast<uint16_t>(slot.characterId));
        if(record != NULL)
            return record->prideGauge;
    }
    const CharacterStatRecord* stats = gameData.characterStats(
        characterDatabaseIndex(static_cast<CharacterId>(slot.characterId)));
    return stats == NULL ? -1 : stats->initialPrideGauge;
}

int16_t
Game::initialiseSlotPride(GameSlotRecord& slot, CharacterId characterId)
{
    const CharacterStatRecord* stats =
        gameData.characterStats(characterDatabaseIndex(characterId));
    if(stats == NULL)
        return -1;
    const PersistentGameRecord* record =
        characterRecord(static_cast<uint16_t>(characterId));
    slot.prideGauge =
        record == NULL ? stats->initialPrideGauge : record->prideGauge;

    slot.attackPowerMultiplier = moveAttackPowerForClass(
        record == NULL ? stats->attackPowerClass : record->attackPowerClass);
    if(slot.slotIndex != 0)
    {
        const int difficulty = persistent.options.difficulty;
        if(difficulty >= 0 && difficulty < MOVE_DIFFICULTY_POWER_COUNT)
            slot.attackPowerMultiplier *= MOVE_DIFFICULTY_POWER[difficulty];
    }
    return slot.prideGauge;
}

bool
Game::battleExitEligible(void) const
{
    return currentScreen == GAME_SCREEN_GAMEPLAY &&
           currentBattleOutcome == BATTLE_OUTCOME_IN_PROGRESS;
}

void
Game::captureBattleRewardRoster(void)
{
    battleSupplementalExperience = 0;
    resetBattleScoreMultipliers();
    for(int i = 0; i < BATTLE_REWARD_ROSTER_SIZE; ++i)
        battleRewardOpponentIds[i] = -1;
    memset(battleRewardOpponentActive, 0,
           sizeof(battleRewardOpponentActive));

    const int rewardCount =
        activeOpponentCount < BATTLE_REWARD_ROSTER_SIZE ?
        activeOpponentCount : BATTLE_REWARD_ROSTER_SIZE;
    for(int i = 0; i < rewardCount; ++i)
    {
        const GameSlotRecord& slot =
            i == 0 ? enemy : additionalEnemies[i - 1];
        battleRewardOpponentIds[i] = slot.characterId;
    }
    const int activeCount = activeOpponentCount < MAX_GAME_OPPONENTS ?
        activeOpponentCount : MAX_GAME_OPPONENTS;
    for(int i = 0; i < activeCount; ++i)
        battleRewardOpponentActive[i] = true;
}

int
Game::battleRewardCharacterLevel(int16_t characterId) const
{
    if(characterId < 0)
        return 0;
    const PersistentGameRecord* persistentRecord =
        characterRecord(static_cast<uint16_t>(characterId));
    if(persistentRecord != NULL)
        return persistentRecord->level;
    const CharacterRecord* defaults =
        characterRecordForId(static_cast<uint16_t>(characterId));
    return defaults == NULL ? 0 : defaults->level;
}

void
Game::scoreDefeatedBattleOpponents(void)
{
    if((persistent.story.battleFlags &
        BATTLE_REWARD_INTERACTION_FLAG) == 0)
        return;
    const int count = activeOpponentCount < MAX_GAME_OPPONENTS ?
        activeOpponentCount : MAX_GAME_OPPONENTS;
    for(int i = 0; i < count; ++i)
    {
        if(!battleRewardOpponentActive[i])
            continue;
        const GameSlotRecord& slot =
            i == 0 ? enemy : additionalEnemies[i - 1];
        if(slot.currentHealth > 0 ||
           slot.action != COMBAT_ACTION_DEFEATED)
            continue;
        battleRewardOpponentActive[i] = false;
        scoreBattleInteractionKnockout(
            battleRewardCharacterLevel(slot.characterId));
    }
}

int
Game::battleExperienceForOpponent(int opponentLevel, int playerLevel,
                                  int comboBonus)
{
    const int gap = opponentLevel - playerLevel + 1;
    int reward = gap * gap + 30;
    if(opponentLevel < playerLevel)
        reward = 10;
    if(reward < 0)
        reward = 0;
    if(comboBonus < 0)
        comboBonus = 0;
    if(comboBonus > BATTLE_COMBO_BONUS_CAP)
        comboBonus = BATTLE_COMBO_BONUS_CAP;
    return reward + comboBonus / 10;
}

int
Game::battleInteractionExperienceForOpponent(
    int opponentLevel, int playerLevel, int comboBonus)
{
    const int raw = battleExperienceForOpponent(
        opponentLevel, playerLevel, comboBonus);
    return static_cast<int>(static_cast<float>(raw) * 0.1f);
}

int
Game::scoreBattleInteractionKnockout(int opponentLevel)
{
    const PersistentGameRecord* const playerRecord = characterRecord(0);
    const int playerLevel = playerRecord == NULL ? 0 : playerRecord->level;
    const int award = battleInteractionExperienceForOpponent(
        opponentLevel, playerLevel, battleComboBonus);
    int accumulated =
        static_cast<int>(battleSupplementalExperience) +
        static_cast<int>(static_cast<int16_t>(award));
    if(accumulated > BATTLE_EXPERIENCE_CAP)
        accumulated = BATTLE_EXPERIENCE_CAP;
    battleSupplementalExperience =
        static_cast<int16_t>(accumulated);
    return battleSupplementalExperience;
}

int
Game::battleExperienceReward(const int* opponentLevels,
                             int8_t* scoreMultipliers,
                             int opponentCount, int playerLevel,
                             int comboBonus, int supplementalReward,
                             bool interactionScore)
{
    int total = interactionScore ? supplementalReward : 0;
    if(!interactionScore)
    {
        if(opponentCount < 0 || opponentLevels == NULL)
            opponentCount = 0;
        if(opponentCount > BATTLE_REWARD_ROSTER_SIZE)
            opponentCount = BATTLE_REWARD_ROSTER_SIZE;
        if(comboBonus < 0)
            comboBonus = 0;
        if(comboBonus > BATTLE_COMBO_BONUS_CAP)
            comboBonus = BATTLE_COMBO_BONUS_CAP;
        const int comboTerm = comboBonus / 10;
        for(int i = 0; i < opponentCount; ++i)
        {
            int multiplierTenths = -1;
            if(scoreMultipliers != NULL)
            {
                multiplierTenths = scoreMultipliers[i];
                if(multiplierTenths != -1)
                    scoreMultipliers[i] = -1;
            }
            const float multiplier = multiplierTenths == -1 ?
                1.0f : static_cast<float>(multiplierTenths) / 10.0f;
            const int gap = opponentLevels[i] - playerLevel + 1;
            int base = gap * gap + 30;
            if(opponentLevels[i] < playerLevel)
                base = 10;
            if(base < 0)
                base = 0;
            total += static_cast<int>(
                static_cast<float>(base + comboTerm) * multiplier);
        }
        total += supplementalReward;
    }
    if(total > BATTLE_EXPERIENCE_CAP)
        total = BATTLE_EXPERIENCE_CAP;
    return total;
}

bool
Game::queueBattleScoreMultiplier(float multiplier)
{
    const int8_t value = static_cast<int8_t>(
        static_cast<int>(multiplier * 10.0f));
    for(int i = 0; i < BATTLE_REWARD_ROSTER_SIZE; ++i)
    {
        if(battleScoreMultipliers[i] != -1)
            continue;
        battleScoreMultipliers[i] = value;
        return true;
    }
    return false;
}

void
Game::resetBattleScoreMultipliers(void)
{
    for(int i = 0; i < BATTLE_REWARD_ROSTER_SIZE; ++i)
        battleScoreMultipliers[i] = -1;
}

int
Game::battleScoreMultiplier(int index) const
{
    return index >= 0 && index < BATTLE_REWARD_ROSTER_SIZE ?
        battleScoreMultipliers[index] : -1;
}

int
Game::battleRewardOpponentId(int index) const
{
    return index >= 0 && index < BATTLE_REWARD_ROSTER_SIZE ?
        battleRewardOpponentIds[index] : -1;
}

int
Game::battleSupplementalExperienceAwarded(void) const
{
    return battleSupplementalExperience;
}

int
Game::playerMaxHealthForProgression(int defaultMaximum,
                                    int growth,
                                    int level,
                                    int maximumBonus)
{

    return defaultMaximum +
           growth * (20 * growth + 80 * growth + 200) +
           30 * (level - 1) + maximumBonus;
}

int
Game::companionMaxHealthForProgression(int defaultMaximum,
                                       int defaultLevel,
                                       int level,
                                       int maximumBonus)
{

    return defaultMaximum + 80 * (level - defaultLevel) + maximumBonus;
}

int
Game::recalculatePlayerMaxHealth(void)
{
    PersistentGameRecord* const record = characterRecord(0);

    const CharacterRecord* const defaults = characterRecordForId(
        static_cast<uint16_t>(persistent.player.resourceProfileId));
    if(record == NULL || defaults == NULL)
        return persistent.player.maxHealth;

    const int growth = static_cast<int8_t>(record->field_0x050);
    const int maximum = playerMaxHealthForProgression(
        defaults->maxHealth, growth, record->level,
        record->characterValues[2]);
    const int delta = maximum - persistent.player.maxHealth;
    persistent.player.maxHealth = static_cast<int16_t>(maximum);
    persistent.player.currentHealth = static_cast<int16_t>(
        persistent.player.currentHealth + delta);

    int storedMaximum = maximum;
    int storedCurrent = record->currentHealth + delta;
    if(storedMaximum > 9999)
        storedMaximum = 9999;
    if(storedCurrent > 9999)
        storedCurrent = 9999;
    record->maxHealth = static_cast<int16_t>(storedMaximum);
    record->currentHealth = static_cast<int16_t>(storedCurrent);
    return storedMaximum;
}

int
Game::recalculatePartyMaxHealth(uint16_t characterId)
{
    PersistentGameRecord* const record = characterRecord(characterId);
    const CharacterRecord* const defaults =
        characterRecordForId(characterId);
    if(record == NULL || defaults == NULL)
        return 0;

    GameSlotRecord* runtime = NULL;
    if(persistent.player.characterId ==
       static_cast<int16_t>(characterId))
        runtime = &persistent.player;
    else if(allyActive &&
            ally.characterId == static_cast<int16_t>(characterId))
        runtime = &ally;
    else
    {
        for(int i = 0; i < activeOpponentCount; ++i)
        {
            GameSlotRecord& slot =
                i == 0 ? enemy : additionalEnemies[i - 1];
            if(slot.characterId == static_cast<int16_t>(characterId))
            {
                runtime = &slot;
                break;
            }
        }
    }

    const int maximum = companionMaxHealthForProgression(
        defaults->maxHealth, defaults->level, record->level,
        record->characterValues[2]);
    const int oldRuntimeMaximum =
        runtime == NULL ? record->maxHealth : runtime->maxHealth;
    const int delta = maximum - oldRuntimeMaximum;
    if(runtime != NULL)
    {
        runtime->maxHealth = static_cast<int16_t>(maximum);
        runtime->currentHealth = static_cast<int16_t>(
            runtime->currentHealth + delta);
    }

    int storedMaximum = maximum;
    int storedCurrent = record->currentHealth + delta;
    if(storedMaximum > 9999)
        storedMaximum = 9999;
    if(storedCurrent > 9999)
        storedCurrent = 9999;
    record->maxHealth = static_cast<int16_t>(storedMaximum);
    record->currentHealth = static_cast<int16_t>(storedCurrent);
    return storedMaximum;
}

int
Game::applyRobberyCashAward(uint16_t victimCharacterId,
                            uint32_t randomValue)
{
    const int randomAddition = static_cast<int>(
        (static_cast<uint64_t>(randomValue & 0xffffu) * 10u) /
        0xffffu) + 1;
    PersistentGameRecord* const victim =
        characterRecord(victimCharacterId);
    const int victimCash = victim == NULL ? 0 : victim->cash;
    const int amount = victimCash < 1 ?
        randomAddition : victimCash + randomAddition;
    if(victim != NULL && victimCash >= 1)
        victim->cash = 0;

    queueBattleScoreMultiplier(1.0f);
    robberyAwardPresentationAmount = amount;
    robberyAwardPresentationPending = true;
    robberyAwardLargeSideEffectPending = amount > 9;

    PersistentGameRecord* const player = characterRecord(0);
    if(player == NULL)
        return amount;
    int64_t cash = static_cast<int64_t>(player->cash) + amount;
    if(cash > 99999999)
        cash = 99999999;
    player->cash = static_cast<int32_t>(cash);
    persistent.story.money = player->cash;
    persistent.story.characters[0].cash = player->cash;
    return amount;
}

bool
Game::robberyPresentationPending(void) const
{
    return robberyAwardPresentationPending;
}

int
Game::robberyPresentationAmount(void) const
{
    return robberyAwardPresentationAmount;
}

bool
Game::robberyLargeAmountSideEffectPending(void) const
{
    return robberyAwardLargeSideEffectPending;
}

void
Game::clearRobberyPresentation(void)
{
    robberyAwardPresentationPending = false;
    robberyAwardLargeSideEffectPending = false;
    robberyAwardPresentationAmount = 0;
}

int
Game::partyLevel(uint16_t characterId) const
{
    const PersistentGameRecord* record = characterRecord(characterId);
    return record == NULL ? 0 : record->level;
}

int
Game::partyExperience(uint16_t characterId) const
{
    const PersistentGameRecord* record = characterRecord(characterId);
    return record == NULL ? 0 : record->experience;
}

int
Game::applyPartyBattleExperience(uint16_t characterId, int reward)
{
    if(characterId == 0xffffu)
        return 0;
    PersistentGameRecord* const record = characterRecord(characterId);
    if(record == NULL || record->level >= BATTLE_MAX_LEVEL)
        return 0;

    const int sum = record->experience + reward;
    if(sum < BATTLE_EXPERIENCE_CAP)
    {
        record->experience = static_cast<int32_t>(sum);
        return 0;
    }

    ++record->level;
    record->experience =
        static_cast<int32_t>(sum - BATTLE_EXPERIENCE_CAP);
    recalculatePartyMaxHealth(characterId);
    return 1;
}

int
Game::applyBattleExperienceTo(PersistentGameRecord* record, int reward)
{
    if(record == NULL)
        return 0;

    int sum = record->experience + reward;
    if(persistent.player.characterId != 0)
    {
        if(sum >= BATTLE_EXPERIENCE_CAP)
            sum = BATTLE_EXPERIENCE_CAP - 1;
        record->experience = static_cast<int32_t>(sum);
        return 0;
    }
    if(record->level >= BATTLE_MAX_LEVEL)
        return 0;
    if(sum < BATTLE_EXPERIENCE_CAP)
    {
        record->experience = static_cast<int32_t>(sum);
        return 0;
    }

    ++record->level;
    sum -= BATTLE_EXPERIENCE_CAP;
    recalculatePlayerMaxHealth();
    if(sum >= BATTLE_EXPERIENCE_CAP)
        sum = BATTLE_EXPERIENCE_CAP - 1;
    record->experience = static_cast<int32_t>(sum);
    return 1;
}

int
Game::applyBattleExperience(int reward)
{
    return applyBattleExperienceTo(characterRecord(0), reward);
}

int
Game::slotCharacterLevel(const GameSlotRecord& slot) const
{
    if(slot.characterId >= 0)
    {
        const PersistentGameRecord* record =
            characterRecord(static_cast<uint16_t>(slot.characterId));
        if(record != NULL)
            return record->level;
    }
    const CharacterStatRecord* stats = gameData.characterStats(
        characterDatabaseIndex(static_cast<CharacterId>(slot.characterId)));
    return stats == NULL ? 0 : stats->level;
}

int Game::battleExperienceAwarded(void) const { return lastBattleExperience; }

int Game::battleLevelsGained(void) const { return lastBattleLevelsGained; }
