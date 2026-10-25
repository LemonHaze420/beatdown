#include "game.h"
#include "game_combat_gauge.h"
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

void
Game::initializePersistentRecord(PersistentGameRecord& record,
                                 uint16_t characterId)
{
    memset(&record, 0, sizeof(record));
    record.characterId = static_cast<int16_t>(characterId);
    record.partnerCharacterId = -1;
    record.partnerAssignmentState = 0xff;
    record.attackPowerPercent = 100;
    record.field_0x384 = 0x40;

    const CharacterRecord* defaults = characterRecordForId(characterId);
    if(defaults != NULL)
    {
        record.resourceId = defaults->resourceId;
        record.aiProfileId = defaults->aiProfileId;
        record.level = defaults->level;
        record.maxHealth = defaults->maxHealth;
        record.currentHealth = defaults->maxHealth;
        record.attackPowerClass = defaults->attackPowerClass;
        record.combatStyleId = defaults->combatStyleId;
        record.encounterMode = defaults->encounterMode;
        record.relationshipState = defaults->initialRelationshipState;
        record.defeatCount = defaults->initialDefeatCount;
        record.prideGauge = defaults->initialPrideGauge;
        record.cash = defaults->cash;
        record.flags_0x388 = defaults->flags;
        memcpy(record.moveSlots, defaults->moveSlots,
               sizeof(record.moveSlots));
        record.profileRevealFlags = defaults->profileRevealFlags;
    }

    CharacterCustomization customization = {};
    const uint16_t databaseId = defaults != NULL && defaults->resourceId >= 0 ?
        static_cast<uint16_t>(defaults->resourceId) : characterId;
    if(gameData.characterCustomization(databaseId, customization))
    {
        record.gender = customization.genderSelector;
        record.bodyModelIndex = customization.bodyModelIndex;
        record.costumeVariant = customization.costumeVariant;
        if(customization.variantFlags != 0)
            record.flags |= 1u << 3;
        memcpy(record.archiveEntries, customization.archiveEntries,
               sizeof(record.archiveEntries));
    }

    if(characterId == 0)
    {
        record.characterFlags = 3;
        record.profileRevealFlags = 0xffff;
    }
}

void
Game::initializePersistentRecords(void)
{
    for(uint16_t i = 0; i < GAME_SAVE_RECORD_COUNT; ++i)
        initializePersistentRecord(primarySaveImage.payload.persistentRecords[i],
                                   i);
}

const PersistentGameRecord*
Game::characterRecord(uint16_t characterId) const
{
    return characterId < GAME_SAVE_RECORD_COUNT ?
        &primarySaveImage.payload.persistentRecords[characterId] : NULL;
}

PersistentGameRecord*
Game::characterRecord(uint16_t characterId)
{
    return characterId < GAME_SAVE_RECORD_COUNT ?
        &primarySaveImage.payload.persistentRecords[characterId] : NULL;
}

int16_t
Game::activeRecordId(size_t index) const
{
    return index < 4 ? primarySaveImage.payload.activeRecordIds[index] :
        GAME_SAVE_INACTIVE_RECORD_ID;
}

bool
Game::setActiveRecordId(size_t index, int16_t characterId)
{
    if(index >= 4 ||
       (characterId != GAME_SAVE_INACTIVE_RECORD_ID &&
        (characterId < 0 || characterId >=
         static_cast<int16_t>(GAME_SAVE_RECORD_COUNT))))
        return false;
    primarySaveImage.payload.activeRecordIds[index] = characterId;
    if(index < STORY_PARTY_COUNT)
        persistent.story.party[index] = characterId < 0 ? 0xffffu :
            static_cast<uint16_t>(characterId);
    return true;
}

const OwnedInventoryBlock& Game::ownedInventory(void) const
{
    return primarySaveImage.payload.itemInventoryBlock;
}

bool
Game::removeInventoryQuantity(uint16_t itemId, uint8_t variant,
                              uint16_t quantity)
{
    if(!removeOwnedInventoryQuantity(
           primarySaveImage.payload.itemInventoryBlock,
           itemId, variant, quantity))
        return false;
    return true;
}

const InventoryActionStateBlock& Game::inventoryActionState(void) const
{
    return primarySaveImage.payload.inventoryActionStateBlock;
}

int16_t Game::moveInventoryCount(size_t index) const
{
    return index < GAME_SAVE_MOVE_COUNT ?
        primarySaveImage.payload.moveInventoryCounts[index] : 0;
}

bool Game::eventFlag(size_t index) const
{
    if(index >= GAME_SAVE_EVENT_BIT_COUNT)
        return false;
    return (primarySaveImage.payload.eventFlags[index / 8] &
            static_cast<uint8_t>(1u << (index & 7))) != 0;
}

void Game::setEventFlag(size_t index, bool enabled)
{
    if(index >= GAME_SAVE_EVENT_BIT_COUNT)
        return;
    uint8_t& byte = primarySaveImage.payload.eventFlags[index / 8];
    const uint8_t mask = static_cast<uint8_t>(1u << (index & 7));
    if(enabled)
        byte |= mask;
    else
        byte &= static_cast<uint8_t>(~mask);
    setStorySaveFlag(persistent.story, static_cast<uint16_t>(index), enabled);
}

uint8_t Game::scenarioState(size_t index) const
{
    return packedScenarioValue(
        primarySaveImage.payload.scenarioStateBits, index);
}

uint8_t Game::scenarioCompletion(size_t index) const
{
    return packedScenarioValue(
        primarySaveImage.payload.scenarioCompletionBits, index);
}

bool Game::setScenarioState(size_t index, uint8_t value)
{
    return setPackedScenarioValue(
        primarySaveImage.payload.scenarioStateBits, index, value);
}

bool Game::setScenarioCompletion(size_t index, uint8_t value)
{
    return setPackedScenarioValue(
        primarySaveImage.payload.scenarioCompletionBits, index, value);
}

uint32_t Game::indexedPersistentValue(size_t index) const
{
    return index < GAME_SAVE_INDEXED_VALUE_COUNT ?
        primarySaveImage.payload.indexedPersistentValues[index] : 0;
}

bool Game::setIndexedPersistentValue(size_t index, uint32_t value)
{
    if(index >= GAME_SAVE_INDEXED_VALUE_COUNT)
        return false;
    primarySaveImage.payload.indexedPersistentValues[index] = value;
    return true;
}

uint8_t Game::characterCallCooldown(size_t characterId) const
{
    return characterId < GAME_SAVE_COOLDOWN_COUNT ?
        primarySaveImage.payload.characterCallCooldowns[characterId] : 0;
}

bool Game::setCharacterCallCooldown(size_t characterId, uint8_t value)
{
    if(characterId >= GAME_SAVE_COOLDOWN_COUNT)
        return false;
    primarySaveImage.payload.characterCallCooldowns[characterId] = value;
    return true;
}

bool
Game::characterCustomization(
    uint16_t characterId, CharacterCustomization& customization) const
{
    const PersistentGameRecord* record = characterRecord(characterId);
    if(record == NULL)
        return false;
    customization.bodyModelIndex = record->bodyModelIndex;
    customization.costumeVariant = record->costumeVariant;
    customization.genderSelector = record->gender;
    customization.variantFlags = (record->flags & (1u << 3)) != 0 ? 1 : 0;
    memcpy(customization.archiveEntries, record->archiveEntries,
           sizeof(customization.archiveEntries));
    return true;
}

bool
Game::commitCharacterCustomization(
    uint16_t characterId, const CharacterCustomization& customization)
{
    PersistentGameRecord* record = characterRecord(characterId);
    if(record == NULL)
        return false;
    record->costumeVariant = customization.costumeVariant;
    memcpy(record->archiveEntries, customization.archiveEntries,
           sizeof(record->archiveEntries));
    return true;
}

bool
Game::applyBattlePersistentStats(GameSlotRecord& slot,
                                 uint16_t authoredCharacterId,
                                 uint16_t persistentRecordId,
                                 bool activeParty) const
{
    const PersistentGameRecord* record =
        persistentRecordId >= 200 &&
        persistentRecordId <
            200 + GAME_STAGE_ACTOR_OVERFLOW_RECORD_COUNT ?
        &stageActorOverflowRecords[persistentRecordId - 200] :
        characterRecord(persistentRecordId);
    if(record == NULL)
        return false;

    slot.characterId = static_cast<int16_t>(authoredCharacterId);
    slot.nativeCharacterId = (persistent.story.gameStateFlags & 2u) != 0 ?
        record->characterId : static_cast<int16_t>(authoredCharacterId);

    if(currentMode == GAME_MODE_STORY && slot.slotIndex == 0)
        slot.nativeCharacterId = 0;
    slot.persistentRecordId = static_cast<int16_t>(persistentRecordId);
    slot.resourceProfileId = resolveBattleResourceProfile(
        static_cast<int16_t>(authoredCharacterId), record->resourceId);

    memcpy(slot.moveSlots, record->moveSlots, sizeof(slot.moveSlots));
    for(size_t i = 0; i < 2; ++i)
        slot.moveSlotActive[i] = record->moveSlots[i].type != 0 ? 1 : 0;

    int32_t current = record->currentHealth;
    if(current < 1)
        current = 1;
    current += record->healthBonus;
    int32_t maximum =
        static_cast<int32_t>(record->maxHealth) + record->healthBonus;
    if(current > 9999)
        current = 9999;
    if(maximum > 9999)
        maximum = 9999;
    slot.currentHealth = static_cast<int16_t>(current);
    slot.maxHealth = static_cast<int16_t>(maximum);

    const int16_t shortGauge = static_cast<int16_t>(
        static_cast<float>(slot.maxHealth) * 0.2f);
    slot.damageGauge = shortGauge;
    slot.damageGaugeMaximum = shortGauge;
    slot.tiredGaugeMaximum = static_cast<int16_t>(
        static_cast<float>(slot.maxHealth) * 1.5f);
    initializeCombatGaugeRuntime(slot);
    for(int i = 0; i < 4; ++i)
    {
        slot.bodyPartHealth[i] = record->damageLevelsA[0][i];
        slot.bodyPartDamage[i] = record->damageLevelsB[0][i];
        slot.auxiliaryGaugeMaximums[i] = shortGauge;
    }
    for(int i = 0; i < COMBAT_ATTACK_GAUGE_COUNT; ++i)
    {
        slot.attackGauges[i] = shortGauge;
        slot.attackGaugeMaximums[i] = shortGauge;
    }

    slot.prideGauge = record->prideGauge;
    const CharacterRecord* profileDefaults = characterRecordForId(
        static_cast<uint16_t>(slot.resourceProfileId));
    slot.maxPrideGauge = profileDefaults != NULL ?
        profileDefaults->initialPrideGauge : record->prideGauge;

    const PersistentGameRecord* authoredRecord =
        characterRecord(authoredCharacterId);
    const CharacterRecord* authoredDefaults =
        characterRecordForId(authoredCharacterId);
    const int level = authoredRecord != NULL ? authoredRecord->level :
        (authoredDefaults != NULL ? authoredDefaults->level : 0);
    slot.levelTerm = static_cast<float>(level) * 0.1f + 11.0f;

    slot.attackPowerMultiplier =
        moveAttackPowerForClass(static_cast<int8_t>(record->attackPowerClass));
    if(record->attackPowerPercent != 100)
        slot.attackPowerMultiplier *=
            static_cast<float>(record->attackPowerPercent) * 0.01f;
    if(!activeParty)
    {
        const int difficulty = persistent.options.difficulty;
        if(difficulty >= 0 && difficulty < MOVE_DIFFICULTY_POWER_COUNT)
            slot.attackPowerMultiplier *= MOVE_DIFFICULTY_POWER[difficulty];
    }
    slot.dynamicAttackMultiplier = 1.0f;

    if((record->flags & (1u << 3)) != 0)
        slot.flags |= 0x1000u;
    else
        slot.flags &= ~0x1000u;
    slot.flags |= 0x10000u;
    return true;
}

bool
Game::projectPersistentRecordToGameSlot(
    uint16_t characterId, GameSlotRecord& slot) const
{
    return applyBattlePersistentStats(
        slot, characterId, characterId, slot.slotIndex < 2);
}

bool
Game::projectGameSlotCombatToPersistentRecord(const GameSlotRecord& slot)
{
    if(slot.persistentRecordId < 0)
        return false;
    PersistentGameRecord* record = characterRecord(
        static_cast<uint16_t>(slot.persistentRecordId));
    if(record == NULL)
        return false;
    record->currentHealth = slot.currentHealth;
    record->maxHealth = slot.maxHealth;
    record->prideGauge = slot.prideGauge;
    for(size_t i = 0; i < 4; ++i)
    {
        record->damageLevelsA[0][i] =
            static_cast<uint8_t>(slot.bodyPartHealth[i]);
        record->damageLevelsB[0][i] =
            static_cast<uint8_t>(slot.bodyPartDamage[i]);
    }
    return true;
}

void
Game::projectPersistentStateToRuntime(void)
{
    const size_t eventBytes = sizeof(persistent.story.saveFlags) <
        sizeof(primarySaveImage.payload.eventFlags) ?
        sizeof(persistent.story.saveFlags) :
        sizeof(primarySaveImage.payload.eventFlags);
    memcpy(persistent.story.saveFlags,
           primarySaveImage.payload.eventFlags, eventBytes);
    const PersistentStoryProgressionBlock& savedProgression =
        primarySaveImage.payload.storyProgressionBlock;
    persistent.story.progression.phase =
        static_cast<StoryProgressionPhase>(savedProgression.phase);
    persistent.story.progression.recruitedCharacterCount =
        savedProgression.recruitedCharacterCount;
    persistent.story.progression.controlFlags = savedProgression.flags;

    for(uint16_t i = 0; i < GAME_SAVE_RECORD_COUNT; ++i)
    {
        const PersistentGameRecord& source =
            primarySaveImage.payload.persistentRecords[i];
        StoryCharacterState& target = persistent.story.characters[i];
        target.relationshipScore = source.relationshipScore;
        target.characterFlags = 0;
        if((source.characterFlags & 1u) != 0)
            target.characterFlags |= STORY_CHARACTER_RECRUITED;
        if((source.characterFlags & 2u) != 0)
            target.characterFlags |= STORY_CHARACTER_ENCOUNTERED;
        target.profileRevealFlags = source.profileRevealFlags;
        target.battleStatusFlags = source.battleStatusFlags;
        target.cash = source.cash;
        target.relationshipState = source.relationshipState;
        target.gender = source.gender;
        target.persistentStatus = source.status;
    }

    for(size_t i = 0; i < STORY_PARTY_COUNT; ++i)
    {
        const int16_t id = i < 4 ?
            primarySaveImage.payload.activeRecordIds[i] :
            GAME_SAVE_INACTIVE_RECORD_ID;
        persistent.story.party[i] = id < 0 ? 0xffffu :
            static_cast<uint16_t>(id);
    }

    const int16_t active = activeRecordId(0);
    if(active >= 0)
    {
        persistent.characterId = static_cast<uint16_t>(active);
        const PersistentGameRecord& record =
            primarySaveImage.payload.persistentRecords[active];
        persistent.story.money = record.cash;
        persistent.story.pride = record.prideGauge;
        projectPersistentRecordToGameSlot(
            static_cast<uint16_t>(active), persistent.player);
        characterCustomization(
            static_cast<uint16_t>(active), persistent.story.customization);
    }
    const GameSaveStageResumeState resume =
        gameSaveStageResumeState(primarySaveImage);
    persistent.stageId = resume.stageId;
    persistent.story.stageId = resume.stageId;
    persistent.player.position.x = resume.position[0];
    persistent.player.position.y = resume.position[1];
    persistent.player.position.z = resume.position[2];
    persistent.player.headingDegrees = resume.headingDegrees;
}

void
Game::projectRuntimeStateToPersistent(GameSaveImage& image) const
{
    const size_t eventBytes = sizeof(persistent.story.saveFlags) <
        sizeof(image.payload.eventFlags) ?
        sizeof(persistent.story.saveFlags) : sizeof(image.payload.eventFlags);
    memcpy(image.payload.eventFlags,
           persistent.story.saveFlags, eventBytes);
    PersistentStoryProgressionBlock& savedProgression =
        image.payload.storyProgressionBlock;
    savedProgression.phase =
        static_cast<uint16_t>(persistent.story.progression.phase);
    savedProgression.recruitedCharacterCount =
        persistent.story.progression.recruitedCharacterCount;
    savedProgression.flags = persistent.story.progression.controlFlags;

    for(uint16_t i = 0; i < GAME_SAVE_RECORD_COUNT; ++i)
    {
        PersistentGameRecord& target = image.payload.persistentRecords[i];
        const StoryCharacterState& source = persistent.story.characters[i];
        target.relationshipScore = source.relationshipScore;
        target.characterFlags = 0;
        if((source.characterFlags & STORY_CHARACTER_RECRUITED) != 0)
            target.characterFlags |= 1u;
        if((source.characterFlags & STORY_CHARACTER_ENCOUNTERED) != 0)
            target.characterFlags |= 2u;
        target.profileRevealFlags = source.profileRevealFlags;
        target.battleStatusFlags =
            static_cast<uint8_t>(source.battleStatusFlags);
        target.cash = source.cash;
        target.relationshipState = source.relationshipState;
        target.status = source.persistentStatus;
    }
    for(size_t i = 0; i < 4; ++i)
    {
        const uint16_t id = i < STORY_PARTY_COUNT ?
            persistent.story.party[i] : 0xffffu;
        image.payload.activeRecordIds[i] =
            id == 0xffffu || id >= GAME_SAVE_RECORD_COUNT ?
            GAME_SAVE_INACTIVE_RECORD_ID : static_cast<int16_t>(id);
    }

    const int16_t active = image.payload.activeRecordIds[0];
    if(active >= 0 && active < static_cast<int16_t>(GAME_SAVE_RECORD_COUNT))
    {
        PersistentGameRecord& record =
            image.payload.persistentRecords[active];
        record.cash = persistent.story.money;
        record.prideGauge = persistent.story.pride;
        record.currentHealth = persistent.player.currentHealth;
        record.maxHealth = persistent.player.maxHealth;
        for(size_t i = 0; i < 4; ++i)
        {
            record.damageLevelsA[0][i] =
                static_cast<uint8_t>(persistent.player.bodyPartHealth[i]);
            record.damageLevelsB[0][i] =
                static_cast<uint8_t>(persistent.player.bodyPartDamage[i]);
        }
    }
    GameSaveStageResumeState resume = gameSaveStageResumeState(image);
    resume.stageId = persistent.stageId;
    resume.position[0] = persistent.player.position.x;
    resume.position[1] = persistent.player.position.y;
    resume.position[2] = persistent.player.position.z;
    resume.headingDegrees = persistent.player.headingDegrees;
    setGameSaveStageResumeState(image, resume);
}

void
Game::refreshCurrentSummary(GameSaveImage& image) const
{
    const size_t selected = activeSummaryIndex < GAME_SAVE_SUMMARY_COUNT ?
        activeSummaryIndex : 0;
    GameSaveSummary& summary = image.summaries[selected].summary;
    summary.formatVersion = GAME_SAVE_SUMMARY_FORMAT;
    const int16_t active = image.payload.activeRecordIds[0];
    if(active >= 0 && active < static_cast<int16_t>(GAME_SAVE_RECORD_COUNT))
    {
        const PersistentGameRecord& record =
            image.payload.persistentRecords[active];
        summary.playerLevel = static_cast<uint8_t>(
            record.level < 0 ? 0 : (record.level > 255 ? 255 : record.level));
        summary.canonicalCharacterId = active;
        summary.cash = record.cash < 0 ? 0 :
            static_cast<uint32_t>(record.cash);
    }
    int recruited = 1;
    for(size_t i = 1; i < GAME_SAVE_RECORD_COUNT; ++i)
        recruited +=
            (image.payload.persistentRecords[i].characterFlags & 1u) != 0;
    summary.recruitedCharacterCount =
        static_cast<int16_t>(recruited);
}

void
Game::hydrateStoryCharacters(void)
{
    projectPersistentStateToRuntime();
}

void
Game::synchronizeBattleStoryState(void)
{
    StoryActorState* player = findStoryActor(persistent.story, 0, true);
    if(player != NULL)
    {
        player->position[0] = persistent.player.position.x;
        player->position[1] = persistent.player.position.y;
        player->position[2] = persistent.player.position.z;
        player->headingDegrees = persistent.player.headingDegrees;
        player->currentHealth = persistent.player.currentHealth;
        player->defeated = persistent.player.currentHealth < 1;
    }

    for(int i = 0; i < activeOpponentCount; ++i)
    {
        const GameSlotRecord& slot = i == 0 ?
            enemy : additionalEnemies[i - 1];
        StoryActorState* actor = findStoryActor(persistent.story,
            static_cast<uint16_t>(slot.characterId), true);
        if(actor == NULL)
            continue;
        actor->position[0] = slot.position.x;
        actor->position[1] = slot.position.y;
        actor->position[2] = slot.position.z;
        actor->headingDegrees = slot.headingDegrees;
        actor->currentHealth = slot.currentHealth;
        actor->defeated = slot.currentHealth < 1;
        actor->active = slot.currentHealth > 0;
    }
}

bool
Game::save(const char* path)
{
    if(path == NULL)
        return false;

    GameSaveImage image = primarySaveImage;
    projectRuntimeStateToPersistent(image);
    refreshCurrentSummary(image);
    finalizeGameSaveImage(image, activeSummaryIndex);

    FILE* output = fopen(path, "wb");
    if(output == NULL)
        return false;
    bool ok = fwrite(&image, 1, sizeof(image), output) == sizeof(image);
    ok = fclose(output) == 0 && ok;
    if(ok)
        primarySaveImage = image;
    return ok;
}

bool
Game::loadSecondaryProfile(const char* path)
{
    if(path == NULL)
        return false;
    FILE* input = fopen(path, "rb");
    if(input == NULL)
        return false;

    GameSaveImage image;
    const bool read = fread(&image, 1, sizeof(image), input) == sizeof(image);
    const int trailing = fgetc(input);
    const bool closed = fclose(input) == 0;
    if(!read || !closed || trailing != EOF ||
       !validateGameSaveImage(image))
        return false;
    secondarySaveImage = image;
    secondaryProfileLoaded = true;
    return true;
}

void
Game::clearSecondaryProfile(void)
{
    memset(&secondarySaveImage, 0, sizeof(secondarySaveImage));
    secondaryProfileLoaded = false;
}

bool Game::hasSecondaryProfile(void) const { return secondaryProfileLoaded; }

const PersistentGameRecord*
Game::secondaryCharacterRecord(uint16_t characterId) const
{
    if(!secondaryProfileLoaded || characterId >= GAME_SAVE_RECORD_COUNT)
        return NULL;
    return &secondarySaveImage.payload.persistentRecords[characterId];
}

bool
Game::load(const char* path)
{
    if(path == NULL)
        return false;
    FILE* input = fopen(path, "rb");
    if(input == NULL)
        return false;

    GameSaveImage image;
    const bool read = fread(&image, 1, sizeof(image), input) == sizeof(image);
    const int trailing = fgetc(input);
    const bool closed = fclose(input) == 0;
    GameSaveBlockStatuses statuses = {};
    if(!read || !closed || trailing != EOF ||
       !validateGameSaveImage(image, &statuses))
        return false;

    size_t selectedSummary = 0;
    for(size_t i = 0; i < GAME_SAVE_SUMMARY_COUNT; ++i)
    {
        if(statuses.summaries[i] == MEMORY_CARD_BLOCK_VALID)
        {
            selectedSummary = i;
            break;
        }
    }

    eventVm.stop();
    messageVm.stop();
    weaponManager.releaseAll();
    pendingWeaponStates.clear();
    primarySaveImage = image;
    activeSummaryIndex = selectedSummary;
    primarySaveLoaded = true;
    savedFieldResumePending = true;
    initializeStoryState(persistent.story);
    projectPersistentStateToRuntime();
    persistent.hudRadar = battleHudDefaultRadarState();
    selectedCharacter = 0;
    for(int i = 0; i < 5; ++i)
    {
        if(persistent.characterId ==
           static_cast<uint16_t>(PLAYABLE_CHARACTER_IDS[i]))
        {
            selectedCharacter = i;
            break;
        }
    }
    menuSelection = 0;
    selectedPauseItem = 0;
    selectedInventoryItem = 0;
    inventoryEquipmentReady = false;
    selectedOption = 0;
    optionsReturnScreen = GAME_SCREEN_MAIN_MENU;
    pendingHostVm = NULL;
    eventWaitingForMessage = false;
    pendingEventFunction = EVENT_CALLFUNC_NONE;
    currentBattleOutcome = BATTLE_OUTCOME_IN_PROGRESS;
    battleTimeScale = BATTLE_TIME_SCALE_NORMAL;
    battleElapsedSeconds = 0.0f;
    activeOpponentCount = 0;
    targetOpponentIndex = 0;
    resetBattleResultController();
    focusedInteractionSlot = -1;
    clearStageInteractions();
    memset(&enemy, 0, sizeof(enemy));
    memset(additionalEnemies, 0, sizeof(additionalEnemies));
    hasSave = true;
    currentScreen = GAME_SCREEN_LANGUAGE_SELECT;
    if(gameFlowTask != NULL)
        switchGameFlowTaskHandler(updateBootLanguageSelection, 1);
    return true;
}

void Game::setSaveAvailable(bool available) { hasSave = available; }

bool Game::loadGameData(const char* archiveDirectory)
{
    if(!gameData.load(archiveDirectory) ||
       !gameData.reloadLocalizedText(persistent.options.language))
        return false;
    if(!primarySaveLoaded)
        initializePersistentRecords();
    hydrateStoryCharacters();
    return true;
}
