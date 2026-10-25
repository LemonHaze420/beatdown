#ifndef GAME_SAVE_H
#define GAME_SAVE_H

#include "game/data.h"

#include <stddef.h>
#include <stdint.h>

const size_t GAME_SAVE_SETTINGS_SIZE = 0x248;
const size_t GAME_SAVE_SUMMARY_SIZE = 0x80;
const size_t GAME_SAVE_SUMMARY_COUNT = 6;
const size_t GAME_SAVE_PAYLOAD_SIZE = 0x36a74;
const size_t GAME_SAVE_RECORD_COUNT = 200;
const size_t GAME_SAVE_RECORD_SIZE = 0x43c;
const size_t GAME_SAVE_IMAGE_SIZE = 0x36fdc;
const size_t GAME_SAVE_INVENTORY_CAPACITY = 512;
const size_t GAME_SAVE_ACTION_STATE_CAPACITY = 38;
const size_t GAME_SAVE_MOVE_COUNT = 128;
const size_t GAME_SAVE_EVENT_BIT_COUNT = 8192;
const size_t GAME_SAVE_SCENARIO_COUNT = 50;
const size_t GAME_SAVE_INDEXED_VALUE_COUNT = 256;
const size_t GAME_SAVE_COOLDOWN_COUNT = 200;
const uint8_t GAME_SAVE_SUMMARY_FORMAT = 0x97;
const int16_t GAME_SAVE_INACTIVE_RECORD_ID = -1;
const size_t GAME_SAVE_CORE_STAGE_SELECTOR_OFFSET = 0x00a;
const size_t GAME_SAVE_CORE_STAGE_ID_OFFSET = 0x00c;
const size_t GAME_SAVE_CORE_PLAYER_POSITION_OFFSET = 0x010;
const size_t GAME_SAVE_CORE_PLAYER_HEADING_OFFSET = 0x01c;

const size_t GAME_SAVE_CORE_PRIMARY_COMBAT_MODIFIER_OFFSET = 0x8c7;

const size_t GAME_SAVE_CORE_TRAINING_LEVELS_OFFSET = 0x8c8;
const size_t GAME_SAVE_TRAINING_LEVEL_COUNT = 8;

#pragma pack(push, 1)
struct MemoryCardBlockTrailer
{
    uint16_t validMarker;
    uint16_t checksum;
};

struct GlobalSettingsBlock
{
    uint8_t field_0x000[0x54];
    uint8_t primaryProfileRecruitedCharacterBits[25];
    uint8_t field_0x06d[0x1db];
};

struct GameSaveSummary
{
    uint8_t dateTime[6];
    uint8_t field_0x006[2];
    uint8_t formatVersion;
    uint8_t playerLevel;
    int16_t rankTitleId;
    int16_t sessionCount;
    int16_t locationNameId;
    int16_t canonicalCharacterId;
    int16_t recruitedCharacterCount;
    float playTime;
    uint32_t cash;
    uint32_t contextValue;
    uint8_t field_0x020[0x60];
};

struct PersistentBattleGaugeState
{
    int16_t damageMaximum;
    int16_t tiredMaximum;
    int16_t attackMaximums[6];
};

struct PersistentMoveSlot
{
    int32_t type;
    int16_t id;
    uint8_t field_0x006[2];
};

struct PersistentGameRecord
{
    uint32_t flags;
    uint8_t gender;
    uint8_t bodyModelIndex;
    uint8_t costumeVariant;
    uint8_t encounterMode;
    uint8_t field_0x008;
    uint8_t field_0x009;
    uint8_t damageLevelsA[6][4];
    uint8_t damageLevelsB[6][4];
    uint8_t battleStatusFlags;
    uint8_t field_0x03b;
    uint8_t relationshipState;
    uint8_t field_0x03d;
    int16_t characterId;
    int32_t cash;
    int16_t resourceId;
    uint16_t aiProfileId;
    int32_t experience;
    int16_t level;
    int16_t maxHealth;
    uint8_t field_0x050;
    uint8_t attackPowerClass;
    int16_t combatStyleId;
    int16_t currentHealth;
    PersistentBattleGaugeState battleGaugeState;
    int16_t field_0x066;
    CharacterArchiveGroup archiveEntries[CHARACTER_ARCHIVE_ENTRY_COUNT];
    int16_t attackPowerPercent;
    int16_t healthBonus;
    int16_t field_0x384;
    int16_t field_0x386;
    uint32_t flags_0x388;
    uint32_t field_0x38c;
    PersistentMoveSlot moveSlots[2];
    int16_t partnerCharacterId;
    uint8_t partnerAssignmentState;
    uint8_t field_0x3a3;
    int16_t defeatCount;
    int16_t prideGauge;
    uint8_t characterFlags;
    uint8_t field_0x3a9;
    int16_t relationshipScore;
    int32_t recruitmentCallCount;
    int16_t characterValues[64];
    uint16_t profileRevealFlags;
    uint8_t status;
    uint8_t field_0x433;
    uint8_t field_0x434[8];
};

struct PersistentStoryProgressionBlock
{
    uint16_t phase;
    uint16_t reserved_0x02;
    uint16_t recruitedCharacterCount;
    uint16_t reserved_0x06;
    uint32_t flags;
    uint32_t transitionState_0x0c;
    uint32_t progressionSlots[10];
};

struct OwnedInventoryBlock
{
    uint32_t count;
    uint32_t entries[GAME_SAVE_INVENTORY_CAPACITY];
};

struct InventoryActionStateBlock
{
    uint32_t count;
    uint32_t entries[GAME_SAVE_ACTION_STATE_CAPACITY];
};

struct GameSavePayload
{
    PersistentGameRecord persistentRecords[GAME_SAVE_RECORD_COUNT];
    uint8_t coreGameStateBlock[0x8d0];
    PersistentStoryProgressionBlock storyProgressionBlock;
    int16_t activeRecordIds[4];
    OwnedInventoryBlock itemInventoryBlock;
    InventoryActionStateBlock inventoryActionStateBlock;
    int16_t moveInventoryCounts[GAME_SAVE_MOVE_COUNT];
    uint8_t eventFlags[GAME_SAVE_EVENT_BIT_COUNT / 8];
    uint8_t scenarioStateBits[13];
    uint8_t scenarioCompletionBits[13];
    uint8_t field_0x365aa[2];
    uint32_t indexedPersistentValues[GAME_SAVE_INDEXED_VALUE_COUNT];
    uint8_t characterCallCooldowns[GAME_SAVE_COOLDOWN_COUNT];
};

struct GameSaveSummaryBlock
{
    GameSaveSummary summary;
    MemoryCardBlockTrailer trailer;
};

struct GameSaveImage
{
    GlobalSettingsBlock settings;
    MemoryCardBlockTrailer settingsTrailer;
    GameSaveSummaryBlock summaries[GAME_SAVE_SUMMARY_COUNT];
    GameSavePayload payload;
    MemoryCardBlockTrailer payloadTrailer;
};
#pragma pack(pop)

enum MemoryCardBlockStatus
{
    MEMORY_CARD_BLOCK_INACTIVE = 0,
    MEMORY_CARD_BLOCK_VALID = 1,
    MEMORY_CARD_BLOCK_CHECKSUM_ERROR = 3
};

struct GameSaveBlockStatuses
{
    uint8_t settings;
    uint8_t summaries[GAME_SAVE_SUMMARY_COUNT];
    uint8_t payload;
};
struct GameSaveStageResumeState
{
    int16_t arrivalSelector;
    uint16_t stageId;
    float position[3];
    float headingDegrees;
};

static_assert(sizeof(MemoryCardBlockTrailer) == 4, "memory-card trailer ABI");
static_assert(sizeof(GlobalSettingsBlock) == GAME_SAVE_SETTINGS_SIZE,
              "settings block ABI");
static_assert(offsetof(GlobalSettingsBlock,
                       primaryProfileRecruitedCharacterBits) == 0x54,
              "primary-profile bitset offset");
static_assert(sizeof(GameSaveSummary) == GAME_SAVE_SUMMARY_SIZE,
              "summary ABI");
static_assert(offsetof(GameSaveSummary, formatVersion) == 0x08,
              "summary format offset");
static_assert(offsetof(GameSaveSummary, cash) == 0x18,
              "summary cash offset");
static_assert(sizeof(GameSaveSummaryBlock) == 0x84,
              "summary plus trailer ABI");
static_assert(sizeof(PersistentBattleGaugeState) == 0x10,
              "battle-gauge ABI");
static_assert(sizeof(PersistentMoveSlot) == 8, "move-slot ABI");
static_assert(sizeof(CharacterArchiveGroup) == 0x24,
              "character archive ABI");
static_assert(sizeof(PersistentGameRecord) == GAME_SAVE_RECORD_SIZE,
              "persistent character-record ABI");
static_assert(offsetof(PersistentGameRecord, damageLevelsA) == 0x00a,
              "damage A offset");
static_assert(offsetof(PersistentGameRecord, damageLevelsB) == 0x022,
              "damage B offset");
static_assert(offsetof(PersistentGameRecord, characterId) == 0x03e,
              "character id offset");
static_assert(offsetof(PersistentGameRecord, experience) == 0x048,
              "experience offset");
static_assert(offsetof(PersistentGameRecord, level) == 0x04c,
              "level offset");
static_assert(offsetof(PersistentGameRecord, maxHealth) == 0x04e,
              "maximum health offset");
static_assert(offsetof(PersistentGameRecord, currentHealth) == 0x054,
              "current health offset");
static_assert(offsetof(PersistentGameRecord, archiveEntries) == 0x068,
              "archive table offset");
static_assert(offsetof(PersistentGameRecord, moveSlots) == 0x390,
              "move slots offset");
static_assert(offsetof(PersistentGameRecord, prideGauge) == 0x3a6,
              "pride offset");
static_assert(offsetof(PersistentGameRecord, characterFlags) == 0x3a8,
              "character flags offset");
static_assert(offsetof(PersistentGameRecord, characterValues) == 0x3b0,
              "character values offset");
static_assert(offsetof(PersistentGameRecord, profileRevealFlags) == 0x430,
              "profile reveal offset");
static_assert(offsetof(PersistentGameRecord, status) == 0x432,
              "status offset");
static_assert(sizeof(PersistentStoryProgressionBlock) == 0x38,
              "story progression block ABI");
static_assert(offsetof(PersistentStoryProgressionBlock, phase) == 0x00,
              "story phase offset");
static_assert(offsetof(PersistentStoryProgressionBlock, flags) == 0x08,
              "story flags offset");
static_assert(offsetof(PersistentStoryProgressionBlock,
                       progressionSlots) == 0x10,
              "story progression-slot offset");
static_assert(sizeof(OwnedInventoryBlock) == 0x804,
              "owned inventory ABI");
static_assert(sizeof(InventoryActionStateBlock) == 0x9c,
              "inventory action-state ABI");
static_assert(sizeof(GameSavePayload) == GAME_SAVE_PAYLOAD_SIZE,
              "save payload ABI");
static_assert(offsetof(GameSavePayload, coreGameStateBlock) == 0x34ee0,
              "core block offset");
static_assert(offsetof(GameSavePayload, storyProgressionBlock) == 0x357b0,
              "story progression offset");
static_assert(offsetof(GameSavePayload, activeRecordIds) == 0x357e8,
              "active record ids offset");
static_assert(offsetof(GameSavePayload, itemInventoryBlock) == 0x357f0,
              "owned inventory offset");
static_assert(offsetof(GameSavePayload, inventoryActionStateBlock) == 0x35ff4,
              "inventory action-state offset");
static_assert(offsetof(GameSavePayload, moveInventoryCounts) == 0x36090,
              "move inventory offset");
static_assert(offsetof(GameSavePayload, eventFlags) == 0x36190,
              "event flags offset");
static_assert(offsetof(GameSavePayload, scenarioStateBits) == 0x36590,
              "scenario state offset");
static_assert(offsetof(GameSavePayload, scenarioCompletionBits) == 0x3659d,
              "scenario completion offset");
static_assert(offsetof(GameSavePayload, indexedPersistentValues) == 0x365ac,
              "indexed persistent-values offset");
static_assert(offsetof(GameSavePayload, characterCallCooldowns) == 0x369ac,
              "character cooldown offset");
static_assert(sizeof(GameSaveImage) == GAME_SAVE_IMAGE_SIZE,
              "complete game-data image ABI");
static_assert(offsetof(GameSaveImage, settingsTrailer) == 0x248,
              "settings trailer offset");
static_assert(offsetof(GameSaveImage, summaries) == 0x24c,
              "summary blocks offset");
static_assert(offsetof(GameSaveImage, payload) == 0x564,
              "payload offset");
static_assert(offsetof(GameSaveImage, payloadTrailer) == 0x36fd8,
              "payload trailer offset");

uint16_t memoryCardBlockChecksum(const void* data, size_t dataSize,
                                 uint16_t validMarker);
void finalizeMemoryCardBlockChecksum(void* data, size_t dataSize,
                                     MemoryCardBlockTrailer& trailer,
                                     uint16_t validMarker);
MemoryCardBlockStatus validateMemoryCardBlockChecksum(
    void* data, size_t dataSize, MemoryCardBlockTrailer& trailer);
bool validateGameSaveImage(GameSaveImage& image,
                           GameSaveBlockStatuses* statuses = NULL);
void initializeGameSaveImage(GameSaveImage& image, size_t summaryIndex = 0);
void finalizeGameSaveImage(GameSaveImage& image, size_t summaryIndex);
GameSaveStageResumeState gameSaveStageResumeState(
    const GameSaveImage& image);
void setGameSaveStageResumeState(
    GameSaveImage& image, const GameSaveStageResumeState& state);

bool settingsPrimaryProfileCharacterRecruited(
    const GlobalSettingsBlock& settings, uint16_t characterId);
void setSettingsPrimaryProfileCharacterRecruited(
    GlobalSettingsBlock& settings, uint16_t characterId, bool recruited);

uint16_t ownedInventoryQuantity(const OwnedInventoryBlock& inventory,
                                uint16_t itemId, int variant = -1);
bool addOwnedInventoryQuantity(OwnedInventoryBlock& inventory,
                               uint16_t itemId, uint8_t variant,
                               uint16_t quantity);
bool removeOwnedInventoryQuantity(OwnedInventoryBlock& inventory,
                                  uint16_t itemId, uint8_t variant,
                                  uint16_t quantity);

uint8_t packedScenarioValue(const uint8_t bits[13], size_t scenarioIndex);
bool setPackedScenarioValue(uint8_t bits[13], size_t scenarioIndex,
                            uint8_t value);

#endif
