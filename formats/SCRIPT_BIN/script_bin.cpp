#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <map>
#include <ostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "script_bin.h"
#include "../common.h"

std::uint8_t EventOpcodeDescriptor::byteLength() const
{
    return static_cast<std::uint8_t>(wordCount * 4);
}

static std::string hex(std::uint64_t value, unsigned width)
{
    std::ostringstream output;
    output << std::uppercase << std::hex << std::setfill('0')
           << std::setw(static_cast<int>(width)) << value;
    return output.str();
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

static std::uint32_t readInstructionValue(
    const std::vector<std::uint32_t>& code, std::uint32_t pc,
    std::uint8_t byteOffset, std::uint8_t size)
{
    const std::uint32_t word = code[pc / 4 + byteOffset / 4];
    const unsigned shift = static_cast<unsigned>(byteOffset & 3) * 8;
    const std::uint32_t mask = size == 4 ? 0xFFFFFFFF :
                               (std::uint32_t{1} << (size * 8)) - 1;
    return (word >> shift) & mask;
}

static bool encodingPreservesInstruction(
    const std::vector<std::uint32_t>& code, std::uint32_t pc,
    const EventOpcodeDescriptor& descriptor,
    const EventInstructionEncoding& encoding)
{
    std::array<bool, 0x100> covered{};
    covered[0] = true;
    covered[1] = true;
    for (std::size_t i = 0; i < encoding.operandCount; ++i)
    {
        const EventEncodedOperand& operand = encoding.operands[i];
        const std::uint8_t size = eventOperandSize(operand.encoding);
        if (operand.byteOffset + size > descriptor.byteLength())
            return false;
        for (std::uint8_t byte = 0; byte < size; ++byte)
            covered[operand.byteOffset + byte] = true;
    }

    for (std::uint8_t byte = 2; byte < descriptor.byteLength(); ++byte)
    {
        if (!covered[byte] &&
            readInstructionValue(code, pc, byte, 1) != 0)
        {
            return false;
        }
    }
    return true;
}

template<std::size_t N>
static std::string formatNamedValue(
    std::uint32_t value, const std::array<EventNamedValue, N>& names)
{
    for (const EventNamedValue& named : names)
    {
        if (named.value == value)
            return std::string(named.name);
    }
    return std::to_string(value);
}

template<std::size_t N>
static std::string formatNamedFlags(
    std::uint32_t value, const std::array<EventNamedValue, N>& names)
{
    if (value == 0)
        return "0";

    std::ostringstream output;
    bool first = true;
    std::uint32_t remaining = value;
    for (const EventNamedValue& named : names)
    {
        if ((remaining & named.value) == 0)
            continue;
        if (!first)
            output << " | ";
        output << named.name;
        first = false;
        remaining &= ~named.value;
    }
    if (remaining != 0)
    {
        if (!first)
            output << " | ";
        output << remaining;
    }
    return output.str();
}

static std::string formatFloat(std::uint32_t bits)
{
    const float value = std::bit_cast<float>(bits);
    if (!std::isfinite(value))
        return "f32_bits(" + std::to_string(bits) + ")";

    std::array<char, 64> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(),
                                      value, std::chars_format::general);
    if (result.ec != std::errc())
        return "f32_bits(" + std::to_string(bits) + ")";

    std::string text(buffer.data(), result.ptr);
    if (text.find_first_of(".eE") == std::string::npos)
        text += ".0";
    text += 'f';
    return text;
}

static std::string formatPackedColor(std::uint32_t value)
{
    const std::uint32_t blue = value & 0xFF;
    const std::uint32_t green = value >> 8 & 0xFF;
    const std::uint32_t red = value >> 16 & 0xFF;
    const std::uint32_t alpha = value >> 24;
    return "rgba8(" + std::to_string(red) + ", " +
           std::to_string(green) + ", " + std::to_string(blue) + ", " +
           std::to_string(alpha) + ")";
}

static std::string formatEncodedOperand(
    std::uint32_t value, EventOperandEncoding encoding,
    const std::vector<std::uint32_t>* pages,
    EventOpcode opcode, EventOpcode messageOpcode,
    std::string_view messagePrefix, std::size_t messageCount,
    std::string_view resourcePrefix, std::size_t resourceCount)
{
    switch (encoding)
    {
    case EventOperandEncoding::S8:
        return std::to_string(static_cast<std::int8_t>(value));
    case EventOperandEncoding::U8:
        return std::to_string(value);
    case EventOperandEncoding::S16:
        return std::to_string(static_cast<std::int16_t>(value));
    case EventOperandEncoding::U16:
        return std::to_string(value);
    case EventOperandEncoding::S32:
        return std::to_string(static_cast<std::int32_t>(value));
    case EventOperandEncoding::U32:
        return std::to_string(value);
    case EventOperandEncoding::F32:
        return formatFloat(value);
    case EventOperandEncoding::PageId:
        if (pages != nullptr && value < pages->size())
            return indexName("page_", value);
        return std::to_string(value);
    case EventOperandEncoding::PageIdOrMinusOne:
        if (value != 0xFFFF && pages != nullptr && value < pages->size())
            return indexName("page_", value);
        return std::to_string(static_cast<std::int16_t>(value));
    case EventOperandEncoding::MessageId:
        if ((messageOpcode == EVENT_OP_NONE || opcode == messageOpcode) &&
            value < messageCount)
        {
            return indexName(messagePrefix, value);
        }
        return std::to_string(value);
    case EventOperandEncoding::ResourceId:
        if (value < resourceCount)
            return indexName(resourcePrefix, value);
        return std::to_string(value);
    case EventOperandEncoding::CallFunctionId:
    {
        const std::string_view name = getCallFunctionName(value);
        return name.empty() ? std::to_string(value) : std::string(name);
    }
    case EventOperandEncoding::PackedColorBgra8:
        return formatPackedColor(value);
    case EventOperandEncoding::TextAlignmentFlags:
        return formatNamedFlags(value, EVENT_TEXT_ALIGNMENT_NAMES);
    case EventOperandEncoding::SoundChannel:
        return formatNamedValue(value, EVENT_SOUND_CHANNEL_NAMES);
    case EventOperandEncoding::MessageSubstitutionTarget:
        return formatNamedValue(value, EVENT_MESSAGE_SUBSTITUTION_TARGET_NAMES);
    case EventOperandEncoding::TransitionFlags:
        return formatNamedFlags(value, EVENT_TRANSITION_FLAG_NAMES);
    case EventOperandEncoding::GamepadButtons:
    case EventOperandEncoding::GamepadButtons16:
        return formatNamedFlags(value, EVENT_GAMEPAD_BUTTON_NAMES);
    case EventOperandEncoding::VmValueType:
        return formatNamedValue(value, EVENT_VM_VALUE_TYPE_NAMES);
    }
    return {};
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

    const std::uint32_t firstOffset = read32(data.data() + tableOffset);
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
        const std::uint32_t offset = read32(data.data() + entry);
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
    const std::uint32_t languageCount = read32(data.data());
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
        offsets[i] = read32(data.data() + entry);
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

        const std::uint16_t eventId = read16(data.data() + start);
        const std::uint16_t pageCount = read16(data.data() + start + 2);
        const std::uint32_t codeEnd = read32(data.data() + start + 4);
        const std::uint32_t textCount = read32(data.data() + start + 8);
        const std::uint32_t textOffset = read32(data.data() + start + 0x0C);
        const std::uint32_t reserved0x10 = read32(data.data() + start + 0x10);
        const std::uint32_t resourceOffset = read32(data.data() + start + 0x14);
        const std::uint32_t reserved0x18 = read32(data.data() + start + 0x18);
        const std::uint32_t reserved0x1C = read32(data.data() + start + 0x1C);
        const std::uint64_t bodyOffset = start + sectionHeaderSize;
        const std::uint64_t bodySize = end - bodyOffset;

        LanguageSection& section = sections[i];
        if ((eventId | pageCount | codeEnd | textCount | textOffset | reserved0x10 |
             resourceOffset | reserved0x18 | reserved0x1C) == 0)
        {
            if (bodySize == 0)
            {
                section.empty = true;
                continue;
            }

            if ((bodySize & 3) != 0)
            {
                printf("ERROR: Invalid headerless %s section.\n",
                       EVENT_LANGUAGE_CODES[i].data());
                return false;
            }

            section.headerless = true;
            section.code.reserve(bodySize / 4);
            for (std::uint64_t offset = 0; offset < bodySize; offset += 4)
                section.code.push_back(read32(data.data() + bodyOffset + offset));
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
        if (reserved0x10 != 0 || reserved0x18 != 0 || reserved0x1C != 0)
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
            section.code.push_back(read32(data.data() + word));
        }

        section.pages.reserve(pageCount);
        for (std::uint32_t page = 0; page < pageCount; ++page)
        {
            const std::uint64_t entry = bodyOffset + codeEnd + page * 4;
            const std::uint32_t target = read32(data.data() + entry);
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
            printf("ERROR: Invalid leading %s EVENT.\n",
                   EVENT_LANGUAGE_CODES[i].data());
            return false;
        }
    }

    bool opcodeSetKnown = false;
    for (const LanguageSection& section : sections)
    {
        if (section.empty)
            continue;

        EventOpcodeSet opcodeSet = EventOpcodeSet::Current;
        if (!section.headerless && !section.pages.empty())
        {
            const std::uint16_t marker = static_cast<std::uint16_t>(
                section.code[section.pages.front() / 4]);
            if (marker == eventPageSetOpcode(EventOpcodeSet::Legacy50Exf))
                opcodeSet = EventOpcodeSet::Legacy50Exf;
            else if (marker == eventPageSetOpcode(EventOpcodeSet::Legacy51Exf))
                opcodeSet = EventOpcodeSet::Legacy51Exf;
            else if (marker != EVENT_OP_PAGESET)
            {
                printf("ERROR: Unknown Event VM opcode set.\n");
                return false;
            }

            for (std::uint32_t target : section.pages)
            {
                if (static_cast<std::uint16_t>(section.code[target / 4]) != marker)
                {
                    printf("ERROR: Mixed Event VM opcode sets.\n");
                    return false;
                }
            }
        }

        std::map<std::uint32_t, bool> pageTargets;
        for (std::uint32_t target : section.pages)
            pageTargets.emplace(target, true);
        std::uint32_t pc = 0;
        while (pc < section.code.size() * 4)
        {
            const std::uint16_t serializedOpcode =
                static_cast<std::uint16_t>(section.code[pc / 4]);
            const std::uint16_t opcode =
                decodeEventOpcode(serializedOpcode, opcodeSet);
            if (opcode >= EVENT_OPCODE_DESCRIPTORS.size())
            {
                printf("ERROR: Invalid Event VM opcode.\n");
                return false;
            }
            const EventOpcodeDescriptor& descriptor =
                EVENT_OPCODE_DESCRIPTORS[opcode];
            if (descriptor.wordCount == 0 ||
                descriptor.byteLength() > section.code.size() * 4 - pc)
            {
                printf("ERROR: Invalid Event VM instruction.\n");
                return false;
            }
            const auto nextTarget = pageTargets.upper_bound(pc);
            if (nextTarget != pageTargets.end() &&
                nextTarget->first < pc + descriptor.byteLength())
            {
                printf("ERROR: Page target splits an Event VM instruction.\n");
                return false;
            }
            pc += descriptor.byteLength();
        }

        if (opcodeSetKnown && script.opcodeSet != opcodeSet)
        {
            printf("ERROR: Languages use different Event VM opcode sets.\n");
            return false;
        }
        script.opcodeSet = opcodeSet;
        opcodeSetKnown = true;
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
            if (c < 0x20 || c >= 0x7F)
                output << "\\x" << hex(c, 2);
            else
                output << static_cast<char>(c);
            break;
        }
    }
    output << '"';
    return output.str();
}

static void disassembleCode(
    std::ostream& output, const std::vector<std::uint32_t>& code,
    const std::map<std::uint32_t, std::string>& labels,
    const std::vector<std::uint32_t>* pages,
    EventOpcodeSet opcodeSet,
    EventOpcode messageOpcode, std::string_view messagePrefix,
    std::size_t messageCount,
    std::string_view resourcePrefix, std::size_t resourceCount)
{
    std::uint32_t pc = 0;
    while (pc < code.size() * 4)
    {
        const auto label = labels.find(pc);
        if (label != labels.end())
            output << label->second << ":\n";

        const std::uint32_t word = code[pc / 4];
        const std::uint16_t opcode = decodeEventOpcode(
            static_cast<std::uint16_t>(word), opcodeSet);
        const EventOpcodeDescriptor* descriptor = nullptr;
        if (opcode < EVENT_OPCODE_DESCRIPTORS.size())
        {
            const auto& candidate = EVENT_OPCODE_DESCRIPTORS[opcode];
            if (candidate.wordCount != 0)
                descriptor = &candidate;
        }

        bool raw = descriptor == nullptr ||
                   pc + descriptor->byteLength() > code.size() * 4;
        if (!raw)
        {
            const auto nextLabel = labels.upper_bound(pc);
            if (nextLabel != labels.end() &&
                nextLabel->first < pc + descriptor->byteLength())
            {
                raw = true;
            }
        }

        if (raw)
            return;

        std::vector<std::string> operands;
        const EventInstructionEncoding& encoding =
            EVENT_INSTRUCTION_ENCODINGS[descriptor->opcode];
        if (!encodingPreservesInstruction(code, pc, *descriptor, encoding))
            return;

        bool includeStored = false;
        for (std::size_t i = 0; i < encoding.operandCount; ++i)
        {
            const EventEncodedOperand& operand = encoding.operands[i];
            if (operand.omitWhenZero &&
                readInstructionValue(code, pc, operand.byteOffset,
                                     eventOperandSize(operand.encoding)) != 0)
            {
                includeStored = true;
            }
        }

        operands.reserve(encoding.operandCount);
        for (std::size_t i = 0; i < encoding.operandCount; ++i)
        {
            const EventEncodedOperand& operand = encoding.operands[i];
            if (operand.omitWhenZero && !includeStored)
                continue;
            const std::uint32_t value = readInstructionValue(
                code, pc, operand.byteOffset,
                eventOperandSize(operand.encoding));
            operands.push_back(formatEncodedOperand(
                value, operand.encoding, pages, descriptor->opcode,
                messageOpcode, messagePrefix, messageCount,
                resourcePrefix, resourceCount));
        }

        output << "    ";
        if (!operands.empty())
            output << std::left << std::setw(24) << descriptor->mnemonic;
        else
            output << descriptor->mnemonic;
        for (std::size_t i = 0; i < operands.size(); ++i)
        {
            if (i != 0)
                output << ", ";
            output << operands[i];
        }
        output << '\n';
        pc += descriptor->byteLength();
    }
}

static void disassembleBody(std::ostream& output,
                            const LanguageSection& section,
                            EventOpcodeSet opcodeSet,
                            std::size_t messageCount,
                            std::size_t resourceCount)
{
    if (section.headerless)
    {
        output << ".headerless\n.code\n";
        const std::map<std::uint32_t, std::string> labels;
        disassembleCode(output, section.code, labels, nullptr, opcodeSet,
                        EVENT_OP_NONE, {}, 0, {}, 0);
        return;
    }

    output << ".pages\n";

    std::map<std::uint32_t, std::string> labels;
    for (std::uint32_t target : section.pages)
    {
        if (!labels.contains(target))
        {
            labels.emplace(target, indexName(
                "label_", static_cast<std::uint32_t>(labels.size())));
        }
    }
    for (std::size_t i = 0; i < section.pages.size(); ++i)
    {
        output << indexName("page_", static_cast<std::uint32_t>(i)) << ": "
               << labels.at(section.pages[i]) << '\n';
    }

    output << "\n.code\n";
    disassembleCode(output, section.code, labels, &section.pages, opcodeSet,
                    EVENT_OP_NONE, "text_", messageCount,
                    "resource_", resourceCount);
}

static void disassembleStrings(std::ostream& output,
                               std::string_view prefix,
                               const std::vector<std::string>& strings,
                               std::string_view suffix = {})
{
    for (std::size_t i = 0; i < strings.size(); ++i)
    {
        output << indexName(prefix, static_cast<std::uint32_t>(i)) << suffix
               << ": " << quoteString(strings[i]) << '\n';
    }
}

static void disassembleLanguage(std::ostream& output,
                                const LanguageSection& section,
                                EventOpcodeSet opcodeSet,
                                std::size_t language)
{
    output << ".language " << EVENT_LANGUAGE_CODES[language] << '\n';
    if (section.empty)
    {
        output << ".empty\n.end\n";
        return;
    }

    disassembleBody(output, section, opcodeSet, section.text.size(),
                    section.resources.size());
    if (section.headerless)
    {
        output << ".end\n";
        return;
    }

    output << "\n.text\n";
    disassembleStrings(output, "text_", section.text);

    output << "\n.resources\n";
    disassembleStrings(output, "resource_", section.resources);
    output << ".end\n";
}

static bool sameLocalizedBody(const LanguageSection& left,
                              const LanguageSection& right)
{
    return left.headerless == right.headerless &&
           left.eventId == right.eventId &&
           left.code == right.code && left.pages == right.pages;
}

std::string disassembleScriptBin(const ScriptBin& script)
{
    std::ostringstream output;
    switch (script.opcodeSet)
    {
    case EventOpcodeSet::Legacy50Exf:
        output << ".format localized_exf50\n\n";
        break;
    case EventOpcodeSet::Legacy51Exf:
        output << ".format localized_exf51\n\n";
        break;
    default:
        output << ".format localized\n\n";
        break;
    }

    std::vector<std::size_t> active;
    for (std::size_t i = 0; i < script.languages.size(); ++i)
    {
        if (!script.languages[i].empty)
            active.push_back(i);
    }

    if (active.empty())
    {
        output << ".empty\n.end\n";
        return output.str();
    }

    const LanguageSection& body = script.languages[active.front()];
    const bool sharedBody = std::all_of(
        active.begin(), active.end(), [&](std::size_t language)
        {
            return sameLocalizedBody(body, script.languages[language]);
        });

    if (!sharedBody)
    {
        for (std::size_t i = 0; i < script.languages.size(); ++i)
        {
            if (i != 0)
                output << '\n';
            disassembleLanguage(output, script.languages[i],
                                script.opcodeSet, i);
        }
        return output.str();
    }

    if (active.size() != EVENT_LANGUAGE_COUNT)
    {
        output << ".languages";
        for (std::size_t language : active)
            output << ' ' << EVENT_LANGUAGE_CODES[language];
        output << "\n\n";
    }

    std::size_t messageCount = 0;
    std::size_t resourceCount = 0;
    for (std::size_t language : active)
    {
        messageCount = std::max(messageCount,
                                script.languages[language].text.size());
        resourceCount = std::max(resourceCount,
                                 script.languages[language].resources.size());
    }

    disassembleBody(output, body, script.opcodeSet,
                    messageCount, resourceCount);
    if (body.headerless)
    {
        output << ".end\n";
        return output.str();
    }

    output << "\n.text\n";
    for (std::size_t i = 0; i < messageCount; ++i)
    {
        for (std::size_t language : active)
        {
            const auto& strings = script.languages[language].text;
            if (i < strings.size())
            {
                output << indexName("text_", static_cast<std::uint32_t>(i))
                       << '_' << EVENT_LANGUAGE_CODES[language] << ": "
                       << quoteString(strings[i]) << '\n';
            }
        }
    }

    const bool sharedResources = std::all_of(
        active.begin(), active.end(), [&](std::size_t language)
        {
            return script.languages[language].resources == body.resources;
        });

    output << "\n.resources\n";
    if (sharedResources)
    {
        disassembleStrings(output, "resource_", body.resources);
    }
    else
    {
        for (std::size_t i = 0; i < resourceCount; ++i)
        {
            for (std::size_t language : active)
            {
                const auto& strings = script.languages[language].resources;
                if (i < strings.size())
                {
                    output << indexName("resource_",
                                        static_cast<std::uint32_t>(i))
                           << '_' << EVENT_LANGUAGE_CODES[language] << ": "
                           << quoteString(strings[i]) << '\n';
                }
            }
        }
    }
    output << ".end\n";
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
        if (section.headerless || !section.code.empty() ||
            !section.pages.empty() || !section.text.empty() ||
            !section.resources.empty())
        {
            printf("ERROR: Empty language section contains data.\n");
            return false;
        }
        data.resize(0x20);
        return true;
    }

    if (section.headerless)
    {
        if (!section.pages.empty() || !section.text.empty() ||
            !section.resources.empty())
        {
            printf("ERROR: Headerless language section contains table data.\n");
            return false;
        }
        if (section.code.size() > (0xFFFFFFFF - 0x20) / 4)
        {
            printf("ERROR: Headerless language section is too large.\n");
            return false;
        }
        data.resize(0x20);
        for (std::uint32_t word : section.code)
            write32(data, word);
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

static bool hasLocalizedHeader(const Bytes& data)
{
    constexpr std::uint32_t fileHeaderSize = 4 + EVENT_LANGUAGE_COUNT * 4;
    if (data.size() < fileHeaderSize || read32(data.data()) != EVENT_LANGUAGE_COUNT)
        return false;

    std::uint32_t previous = fileHeaderSize;
    for (std::size_t i = 0; i < EVENT_LANGUAGE_COUNT; ++i)
    {
        const std::uint32_t offset = read32(data.data() + 4 + i * 4);
        if (i == 0 && offset != fileHeaderSize)
            return false;
        if ((i != 0 && static_cast<std::uint64_t>(offset) <
                       static_cast<std::uint64_t>(previous) + 0x20) ||
            offset > data.size())
            return false;
        previous = offset;
    }

    return data.size() - previous >= 0x20;
}

static bool hasValidIndexedEventCode(
    const std::uint8_t* code, std::uint32_t codeSize,
    const std::uint8_t* recordIndex, std::uint16_t recordCount,
    std::uint16_t eventId)
{
    if (codeSize < 4 || read16(code) != EVENT_OP_EVENT ||
        read16(code + 2) != eventId)
    {
        return false;
    }

    std::uint32_t record = 0;
    std::uint32_t pc = 0;
    while (pc < codeSize)
    {
        while (record < recordCount &&
               read32(recordIndex + record * 4) == pc)
        {
            ++record;
        }
        if (record < recordCount && read32(recordIndex + record * 4) < pc)
            return false;

        const std::uint32_t word = read32(code + pc);
        const std::uint16_t opcode = static_cast<std::uint16_t>(word);
        if (opcode >= EVENT_OPCODE_DESCRIPTORS.size())
            return false;
        const EventOpcodeDescriptor& descriptor = EVENT_OPCODE_DESCRIPTORS[opcode];
        if (descriptor.wordCount == 0 ||
            descriptor.byteLength() > codeSize - pc)
        {
            return false;
        }
        pc += descriptor.byteLength();
    }

    return record == recordCount;
}

static bool hasSmsXafHeader(const Bytes& data)
{
    constexpr std::uint32_t headerSize = 0x20;
    if (data.size() < headerSize + 4 ||
        data.size() - headerSize > std::numeric_limits<std::uint32_t>::max())
    {
        return false;
    }

    const std::uint16_t scriptId = read16(data.data());
    const std::uint16_t recordCount = read16(data.data() + 2);
    const std::uint32_t recordIndexOffset = read32(data.data() + 4);
    const std::uint32_t stringCount = read32(data.data() + 8);
    const std::uint32_t stringTableOffset = read32(data.data() + 0x0C);
    const std::uint32_t endOffset = read32(data.data() + 0x14);
    const std::uint8_t* body = data.data() + headerSize;

    if (endOffset != data.size() - headerSize || recordIndexOffset < 4 ||
        (recordIndexOffset & 3) != 0 || read32(data.data() + 0x10) != 0 ||
        read32(data.data() + 0x18) != 0 || read32(data.data() + 0x1C) != 0)
    {
        return false;
    }

    const std::uint64_t expectedStringTable =
        static_cast<std::uint64_t>(recordIndexOffset) +
        static_cast<std::uint64_t>(recordCount) * 4;
    if (expectedStringTable != stringTableOffset || stringTableOffset > endOffset)
        return false;

    std::uint32_t previous = 4;
    for (std::uint32_t i = 0; i < recordCount; ++i)
    {
        const std::uint32_t offset = read32(body + recordIndexOffset + i * 4);
        if ((offset & 3) != 0 || offset < previous || offset >= recordIndexOffset)
            return false;
        if (i != 0 && offset == previous)
            return false;
        previous = offset;
    }
    if (!hasValidIndexedEventCode(body, recordIndexOffset,
                                  body + recordIndexOffset,
                                  recordCount, scriptId))
    {
        return false;
    }

    if (stringCount == 0)
        return stringTableOffset == endOffset;

    const std::uint64_t stringIndexSize =
        static_cast<std::uint64_t>(stringCount) * 4;
    if (stringIndexSize >= endOffset - stringTableOffset)
        return false;

    const std::uint8_t* table = body + stringTableOffset;
    std::uint32_t stringOffset = read32(table);
    if (stringOffset != stringIndexSize)
        return false;

    for (std::uint32_t i = 0; i < stringCount; ++i)
    {
        const std::uint32_t nextOffset = i + 1 < stringCount ?
            read32(table + (i + 1) * 4) : endOffset - stringTableOffset;
        if (stringOffset >= nextOffset || nextOffset > endOffset - stringTableOffset)
            return false;

        const std::uint8_t* begin = table + stringOffset;
        const std::uint8_t* end = table + nextOffset;
        const std::uint8_t* terminator = std::find(begin, end, 0);
        if (terminator == end)
            return false;

        if (i + 1 < stringCount)
        {
            if (terminator + 1 != end)
                return false;
        }
        else
        {
            if (end - (terminator + 1) > 3 ||
                std::find_if(terminator + 1, end,
                             [](std::uint8_t value) { return value != 0; }) != end)
            {
                return false;
            }
        }
        stringOffset = nextOffset;
    }

    return true;
}

bool readSmsXaf(const Bytes& data, SmsXaf& sms)
{
    constexpr std::uint32_t headerSize = 0x20;
    if (!hasSmsXafHeader(data))
    {
        printf("ERROR: Invalid SMS_XAF header or layout.\n");
        return false;
    }

    sms = {};
    sms.scriptId = read16(data.data());
    const std::uint16_t recordCount = read16(data.data() + 2);
    const std::uint32_t recordIndexOffset = read32(data.data() + 4);
    const std::uint32_t stringCount = read32(data.data() + 8);
    const std::uint32_t stringTableOffset = read32(data.data() + 0x0C);
    const std::uint32_t endOffset = read32(data.data() + 0x14);

    const std::uint8_t* body = data.data() + headerSize;
    sms.code.reserve(recordIndexOffset / 4);
    for (std::uint32_t offset = 0; offset < recordIndexOffset; offset += 4)
        sms.code.push_back(read32(body + offset));

    sms.recordOffsets.reserve(recordCount);
    for (std::uint32_t i = 0; i < recordCount; ++i)
        sms.recordOffsets.push_back(read32(body + recordIndexOffset + i * 4));

    const std::uint8_t* table = body + stringTableOffset;
    sms.strings.reserve(stringCount);
    for (std::uint32_t i = 0; i < stringCount; ++i)
    {
        const std::uint32_t begin = read32(table + i * 4);
        const std::uint32_t end = i + 1 < stringCount ?
            read32(table + (i + 1) * 4) : endOffset - stringTableOffset;
        const std::uint8_t* terminator =
            std::find(table + begin, table + end, 0);
        sms.strings.emplace_back(
            reinterpret_cast<const char*>(table + begin),
            static_cast<std::size_t>(terminator - (table + begin)));
    }

    return true;
}

bool writeSmsXaf(const SmsXaf& sms, Bytes& output)
{
    if (sms.recordOffsets.size() > 0xFFFF ||
        sms.strings.size() > 0xFFFFFFFF)
    {
        printf("ERROR: Invalid SMS_XAF count.\n");
        return false;
    }
    if (sms.code.empty() || sms.code.size() > 0xFFFFFFFF / 4 ||
        (sms.code.front() & 0xFFFF) != EVENT_OP_EVENT ||
        (sms.code.front() >> 16) != sms.scriptId)
    {
        printf("ERROR: Invalid leading SMS_XAF EVENT.\n");
        return false;
    }

    Bytes body;
    for (std::uint32_t word : sms.code)
        write32(body, word);

    std::uint32_t previous = 0;
    for (std::size_t i = 0; i < sms.recordOffsets.size(); ++i)
    {
        const std::uint32_t offset = sms.recordOffsets[i];
        if ((offset & 3) != 0 || offset < 4 || offset >= body.size() ||
            (i != 0 && offset <= previous))
        {
            printf("ERROR: Invalid SMS_XAF record offset.\n");
            return false;
        }
        previous = offset;
    }

    const std::uint32_t recordIndexOffset =
        static_cast<std::uint32_t>(body.size());
    if (sms.recordOffsets.size() > (0xFFFFFFFF - body.size()) / 4)
    {
        printf("ERROR: SMS_XAF record index is too large.\n");
        return false;
    }
    for (std::uint32_t offset : sms.recordOffsets)
        write32(body, offset);
    if (!hasValidIndexedEventCode(
            body.data(), recordIndexOffset, body.data() + recordIndexOffset,
            static_cast<std::uint16_t>(sms.recordOffsets.size()),
            sms.scriptId))
    {
        printf("ERROR: Invalid SMS_XAF Event VM code.\n");
        return false;
    }

    if (body.size() > 0xFFFFFFFF)
    {
        printf("ERROR: SMS_XAF record index is too large.\n");
        return false;
    }
    const std::uint32_t stringTableOffset =
        static_cast<std::uint32_t>(body.size());

    Bytes strings;
    for (const std::string& value : sms.strings)
    {
        if (value.find('\0') != std::string::npos)
        {
            printf("ERROR: SMS string contains an embedded NUL.\n");
            return false;
        }
    }
    if (!buildStringTable(sms.strings, strings) ||
        strings.size() > 0xFFFFFFFF - body.size())
    {
        printf("ERROR: SMS_XAF string table is too large.\n");
        return false;
    }
    body.insert(body.end(), strings.begin(), strings.end());

    output.clear();
    write16(output, sms.scriptId);
    write16(output, static_cast<std::uint16_t>(sms.recordOffsets.size()));
    write32(output, recordIndexOffset);
    write32(output, static_cast<std::uint32_t>(sms.strings.size()));
    write32(output, stringTableOffset);
    write32(output, 0);
    write32(output, static_cast<std::uint32_t>(body.size()));
    write32(output, 0);
    write32(output, 0);
    output.insert(output.end(), body.begin(), body.end());
    return true;
}

std::string disassembleSmsXaf(const SmsXaf& sms)
{
    std::ostringstream output;
    output << ".format sms_xaf\n\n.records\n";

    std::map<std::uint32_t, std::string> labels;
    for (std::uint32_t offset : sms.recordOffsets)
    {
        if (!labels.contains(offset))
        {
            labels.emplace(offset, indexName(
                "label_", static_cast<std::uint32_t>(labels.size())));
        }
    }
    for (std::size_t i = 0; i < sms.recordOffsets.size(); ++i)
    {
        output << indexName("record_", static_cast<std::uint32_t>(i)) << ": "
               << labels.at(sms.recordOffsets[i]) << '\n';
    }

    output << "\n.code\n";
    disassembleCode(output, sms.code, labels, nullptr,
                    EventOpcodeSet::Current,
                    EVENT_OP_EXF_LOG_MESSAGE,
                    "string_", sms.strings.size(), {}, 0);

    output << "\n.strings\n";
    for (std::size_t i = 0; i < sms.strings.size(); ++i)
    {
        output << indexName("string_", static_cast<std::uint32_t>(i)) << ": "
               << quoteString(sms.strings[i]) << '\n';
    }
    output << ".end\n";
    return output.str();
}

bool detectEventScriptFormat(const Bytes& data, EventScriptFormat& format)
{
    const bool localized = hasLocalizedHeader(data);
    const bool sms = hasSmsXafHeader(data);
    if (localized == sms)
        return false;

    format = localized ? EventScriptFormat::Localized : EventScriptFormat::SmsXaf;
    return true;
}

bool readEventScript(const Bytes& data, EventScript& script)
{
    EventScriptFormat format;
    if (!detectEventScriptFormat(data, format))
    {
        printf("ERROR: Unrecognized Event VM container.\n");
        return false;
    }

    script = {};
    script.format = format;
    return format == EventScriptFormat::Localized ?
        readScriptBin(data, script.localized) : readSmsXaf(data, script.sms);
}

bool writeEventScript(const EventScript& script, Bytes& data)
{
    return script.format == EventScriptFormat::Localized ?
        writeScriptBin(script.localized, data) : writeSmsXaf(script.sms, data);
}

std::string disassembleEventScript(const EventScript& script)
{
    return script.format == EventScriptFormat::Localized ?
        disassembleScriptBin(script.localized) : disassembleSmsXaf(script.sms);
}
