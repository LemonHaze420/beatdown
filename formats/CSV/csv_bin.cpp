#include <array>
#include <cinttypes>
#include <cstdio>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include "csv.h"
const std::array<CsvFieldDescriptor, CHARACTER_CSV_COLUMN_COUNT>
    CHARACTER_CUSTOMIZATION_CSV_FIELDS = {{
        {CsvFieldKind::ParseS32, 4}, {CsvFieldKind::Skip, 0},
        {CsvFieldKind::ParseS32, 4}, {CsvFieldKind::ParseS32, 4},
        {CsvFieldKind::ParseS32, 4}, {CsvFieldKind::ParseS32, 4},
        {CsvFieldKind::ParseS32, 4}, {CsvFieldKind::ParseS32, 4},
        {CsvFieldKind::ParseS32, 4}, {CsvFieldKind::ParseS32, 4},
        {CsvFieldKind::ParseS32, 4}, {CsvFieldKind::ParseS32, 4},
        {CsvFieldKind::ParseS32, 4}, {CsvFieldKind::Skip, 0},
        {CsvFieldKind::ParseS32, 4}, {CsvFieldKind::Skip, 0},
        {CsvFieldKind::ParseS32, 4}, {CsvFieldKind::Skip, 0},
        {CsvFieldKind::ParseS32, 4}, {CsvFieldKind::Skip, 0},
        {CsvFieldKind::ParseS32, 4}, {CsvFieldKind::ParseS32, 4},
        {CsvFieldKind::ParseS32, 4}, {CsvFieldKind::ParseS32, 4},
        {CsvFieldKind::Skip, 0}, {CsvFieldKind::ParseS32, 4},
        {CsvFieldKind::Skip, 0}, {CsvFieldKind::ParseS32, 4},
        {CsvFieldKind::Skip, 0}, {CsvFieldKind::ParseS32, 4},
        {CsvFieldKind::Skip, 0}, {CsvFieldKind::ParseS32, 4},
        {CsvFieldKind::Skip, 0}, {CsvFieldKind::ParseS32, 4},
        {CsvFieldKind::Skip, 0}, {CsvFieldKind::ParseS32, 4},
        {CsvFieldKind::Skip, 0}, {CsvFieldKind::ParseS32, 4},
        {CsvFieldKind::Skip, 0}, {CsvFieldKind::ParseS32, 4},
        {CsvFieldKind::Skip, 0}, {CsvFieldKind::ParseS32, 4},
        {CsvFieldKind::Skip, 0}
    }};
static const std::array<const char*, CHARACTER_CSV_COLUMN_COUNT> COLUMN_NAMES = {{
    "itemId",
    "itemLabel",
    "itemNameTextId",
    "itemHelpTextId",
    "priceOrVariantCount",
    "unlockThreshold",
    "field0x14",
    "field0x18",
    "field0x1C",
    "field0x20",
    "field0x24",
    "variantSelector",
    "menuCategory",
    "menuCategoryLabel",
    "categorySlot",
    "categorySlotLabel",
    "modelPart",
    "modelPartLabel",
    "variantSlot",
    "variantSlotLabel",
    "variantBase",
    "variantCount",
    "field0x44",
    "variantLabelIndices[0]",
    "variantLabel[0]",
    "variantLabelIndices[1]",
    "variantLabel[1]",
    "variantLabelIndices[2]",
    "variantLabel[2]",
    "variantLabelIndices[3]",
    "variantLabel[3]",
    "variantLabelIndices[4]",
    "variantLabel[4]",
    "variantLabelIndices[5]",
    "variantLabel[5]",
    "variantLabelIndices[6]",
    "variantLabel[6]",
    "variantLabelIndices[7]",
    "variantLabel[7]",
    "variantLabelIndices[8]",
    "variantLabel[8]",
    "variantLabelIndices[9]",
    "variantLabel[9]"
}};
static bool validateUtf8(const std::string& text, std::size_t row,
                         std::size_t column)
{
    std::size_t i = 0;
    while (i < text.size())
    {
        const std::uint8_t first = static_cast<std::uint8_t>(text[i]);
        if (first < 0x80)
        {
            ++i;
            continue;
        }
        std::size_t length = 0;
        std::uint32_t codePoint = 0;
        std::uint32_t minimum = 0;
        if ((first & 0xE0) == 0xC0)
        {
            length = 2;
            codePoint = first & 0x1F;
            minimum = 0x80;
        }
        else if ((first & 0xF0) == 0xE0)
        {
            length = 3;
            codePoint = first & 0x0F;
            minimum = 0x800;
        }
        else if ((first & 0xF8) == 0xF0)
        {
            length = 4;
            codePoint = first & 0x07;
            minimum = 0x10000;
        }
        else
        {
            printf("ERROR: Invalid UTF-8 at row %zu column %zu byte %zu.\n",
                   row, column, i);
            return false;
        }
        if (length > text.size() - i)
        {
            printf("ERROR: Truncated UTF-8 at row %zu column %zu byte %zu.\n",
                   row, column, i);
            return false;
        }
        for (std::size_t j = 1; j < length; ++j)
        {
            const std::uint8_t next = static_cast<std::uint8_t>(text[i + j]);
            if ((next & 0xC0) != 0x80)
            {
                printf("ERROR: Invalid UTF-8 at row %zu column %zu byte %zu.\n",
                       row, column, i + j);
                return false;
            }
            codePoint = (codePoint << 6) | (next & 0x3F);
        }
        if (codePoint < minimum || codePoint > 0x10FFFF ||
            (codePoint >= 0xD800 && codePoint <= 0xDFFF))
        {
            printf("ERROR: Invalid UTF-8 scalar at row %zu column %zu byte %zu.\n",
                   row, column, i);
            return false;
        }
        i += length;
    }
    return true;
}
static bool parseS32(const std::string& text, std::int32_t& value,
                     std::size_t row, std::size_t column)
{
    if (text.empty())
    {
        value = 0;
        return true;
    }
    bool negative = false;
    std::size_t i = 0;
    if (text[0] == '-' || text[0] == '+')
    {
        negative = text[0] == '-';
        i = 1;
    }
    if (i == text.size())
    {
        printf("ERROR: Invalid decimal at row %zu column %zu.\n", row, column);
        return false;
    }
    const std::uint64_t limit = negative
                              ? static_cast<std::uint64_t>(
                                    std::numeric_limits<std::int32_t>::max()) + 1
                              : static_cast<std::uint64_t>(
                                    std::numeric_limits<std::int32_t>::max());
    std::uint64_t magnitude = 0;
    for (; i < text.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c < '0' || c > '9')
        {
            printf("ERROR: Invalid decimal at row %zu column %zu.\n", row, column);
            return false;
        }
        const std::uint32_t digit = c - '0';
        if (magnitude > (limit - digit) / 10)
        {
            printf("ERROR: Decimal overflow at row %zu column %zu.\n", row, column);
            return false;
        }
        magnitude = magnitude * 10 + digit;
    }
    if (negative && magnitude == limit)
        value = std::numeric_limits<std::int32_t>::min();
    else if (negative)
        value = -static_cast<std::int32_t>(magnitude);
    else
        value = static_cast<std::int32_t>(magnitude);
    return true;
}
static bool decodeDefinition(const CsvRow& row, std::size_t rowIndex,
                             InventoryItemDefinition& definition)
{
    std::array<std::int32_t, CHARACTER_CSV_VALUE_COUNT> values{};
    std::size_t valueIndex = 0;
    for (std::size_t column = 0; column < row.fields.size(); ++column)
    {
        if (CHARACTER_CUSTOMIZATION_CSV_FIELDS[column].kind ==
            CsvFieldKind::Skip)
        {
            continue;
        }
        if (!parseS32(row.fields[column], values[valueIndex], rowIndex, column))
            return false;
        ++valueIndex;
    }
    definition.itemId = static_cast<std::uint32_t>(values[0]);
    definition.itemNameTextId = static_cast<std::uint32_t>(values[1]);
    definition.itemHelpTextId = static_cast<std::uint32_t>(values[2]);
    definition.priceOrVariantCount = values[3];
    definition.unlockThreshold = values[4];
    definition.field0x14 = values[5];
    definition.field0x18 = values[6];
    definition.field0x1C = values[7];
    definition.field0x20 = values[8];
    definition.field0x24 = values[9];
    definition.variantSelector = values[10];
    definition.menuCategory = static_cast<InventoryMenuCategory>(values[11]);
    definition.categorySlot = values[12];
    definition.modelPart = static_cast<InventoryModelPart>(values[13]);
    definition.variantSlot = values[14];
    definition.variantBase = values[15];
    definition.variantCount = values[16];
    definition.field0x44 = values[17];
    for (std::size_t i = 0; i < definition.variantLabelIndices.size(); ++i)
    {
        definition.variantLabelIndices[i] = static_cast<std::uint32_t>(
            values[18 + i]);
    }
    return true;
}
bool readCsv(const CsvBytes& data, CsvFile& csv)
{
    csv = CsvFile{};
    std::size_t rowStart = 0;
    std::size_t cursor = 0;
    while (cursor < data.size())
    {
        if (data[cursor] == '\n')
        {
            printf("ERROR: Bare LF at offset %zu.\n", cursor);
            return false;
        }
        if (data[cursor] != '\r')
        {
            ++cursor;
            continue;
        }
        if (cursor + 1 >= data.size() || data[cursor + 1] != '\n')
        {
            printf("ERROR: CR is not followed by LF at offset %zu.\n", cursor);
            return false;
        }
        CsvRow row;
        const char* begin = reinterpret_cast<const char*>(data.data() + rowStart);
        const std::size_t length = cursor - rowStart;
        row.comment = length != 0 && data[rowStart] == '#';
        if (row.comment)
        {
            row.commentText.assign(begin, length);
        }
        else
        {
            std::size_t fieldStart = rowStart;
            for (std::size_t i = rowStart; i < cursor; ++i)
            {
                if (data[i] != ',')
                    continue;
                row.fields.emplace_back(
                    reinterpret_cast<const char*>(data.data() + fieldStart),
                    i - fieldStart);
                fieldStart = i + 1;
            }
            row.fields.emplace_back(
                reinterpret_cast<const char*>(data.data() + fieldStart),
                cursor - fieldStart);
        }
        const std::size_t rowIndex = csv.rows.size();
        if (row.comment)
        {
            if (!validateUtf8(row.commentText, rowIndex, 0))
                return false;
        }
        else
        {
            for (std::size_t column = 0; column < row.fields.size(); ++column)
            {
                if (!validateUtf8(row.fields[column], rowIndex, column))
                    return false;
            }
        }
        csv.rows.push_back(std::move(row));
        cursor += 2;
        rowStart = cursor;
    }
    if (rowStart != data.size())
    {
        printf("ERROR: Final CSV row is not CRLF-terminated.\n");
        return false;
    }
    return true;
}
bool writeCsv(const CsvFile& csv, CsvBytes& data)
{
    data.clear();
    for (std::size_t rowIndex = 0; rowIndex < csv.rows.size(); ++rowIndex)
    {
        const CsvRow& row = csv.rows[rowIndex];
        if (row.comment)
        {
            if (row.commentText.empty() || row.commentText[0] != '#')
            {
                printf("ERROR: Comment row %zu does not begin with '#'.\n", rowIndex);
                return false;
            }
            for (char c : row.commentText)
            {
                if (c == '\r' || c == '\n')
                {
                    printf("ERROR: Comment row %zu contains a line ending.\n", rowIndex);
                    return false;
                }
            }
            data.insert(data.end(), row.commentText.begin(), row.commentText.end());
        }
        else
        {
            if (row.fields.empty())
            {
                printf("ERROR: Data row %zu has no fields.\n", rowIndex);
                return false;
            }
            for (std::size_t column = 0; column < row.fields.size(); ++column)
            {
                if (column != 0)
                    data.push_back(',');
                for (char c : row.fields[column])
                {
                    if (c == ',' || c == '\r' || c == '\n')
                    {
                        printf("ERROR: Row %zu column %zu contains a delimiter.\n",
                               rowIndex, column);
                        return false;
                    }
                    data.push_back(static_cast<std::uint8_t>(c));
                }
            }
        }
        data.push_back('\r');
        data.push_back('\n');
    }
    return true;
}
bool readCharacterCustomizationCsv(const CsvBytes& data,
                                   CharacterCustomizationCsv& csv)
{
    csv = CharacterCustomizationCsv{};
    if (!readCsv(data, csv.source))
        return false;
    std::set<std::uint32_t> itemIds;
    for (std::size_t rowIndex = 0; rowIndex < csv.source.rows.size(); ++rowIndex)
    {
        const CsvRow& row = csv.source.rows[rowIndex];
        if (row.comment)
            continue;
        if (row.fields.size() != CHARACTER_CSV_COLUMN_COUNT)
        {
            printf("ERROR: Row %zu has %zu fields; expected %zu.\n",
                   rowIndex, row.fields.size(), CHARACTER_CSV_COLUMN_COUNT);
            return false;
        }
        InventoryItemDefinition definition;
        if (!decodeDefinition(row, rowIndex, definition))
            return false;
        if (!itemIds.insert(definition.itemId).second)
        {
            printf("ERROR: Duplicate item ID %" PRIu32 " at row %zu.\n",
                   definition.itemId, rowIndex);
            return false;
        }
        csv.definitionRows.push_back(rowIndex);
        csv.definitions.push_back(definition);
    }
    if (csv.definitions.size() > CHARACTER_CSV_MAX_ROWS)
    {
        printf("ERROR: Character CSV has %zu data rows; maximum is %zu.\n",
               csv.definitions.size(), CHARACTER_CSV_MAX_ROWS);
        return false;
    }
    for (std::size_t i = 0; i < csv.definitions.size(); ++i)
    {
        if (csv.definitions[i].itemId >= 100)
            continue;
        if (csv.definitions[i].itemId == 0)
        {
            printf("ERROR: Invalid category item ID %" PRIu32 ".\n",
                   csv.definitions[i].itemId);
            return false;
        }
        if (csv.definitions[i].priceOrVariantCount < 0)
        {
            printf("ERROR: Category %" PRIu32 " has a negative item count.\n",
                   csv.definitions[i].itemId);
            return false;
        }
        const std::size_t count = static_cast<std::size_t>(
            csv.definitions[i].priceOrVariantCount);
        if (count > csv.definitions.size() - i - 1)
        {
            printf("ERROR: Category %" PRIu32 " exceeds the row table.\n",
                   csv.definitions[i].itemId);
            return false;
        }
        for (std::size_t j = 1; j <= count; ++j)
        {
            if (csv.definitions[i + j].itemId < 100)
            {
                printf("ERROR: Category %" PRIu32
                       " reaches another category before its declared end.\n",
                       csv.definitions[i].itemId);
                return false;
            }
        }
        if (i + count + 1 < csv.definitions.size() &&
            csv.definitions[i + count + 1].itemId >= 100)
        {
            printf("ERROR: Category %" PRIu32
                   " does not cover all following item rows.\n",
                   csv.definitions[i].itemId);
            return false;
        }
        csv.categoryDefinitions.push_back(i);
    }
    return true;
}
const char* characterCustomizationColumnName(std::size_t column)
{
    return column < COLUMN_NAMES.size() ? COLUMN_NAMES[column] : nullptr;
}
