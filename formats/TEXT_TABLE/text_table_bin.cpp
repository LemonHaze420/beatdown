#include <cstdio>
#include <limits>
#include "../common.h"
#include "text_table.h"

static const char* const ENCODING_NAMES[TEXT_TABLE_LANGUAGE_COUNT] = {
    "Japanese", "English", "German", "Spanish", "French", "Italian"};
static const char* const LANGUAGE_NAMES[TEXT_TABLE_LANGUAGE_COUNT] = {
    "Japanese", "English", "French", "German", "Spanish", "Italian"};
static const char LANGUAGE_SUFFIXES[TEXT_TABLE_LANGUAGE_COUNT] = {
    'J', 'E', 'F', 'G', 'S', 'I'};

static const std::uint16_t LANGUAGE_ENCODINGS[TEXT_TABLE_LANGUAGE_COUNT] = {
    0, 1, 4, 2, 3, 5};
const char* textTableEncodingName(std::uint16_t encoding)
{
    if (encoding >= TEXT_TABLE_LANGUAGE_COUNT)
        return "unknown";
    return ENCODING_NAMES[encoding];
}
const char* textTableLanguageName(std::uint8_t language)
{
    if (language >= TEXT_TABLE_LANGUAGE_COUNT)
        return "unknown";
    return LANGUAGE_NAMES[language];
}
char textTableLanguageSuffix(std::uint8_t language)
{
    if (language >= TEXT_TABLE_LANGUAGE_COUNT)
        return '?';
    return LANGUAGE_SUFFIXES[language];
}
bool textTableEncodingForLanguage(std::uint8_t language,
                                  std::uint16_t& encoding)
{
    if (language >= TEXT_TABLE_LANGUAGE_COUNT)
        return false;
    encoding = LANGUAGE_ENCODINGS[language];
    return true;
}
const TextTableRecord* findTextTableRecord(const TextTable& table,
                                           std::uint16_t textId)
{
    std::size_t low = 0;
    std::size_t high = table.records.size();
    while (low < high)
    {
        const std::size_t mid = low + (high - low) / 2;
        const TextTableRecord& record = table.records[mid];
        if (record.textId == textId)
            return &record;
        if (record.textId < textId)
            low = mid + 1;
        else
            high = mid;
    }
    return nullptr;
}
bool readTextTable(const TextTableBytes& data, TextTable& table)
{
    table = TextTable();
    if (data.size() < TEXT_TABLE_HEADER_SIZE)
    {
        printf("ERROR: File is shorter than the 8-byte text table header.\n");
        return false;
    }
    const std::uint32_t dataSize = read32(data.data());
    const std::uint16_t entryCount = read16(data.data() + 4);
    table.encoding = read16(data.data() + 6);
    const std::size_t expected = TEXT_TABLE_HEADER_SIZE +
                                 TEXT_TABLE_ENTRY_SIZE * entryCount + dataSize;
    if (data.size() != expected)
    {
        printf("ERROR: File is %zu bytes; the header describes %zu "
               "(8 + 8 * %u + %u).\n",
               data.size(), expected, entryCount, dataSize);
        return false;
    }
    const std::uint8_t* const payload =
        data.data() + TEXT_TABLE_HEADER_SIZE + TEXT_TABLE_ENTRY_SIZE * entryCount;
    table.records.resize(entryCount);
    std::uint32_t cursor = 0;
    for (std::size_t i = 0; i < entryCount; ++i)
    {
        const std::uint8_t* const entry =
            data.data() + TEXT_TABLE_HEADER_SIZE + TEXT_TABLE_ENTRY_SIZE * i;
        const std::uint16_t textId = read16(entry);
        const std::uint16_t byteLength = read16(entry + 2);
        const std::uint32_t dataOffset = read32(entry + 4);
        if (i > 0 && textId <= table.records[i - 1].textId)
        {
            printf("ERROR: Entry %zu has id %u after id %u; "
                   "FindTextTableEntryById requires ascending ids.\n",
                   i, textId, table.records[i - 1].textId);
            return false;
        }
        TextTableRecord& record = table.records[i];
        record.textId = textId;
        if (byteLength == 0)
        {
            record.suppressed = true;
            record.terminated = false;
            continue;
        }
        if (dataOffset > dataSize || byteLength > dataSize - dataOffset)
        {
            printf("ERROR: Entry %zu (id %u) spans %u bytes at offset %u, "
                   "past the %u-byte payload.\n",
                   i, textId, byteLength, dataOffset, dataSize);
            return false;
        }

        if (dataOffset != cursor)
        {
            printf("ERROR: Entry %zu (id %u) starts at offset %u; the "
                   "contiguous layout expects %u.\n",
                   i, textId, dataOffset, cursor);
            return false;
        }
        const std::uint8_t* const bytes = payload + dataOffset;
        record.terminated = bytes[byteLength - 1] == 0;
        const std::size_t textLength =
            record.terminated ? byteLength - 1u : byteLength;
        record.text.assign(reinterpret_cast<const char*>(bytes), textLength);
        cursor += byteLength;
    }
    table.tailPadding.assign(payload + cursor, payload + dataSize);
    return true;
}
bool writeTextTable(const TextTable& table, TextTableBytes& data)
{
    data.clear();
    if (table.records.size() > std::numeric_limits<std::uint16_t>::max())
    {
        printf("ERROR: %zu records exceed the 16-bit entry count.\n",
               table.records.size());
        return false;
    }
    TextTableBytes payload;
    std::vector<std::uint32_t> offsets(table.records.size());
    std::vector<std::uint16_t> lengths(table.records.size());
    for (std::size_t i = 0; i < table.records.size(); ++i)
    {
        const TextTableRecord& record = table.records[i];
        if (i > 0 && record.textId <= table.records[i - 1].textId)
        {
            printf("ERROR: Record %zu has id %u after id %u; ids must "
                   "ascend.\n",
                   i, record.textId, table.records[i - 1].textId);
            return false;
        }
        offsets[i] = static_cast<std::uint32_t>(payload.size());
        if (record.suppressed)
        {
            lengths[i] = 0;
            continue;
        }
        const std::size_t byteLength =
            record.text.size() + (record.terminated ? 1u : 0u);
        if (byteLength > std::numeric_limits<std::uint16_t>::max())
        {
            printf("ERROR: Record id %u is %zu bytes; the maximum is %u.\n",
                   record.textId, byteLength,
                   std::numeric_limits<std::uint16_t>::max());
            return false;
        }
        lengths[i] = static_cast<std::uint16_t>(byteLength);
        payload.insert(payload.end(), record.text.begin(), record.text.end());
        if (record.terminated)
            payload.push_back(0);
    }
    const std::size_t remainder = payload.size() % TEXT_TABLE_PAYLOAD_ALIGN;
    const std::size_t padding =
        remainder == 0 ? 0 : TEXT_TABLE_PAYLOAD_ALIGN - remainder;
    if (table.tailPadding.size() == padding)
        payload.insert(payload.end(), table.tailPadding.begin(),
                       table.tailPadding.end());
    else
        payload.insert(payload.end(), padding, 0);
    if (payload.size() > std::numeric_limits<std::uint32_t>::max())
    {
        printf("ERROR: Payload of %zu bytes exceeds the 32-bit size field.\n",
               payload.size());
        return false;
    }
    data.reserve(TEXT_TABLE_HEADER_SIZE +
                 TEXT_TABLE_ENTRY_SIZE * table.records.size() + payload.size());
    write32(data, static_cast<std::uint32_t>(payload.size()));
    write16(data, static_cast<std::uint16_t>(table.records.size()));
    write16(data, table.encoding);
    for (std::size_t i = 0; i < table.records.size(); ++i)
    {
        write16(data, table.records[i].textId);
        write16(data, lengths[i]);
        write32(data, offsets[i]);
    }
    data.insert(data.end(), payload.begin(), payload.end());
    return true;
}
static void appendHexByte(std::string& out, std::uint8_t value)
{
    static const char DIGITS[] = "0123456789abcdef";
    out.push_back(DIGITS[value >> 4]);
    out.push_back(DIGITS[value & 0x0f]);
}

static void appendEscaped(std::string& out, const std::string& text)
{
    for (const char c : text)
    {
        const std::uint8_t value = static_cast<std::uint8_t>(c);
        if (value == '"' || value == '\\')
        {
            out.push_back('\\');
            out.push_back(static_cast<char>(value));
        }
        else if (value < 0x20 || value == 0x7f)
        {
            out += "\\x";
            appendHexByte(out, value);
        }
        else
        {
            out.push_back(c);
        }
    }
}
bool writeTextTableSource(const TextTable& table, std::string& source)
{
    source.clear();
    source += "# Beat Down keyed text table\n";
    source += "#\n";
    source += "#   <id> \"text\"    record; the packer appends the counted NUL\n";
    source += "#   <id> -          suppressed record (byteLength 0)\n";
    source += "#   <id> ! \"text\"  record stored without a trailing NUL\n";
    source += "#\n";
    source += "# A game line break is the two literal bytes \\ and n, so it is\n";
    source += "# written \\\\n here. Ids must stay in ascending order.\n";
    source += "#\n";
    char line[128];
    std::snprintf(line, sizeof(line), "# encoding %u (%s), %zu records\n",
                  table.encoding, textTableEncodingName(table.encoding),
                  table.records.size());
    source += line;
    std::snprintf(line, sizeof(line), ".encoding %u\n", table.encoding);
    source += line;
    if (!table.tailPadding.empty())
    {
        source += ".tail";
        for (const std::uint8_t value : table.tailPadding)
        {
            source.push_back(' ');
            appendHexByte(source, value);
        }
        source.push_back('\n');
    }
    source.push_back('\n');
    for (const TextTableRecord& record : table.records)
    {
        std::snprintf(line, sizeof(line), "%u ", record.textId);
        source += line;
        if (record.suppressed)
        {
            source += "-\n";
            continue;
        }
        if (!record.terminated)
            source += "! ";
        source.push_back('"');
        appendEscaped(source, record.text);
        source += "\"\n";
    }
    return true;
}
static bool sourceError(std::size_t line, const char* message)
{
    printf("ERROR: line %zu: %s\n", line, message);
    return false;
}
static bool parseHexDigit(char c, std::uint8_t& value)
{
    if (c >= '0' && c <= '9')
        value = static_cast<std::uint8_t>(c - '0');
    else if (c >= 'a' && c <= 'f')
        value = static_cast<std::uint8_t>(c - 'a' + 10);
    else if (c >= 'A' && c <= 'F')
        value = static_cast<std::uint8_t>(c - 'A' + 10);
    else
        return false;
    return true;
}
static bool parseHexByte(const std::string& text, std::size_t& pos,
                         std::uint8_t& value)
{
    std::uint8_t high = 0;
    std::uint8_t low = 0;
    if (pos + 1 >= text.size() || !parseHexDigit(text[pos], high) ||
        !parseHexDigit(text[pos + 1], low))
    {
        return false;
    }
    value = static_cast<std::uint8_t>((high << 4) | low);
    pos += 2;
    return true;
}
static void skipSpaces(const std::string& text, std::size_t& pos)
{
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t'))
        ++pos;
}
static bool parseQuoted(const std::string& line, std::size_t& pos,
                        std::size_t lineNumber, std::string& text)
{
    if (pos >= line.size() || line[pos] != '"')
        return sourceError(lineNumber, "expected a quoted string");
    ++pos;
    text.clear();
    while (pos < line.size())
    {
        const char c = line[pos];
        if (c == '"')
        {
            ++pos;
            return true;
        }
        if (c != '\\')
        {
            text.push_back(c);
            ++pos;
            continue;
        }
        ++pos;
        if (pos >= line.size())
            return sourceError(lineNumber, "trailing escape character");
        const char escape = line[pos];
        if (escape == '"' || escape == '\\')
        {
            text.push_back(escape);
            ++pos;
        }
        else if (escape == 'x')
        {
            ++pos;
            std::uint8_t value = 0;
            if (!parseHexByte(line, pos, value))
                return sourceError(lineNumber, "malformed \\xNN escape");
            text.push_back(static_cast<char>(value));
        }
        else
        {
            return sourceError(lineNumber,
                               "unknown escape; use \\\\, \\\" or \\xNN");
        }
    }
    return sourceError(lineNumber, "unterminated string");
}
bool readTextTableSource(const std::string& source, TextTable& table)
{
    table = TextTable();
    bool encodingSeen = false;
    std::size_t lineNumber = 0;
    std::size_t start = 0;
    while (start <= source.size())
    {
        std::size_t end = source.find('\n', start);
        if (end == std::string::npos)
            end = source.size();
        std::string line = source.substr(start, end - start);
        start = end + 1;
        ++lineNumber;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        std::size_t pos = 0;
        skipSpaces(line, pos);
        if (pos >= line.size() || line[pos] == '#')
        {
            if (end == source.size())
                break;
            continue;
        }
        if (line[pos] == '.')
        {
            ++pos;
            const std::size_t nameStart = pos;
            while (pos < line.size() && line[pos] != ' ' && line[pos] != '\t')
                ++pos;
            const std::string directive = line.substr(nameStart, pos - nameStart);
            skipSpaces(line, pos);
            if (directive == "encoding")
            {
                if (pos >= line.size())
                    return sourceError(lineNumber, ".encoding needs a value");
                unsigned long value = 0;
                std::size_t digits = 0;
                while (pos < line.size() && line[pos] >= '0' && line[pos] <= '9')
                {
                    value = value * 10 + static_cast<unsigned long>(line[pos] - '0');
                    ++pos;
                    ++digits;
                }
                if (digits == 0 || value > std::numeric_limits<std::uint16_t>::max())
                    return sourceError(lineNumber, "invalid .encoding value");
                table.encoding = static_cast<std::uint16_t>(value);
                encodingSeen = true;
            }
            else if (directive == "tail")
            {
                table.tailPadding.clear();
                while (pos < line.size())
                {
                    std::uint8_t value = 0;
                    if (!parseHexByte(line, pos, value))
                        return sourceError(lineNumber, "malformed .tail byte");
                    table.tailPadding.push_back(value);
                    skipSpaces(line, pos);
                }
            }
            else
            {
                return sourceError(lineNumber, "unknown directive");
            }
            if (end == source.size())
                break;
            continue;
        }
        if (line[pos] < '0' || line[pos] > '9')
            return sourceError(lineNumber, "expected a decimal text id");
        unsigned long id = 0;
        while (pos < line.size() && line[pos] >= '0' && line[pos] <= '9')
        {
            id = id * 10 + static_cast<unsigned long>(line[pos] - '0');
            if (id > std::numeric_limits<std::uint16_t>::max())
                return sourceError(lineNumber, "text id exceeds 65535");
            ++pos;
        }
        skipSpaces(line, pos);
        TextTableRecord record;
        record.textId = static_cast<std::uint16_t>(id);
        if (pos < line.size() && line[pos] == '-')
        {
            record.suppressed = true;
            record.terminated = false;
            ++pos;
        }
        else
        {
            if (pos < line.size() && line[pos] == '!')
            {
                record.terminated = false;
                ++pos;
                skipSpaces(line, pos);
            }
            if (!parseQuoted(line, pos, lineNumber, record.text))
                return false;
        }
        skipSpaces(line, pos);
        if (pos < line.size() && line[pos] != '#')
            return sourceError(lineNumber, "unexpected text after the record");
        if (!table.records.empty() &&
            record.textId <= table.records.back().textId)
        {
            return sourceError(lineNumber, "text ids must ascend");
        }
        table.records.push_back(std::move(record));
        if (end == source.size())
            break;
    }
    if (!encodingSeen)
    {
        printf("ERROR: Source has no .encoding directive.\n");
        return false;
    }
    return true;
}
