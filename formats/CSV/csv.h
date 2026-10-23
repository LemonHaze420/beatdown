#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
using CsvBytes = std::vector<std::uint8_t>;
constexpr std::size_t CHARACTER_CSV_COLUMN_COUNT = 43;
constexpr std::size_t CHARACTER_CSV_VALUE_COUNT = 28;
constexpr std::size_t CHARACTER_CSV_MAX_ROWS = 485;
constexpr std::uint32_t PACKED_INVENTORY_ITEM_ID_MASK = 0x00000FFF;
constexpr std::uint32_t PACKED_INVENTORY_VARIANT_MASK = 0x0000F000;
constexpr std::uint32_t PACKED_INVENTORY_RANDOM_MASK = 0x03FF0000;
constexpr std::uint32_t PACKED_INVENTORY_RESERVED_MASK = 0xFC000000;
enum class CsvFieldKind : std::uint8_t
{
    Skip = 0,
    RawU32 = 1,
    RawFloat = 2,
    FixedString = 3,
    ParseS32 = 4,
    ParseFloat = 5
};
struct CsvFieldDescriptor
{
    CsvFieldKind kind = CsvFieldKind::Skip;
    std::uint8_t byteSize = 0;
};
struct CsvRow
{
    bool comment = false;
    std::string commentText;
    std::vector<std::string> fields;
};
struct CsvFile
{
    std::vector<CsvRow> rows;
};
enum class InventoryMenuCategory : std::int32_t
{
    Face = 1,
    Body = 2,
    Accessory = 3,
    Misc = 4
};
enum class InventoryModelPart : std::int32_t
{
    Body = 0,
    Face = 1,
    Hair = 2,
    Eye = 4,
    LeftHand = 5,
    RightHand = 6,
    LeftFoot = 7,
    Necklace = 9,
    LeftBracelet = 10,
    RightBracelet = 11,
    LeftEarring = 12,
    RightEarring = 13,
    Glasses = 14,
    Belt = 15,
    Cap = 16,
    Mask = 17,
    Wear = 18,
    Pants = 19,
    Mouth = 20,
    Weapon = 21
};
struct InventoryItemDefinition
{
    std::uint32_t itemId = 0;
    std::uint32_t itemNameTextId = 0;
    std::uint32_t itemHelpTextId = 0;
    std::int32_t priceOrVariantCount = 0;
    std::int32_t unlockThreshold = 0;
    std::int32_t field0x14 = 0;
    std::int32_t field0x18 = 0;
    std::int32_t field0x1C = 0;
    std::int32_t field0x20 = 0;
    std::int32_t field0x24 = 0;
    std::int32_t variantSelector = 0;
    InventoryMenuCategory menuCategory = static_cast<InventoryMenuCategory>(0);
    std::int32_t categorySlot = 0;
    InventoryModelPart modelPart = static_cast<InventoryModelPart>(0);
    std::int32_t variantSlot = 0;
    std::int32_t variantBase = 0;
    std::int32_t variantCount = 0;
    std::int32_t field0x44 = 0;
    std::array<std::uint32_t, 10> variantLabelIndices{};
};
static_assert(sizeof(InventoryItemDefinition) == 0x70);
struct CharacterCustomizationCsv
{
    CsvFile source;
    std::vector<InventoryItemDefinition> definitions;
    std::vector<std::size_t> definitionRows;
    std::vector<std::size_t> categoryDefinitions;
};
extern const std::array<CsvFieldDescriptor, CHARACTER_CSV_COLUMN_COUNT>
    CHARACTER_CUSTOMIZATION_CSV_FIELDS;
bool readCsv(const CsvBytes& data, CsvFile& csv);
bool writeCsv(const CsvFile& csv, CsvBytes& data);
bool readCharacterCustomizationCsv(const CsvBytes& data,
                                   CharacterCustomizationCsv& csv);
const char* characterCustomizationColumnName(std::size_t column);
