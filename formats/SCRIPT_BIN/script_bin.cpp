#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdint>
#include <iomanip>
#include <map>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "script_bin.h"

std::uint8_t EventOpcodeDescriptor::byteLength() const
{
    return static_cast<std::uint8_t>(wordCount * 4);
}

static void write16(Bytes& data, std::uint16_t value)
{
    data.push_back(static_cast<std::uint8_t>(value));
    data.push_back(static_cast<std::uint8_t>(value >> 8));
}

static void write32(Bytes& data, std::uint32_t value)
{
    data.push_back(static_cast<std::uint8_t>(value));
    data.push_back(static_cast<std::uint8_t>(value >> 8));
    data.push_back(static_cast<std::uint8_t>(value >> 16));
    data.push_back(static_cast<std::uint8_t>(value >> 24));
}

static std::string hex(std::uint64_t value, unsigned width)
{
    std::ostringstream output;
    output << std::uppercase << std::hex << std::setfill('0')
           << std::setw(static_cast<int>(width)) << value;
    return output.str();
}

bool hasHeadOperand(const EventOpcodeDescriptor& descriptor)
{
    if (descriptor.opcode == EVENT_OP_EVENT)
        return true;

    for (std::size_t i = 0; i < descriptor.operandCount; ++i)
    {
        const auto& operand =
            EVENT_OPERAND_METADATA[descriptor.operandOffset + i];
        if (operand.wordIndex == 0 && operand.typeCode != EVENT_OPERAND_NONE)
            return true;
    }

    return false;
}

bool hasOperandType(const EventOpcodeDescriptor& descriptor,
                    std::uint8_t wordIndex,
                    EventOperandTypeCode type)
{
    for (std::size_t i = 0; i < descriptor.operandCount; ++i)
    {
        const auto& operand =
            EVENT_OPERAND_METADATA[descriptor.operandOffset + i];
        if (operand.wordIndex == wordIndex && operand.typeCode == type)
            return true;
    }
    return false;
}

static std::string_view getCallFunctionName(std::uint32_t id)
{
    if (id < EVENT_CALL_FUNCTION_NAMES.size())
        return EVENT_CALL_FUNCTION_NAMES[id];
    return {};
}

static std::string indexName(std::string_view prefix, std::uint32_t index)
{
    std::ostringstream output;
    output << prefix << std::setfill('0') << std::setw(4) << index;
    return output.str();
}

static bool parseStringTable(const Bytes& data, std::uint64_t tableOffset,
                             std::uint32_t tableSize,
                             std::vector<std::string>& strings)
{
    strings.clear();
    if (tableSize == 0)
        return true;
    if (tableSize < 4)
    {
        printf("ERROR: Invalid string table.\n");
        return false;
    }

    const std::uint32_t firstOffset =
        static_cast<std::uint32_t>(data[tableOffset]) |
        (static_cast<std::uint32_t>(data[tableOffset + 1]) << 8) |
        (static_cast<std::uint32_t>(data[tableOffset + 2]) << 16) |
        (static_cast<std::uint32_t>(data[tableOffset + 3]) << 24);
    if (firstOffset == 0 || (firstOffset & 3) != 0 || firstOffset > tableSize)
    {
        printf("ERROR: Invalid string table.\n");
        return false;
    }

    const std::size_t count = firstOffset / 4;
    strings.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        const std::uint64_t entry = tableOffset + i * 4;
        const std::uint32_t offset =
            static_cast<std::uint32_t>(data[entry]) |
            (static_cast<std::uint32_t>(data[entry + 1]) << 8) |
            (static_cast<std::uint32_t>(data[entry + 2]) << 16) |
            (static_cast<std::uint32_t>(data[entry + 3]) << 24);
        if (offset < firstOffset || offset >= tableSize)
        {
            printf("ERROR: Invalid string offset.\n");
            return false;
        }

        const std::size_t begin = static_cast<std::size_t>(tableOffset + offset);
        const std::size_t end = static_cast<std::size_t>(tableOffset + tableSize);
        const auto terminator = std::find(data.begin() + begin, data.begin() + end, 0);
        if (terminator == data.begin() + end)
        {
            printf("ERROR: Unterminated string.\n");
            return false;
        }

        strings.emplace_back(reinterpret_cast<const char*>(data.data() + begin),
                             static_cast<std::size_t>(terminator - (data.begin() + begin)));
    }

    return true;
}

bool readScriptBin(const Bytes& data, ScriptBin& script)
{
    constexpr std::uint32_t fileHeaderSize = 4 + EVENT_LANGUAGE_COUNT * 4;
    constexpr std::uint32_t sectionHeaderSize = 0x20;

    if (data.size() < fileHeaderSize)
    {
        printf("ERROR: Invalid SCRIPT_BIN header.\n");
        return false;
    }
    const std::uint32_t languageCount =
        static_cast<std::uint32_t>(data[0]) |
        (static_cast<std::uint32_t>(data[1]) << 8) |
        (static_cast<std::uint32_t>(data[2]) << 16) |
        (static_cast<std::uint32_t>(data[3]) << 24);
    if (languageCount != EVENT_LANGUAGE_COUNT)
    {
        printf("ERROR: Expected six languages.\n");
        return false;
    }

    std::array<std::uint32_t, EVENT_LANGUAGE_COUNT> offsets{};
    std::uint32_t previous = fileHeaderSize;
    for (std::size_t i = 0; i < offsets.size(); ++i)
    {
        const std::size_t entry = 4 + i * 4;
        offsets[i] = static_cast<std::uint32_t>(data[entry]) |
                     (static_cast<std::uint32_t>(data[entry + 1]) << 8) |
                     (static_cast<std::uint32_t>(data[entry + 2]) << 16) |
                     (static_cast<std::uint32_t>(data[entry + 3]) << 24);
        if (offsets[i] < previous || offsets[i] > data.size())
        {
            printf("ERROR: Invalid %s language offset.\n",
                   EVENT_LANGUAGE_CODES[i].data());
            return false;
        }
        previous = offsets[i];
    }

    script = {};
    LanguageSections& sections = script.languages;
    for (std::size_t i = 0; i < sections.size(); ++i)
    {
        const std::uint64_t start = offsets[i];
        const std::uint64_t end = i + 1 < offsets.size() ? offsets[i + 1] : data.size();
        if (end - start < sectionHeaderSize)
        {
            printf("ERROR: Invalid %s section header.\n",
                   EVENT_LANGUAGE_CODES[i].data());
            return false;
        }

        const std::uint16_t eventId =
            static_cast<std::uint16_t>(data[start]) |
            static_cast<std::uint16_t>(data[start + 1] << 8);
        const std::uint16_t pageCount =
            static_cast<std::uint16_t>(data[start + 2]) |
            static_cast<std::uint16_t>(data[start + 3] << 8);
        const std::uint32_t codeEnd =
            static_cast<std::uint32_t>(data[start + 4]) |
            (static_cast<std::uint32_t>(data[start + 5]) << 8) |
            (static_cast<std::uint32_t>(data[start + 6]) << 16) |
            (static_cast<std::uint32_t>(data[start + 7]) << 24);
        const std::uint32_t textCount =
            static_cast<std::uint32_t>(data[start + 8]) |
            (static_cast<std::uint32_t>(data[start + 9]) << 8) |
            (static_cast<std::uint32_t>(data[start + 0x0A]) << 16) |
            (static_cast<std::uint32_t>(data[start + 0x0B]) << 24);
        const std::uint32_t textOffset =
            static_cast<std::uint32_t>(data[start + 0x0C]) |
            (static_cast<std::uint32_t>(data[start + 0x0D]) << 8) |
            (static_cast<std::uint32_t>(data[start + 0x0E]) << 16) |
            (static_cast<std::uint32_t>(data[start + 0x0F]) << 24);
        const std::uint32_t unknown_0x10 =
            static_cast<std::uint32_t>(data[start + 0x10]) |
            (static_cast<std::uint32_t>(data[start + 0x11]) << 8) |
            (static_cast<std::uint32_t>(data[start + 0x12]) << 16) |
            (static_cast<std::uint32_t>(data[start + 0x13]) << 24);
        const std::uint32_t resourceOffset =
            static_cast<std::uint32_t>(data[start + 0x14]) |
            (static_cast<std::uint32_t>(data[start + 0x15]) << 8) |
            (static_cast<std::uint32_t>(data[start + 0x16]) << 16) |
            (static_cast<std::uint32_t>(data[start + 0x17]) << 24);
        const std::uint32_t unknown_0x18 =
            static_cast<std::uint32_t>(data[start + 0x18]) |
            (static_cast<std::uint32_t>(data[start + 0x19]) << 8) |
            (static_cast<std::uint32_t>(data[start + 0x1A]) << 16) |
            (static_cast<std::uint32_t>(data[start + 0x1B]) << 24);
        const std::uint32_t unknown_0x1C =
            static_cast<std::uint32_t>(data[start + 0x1C]) |
            (static_cast<std::uint32_t>(data[start + 0x1D]) << 8) |
            (static_cast<std::uint32_t>(data[start + 0x1E]) << 16) |
            (static_cast<std::uint32_t>(data[start + 0x1F]) << 24);
        const std::uint64_t bodyOffset = start + sectionHeaderSize;
        const std::uint64_t bodySize = end - bodyOffset;

        LanguageSection& section = sections[i];
        if ((eventId | pageCount | codeEnd | textCount | textOffset | unknown_0x10 |
             resourceOffset | unknown_0x18 | unknown_0x1C) == 0)
        {
            if (bodySize != 0)
            {
                printf("ERROR: Empty %s section contains data.\n",
                       EVENT_LANGUAGE_CODES[i].data());
                return false;
            }
            section.empty = true;
            continue;
        }

        if ((codeEnd & 3) != 0 || (textOffset & 3) != 0 ||
            (resourceOffset & 3) != 0 || codeEnd > textOffset ||
            textOffset > resourceOffset || resourceOffset > bodySize)
        {
            printf("ERROR: Invalid %s section layout.\n",
                   EVENT_LANGUAGE_CODES[i].data());
            return false;
        }
        if (unknown_0x10 != 0 || unknown_0x18 != 0 || unknown_0x1C != 0)
        {
            printf("ERROR: Unsupported %s section header.\n",
                   EVENT_LANGUAGE_CODES[i].data());
            return false;
        }

        const std::uint32_t pageBytes = textOffset - codeEnd;
        if (pageBytes / 4 != pageCount)
        {
            printf("ERROR: Invalid %s page table.\n",
                   EVENT_LANGUAGE_CODES[i].data());
            return false;
        }

        section.eventId = eventId;
        section.code.reserve(codeEnd / 4);
        for (std::uint32_t offset = 0; offset < codeEnd; offset += 4)
        {
            const std::uint64_t word = bodyOffset + offset;
            section.code.push_back(
                static_cast<std::uint32_t>(data[word]) |
                (static_cast<std::uint32_t>(data[word + 1]) << 8) |
                (static_cast<std::uint32_t>(data[word + 2]) << 16) |
                (static_cast<std::uint32_t>(data[word + 3]) << 24));
        }

        section.pages.reserve(pageCount);
        for (std::uint32_t page = 0; page < pageCount; ++page)
        {
            const std::uint64_t entry = bodyOffset + codeEnd + page * 4;
            const std::uint32_t target =
                static_cast<std::uint32_t>(data[entry]) |
                (static_cast<std::uint32_t>(data[entry + 1]) << 8) |
                (static_cast<std::uint32_t>(data[entry + 2]) << 16) |
                (static_cast<std::uint32_t>(data[entry + 3]) << 24);
            if ((target & 3) != 0 || target >= codeEnd)
            {
                printf("ERROR: Invalid %s page target.\n",
                       EVENT_LANGUAGE_CODES[i].data());
                return false;
            }
            section.pages.push_back(target);
        }

        if (!parseStringTable(data, bodyOffset + textOffset,
                              resourceOffset - textOffset, section.text) ||
            !parseStringTable(data, bodyOffset + resourceOffset,
                              static_cast<std::uint32_t>(bodySize - resourceOffset),
                              section.resources))
        {
            return false;
        }
        if (section.text.size() != textCount)
        {
            printf("ERROR: Invalid %s text table.\n",
                   EVENT_LANGUAGE_CODES[i].data());
            return false;
        }

        if (section.code.empty() ||
            (section.code.front() & 0xFFFF) != EVENT_OP_EVENT ||
            (section.code.front() >> 16) != eventId)
        {
            printf("ERROR: Invalid %s EVENT prologue.\n",
                   EVENT_LANGUAGE_CODES[i].data());
            return false;
        }
    }

    return true;
}

static std::string quoteString(std::string_view value)
{
    std::ostringstream output;
    output << '"';
    for (unsigned char c : value)
    {
        switch (c)
        {
        case '\\': output << "\\\\"; break;
        case '"':  output << "\\\""; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (c < 0x20 || c == 0x7F)
                output << "\\x" << hex(c, 2);
            else
                output << static_cast<char>(c);
            break;
        }
    }
    output << '"';
    return output.str();
}

static void disassembleLanguage(std::ostream& output, const LanguageSection& section,
                                std::size_t language)
{
    output << ".language " << EVENT_LANGUAGE_CODES[language] << '\n';
    if (section.empty)
    {
        output << ".empty\n.end\n";
        return;
    }

    output << ".pages\n";

    std::map<std::uint32_t, std::string> labels;
    for (std::uint32_t target : section.pages)
        labels.emplace(target, "loc_" + hex(target, 8));
    for (std::size_t i = 0; i < section.pages.size(); ++i)
    {
        output << indexName("page_", static_cast<std::uint32_t>(i)) << ": "
               << labels.at(section.pages[i]) << '\n';
    }

    output << "\n.code\n";
    std::uint32_t pc = 0;
    while (pc < section.code.size() * 4)
    {
        const auto label = labels.find(pc);
        if (label != labels.end())
            output << label->second << ":\n";

        const std::uint32_t word = section.code[pc / 4];
        const std::uint16_t opcode = static_cast<std::uint16_t>(word);
        const EventOpcodeDescriptor* descriptor = nullptr;
        if (opcode < EVENT_OPCODE_DESCRIPTORS.size())
        {
            const auto& candidate = EVENT_OPCODE_DESCRIPTORS[opcode];
            if (candidate.wordCount != 0)
                descriptor = &candidate;
        }

        bool raw = descriptor == nullptr ||
                   pc + descriptor->byteLength() > section.code.size() * 4;
        if (!raw)
        {
            const auto nextLabel = labels.upper_bound(pc);
            if (nextLabel != labels.end() &&
                nextLabel->first < pc + descriptor->byteLength())
            {
                raw = true;
            }
            if (!hasHeadOperand(*descriptor) && (word >> 16) != 0)
                raw = true;
        }

        if (raw)
        {
            output << "    .word 0x" << hex(word, 8) << '\n';
            pc += 4;
            continue;
        }

        const bool hasArguments = hasHeadOperand(*descriptor) || descriptor->wordCount > 1;
        output << "    ";
        if (hasArguments)
            output << std::left << std::setw(24) << descriptor->mnemonic;
        else
            output << descriptor->mnemonic;
        bool first = true;
        if (hasHeadOperand(*descriptor))
        {
            const std::uint32_t value = word >> 16;
            if (hasOperandType(*descriptor, 0, EVENT_OPERAND_PAGE_ID) &&
                value < section.pages.size())
            {
                output << indexName("page_", value);
            }
            else
            {
                output << "0x" << hex(value, 4);
            }
            first = false;
        }
        for (std::uint32_t i = 1; i < descriptor->wordCount; ++i)
        {
            if (!first)
                output << ", ";
            const std::uint32_t value = section.code[pc / 4 + i];
            const std::string_view callFunction =
                descriptor->opcode == EVENT_OP_CALLFUNC && i == 1 ?
                getCallFunctionName(value) : std::string_view();
            if (!callFunction.empty())
                output << callFunction;
            else if (hasOperandType(*descriptor, static_cast<std::uint8_t>(i),
                                    EVENT_OPERAND_MESSAGE_ID) &&
                     value < section.text.size())
                output << indexName("text_", value);
            else if (hasOperandType(*descriptor, static_cast<std::uint8_t>(i),
                                    EVENT_OPERAND_TEXTURE_RESOURCE_ID) &&
                     value < section.resources.size())
                output << indexName("resource_", value);
            else
                output << "0x" << hex(value, 8);
            first = false;
        }
        output << '\n';
        pc += descriptor->byteLength();
    }

    output << "\n.text\n";
    for (std::size_t i = 0; i < section.text.size(); ++i)
    {
        output << indexName("text_", static_cast<std::uint32_t>(i)) << ": "
               << quoteString(section.text[i]) << '\n';
    }

    output << "\n.resources\n";
    for (std::size_t i = 0; i < section.resources.size(); ++i)
    {
        output << indexName("resource_", static_cast<std::uint32_t>(i)) << ": "
               << quoteString(section.resources[i]) << '\n';
    }
    output << ".end\n";
}

std::string disassembleScriptBin(const ScriptBin& script)
{
    std::ostringstream output;
    for (std::size_t i = 0; i < script.languages.size(); ++i)
    {
        if (i != 0)
            output << '\n';
        disassembleLanguage(output, script.languages[i], i);
    }
    return output.str();
}

static bool buildStringTable(const std::vector<std::string>& strings, Bytes& table)
{
    table.clear();
    if (strings.empty())
        return true;

    std::uint64_t offset = strings.size() * 4;
    if (offset > 0xFFFFFFFF)
    {
        printf("ERROR: String table is too large.\n");
        return false;
    }
    for (const std::string& value : strings)
    {
        write32(table, static_cast<std::uint32_t>(offset));
        offset += value.size() + 1;
        if (offset > 0xFFFFFFFF)
        {
            printf("ERROR: String table is too large.\n");
            return false;
        }
    }
    for (const std::string& value : strings)
    {
        table.insert(table.end(), value.begin(), value.end());
        table.push_back(0);
    }
    while ((table.size() & 3) != 0)
        table.push_back(0);
    return true;
}

static bool buildLanguage(const LanguageSection& section, Bytes& data)
{
    data.clear();
    if (section.empty)
    {
        data.resize(0x20);
        return true;
    }

    Bytes text;
    Bytes resources;
    if (!buildStringTable(section.text, text) ||
        !buildStringTable(section.resources, resources))
    {
        return false;
    }

    const std::uint64_t codeSize = section.code.size() * 4;
    const std::uint64_t textOffset = codeSize + section.pages.size() * 4;
    const std::uint64_t resourceOffset = textOffset + text.size();
    const std::uint64_t totalSize = 0x20 + resourceOffset + resources.size();
    if (section.pages.size() > 0xFFFF || section.text.size() > 0xFFFFFFFF ||
        codeSize > 0xFFFFFFFF || textOffset > 0xFFFFFFFF ||
        resourceOffset > 0xFFFFFFFF || totalSize > 0xFFFFFFFF)
    {
        printf("ERROR: Language section is too large.\n");
        return false;
    }

    write16(data, section.eventId);
    write16(data, static_cast<std::uint16_t>(section.pages.size()));
    write32(data, static_cast<std::uint32_t>(codeSize));
    write32(data, static_cast<std::uint32_t>(section.text.size()));
    write32(data, static_cast<std::uint32_t>(textOffset));
    write32(data, 0);
    write32(data, static_cast<std::uint32_t>(resourceOffset));
    write32(data, 0);
    write32(data, 0);

    for (std::uint32_t word : section.code)
        write32(data, word);
    for (std::uint32_t page : section.pages)
        write32(data, page);
    data.insert(data.end(), text.begin(), text.end());
    data.insert(data.end(), resources.begin(), resources.end());
    return true;
}

bool writeScriptBin(const ScriptBin& script, Bytes& output)
{
    std::array<Bytes, EVENT_LANGUAGE_COUNT> languages;
    std::uint64_t offset = 4 + EVENT_LANGUAGE_COUNT * 4;
    output.clear();
    write32(output, EVENT_LANGUAGE_COUNT);
    for (std::size_t i = 0; i < languages.size(); ++i)
    {
        if (!buildLanguage(script.languages[i], languages[i]))
            return false;
        if (offset > 0xFFFFFFFF)
        {
            printf("ERROR: SCRIPT_BIN output is too large.\n");
            return false;
        }
        write32(output, static_cast<std::uint32_t>(offset));
        offset += languages[i].size();
    }
    if (offset > 0xFFFFFFFF)
    {
        printf("ERROR: SCRIPT_BIN output is too large.\n");
        return false;
    }
    for (const Bytes& language : languages)
        output.insert(output.end(), language.begin(), language.end());

    return true;
}
