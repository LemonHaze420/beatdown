#include "game_data.h"
#include "character_records.h"

#include "xaf_runtime.h"

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace
{

const size_t CHARACTER_RECORD_COUNT = 189;
const size_t CHARACTER_RECORD_SIZE = 0x498;
const uint32_t RW_CHUNK_IMAGE = 0x18;
const uint32_t RW_CHUNK_CLUMP = 0x10;

const uint16_t HOST_TEXT_IDS[] =
{
    5901,
    9000, 9001, 9002, 9003, 9004, 9005, 9006, 9007, 9008, 9009,
    9010, 9011, 9012, 9013, 9014, 9015, 9016, 9017, 9018, 9019,
    9020, 9021, 9022, 9023, 9024, 9025, 9026, 9027, 9028, 9029,
    9030, 9031, 9032, 9033, 9034, 9035, 9036, 9037, 9038, 9039,
    9040, 9041, 9042, 9043, 9044, 9045, 9046, 9047, 9048, 9049,
    9050, 9051, 9052, 9053, 9054, 9055, 9056, 9057, 9058, 9059,
    9060, 9061, 9062, 9063, 9064, 9080, 9081,
    10402, 10403, 10406, 10407, 10470, 10471, 10473, 10474, 10475,
    10476, 10582, 10603, 10609, 10611, 10613, 10904, 10905, 10906,
    11023, 11054, 11069, 11108, 11109,
    18001, 18002, 18003, 18004, 18005, 18007, 18008, 18009, 18010,
    18011, 18012, 18013, 18014, 18015, 18016, 18017, 18019, 18020,
    18021, 18022, 18023, 18024, 18027, 18028, 18029, 18030, 18031,
    18032, 18033, 18034, 18036, 18037, 18038, 18039, 18040, 18041,
    18042, 18044, 18045, 18046, 18047, 18048, 18049, 18050, 18051,
    18052,
    30001, 30002, 30004, 30005, 30207, 30232, 30241, 30270, 30821,
    31140,
    39506, 39507, 39508, 39509, 39510, 39513, 52116, 52121, 52140,
    52295, 52296, 52297, 52298, 52299, 53009, 53021
};
const size_t HOST_TEXT_ID_COUNT =
    sizeof(HOST_TEXT_IDS) / sizeof(HOST_TEXT_IDS[0]);

uint16_t read16(const uint8_t* data)
{
    uint16_t value;
    memcpy(&value, data, sizeof(value));
    return value;
}

uint32_t read32(const uint8_t* data)
{
    uint32_t value;
    memcpy(&value, data, sizeof(value));
    return value;
}

void makeArchivePath(char* path, size_t size, const char* directory,
                     const char* fileName)
{
    if(directory == NULL || directory[0] == 0)
        snprintf(path, size, "%s", fileName);
    else
    {
        const size_t length = strlen(directory);
        const char separator = directory[length - 1] == '/' ||
            directory[length - 1] == '\\' ? 0 : '/';
        if(separator == 0)
            snprintf(path, size, "%s%s", directory, fileName);
        else
            snprintf(path, size, "%s%c%s", directory, separator, fileName);
    }
}

bool parseInteger(const std::string& text, int32_t& value)
{
    if(text.empty())
    {
        value = 0;
        return true;
    }
    char* end = NULL;
    const long parsed = strtol(text.c_str(), &end, 10);
    if(end == text.c_str() || *end != 0)
        return false;
    value = static_cast<int32_t>(parsed);
    return true;
}

bool parseCsvLine(const char* begin, const char* end,
                  std::vector<std::string>& fields)
{
    fields.clear();
    const char* cursor = begin;
    while(cursor <= end)
    {
        std::string field;
        if(cursor < end && *cursor == '"')
        {
            ++cursor;
            while(cursor < end)
            {
                if(*cursor != '"')
                {
                    field.push_back(*cursor++);
                    continue;
                }
                ++cursor;
                if(cursor < end && *cursor == '"')
                {
                    field.push_back('"');
                    ++cursor;
                    continue;
                }
                break;
            }
            if(cursor < end && *cursor != ',')
                return false;
        }
        else
        {
            while(cursor < end && *cursor != ',')
                field.push_back(*cursor++);
        }
        fields.push_back(field);
        if(cursor >= end)
            break;
        ++cursor;
    }
    return true;
}

void appendUtf8(std::string& text, uint32_t codepoint)
{
    if(codepoint < 0x80)
        text.push_back(static_cast<char>(codepoint));
    else if(codepoint < 0x800)
    {
        text.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        text.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
    else if(codepoint < 0x10000)
    {
        text.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        text.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        text.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
    else
    {
        text.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        text.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        text.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        text.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
}

bool isItemCategoryActionable(uint32_t itemId)
{
    const uint32_t category = itemId / 100;

    return category == 1 || category == 2 || category == 3 ||
        category == 4 || category == 6 ||
        (category >= 0x15 && category <= 0x1e);
}

bool itemMatchesGeneratedRecord(const InventoryItemDefinition& item,
                                const ItemTableRecord& record)
{
    if(item.itemId != record.itemId ||
       item.nameTextId != record.nameTextId ||
       item.helpTextId != record.helpTextId ||
       item.price != record.priceOrVariantCount ||
       item.unlockTier != record.unlockThreshold ||
       item.field_0x14 != record.field_0x14 ||
       item.field_0x18 != record.field_0x18 ||
       item.field_0x1c != record.field_0x1c ||
       item.field_0x20 != record.field_0x20 ||
       item.field_0x24 != record.field_0x24 ||
       item.variantSelector != record.variantSelector ||
       item.menuCategory != record.menuCategory ||
       item.categorySlot != record.categorySlot ||
       item.modelPart != record.modelPart ||
       item.variantSlot != record.variantSlot ||
       item.variantBase != record.variantBase ||
       item.variantCount != record.variantCount ||
       item.field_0x44 != record.field_0x44 ||
       item.female != record.female)
        return false;
    for(int index = 0; index < ITEM_VARIANT_LABEL_COUNT; ++index)
        if(item.variantLabelIndices[index] !=
           record.variantLabelIndices[index])
            return false;
    return true;
}

}
uint32_t packInventoryEntry(uint16_t itemId, uint8_t variant,
                            uint16_t quantity)
{

    return (static_cast<uint32_t>(itemId) & ITEM_PACKED_ID_MASK) |
        ((static_cast<uint32_t>(variant) & 0x0fu) << 12) |
        ((static_cast<uint32_t>(quantity) & 0x03ffu) << 16);
}

uint16_t packedInventoryItemId(uint32_t entry)
{
    return static_cast<uint16_t>(entry & ITEM_PACKED_ID_MASK);
}

uint8_t packedInventoryVariant(uint32_t entry)
{
    return static_cast<uint8_t>((entry & ITEM_PACKED_VARIANT_MASK) >> 12);
}

uint16_t packedInventoryQuantity(uint32_t entry)
{
    return static_cast<uint16_t>(
        (entry & ITEM_PACKED_QUANTITY_MASK) >> 16);
}

bool packedInventoryItemPresent(const uint32_t* entries, size_t count,
                                uint16_t itemId, int variant)
{
    if(entries == NULL)
        return false;
    for(size_t index = 0; index < count; ++index)
        if(packedInventoryItemId(entries[index]) == itemId &&
           (variant < 0 ||
            packedInventoryVariant(entries[index]) == variant))
            return true;
    return false;
}

int classifyInventoryCategory(uint16_t itemId)
{

    switch(itemId / 100)
    {
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x18:
    case 0x1a:
    case 0x1b:
    case 0x1c:
    case 0x1d:
    case 0x1e:
        return INVENTORY_CATEGORY_GROUP_BODY;
    case 7:
    case 0x19:
        return INVENTORY_CATEGORY_GROUP_CLOTHING;
    case 8:
    case 9:
    case 0x0a:
    case 0x0b:
    case 0x0c:
    case 0x0e:
    case 0x0f:
    case 0x10:
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14:
    case 0x1f:
    case 0x20:
    case 0x25:
        return INVENTORY_CATEGORY_GROUP_ACCESSORY;
    case INVENTORY_CATEGORY_SUPPLIES:
        return INVENTORY_CATEGORY_GROUP_CONSUMABLE;
    default:
        return INVENTORY_CATEGORY_GROUP_OTHER;
    }
}
bool inventoryItemCategoryActionable(uint16_t itemId)
{
    return isItemCategoryActionable(itemId);
}

bool inventoryItemMatchesDressCategory(uint16_t itemId, uint8_t variant,
                                       int category, int requiredVariant)
{

    if(category == INVENTORY_CATEGORY_VM_SPECIAL_RANGE)
        return itemId >= 0x09e8 && itemId <= 0x09fc &&
            (requiredVariant < 0 || variant == requiredVariant);
    if(category == INVENTORY_CATEGORY_VM_BODY_MARKS)
    {
        const int itemCategory = itemId / 100;
        return (itemCategory == 1 || itemCategory == 3 ||
                itemCategory == 6 ||
                (itemCategory >= 0x15 && itemCategory <= 0x18)) &&
            itemId % 100 != 1;
    }
    return (category == 0 || itemId / 100 == category) &&
        (requiredVariant < 0 || variant == requiredVariant);
}

bool commitCharacterArchiveEntries(
    CharacterCustomization& customization,
    const CharacterArchiveGroup* archiveEntries, size_t count)
{
    if(archiveEntries == NULL ||
       count != CHARACTER_ARCHIVE_ENTRY_COUNT)
        return false;

    memcpy(customization.archiveEntries, archiveEntries,
           sizeof(customization.archiveEntries));
    return true;
}
uint8_t nextOwnedInventoryVariant(uint16_t variantMask, uint8_t current,
                                  int direction)
{
    if(variantMask == 0)
        return current;
    if(direction == 0 &&
       (variantMask & (1u << (current & 0x0f))) != 0)
        return static_cast<uint8_t>(current & 0x0f);
    const int step = direction < 0 ? -1 : 1;
    for(int offset = 1; offset <= 16; ++offset)
    {
        const uint8_t candidate = static_cast<uint8_t>(
            (static_cast<int>(current) + step * offset) & 0x0f);
        if((variantMask & (1u << candidate)) != 0)
            return candidate;
    }
    return current;
}

std::string normalizeDisplayText(const uint8_t* data, size_t length)
{
    std::string text;
    text.reserve(length);
    size_t offset = 0;
    while(offset < length && data[offset] != 0)
    {
        if(data[offset] == '\\' && offset + 1 < length)
        {
            if(data[offset + 1] == 'n')
            {
                text.push_back('\n');
                offset += 2;
                continue;
            }
            if(data[offset + 1] == 't')
            {
                text.push_back('\t');
                offset += 2;
                continue;
            }
        }

        uint32_t codepoint = data[offset++];
        int continuationCount = 0;
        if((codepoint & 0xe0) == 0xc0)
        {
            codepoint &= 0x1f;
            continuationCount = 1;
        }
        else if((codepoint & 0xf0) == 0xe0)
        {
            codepoint &= 0x0f;
            continuationCount = 2;
        }
        else if((codepoint & 0xf8) == 0xf0)
        {
            codepoint &= 0x07;
            continuationCount = 3;
        }
        else if(codepoint >= 0x80)
        {
            text.push_back('?');
            continue;
        }

        bool valid = offset + continuationCount <= length;
        for(int i = 0; valid && i < continuationCount; ++i)
        {
            const uint8_t continuation = data[offset++];
            if((continuation & 0xc0) != 0x80)
            {
                valid = false;
                break;
            }
            codepoint = (codepoint << 6) | (continuation & 0x3f);
        }
        if(!valid)
        {

            text.push_back('?');
            continue;
        }

        if(codepoint == 0x3000 || codepoint == 0x00a0)
            codepoint = ' ';
        else if(codepoint >= 0xff01 && codepoint <= 0xff5e)
            codepoint -= 0xfee0;
        else if(codepoint == 0x2019)
            codepoint = '\'';
        else if(codepoint == 0x201c || codepoint == 0x201d)
            codepoint = '"';

        else if(codepoint == 0xffe5)
            codepoint = '\\';

        else if(codepoint == 0x2018)
            codepoint = '`';

        else if(codepoint >= 0x0150 && codepoint <= 0x0153)
        {
            text.push_back(static_cast<char>(codepoint - 0x00d0));
            continue;
        }

        else if(codepoint >= 0x0800)
        {
            text.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
            continue;
        }
        appendUtf8(text, codepoint);
    }
    return text;
}

bool gameTextEncoding(uint8_t language, uint16_t& encoding)
{
    static const uint16_t ENCODINGS[GAME_LANGUAGE_COUNT] =
    {
        0, 1, 4, 2, 3, 5
    };
    if(language >= GAME_LANGUAGE_COUNT)
        return false;
    encoding = ENCODINGS[language];
    return true;
}

bool
parseLocalizedTextTable(const std::vector<uint8_t>& data, uint8_t language,
                        std::vector<LocalizedTextEntry>& entries)
{
    entries.clear();
    if(data.size() < 8)
        return false;

    uint16_t expectedEncoding = 0;
    const uint32_t dataSize = read32(data.data());
    const uint16_t entryCount = read16(data.data() + 4);
    const uint16_t encoding = read16(data.data() + 6);
    const size_t entryBytes = static_cast<size_t>(entryCount) * 8;
    const size_t textOffset = 8 + entryBytes;
    if(!gameTextEncoding(language, expectedEncoding) ||
       encoding != expectedEncoding || textOffset > data.size() ||
       dataSize != data.size() - textOffset)
        return false;

    entries.reserve(entryCount);
    const uint8_t* textData = data.data() + textOffset;
    uint16_t previousId = 0;
    for(uint16_t i = 0; i < entryCount; ++i)
    {
        const uint8_t* serialized = data.data() + 8 + i * 8;
        const uint16_t textId = read16(serialized);
        const uint16_t byteLength = read16(serialized + 2);
        const uint32_t dataOffset = read32(serialized + 4);
        if((i != 0 && textId <= previousId) || dataOffset > dataSize ||
           byteLength > dataSize - dataOffset ||
           (byteLength != 0 &&
            textData[dataOffset + byteLength - 1] != 0))
        {
            entries.clear();
            return false;
        }

        LocalizedTextEntry entry;
        entry.textId = textId;
        entry.byteLength = byteLength;
        if(byteLength != 0)
            entry.text = normalizeDisplayText(
                textData + dataOffset, byteLength);
        entries.push_back(entry);
        previousId = textId;
    }
    return true;
}

const char* const GAME_TEXT_ARCHIVE_PREFIXES[GAME_TEXT_TABLE_COUNT] =
{
    "MAIN", "WAZA", "ITEM", "STAFF", "NEGO", "SUB", "MCARD", "PHONE"
};

GameData::GameData()
    : scratchTextTable(GAME_TEXT_TABLE_COUNT),
      ownedTextTable(GAME_TEXT_TABLE_COUNT),
      activeTransientTable(GAME_TEXT_TABLE_COUNT),
      currentTextLanguage(GAME_LANGUAGE_COUNT),
      currentTextEncoding(UINT16_MAX),
      activeTransientOwned(false),
      textLoaded(false),
      dataLoaded(false)
{
    memset(&itemCatalog, 0, sizeof(itemCatalog));
}

bool
GameData::load(const char* archiveDirectory)
{
    char path[512];
    makeArchivePath(path, sizeof(path), archiveDirectory, "CDATA.XAF");
    const bool characterOk = loadCharacterDatabase(path);
    const bool textOk = loadLocalizedText(path, GAME_LANGUAGE_ENGLISH);

    makeArchivePath(path, sizeof(path), archiveDirectory, "DATA.XAF");
    const bool itemOk = loadItemResources(path);

    makeArchivePath(path, sizeof(path), archiveDirectory, "FSCENE.XAF");
    const bool definitionsOk = loadInventoryDefinitions(path);

    dataLoaded = characterOk && textOk && itemOk && definitionsOk;
    return dataLoaded;
}

bool
GameData::loadCharacterDatabase(const char* archivePath)
{
    std::vector<uint8_t> data;
    if(!readXafEntry(archivePath, "CHARA.BIN", data))
        return false;
    if(data.size() != CHARACTER_RECORD_COUNT * CHARACTER_RECORD_SIZE)
    {
        printf("ERROR: CHARA.BIN has invalid size %u.\n",
               static_cast<unsigned>(data.size()));
        return false;
    }

    characters.resize(CHARACTER_RECORD_COUNT);
    memcpy(characters.data(), data.data(), data.size());
    return true;
}

bool
GameData::loadItemResources(const char* archivePath)
{
    std::vector<uint8_t> data;
    if(!readXafEntry(archivePath, "..\\..\\D\\ITEM\\ITEM.BIN", data))
        return false;

    uint32_t offset = 0;
    uint16_t imageCount = 0;
    uint16_t clumpCount = 0;
    while(offset < data.size())
    {
        if(data.size() - offset < 12)
            return false;
        const uint32_t type = read32(data.data() + offset);
        const uint32_t length = read32(data.data() + offset + 4);
        const uint64_t end = static_cast<uint64_t>(offset) + 12 + length;
        if(end > data.size())
            return false;
        if(type == RW_CHUNK_IMAGE)
            ++imageCount;
        else if(type == RW_CHUNK_CLUMP)
            ++clumpCount;
        else
            return false;
        offset = static_cast<uint32_t>(end);
    }

    if(imageCount != 8 || clumpCount != 8 || offset != data.size())
    {
        printf("ERROR: ITEM.BIN resource table is incomplete.\n");
        return false;
    }
    itemCatalog.iconCount = imageCount;
    itemCatalog.modelCount = clumpCount;
    return true;
}

bool
GameData::loadInventoryDefinitions(const char* archivePath)
{
    inventoryItems.clear();
    std::vector<uint8_t> data;
    if(!readXafEntry(archivePath, "MAN.CSV", data) ||
       !parseInventoryCsv(data, false))
        return false;
    if(!readXafEntry(archivePath, "WOMAN.CSV", data) ||
       !parseInventoryCsv(data, true))
    {
        inventoryItems.clear();
        return false;
    }
    return inventoryItems.size() ==
        static_cast<size_t>(ITEM_MAN_RECORD_COUNT +
                            ITEM_WOMAN_RECORD_COUNT);
}

bool
GameData::loadTextTable(const char* entryName, uint8_t language,
                        std::vector<LocalizedTextEntry>& entries)
{
    std::vector<uint8_t> data;
    if(localizedTextArchivePath.empty() ||
       !readXafEntry(localizedTextArchivePath.c_str(), entryName, data))
    {
        entries.clear();
        return false;
    }
    return parseLocalizedTextTable(data, language, entries);
}

bool
GameData::loadLocalizedText(const char* archivePath, uint8_t language)
{
    if(archivePath == NULL)
        return false;
    localizedTextArchivePath = archivePath;
    currentTextLanguage = GAME_LANGUAGE_COUNT;
    currentTextEncoding = UINT16_MAX;
    textLoaded = false;
    return reloadLocalizedText(language);
}

bool
GameData::reloadLocalizedText(uint8_t language)
{
    static const char* LANGUAGE_CODES[GAME_LANGUAGE_COUNT] =
    {
        "J", "E", "F", "G", "S", "I"
    };

    uint16_t encoding = 0;
    if(localizedTextArchivePath.empty() ||
       !gameTextEncoding(language, encoding))
        return false;
    if(textLoaded && currentTextLanguage == language &&
       currentTextEncoding == encoding)
        return true;

    for(int slot = 0; slot < 3; ++slot)
        coreText[slot].clear();
    scratchText.clear();
    ownedText.clear();
    scratchTextTable = GAME_TEXT_TABLE_COUNT;
    ownedTextTable = GAME_TEXT_TABLE_COUNT;
    activeTransientTable = GAME_TEXT_TABLE_COUNT;
    activeTransientOwned = false;
    currentTextLanguage = language;
    currentTextEncoding = encoding;
    textLoaded = false;

    for(int slot = 0; slot < 3; ++slot)
    {
        char entryName[32];
        snprintf(entryName, sizeof(entryName), "%s_%s.BIN",
                 GAME_TEXT_ARCHIVE_PREFIXES[slot],
                 LANGUAGE_CODES[language]);
        if(!loadTextTable(entryName, language, coreText[slot]))
        {
            for(int clearSlot = 0; clearSlot < 3; ++clearSlot)
                coreText[clearSlot].clear();
            return false;
        }
    }
    textLoaded = true;
    return true;
}

bool
GameData::loadTransientText(GameTextTable table, bool owned)
{
    static const char* LANGUAGE_CODES[GAME_LANGUAGE_COUNT] =
    {
        "J", "E", "F", "G", "S", "I"
    };
    if(!textLoaded || table < GAME_TEXT_TABLE_STAFF ||
       table >= GAME_TEXT_TABLE_COUNT ||
       currentTextLanguage >= GAME_LANGUAGE_COUNT)
        return false;

    char entryName[32];
    snprintf(entryName, sizeof(entryName), "%s_%s.BIN",
             GAME_TEXT_ARCHIVE_PREFIXES[table],
             LANGUAGE_CODES[currentTextLanguage]);
    std::vector<LocalizedTextEntry> replacement;
    if(!loadTextTable(entryName, currentTextLanguage, replacement))
        return false;

    if(owned)
    {
        ownedText.clear();
        ownedText.swap(replacement);
        ownedTextTable = table;
    }
    else
    {
        scratchText.swap(replacement);
        scratchTextTable = table;
    }
    activeTransientTable = table;
    activeTransientOwned = owned;
    return true;
}

bool
GameData::parseInventoryCsv(const std::vector<uint8_t>& data, bool female)
{
    const char* cursor = reinterpret_cast<const char*>(data.data());
    const char* end = cursor + data.size();
    std::vector<std::string> fields;
    size_t parsedCount = 0;
    while(cursor < end)
    {
        const char* line = cursor;
        while(cursor < end && *cursor != '\r' && *cursor != '\n')
            ++cursor;
        const char* lineEnd = cursor;
        while(cursor < end && (*cursor == '\r' || *cursor == '\n'))
            ++cursor;
        if(line == lineEnd || *line == '#')
            continue;
        if(!parseCsvLine(line, lineEnd, fields) || fields.size() != 43)
            return false;

        int32_t values[28] = {};
        const int columns[28] =
        {
            0, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 16, 18,
            20, 21, 22, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41
        };
        for(int i = 0; i < 28; ++i)
        {
            if(columns[i] < static_cast<int>(fields.size()) &&
               !parseInteger(fields[columns[i]], values[i]))
                return false;
        }

        InventoryItemDefinition item = {};
        item.itemId = static_cast<uint32_t>(values[0]);
        item.displayName = fields[1];
        item.nameTextId = static_cast<uint32_t>(values[1]);
        item.helpTextId = static_cast<uint32_t>(values[2]);
        item.price = values[3];
        item.unlockTier = values[4];
        item.field_0x14 = values[5];
        item.field_0x18 = values[6];
        item.field_0x1c = values[7];
        item.field_0x20 = values[8];
        item.field_0x24 = values[9];
        item.variantSelector = values[10];
        item.menuCategory = values[11];
        item.categorySlot = values[12];
        item.modelPart = values[13];
        item.variantSlot = values[14];
        item.variantBase = values[15];
        item.variantCount = values[16];
        item.field_0x44 = values[17];
        for(int i = 0; i < ITEM_VARIANT_LABEL_COUNT; ++i)
            item.variantLabelIndices[i] =
                static_cast<uint32_t>(values[18 + i]);
        item.female = female;

        const ItemTableRecord* generated = findItemTableRecord(
            static_cast<uint16_t>(item.itemId), female);
        if(generated == NULL ||
           !itemMatchesGeneratedRecord(item, *generated))
        {
            printf("ERROR: %s.CSV item %u differs from ITEM corpus.\n",
                   female ? "WOMAN" : "MAN",
                   static_cast<unsigned>(item.itemId));
            return false;
        }
        item.useEffect = generated->useEffect;
        item.consumable = generated->consumable;
        inventoryItems.push_back(item);
        ++parsedCount;
    }
    return parsedCount == static_cast<size_t>(
        female ? ITEM_WOMAN_RECORD_COUNT : ITEM_MAN_RECORD_COUNT);
}

bool GameData::loaded(void) const { return dataLoaded && textLoaded; }

const CharacterDatabaseRecord*
GameData::character(uint16_t characterId) const
{
    if(characterId >= characters.size())
        return NULL;
    return &characters[characterId];
}

bool
GameData::characterCustomization(uint16_t characterId,
                                 CharacterCustomization& customization) const
{
    const CharacterDatabaseRecord* record = character(characterId);
    if(record == NULL)
        return false;
    customization.bodyModelIndex =
        static_cast<uint8_t>(record->variantWords[0]);
    customization.costumeVariant =
        static_cast<uint8_t>(record->variantWords[1]);
    customization.genderSelector =
        static_cast<uint8_t>(record->variantWords[2]);
    customization.variantFlags =
        record->variantWords[6] == 0 ? 0 : 1;
    memcpy(customization.archiveEntries, record->groups,
           sizeof(customization.archiveEntries));
    return true;
}

const ItemResourceCatalog& GameData::itemResources(void) const
{
    return itemCatalog;
}

const InventoryItemDefinition*
GameData::item(uint32_t itemId, bool female) const
{
    for(size_t i = 0; i < inventoryItems.size(); ++i)
    {
        const InventoryItemDefinition& definition = inventoryItems[i];
        if(definition.itemId == itemId &&
           definition.female == female)
            return &definition;
    }
    return NULL;
}

void
GameData::shopItems(int category, int maximumUnlockTier, bool female,
                    std::vector<const InventoryItemDefinition*>& items) const
{
    items.clear();
    for(size_t i = 0; i < inventoryItems.size(); ++i)
    {
        const InventoryItemDefinition& definition = inventoryItems[i];
        if(definition.itemId < 100 || definition.female != female ||
           definition.unlockTier > maximumUnlockTier ||
           (category >= 0 &&
            static_cast<int>(definition.itemId / 100) != category))
            continue;

        bool duplicate = false;
        for(size_t j = 0; j < items.size(); ++j)
        {
            if(items[j]->itemId == definition.itemId)
            {
                duplicate = true;
                break;
            }
        }
        if(!duplicate)
            items.push_back(&definition);
    }
}
void
GameData::consumableItems(
    bool female,
    std::vector<const InventoryItemDefinition*>& items) const
{
    items.clear();
    for(size_t index = 0; index < inventoryItems.size(); ++index)
    {
        const InventoryItemDefinition& definition = inventoryItems[index];
        if(definition.female == female && definition.consumable)
            items.push_back(&definition);
    }
}

void
GameData::collectOwnedInventory(
    const uint32_t* packedEntries, size_t packedEntryCount, bool female,
    std::vector<InventoryOwnedItem>& items) const
{
    items.clear();
    if(packedEntries == NULL)
        return;
    for(size_t packedIndex = 0;
        packedIndex < packedEntryCount; ++packedIndex)
    {
        const uint32_t packed = packedEntries[packedIndex];
        const uint16_t itemId = packedInventoryItemId(packed);
        const uint8_t variant = packedInventoryVariant(packed);
        const InventoryItemDefinition* definition = item(itemId, female);
        if(definition == NULL)
            continue;

        size_t itemIndex = 0;
        while(itemIndex < items.size() &&
              items[itemIndex].definition != definition)
            ++itemIndex;
        if(itemIndex == items.size())
        {
            InventoryOwnedItem owned = {};
            owned.definition = definition;
            owned.selectedVariant = variant;
            items.push_back(owned);
        }
        InventoryOwnedItem& owned = items[itemIndex];
        owned.variantMask = static_cast<uint16_t>(
            owned.variantMask | (1u << variant));
        const uint32_t quantity = static_cast<uint32_t>(owned.quantity) +
            packedInventoryQuantity(packed);
        owned.quantity = static_cast<uint16_t>(
            quantity > UINT16_MAX ? UINT16_MAX : quantity);
    }
}

const InventoryItemDefinition*
GameData::itemVariant(uint32_t itemId, int variantSelector, int variantIndex,
                      bool female) const
{
    size_t baseIndex = 0;
    while(baseIndex < inventoryItems.size() &&
          (inventoryItems[baseIndex].itemId != itemId ||
           inventoryItems[baseIndex].female != female))
        ++baseIndex;
    if(baseIndex == inventoryItems.size())
        return NULL;

    const InventoryItemDefinition& base = inventoryItems[baseIndex];
    for(int childIndex = 0; childIndex < base.price; ++childIndex)
    {
        const size_t index = baseIndex + 1 +
            static_cast<size_t>(childIndex);
        if(index >= inventoryItems.size())
            break;
        const InventoryItemDefinition& child = inventoryItems[index];
        if(child.female != female)
            break;
        const int selector = child.variantSelector;
        if(selector != variantSelector && selector >= 0)
            continue;
        if(selector == -3)
        {
            if(child.variantBase == variantSelector)
                return &child;
        }
        else if(selector == -2)
        {
            if(child.variantBase == variantIndex)
                return &child;
        }
        else if(child.variantCount < 1 ||
                (child.variantBase <= variantIndex &&
                 variantIndex < child.variantBase + child.variantCount))
            return &child;
    }
    return NULL;
}

size_t GameData::itemDefinitionCount(bool female) const
{
    size_t count = 0;
    for(size_t index = 0; index < inventoryItems.size(); ++index)
        if(inventoryItems[index].female == female)
            ++count;
    return count;
}

size_t GameData::consumableItemCount(bool female) const
{
    size_t count = 0;
    for(size_t index = 0; index < inventoryItems.size(); ++index)
        if(inventoryItems[index].female == female &&
           inventoryItems[index].consumable)
            ++count;
    return count;
}

size_t GameData::itemDefinitionCount(void) const
{
    return inventoryItems.size();
}

const char*
lookupLocalizedTextTable(const std::vector<LocalizedTextEntry>& entries,
                         uint16_t textId, int16_t* byteLength)
{
    if(byteLength != NULL)
        *byteLength = -1;
    size_t first = 0;
    size_t count = entries.size();
    while(count != 0)
    {
        const size_t step = count / 2;
        const size_t index = first + step;
        if(entries[index].textId < textId)
        {
            first = index + 1;
            count -= step + 1;
        }
        else
            count = step;
    }
    if(first >= entries.size() || entries[first].textId != textId ||
       entries[first].byteLength == 0)
        return NULL;
    if(byteLength != NULL)
        *byteLength = static_cast<int16_t>(entries[first].byteLength);
    return entries[first].text.c_str();
}

GameTextDescriptor gameTextDescriptorForId(uint16_t textId)
{
    if(textId >= 40000 && textId < 50000)
        return GAME_TEXT_DESCRIPTOR_ITEM;
    if(textId >= 12000 && textId < 18000)
        return GAME_TEXT_DESCRIPTOR_WAZA;
    if((textId >= 5000 && textId < 9000) ||
       (textId >= 9100 && textId < 9400) ||
       (textId >= 25000 && textId < 27000) ||
       (textId >= 29000 && textId < 30000) ||
       (textId >= 35000 && textId < 39499) ||
       (textId >= 61000 && textId < 64000))
        return GAME_TEXT_DESCRIPTOR_TRANSIENT;
    return GAME_TEXT_DESCRIPTOR_MAIN;
}

const std::vector<LocalizedTextEntry>*
GameData::textDescriptor(uint16_t textId) const
{
    switch(gameTextDescriptorForId(textId))
    {
    case GAME_TEXT_DESCRIPTOR_ITEM:
        return &coreText[GAME_TEXT_TABLE_ITEM];
    case GAME_TEXT_DESCRIPTOR_WAZA:
        return &coreText[GAME_TEXT_TABLE_WAZA];
    case GAME_TEXT_DESCRIPTOR_TRANSIENT:
        if(activeTransientTable == GAME_TEXT_TABLE_COUNT)
            return NULL;
        if(activeTransientOwned)
            return ownedTextTable == activeTransientTable ?
                &ownedText : NULL;
        return scratchTextTable == activeTransientTable ?
            &scratchText : NULL;
    default:
        return &coreText[GAME_TEXT_TABLE_MAIN];
    }
}

size_t
GameData::textTableSize(GameTextTable table) const
{
    if(table >= GAME_TEXT_TABLE_MAIN && table <= GAME_TEXT_TABLE_WAZA)
        return coreText[table].size();
    if(table != activeTransientTable)
        return 0;
    return activeTransientOwned ? ownedText.size() : scratchText.size();
}

uint8_t GameData::textLanguage(void) const { return currentTextLanguage; }
uint16_t GameData::textEncoding(void) const { return currentTextEncoding; }
GameTextTable
GameData::activeTransientTextTable(void) const
{
    return activeTransientTable;
}
bool
GameData::activeTransientTextOwned(void) const
{
    return activeTransientTable != GAME_TEXT_TABLE_COUNT &&
        activeTransientOwned;
}

const char*
GameData::localizedText(uint16_t textId, int16_t* byteLength) const
{
    if(byteLength != NULL)
        *byteLength = -1;
    if(!textLoaded)
        return NULL;
    const std::vector<LocalizedTextEntry>* descriptor =
        textDescriptor(textId);
    return descriptor != NULL ?
        lookupLocalizedTextTable(*descriptor, textId, byteLength) : NULL;
}

void
GameData::usedLocalizedTextIds(std::vector<uint16_t>& textIds) const
{
    textIds.clear();
    textIds.reserve(HOST_TEXT_ID_COUNT + inventoryItems.size() * 2);
    textIds.insert(textIds.end(), HOST_TEXT_IDS,
                   HOST_TEXT_IDS + HOST_TEXT_ID_COUNT);

    for(size_t i = 0; i < inventoryItems.size(); ++i)
    {
        const InventoryItemDefinition& item = inventoryItems[i];
        if(item.nameTextId != 0 && item.nameTextId <= UINT16_MAX)
            textIds.push_back(static_cast<uint16_t>(item.nameTextId));
        if(item.helpTextId != 0 && item.helpTextId <= UINT16_MAX)
            textIds.push_back(static_cast<uint16_t>(item.helpTextId));
    }

    std::sort(textIds.begin(), textIds.end());
    textIds.erase(std::unique(textIds.begin(), textIds.end()), textIds.end());
}

const CharacterStatRecord*
GameData::characterStats(uint16_t resourceId) const
{

    static CharacterStatRecord cache[CHARACTER_DB_RECORD_COUNT];
    static bool built = false;
    if(!built)
    {
        for(int i = 0; i < CHARACTER_DB_RECORD_COUNT; ++i)
        {
            const CharacterRecord& record = CHARACTER_RECORDS[i];
            CharacterStatRecord& stats = cache[i];
            stats.resourceId = static_cast<uint16_t>(record.resourceId);
            stats.combatStyleId =
                static_cast<uint16_t>(record.combatStyleId);
            stats.aiProfileId = record.aiProfileId;
            stats.groupId = record.groupId;
            stats.level = static_cast<uint8_t>(record.level);
            stats.attackPowerClass = record.attackPowerClass;
            stats.maxHealth = record.maxHealth;
            stats.initialPrideGauge = record.initialPrideGauge;
            stats.initialDefeatCount = record.initialDefeatCount;
            stats.initialCash = record.cash;
        }
        built = true;
    }
    for(int i = 0; i < CHARACTER_DB_RECORD_COUNT; ++i)
    {
        if(cache[i].resourceId == resourceId)
            return &cache[i];
    }
    return NULL;
}

int
GameData::itemActionCategory(uint32_t itemId, bool female) const
{
    const InventoryItemDefinition* definition = item(itemId, female);
    if(definition == NULL)
        return INVENTORY_ACTION_NONE;
    if(definition->menuCategory < 2)
        return INVENTORY_ACTIONABLE;
    return isItemCategoryActionable(itemId) ?
        INVENTORY_ACTIONABLE : INVENTORY_ACTION_NONE;
}

bool
GameData::itemActionable(uint32_t itemId, bool female) const
{
    return itemActionCategory(itemId, female) == INVENTORY_ACTIONABLE;
}
