#ifndef GAME_DATA_H
#define GAME_DATA_H

#include <stdint.h>
#include <string>
#include <vector>
#include "../formats/ITEM/item_data.h"

enum GameLanguage
{
    GAME_LANGUAGE_JAPANESE = 0,
    GAME_LANGUAGE_ENGLISH = 1,
    GAME_LANGUAGE_FRENCH = 2,
    GAME_LANGUAGE_GERMAN = 3,
    GAME_LANGUAGE_SPANISH = 4,
    GAME_LANGUAGE_ITALIAN = 5,
    GAME_LANGUAGE_COUNT = 6
};

enum GameTextId
{
    GAME_TEXT_PRESS_START = 9000,
    GAME_TEXT_NEW_GAME = 9001,
    GAME_TEXT_LOAD_GAME = 9002,
    GAME_TEXT_VS_MODE_NORMAL = 9003,
    GAME_TEXT_VS_MODE_CUSTOM = 9004,
    GAME_TEXT_DIFFICULTY = 9005,
    GAME_TEXT_SUBTITLES = 9006,
    GAME_TEXT_MAP = 9007,
    GAME_TEXT_LIFE_GAUGE = 9008,
    GAME_TEXT_BLOOD_MALE = 9009,
    GAME_TEXT_BLOOD_FEMALE = 9010,
    GAME_TEXT_BRUISING_MALE = 9011,
    GAME_TEXT_BRUISING_FEMALE = 9012,
    GAME_TEXT_AUDIO_CONFIG = 9013,
    GAME_TEXT_CONTROL_CONFIG = 9014,
    GAME_TEXT_SCREEN_ADJUST = 9015,
    GAME_TEXT_BRIGHTNESS = 9016,
    GAME_TEXT_DEFAULT_SETTING = 9017,
    GAME_TEXT_EXIT = 9018,
    GAME_TEXT_EASY = 9019,
    GAME_TEXT_NORMAL = 9020,
    GAME_TEXT_HARD = 9021,
    GAME_TEXT_OFF = 9022,
    GAME_TEXT_ON = 9023,
    GAME_TEXT_FIXED = 9024,
    GAME_TEXT_DYNAMIC = 9025,
    GAME_TEXT_AUDIO = 9026,
    GAME_TEXT_STEREO = 9027,
    GAME_TEXT_MONO = 9028,
    GAME_TEXT_DOLBY_PRO_LOGIC_II = 9029,
    GAME_TEXT_MUTE = 9030,
    GAME_TEXT_VOLUME_1 = 9031,
    GAME_TEXT_VOLUME_2 = 9032,
    GAME_TEXT_VOLUME_3 = 9033,
    GAME_TEXT_VOLUME_4 = 9034,
    GAME_TEXT_VOLUME_5 = 9035,
    GAME_TEXT_VOLUME_6 = 9036,
    GAME_TEXT_VOLUME_7 = 9037,
    GAME_TEXT_VOLUME_8 = 9038,
    GAME_TEXT_VOLUME_9 = 9039,
    GAME_TEXT_VOLUME_10 = 9040,
    GAME_TEXT_VOLUME_11 = 9041,
    GAME_TEXT_VOLUME_12 = 9042,
    GAME_TEXT_VOLUME_13 = 9043,
    GAME_TEXT_VOLUME_14 = 9044,
    GAME_TEXT_VOLUME_MAXIMUM = 9045,
    GAME_TEXT_CONTROLLER_TYPE_1 = 9046,
    GAME_TEXT_CONTROLLER_TYPE_2 = 9047,
    GAME_TEXT_CONTROLLER_TYPE_3 = 9048,
    GAME_TEXT_CONTROLLER_TYPE_4 = 9049,
    GAME_TEXT_CONTROLLER_TYPE_5 = 9050,
    GAME_TEXT_CONTROLLER_TYPE_6 = 9051,
    GAME_TEXT_CONTROLLER_NORMAL = 9052,
    GAME_TEXT_CONTROLLER_REVERSE = 9053,
    GAME_TEXT_TYPE_PLAYER_1 = 9054,
    GAME_TEXT_VIBRATION_PLAYER_1 = 9055,
    GAME_TEXT_TYPE_PLAYER_2 = 9056,
    GAME_TEXT_VIBRATION_PLAYER_2 = 9057,
    GAME_TEXT_CAMERA_TYPE_X = 9058,
    GAME_TEXT_CAMERA_TYPE_Y = 9059,
    GAME_TEXT_CAMERA_FOLLOW = 9060,
    GAME_TEXT_X_POSITION = 9061,
    GAME_TEXT_Y_POSITION = 9062,
    GAME_TEXT_MOVIE_VIEWER = 9063,
    GAME_TEXT_NOW_LOADING = 9064,
    GAME_TEXT_BGM_LEVEL = 9080,
    GAME_TEXT_SE_LEVEL = 9081,
    GAME_TEXT_SELECT = 10402,
    GAME_TEXT_OK = 10403,
    GAME_TEXT_CANCEL = 10406,
    GAME_TEXT_BACK = 10407,
    GAME_TEXT_QUIT_GAME = 10476,
    GAME_TEXT_PAUSE = 10470,
    GAME_TEXT_RESUME = 10471,
    GAME_TEXT_RESTART_MATCH = 10473,
    GAME_TEXT_RETURN_MODE_SELECT = 10474,
    GAME_TEXT_RETURN_CHARACTER_SELECT = 10475,
    GAME_TEXT_ITEMS = 10603,
    GAME_TEXT_OPTIONS = 10609,
    GAME_TEXT_RETRY_BATTLE = 10904,
    GAME_TEXT_RETURN_TO_MODE_SELECT = 10905,
    GAME_TEXT_RETURN_TO_CHARACTER_SELECT = 10906,

    GAME_TEXT_CHARACTER_LOLA_NAME = 11023,
    GAME_TEXT_CHARACTER_JASON_G_NAME = 11054,
    GAME_TEXT_CHARACTER_AARON_NAME = 11069,
    GAME_TEXT_CHARACTER_RAVEN_NAME = 11108,
    GAME_TEXT_CHARACTER_GINA_NAME = 11109,
    GAME_TEXT_USE = 30821,

    GAME_TEXT_TITLE_LAS_SOMBRAS = 30001,
    GAME_TEXT_TITLE_WAR_ZONE = 30002,
    GAME_TEXT_TITLE_LOGO = 30004,
    GAME_TEXT_TITLE_BEAT_DOWN = 30005,
    GAME_TEXT_TITLE_FISTS_OF_VENGEANCE = 5901,
    GAME_TEXT_RESULTS = 30207,
    GAME_TEXT_GAMEPLAY_CONTROLS = 30232,
    GAME_TEXT_YOU_LOSE = 31140,
    GAME_TEXT_SELECT_STAGE_HELP = 39513,
    GAME_TEXT_TITLE_OPTIONS = 52121,
    GAME_TEXT_CHARACTER_MOBSTER = 52140,
    GAME_TEXT_CHARACTER_RAVEN = 52295,
    GAME_TEXT_CHARACTER_JASON_G = 52296,
    GAME_TEXT_CHARACTER_GINA = 52297,
    GAME_TEXT_CHARACTER_LOLA = 52298,
    GAME_TEXT_CHARACTER_AARON = 52299
};

std::string normalizeDisplayText(const uint8_t* data, size_t length);

enum GameTextTable
{
    GAME_TEXT_TABLE_MAIN = 0,
    GAME_TEXT_TABLE_ITEM,
    GAME_TEXT_TABLE_WAZA,
    GAME_TEXT_TABLE_STAFF,
    GAME_TEXT_TABLE_NEGO,
    GAME_TEXT_TABLE_SUB,
    GAME_TEXT_TABLE_MCARD,
    GAME_TEXT_TABLE_PHONE,
    GAME_TEXT_TABLE_COUNT
};

enum GameTextDescriptor
{
    GAME_TEXT_DESCRIPTOR_MAIN = 0,
    GAME_TEXT_DESCRIPTOR_ITEM,
    GAME_TEXT_DESCRIPTOR_WAZA,
    GAME_TEXT_DESCRIPTOR_TRANSIENT
};

GameTextDescriptor gameTextDescriptorForId(uint16_t textId);

extern const char* const GAME_TEXT_ARCHIVE_PREFIXES[GAME_TEXT_TABLE_COUNT];

struct LocalizedTextEntry
{
    uint16_t textId;
    uint16_t byteLength;
    std::string text;
};

bool gameTextEncoding(uint8_t language, uint16_t& encoding);
bool parseLocalizedTextTable(const std::vector<uint8_t>& data,
                             uint8_t language,
                             std::vector<LocalizedTextEntry>& entries);
const char* lookupLocalizedTextTable(
    const std::vector<LocalizedTextEntry>& entries, uint16_t textId,
    int16_t* byteLength = NULL);

const int CHARACTER_ARCHIVE_ENTRY_COUNT = 22;
const int CHARACTER_ARCHIVE_TEXTURE_COUNT = 16;

struct CharacterArchiveGroup
{
    uint32_t archiveId;
    int16_t textureIds[CHARACTER_ARCHIVE_TEXTURE_COUNT];
};

struct CharacterDatabaseRecord
{
    CharacterArchiveGroup groups[CHARACTER_ARCHIVE_ENTRY_COUNT];
    uint32_t variantWords[96];
};

struct CharacterCustomization
{
    uint8_t bodyModelIndex;
    uint8_t costumeVariant;
    uint8_t genderSelector;
    uint8_t variantFlags;
    CharacterArchiveGroup archiveEntries[CHARACTER_ARCHIVE_ENTRY_COUNT];
};
struct CharacterStatRecord
{
    uint16_t resourceId;
    uint16_t combatStyleId;
    uint8_t aiProfileId;
    uint8_t groupId;
    uint8_t level;
    uint8_t attackPowerClass;
    int16_t maxHealth;
    int16_t initialPrideGauge;
    int16_t initialDefeatCount;
    int32_t initialCash;
};

enum InventoryItemCategoryId
{
    INVENTORY_CATEGORY_BREAST_TATTOO = 0x01,
    INVENTORY_CATEGORY_CHEST_HAIR = 0x02,
    INVENTORY_CATEGORY_BELLY_TATTOO = 0x03,
    INVENTORY_CATEGORY_FACE = 0x04,
    INVENTORY_CATEGORY_HAIR = 0x05,
    INVENTORY_CATEGORY_BACK_TATTOO = 0x06,
    INVENTORY_CATEGORY_PANTS = 0x07,
    INVENTORY_CATEGORY_RIGHT_HAND = 0x08,
    INVENTORY_CATEGORY_LEG_PANTS_COUPLING = 0x09,
    INVENTORY_CATEGORY_CAP = 0x0a,
    INVENTORY_CATEGORY_GLASSES = 0x0b,
    INVENTORY_CATEGORY_MASK = 0x0c,
    INVENTORY_CATEGORY_MOUTH = 0x0d,
    INVENTORY_CATEGORY_NECKLACE = 0x0e,
    INVENTORY_CATEGORY_PENDANT = 0x0f,
    INVENTORY_CATEGORY_LEFT_EARRING = 0x10,
    INVENTORY_CATEGORY_LEFT_WRIST = 0x11,
    INVENTORY_CATEGORY_LEFT_RING = 0x12,
    INVENTORY_CATEGORY_BELT = 0x13,
    INVENTORY_CATEGORY_SHOES = 0x14,
    INVENTORY_CATEGORY_LEFT_ARM = 0x15,
    INVENTORY_CATEGORY_RIGHT_ARM = 0x16,
    INVENTORY_CATEGORY_LEFT_LEG = 0x17,
    INVENTORY_CATEGORY_RIGHT_LEG = 0x18,
    INVENTORY_CATEGORY_TOP = 0x19,
    INVENTORY_CATEGORY_EYES = 0x1a,
    INVENTORY_CATEGORY_FACE_VARIANT_1 = 0x1b,
    INVENTORY_CATEGORY_FACE_VARIANT_2 = 0x1c,
    INVENTORY_CATEGORY_FACE_VARIANT_3 = 0x1d,
    INVENTORY_CATEGORY_FACE_VARIANT_4 = 0x1e,
    INVENTORY_CATEGORY_RIGHT_EARRING = 0x1f,
    INVENTORY_CATEGORY_RIGHT_WRIST = 0x20,
    INVENTORY_CATEGORY_SUPPLIES = 0x21,
    INVENTORY_CATEGORY_WEAPON = 0x22,
    INVENTORY_CATEGORY_ETC_1 = 0x23,
    INVENTORY_CATEGORY_ETC_2 = 0x24,
    INVENTORY_CATEGORY_RIGHT_RING = 0x25,
    INVENTORY_CATEGORY_VM_SPECIAL_RANGE = 0x26,
    INVENTORY_CATEGORY_VM_BODY_MARKS = 0x27
};

enum InventoryCategoryGroup
{
    INVENTORY_CATEGORY_GROUP_OTHER = 0,
    INVENTORY_CATEGORY_GROUP_CLOTHING = 1,
    INVENTORY_CATEGORY_GROUP_ACCESSORY = 2,
    INVENTORY_CATEGORY_GROUP_BODY = 3,
    INVENTORY_CATEGORY_GROUP_CONSUMABLE = 4
};

enum InventoryActionCategory
{
    INVENTORY_ACTION_NONE = 0,
    INVENTORY_ACTIONABLE = 1
};

struct ItemResourceCatalog
{
    uint16_t iconCount;
    uint16_t modelCount;
};

struct InventoryItemDefinition
{
    uint32_t itemId;
    std::string displayName;
    uint32_t nameTextId;
    uint32_t helpTextId;
    int32_t price;
    int32_t unlockTier;
    int32_t field_0x14;
    int32_t field_0x18;
    int32_t field_0x1c;
    int32_t field_0x20;
    int32_t field_0x24;
    int32_t variantSelector;
    int32_t menuCategory;
    int32_t categorySlot;
    int32_t modelPart;
    int32_t variantSlot;
    int32_t variantBase;
    int32_t variantCount;
    int32_t field_0x44;
    uint32_t variantLabelIndices[10];
    ItemUseEffect useEffect;
    bool consumable;
    bool female;
};
struct InventoryOwnedItem
{
    const InventoryItemDefinition* definition;
    uint16_t variantMask;
    uint16_t quantity;
    uint8_t selectedVariant;
};

uint8_t nextOwnedInventoryVariant(uint16_t variantMask, uint8_t current,
                                  int direction);

uint32_t packInventoryEntry(uint16_t itemId, uint8_t variant,
                            uint16_t quantity);
uint16_t packedInventoryItemId(uint32_t entry);
uint8_t packedInventoryVariant(uint32_t entry);
uint16_t packedInventoryQuantity(uint32_t entry);
bool packedInventoryItemPresent(const uint32_t* entries, size_t count,
                                uint16_t itemId, int variant);
int classifyInventoryCategory(uint16_t itemId);
bool inventoryItemCategoryActionable(uint16_t itemId);
bool inventoryItemMatchesDressCategory(uint16_t itemId, uint8_t variant,
                                       int category, int requiredVariant);
bool commitCharacterArchiveEntries(
    CharacterCustomization& customization,
    const CharacterArchiveGroup* archiveEntries, size_t count);

class GameData
{
public:
    GameData();

    bool load(const char* archiveDirectory);
    bool loaded(void) const;
    const CharacterDatabaseRecord* character(uint16_t characterId) const;
    bool characterCustomization(uint16_t characterId,
                                CharacterCustomization& customization) const;
    const ItemResourceCatalog& itemResources(void) const;
    const InventoryItemDefinition* item(uint32_t itemId, bool female) const;
    void shopItems(int category, int maximumUnlockTier, bool female,
                   std::vector<const InventoryItemDefinition*>& items) const;
    void consumableItems(
        bool female,
        std::vector<const InventoryItemDefinition*>& items) const;
    void collectOwnedInventory(
        const uint32_t* packedEntries, size_t packedEntryCount, bool female,
        std::vector<InventoryOwnedItem>& items) const;
    const InventoryItemDefinition* itemVariant(
        uint32_t itemId, int variantSelector, int variantIndex,
        bool female) const;
    size_t itemDefinitionCount(bool female) const;
    size_t consumableItemCount(bool female) const;
    size_t itemDefinitionCount(void) const;

    size_t textTableSize(GameTextTable table) const;
    bool reloadLocalizedText(uint8_t language);
    bool loadTransientText(GameTextTable table, bool owned);
    uint8_t textLanguage(void) const;
    uint16_t textEncoding(void) const;
    GameTextTable activeTransientTextTable(void) const;
    bool activeTransientTextOwned(void) const;

    const char* localizedText(uint16_t textId,
                              int16_t* byteLength = NULL) const;

    void usedLocalizedTextIds(std::vector<uint16_t>& textIds) const;
    const CharacterStatRecord* characterStats(uint16_t resourceId) const;
    int itemActionCategory(uint32_t itemId, bool female) const;
    bool itemActionable(uint32_t itemId, bool female) const;

private:
    bool loadCharacterDatabase(const char* archivePath);
    bool loadItemResources(const char* archivePath);
    bool loadInventoryDefinitions(const char* archivePath);
    bool parseInventoryCsv(const std::vector<uint8_t>& data, bool female);
    bool loadLocalizedText(const char* archivePath, uint8_t language);
    bool loadTextTable(const char* entryName, uint8_t language,
                       std::vector<LocalizedTextEntry>& entries);
    const std::vector<LocalizedTextEntry>* textDescriptor(
        uint16_t textId) const;

    std::vector<CharacterDatabaseRecord> characters;
    ItemResourceCatalog itemCatalog;
    std::vector<InventoryItemDefinition> inventoryItems;
    std::vector<LocalizedTextEntry> coreText[3];
    std::vector<LocalizedTextEntry> scratchText;
    std::vector<LocalizedTextEntry> ownedText;
    std::string localizedTextArchivePath;
    GameTextTable scratchTextTable;
    GameTextTable ownedTextTable;
    GameTextTable activeTransientTable;
    uint8_t currentTextLanguage;
    uint16_t currentTextEncoding;
    bool activeTransientOwned;
    bool textLoaded;
    bool dataLoaded;
};

#endif
