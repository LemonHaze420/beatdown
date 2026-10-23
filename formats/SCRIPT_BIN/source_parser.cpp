#include <algorithm>
#include <cerrno>
#include <bit>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "source_parser.h"
static std::string trim(std::string_view text)
{
    std::size_t begin = 0;
    while (begin < text.size() &&
           std::isspace(static_cast<unsigned char>(text[begin])))
    {
        ++begin;
    }
    std::size_t end = text.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(text[end - 1])))
    {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}
static bool parseNumber(const std::string& text, std::uint32_t maximum,
                        std::uint32_t& output)
{
    if (text.empty() || text[0] == '-')
        return false;
    errno = 0;
    char* end = nullptr;
    const unsigned long long value = std::strtoull(text.c_str(), &end, 0);
    if (errno == ERANGE || end != text.c_str() + text.size() || value > maximum)
        return false;
    output = static_cast<std::uint32_t>(value);
    return true;
}
static std::string stripComment(std::string_view line)
{
    bool quoted = false;
    bool escaped = false;
    for (std::size_t i = 0; i < line.size(); ++i)
    {
        const char c = line[i];
        if (quoted && escaped)
        {
            escaped = false;
            continue;
        }
        if (quoted && c == '\\')
        {
            escaped = true;
            continue;
        }
        if (c == '"')
        {
            quoted = !quoted;
            continue;
        }
        if (!quoted && (c == ';' || c == '#'))
            return std::string(line.substr(0, i));
    }
    return std::string(line);
}
static bool validLabel(std::string_view label)
{
    if (label.empty() ||
        !(std::isalpha(static_cast<unsigned char>(label[0])) || label[0] == '_'))
    {
        return false;
    }
    for (std::size_t i = 1; i < label.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(label[i]);
        if (!std::isalnum(c) && c != '_')
            return false;
    }
    return true;
}
static const EventOpcodeDescriptor* findOpcode(std::string_view mnemonic)
{
    for (const auto& descriptor : EVENT_OPCODE_DESCRIPTORS)
    {
        if (descriptor.opcode != EVENT_OP_NONE && descriptor.mnemonic == mnemonic)
            return &descriptor;
    }
    return nullptr;
}
static bool parseCallFunction(std::string_view text, std::uint32_t& id)
{
    for (std::size_t i = 0; i < EVENT_CALL_FUNCTION_NAMES.size(); ++i)
    {
        if (text == EVENT_CALL_FUNCTION_NAMES[i] ||
            text == EVENT_CALL_CALLBACK_NAMES[i])
        {
            id = static_cast<std::uint32_t>(i);
            return true;
        }
    }
    return false;
}
static bool parseIndexName(std::string_view text, std::string_view prefix,
                           std::uint32_t& index)
{
    if (!text.starts_with(prefix) || text.size() == prefix.size())
        return false;
    std::uint64_t value = 0;
    for (const unsigned char c : text.substr(prefix.size()))
    {
        if (!std::isdigit(c))
            return false;
        value = value * 10 + (c - '0');
        if (value > 0xFFFFFFFF)
            return false;
    }
    index = static_cast<std::uint32_t>(value);
    return true;
}
static bool parseQuotedString(const std::string& text, std::string& value)
{
    if (text.size() < 2 || text.front() != '"')
        return false;
    value.clear();
    std::size_t i = 1;
    for (; i < text.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(text[i]);
        if (c == '"')
        {
            ++i;
            break;
        }
        if (c != '\\')
        {
            value.push_back(static_cast<char>(c));
            continue;
        }
        if (++i >= text.size())
            return false;
        switch (text[i])
        {
        case '\\': value.push_back('\\'); break;
        case '"':  value.push_back('"'); break;
        case 'n':  value.push_back('\n'); break;
        case 'r':  value.push_back('\r'); break;
        case 't':  value.push_back('\t'); break;
        case 'x':
        {
            if (i + 2 >= text.size() ||
                !std::isxdigit(static_cast<unsigned char>(text[i + 1])) ||
                !std::isxdigit(static_cast<unsigned char>(text[i + 2])))
            {
                return false;
            }
            std::uint32_t byte = 0;
            if (!parseNumber("0x" + text.substr(i + 1, 2), 0xFF, byte))
                return false;
            value.push_back(static_cast<char>(byte));
            i += 2;
            break;
        }
        default:
            return false;
        }
    }
    if (i == 0 || i > text.size() || text[i - 1] != '"' || !trim(text.substr(i)).empty())
        return false;
    return true;
}
static bool splitOperands(const std::string& text,
                          std::vector<std::string>& operands)
{
    operands.clear();
    if (trim(text).empty())
        return true;
    std::size_t begin = 0;
    unsigned depth = 0;
    for (std::size_t i = 0; i <= text.size(); ++i)
    {
        if (i < text.size() && text[i] == '(')
        {
            ++depth;
            continue;
        }
        if (i < text.size() && text[i] == ')')
        {
            if (depth == 0)
                return false;
            --depth;
            continue;
        }
        if (i < text.size() && (text[i] != ',' || depth != 0))
            continue;
        if (i == text.size() && depth != 0)
            return false;
        const std::string operand = trim(text.substr(begin, i - begin));
        if (operand.empty())
            return false;
        operands.push_back(operand);
        begin = i + 1;
    }
    return true;
}
static bool parseSignedBits(const std::string& text, std::uint8_t size,
                            std::uint32_t& output)
{
    const unsigned bits = static_cast<unsigned>(size) * 8;
    const std::uint32_t mask = size == 4 ? 0xFFFFFFFF :
                               (std::uint32_t{1} << bits) - 1;
    if (!text.empty() && text[0] == '-')
    {
        errno = 0;
        char* end = nullptr;
        const long long value = std::strtoll(text.c_str(), &end, 0);
        const long long minimum = size == 4 ?
            std::numeric_limits<std::int32_t>::min() : -(1LL << (bits - 1));
        if (errno == ERANGE || end != text.c_str() + text.size() ||
            value < minimum)
        {
            return false;
        }
        output = static_cast<std::uint32_t>(value) & mask;
        return true;
    }
    return parseNumber(text, mask, output);
}
static bool parseFloatBits(const std::string& text, std::uint32_t& output)
{
    if (text.starts_with("f32_bits(") && text.ends_with(')'))
    {
        return parseNumber(text.substr(9, text.size() - 10),
                           0xFFFFFFFF, output);
    }
    if (text.starts_with("f32(0x") && text.ends_with(')'))
    {
        return parseNumber(text.substr(4, text.size() - 5),
                           0xFFFFFFFF, output);
    }
    if (text.starts_with("0x") &&
        text.find_first_of("pP") == std::string::npos)
    {
        return parseNumber(text, 0xFFFFFFFF, output);
    }
    std::string valueText = text;
    if (!valueText.empty() && (valueText.back() == 'f' || valueText.back() == 'F'))
        valueText.pop_back();
    if (valueText.empty())
        return false;
    errno = 0;
    char* end = nullptr;
    const float value = std::strtof(valueText.c_str(), &end);
    if (end != valueText.c_str() + valueText.size() || !std::isfinite(value))
        return false;
    output = std::bit_cast<std::uint32_t>(value);
    return true;
}
template<std::size_t N>
static bool parseNamedValue(const std::string& text,
                            const std::array<EventNamedValue, N>& names,
                            std::uint32_t maximum, std::uint32_t& output)
{
    for (const EventNamedValue& named : names)
    {
        if (text == named.name)
        {
            output = named.value;
            return true;
        }
    }
    return parseNumber(text, maximum, output);
}
template<std::size_t N>
static bool parseNamedFlags(const std::string& text,
                            const std::array<EventNamedValue, N>& names,
                            std::uint32_t maximum, std::uint32_t& output)
{
    output = 0;
    std::size_t begin = 0;
    while (begin < text.size())
    {
        const std::size_t separator = text.find('|', begin);
        const std::string part = trim(text.substr(begin, separator - begin));
        std::uint32_t value = 0;
        if (part.empty() || !parseNamedValue(part, names, maximum, value))
            return false;
        output |= value;
        if (separator == std::string::npos)
            return true;
        begin = separator + 1;
    }
    return false;
}
static bool parsePackedColor(const std::string& text, std::uint32_t& output)
{
    if (!text.starts_with("rgba8(") || !text.ends_with(')'))
        return parseNumber(text, 0xFFFFFFFF, output);
    std::vector<std::string> components;
    if (!splitOperands(text.substr(6, text.size() - 7), components) ||
        components.size() != 4)
    {
        return false;
    }
    std::array<std::uint32_t, 4> rgba{};
    for (std::size_t i = 0; i < rgba.size(); ++i)
    {
        if (!parseNumber(components[i], 0xFF, rgba[i]))
            return false;
    }
    output = rgba[2] | rgba[1] << 8 | rgba[0] << 16 | rgba[3] << 24;
    return true;
}
static bool parseEncodedOperand(
    const std::string& text, EventOperandEncoding encoding,
    std::size_t pageCount, std::string_view pagePrefix,
    EventOpcode opcode, EventOpcode messageOpcode,
    std::string_view messagePrefix, std::string_view resourcePrefix,
    std::uint32_t& output)
{
    switch (encoding)
    {
    case EventOperandEncoding::U32:
        return parseNumber(text, 0xFFFFFFFF, output);
    case EventOperandEncoding::S8:
        return parseSignedBits(text, 1, output);
    case EventOperandEncoding::U8:
        return parseNumber(text, 0xFF, output);
    case EventOperandEncoding::S16:
        return parseSignedBits(text, 2, output);
    case EventOperandEncoding::U16:
        return parseNumber(text, 0xFFFF, output);
    case EventOperandEncoding::S32:
        return parseSignedBits(text, 4, output);
    case EventOperandEncoding::F32:
        return parseFloatBits(text, output);
    case EventOperandEncoding::PageId:
    case EventOperandEncoding::PageIdOrMinusOne:
        if (!pagePrefix.empty() && parseIndexName(text, pagePrefix, output))
            return output < pageCount;
        if (encoding == EventOperandEncoding::PageIdOrMinusOne)
            return parseSignedBits(text, 2, output);
        return parseNumber(text, 0xFFFF, output);
    case EventOperandEncoding::MessageId:
        if (!messagePrefix.empty() &&
            (messageOpcode == EVENT_OP_NONE || opcode == messageOpcode) &&
            parseIndexName(text, messagePrefix, output))
        {
            return true;
        }
        return parseNumber(text, 0xFFFFFFFF, output);
    case EventOperandEncoding::ResourceId:
        if (!resourcePrefix.empty() && parseIndexName(text, resourcePrefix, output))
            return true;
        return parseNumber(text, 0xFFFFFFFF, output);
    case EventOperandEncoding::CallFunctionId:
        return parseCallFunction(text, output) ||
               parseNumber(text, 0xFFFFFFFF, output);
    case EventOperandEncoding::PackedColorBgra8:
        return parsePackedColor(text, output);
    case EventOperandEncoding::TextAlignmentFlags:
        return parseNamedFlags(text, EVENT_TEXT_ALIGNMENT_NAMES, 0xFF, output);
    case EventOperandEncoding::SoundChannel:
        return parseNamedValue(text, EVENT_SOUND_CHANNEL_NAMES, 0xFF, output);
    case EventOperandEncoding::MessageSubstitutionTarget:
        return parseNamedValue(text, EVENT_MESSAGE_SUBSTITUTION_TARGET_NAMES,
                               0xFFFFFFFF, output);
    case EventOperandEncoding::TransitionFlags:
        return parseNamedFlags(text, EVENT_TRANSITION_FLAG_NAMES,
                               0xFFFFFFFF, output);
    case EventOperandEncoding::GamepadButtons:
    case EventOperandEncoding::GamepadButtons16:
        return parseNamedFlags(text, EVENT_GAMEPAD_BUTTON_NAMES,
                               encoding == EventOperandEncoding::GamepadButtons16 ?
                               0xFFFF : 0xFFFFFFFF, output);
    case EventOperandEncoding::VmValueType:
        return parseNamedValue(text, EVENT_VM_VALUE_TYPE_NAMES,
                               0xFFFFFFFF, output);
    }
    return false;
}
static void writeInstructionValue(std::vector<std::uint32_t>& words,
                                  std::uint8_t byteOffset,
                                  std::uint8_t size, std::uint32_t value)
{
    const std::size_t wordIndex = byteOffset / 4;
    const unsigned shift = static_cast<unsigned>(byteOffset & 3) * 8;
    const std::uint32_t mask = size == 4 ? 0xFFFFFFFF :
                               (std::uint32_t{1} << (size * 8)) - 1;
    words[wordIndex] &= ~(mask << shift);
    words[wordIndex] |= (value & mask) << shift;
}
static bool encodeEventInstruction(
    const EventOpcodeDescriptor& descriptor,
    const std::vector<std::string>& operands,
    EventOpcodeSet opcodeSet,
    std::size_t pageCount, std::string_view pagePrefix,
    EventOpcode messageOpcode, std::string_view messagePrefix,
    std::string_view resourcePrefix, std::vector<std::uint32_t>& words)
{
    const EventInstructionEncoding& encoding =
        EVENT_INSTRUCTION_ENCODINGS[descriptor.opcode];
    if (!encoding.dispatched)
        return false;
    std::size_t requiredCount = 0;
    for (std::size_t i = 0; i < encoding.operandCount; ++i)
        requiredCount += !encoding.operands[i].omitWhenZero;
    const bool includeStored = operands.size() == encoding.operandCount;
    if (!includeStored && operands.size() != requiredCount)
        return false;
    words.assign(descriptor.wordCount, 0);
    words[0] = encodeEventOpcode(descriptor.opcode, opcodeSet);
    std::size_t sourceOperand = 0;
    for (std::size_t i = 0; i < encoding.operandCount; ++i)
    {
        const EventEncodedOperand& operand = encoding.operands[i];
        if (operand.omitWhenZero && !includeStored)
            continue;
        std::uint32_t value = 0;
        if (!parseEncodedOperand(operands[sourceOperand], operand.encoding,
                                 pageCount, pagePrefix, descriptor.opcode,
                                 messageOpcode, messagePrefix, resourcePrefix,
                                 value))
        {
            return false;
        }
        writeInstructionValue(words, operand.byteOffset,
                              eventOperandSize(operand.encoding), value);
        ++sourceOperand;
    }
    return true;
}
static const char* appendEventInstruction(
    const std::string& line, std::vector<std::uint32_t>& code,
    EventOpcodeSet opcodeSet,
    std::size_t pageCount, std::string_view pagePrefix,
    EventOpcode messageOpcode, std::string_view messagePrefix,
    std::string_view resourcePrefix)
{
    const std::size_t space = line.find_first_of(" \t");
    const std::string mnemonic = line.substr(0, space);
    const std::string argumentText = space == std::string::npos ?
                                     std::string() : trim(line.substr(space + 1));
    const EventOpcodeDescriptor* descriptor = findOpcode(mnemonic);
    if (descriptor == nullptr)
        return "Unknown opcode";
    if (!supportsEventOpcode(descriptor->opcode, opcodeSet))
        return "Opcode is not available in this Event VM opcode set";
    if (code.size() > 0xFFFFFFFF / 4 - descriptor->wordCount)
        return "Code is too large";
    std::vector<std::string> operands;
    if (!splitOperands(argumentText, operands))
        return "Invalid operand list";
    std::vector<std::uint32_t> words;
    if (encodeEventInstruction(*descriptor, operands, opcodeSet,
                               pageCount, pagePrefix,
                               messageOpcode, messagePrefix,
                               resourcePrefix, words))
    {
        code.insert(code.end(), words.begin(), words.end());
        return nullptr;
    }
    return "Invalid operands";
}
SourceParser::SourceParser(std::string_view input)
    : source(input)
{
    activeLanguages.fill(true);
}
bool SourceParser::parse(ScriptBin& output)
{
    std::istringstream input(source);
    std::string rawLine;
    while (std::getline(input, rawLine))
    {
        ++lineNumber;
        if (!rawLine.empty() && rawLine.back() == '\r')
            rawLine.pop_back();
        if (!parseLine(trim(stripComment(rawLine))))
            return false;
    }
    if (form == Form::Shared)
    {
        if (!sharedEnded)
            return compile_error("Missing .end");
    }
    else if (form == Form::Explicit)
    {
        if (currentLanguage >= 0)
            return compile_error("Missing .end");
        if (std::find(seen.begin(), seen.end(), false) != seen.end())
            return compile_error("Expected six languages");
    }
    else
    {
        return compile_error("Missing localized script body");
    }
    output.opcodeSet = opcodeSet;
    output.languages = sections;
    return true;
}
bool SourceParser::compile_error(const char* message) const
{
    printf("ERROR: Line %zu: %s.\n", lineNumber, message);
    return false;
}
int SourceParser::languageIndex(std::string_view code)
{
    for (std::size_t i = 0; i < EVENT_LANGUAGE_CODES.size(); ++i)
    {
        if (EVENT_LANGUAGE_CODES[i] == code)
            return static_cast<int>(i);
    }
    return -1;
}
LanguageSection* SourceParser::current()
{
    if (currentLanguage >= 0)
        return &sections[static_cast<std::size_t>(currentLanguage)];
    if (form == Form::Shared && !sharedEnded)
        return &sharedSection;
    compile_error("Directive outside a script body");
    return nullptr;
}
bool SourceParser::selectSharedForm()
{
    if (form == Form::Explicit)
        return compile_error("Cannot mix shared and explicit language forms");
    if (sharedEnded)
        return compile_error("Content after .end");
    form = Form::Shared;
    return true;
}
bool SourceParser::beginLanguage(const std::string& code)
{
    if (form == Form::Shared)
        return compile_error("Cannot mix shared and explicit language forms");
    if (currentLanguage >= 0)
        return compile_error("Missing .end before the next language");
    const int index = languageIndex(code);
    if (index < 0)
        return compile_error("Unknown language");
    if (seen[static_cast<std::size_t>(index)])
        return compile_error("Duplicate language");
    form = Form::Explicit;
    seen[static_cast<std::size_t>(index)] = true;
    currentLanguage = index;
    part = Part::None;
    labels.clear();
    pageLabels.clear();
    return true;
}
bool SourceParser::finishSection(LanguageSection& section)
{
    if (section.empty && section.headerless)
        return compile_error("Script body cannot be empty and headerless");
    if (section.headerless)
    {
        if (!pageLabels.empty() || !section.pages.empty() ||
            !section.text.empty() || !section.resources.empty())
        {
            return compile_error("Headerless script body contains table data");
        }
        if (section.code.empty())
            return compile_error("Headerless script body has no code words");
    }
    else if (!section.empty)
    {
        for (const std::string& label : pageLabels)
        {
            const auto found = labels.find(label);
            if (found == labels.end())
                return compile_error("Undefined page label");
            section.pages.push_back(found->second);
        }
        if (section.pages.size() > 0xFFFF)
            return compile_error("Too many pages");
        if (section.code.empty() ||
            (section.code.front() & 0xFFFF) != EVENT_OP_EVENT)
            return compile_error("Code must begin with EVENT");
        section.eventId = static_cast<std::uint16_t>(section.code.front() >> 16);
    }
    else if (!pageLabels.empty() || !section.code.empty() ||
             !section.text.empty() || !section.resources.empty())
    {
        return compile_error("Empty script body contains data");
    }
    return true;
}
bool SourceParser::endLanguage()
{
    LanguageSection* section = current();
    if (section == nullptr || !finishSection(*section))
        return false;
    currentLanguage = -1;
    part = Part::None;
    return true;
}
bool SourceParser::endShared()
{
    if (form != Form::Shared || sharedEnded)
        return compile_error("Invalid .end directive");
    for (std::size_t i = 0; i < sections.size(); ++i)
    {
        if ((!sections[i].text.empty() || !sections[i].resources.empty()) &&
            (sharedSection.empty || sharedSection.headerless))
        {
            return compile_error("Script body without tables contains localized strings");
        }
    }
    if (!finishSection(sharedSection))
        return false;
    for (std::size_t i = 0; i < sections.size(); ++i)
    {
        std::vector<std::string> text = std::move(sections[i].text);
        std::vector<std::string> resources = std::move(sections[i].resources);
        sections[i] = {};
        sections[i].empty = true;
        if (!activeLanguages[i])
            continue;
        sections[i] = sharedSection;
        if (textForm == StringTableForm::Localized)
            sections[i].text = std::move(text);
        if (resourceForm == StringTableForm::Localized)
            sections[i].resources = std::move(resources);
    }
    sharedEnded = true;
    part = Part::None;
    return true;
}
bool SourceParser::parseLanguages(const std::string& argument)
{
    if (!selectSharedForm() || activeLanguagesSeen || argument.empty() ||
        part != Part::None || !sharedSection.code.empty() ||
        !sharedSection.pages.empty() || !sharedSection.text.empty() ||
        !sharedSection.resources.empty() || sharedSection.empty ||
        sharedSection.headerless || !pageLabels.empty() || !labels.empty())
    {
        return compile_error("Invalid .languages directive");
    }
    activeLanguages.fill(false);
    std::istringstream input(argument);
    std::string code;
    while (input >> code)
    {
        const int index = languageIndex(code);
        if (index < 0)
            return compile_error("Unknown language");
        if (activeLanguages[static_cast<std::size_t>(index)])
            return compile_error("Duplicate language");
        activeLanguages[static_cast<std::size_t>(index)] = true;
    }
    activeLanguagesSeen = true;
    return true;
}
bool SourceParser::parseDirective(const std::string& line)
{
    const std::size_t space = line.find_first_of(" \t");
    const std::string directive = line.substr(0, space);
    const std::string argument = space == std::string::npos ?
                                 std::string() : trim(line.substr(space + 1));
    if (directive == ".format")
    {
        if (form != Form::Unknown || formatSeen)
        {
            return compile_error("Invalid .format directive");
        }
        if (argument == "localized")
            opcodeSet = EventOpcodeSet::Current;
        else if (argument == "localized_exf50")
            opcodeSet = EventOpcodeSet::Legacy50Exf;
        else if (argument == "localized_exf51")
            opcodeSet = EventOpcodeSet::Legacy51Exf;
        else
            return compile_error("Invalid .format directive");
        formatSeen = true;
        return true;
    }
    if (directive == ".language")
    {
        if (argument.empty())
            return compile_error(".language requires a code");
        return beginLanguage(argument);
    }
    if (directive == ".languages")
        return parseLanguages(argument);
    if (form == Form::Unknown && !selectSharedForm())
        return false;
    if (directive == ".end")
    {
        if (!argument.empty())
            return compile_error(".end does not take an argument");
        return form == Form::Shared ? endShared() : endLanguage();
    }
    LanguageSection* section = current();
    if (section == nullptr)
        return false;
    if (directive == ".empty")
    {
        if (!argument.empty())
            return compile_error("Invalid .empty directive");
        section->empty = true;
    }
    else if (directive == ".headerless")
    {
        if (!argument.empty())
            return compile_error("Invalid .headerless directive");
        section->headerless = true;
    }
    else if (directive == ".pages")
    {
        if (!argument.empty())
            return compile_error(".pages does not take an argument");
        part = Part::Pages;
    }
    else if (directive == ".code")
    {
        if (!argument.empty())
            return compile_error(".code does not take an argument");
        part = Part::Code;
    }
    else if (directive == ".text")
    {
        if (!argument.empty())
            return compile_error(".text does not take an argument");
        part = Part::Text;
    }
    else if (directive == ".resources")
    {
        if (!argument.empty())
            return compile_error(".resources does not take an argument");
        part = Part::Resources;
    }
    else
    {
        return compile_error("Unknown directive");
    }
    return true;
}
bool SourceParser::parseInstruction(const std::string& line)
{
    LanguageSection* section = current();
    if (section == nullptr)
        return false;
    const char* error = appendEventInstruction(
        line, section->code, opcodeSet,
        pageLabels.size(), "page_", EVENT_OP_NONE,
        "text_", "resource_");
    return error == nullptr ? true : compile_error(error);
}
bool SourceParser::parseStringEntry(const std::string& line,
                                    std::string_view prefix)
{
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos)
        return compile_error("Invalid string entry");
    const std::string name = trim(line.substr(0, colon));
    std::string value;
    if (!parseQuotedString(trim(line.substr(colon + 1)), value))
        return compile_error("Invalid quoted string");
    LanguageSection* section = current();
    if (section == nullptr)
        return false;
    std::vector<std::string>& common = part == Part::Text ?
        section->text : section->resources;
    if (form == Form::Explicit)
    {
        std::uint32_t index = 0;
        if (!parseIndexName(name, prefix, index) || index != common.size())
            return compile_error("Invalid string entry");
        common.push_back(std::move(value));
        return true;
    }
    StringTableForm& tableForm = part == Part::Text ? textForm : resourceForm;
    std::uint32_t index = 0;
    if (parseIndexName(name, prefix, index))
    {
        if (tableForm == StringTableForm::Localized || index != common.size())
            return compile_error("Invalid shared string entry");
        tableForm = StringTableForm::Shared;
        common.push_back(std::move(value));
        return true;
    }
    if (name.size() < 3 || name[name.size() - 3] != '_')
        return compile_error("Invalid localized string entry");
    const int language = languageIndex(
        std::string_view(name).substr(name.size() - 2));
    if (language < 0 || !activeLanguages[static_cast<std::size_t>(language)] ||
        !parseIndexName(std::string_view(name).substr(0, name.size() - 3),
                        prefix, index))
    {
        return compile_error("Invalid localized string entry");
    }
    std::vector<std::string>& localized = part == Part::Text ?
        sections[static_cast<std::size_t>(language)].text :
        sections[static_cast<std::size_t>(language)].resources;
    if (tableForm == StringTableForm::Shared || index != localized.size())
        return compile_error("Invalid localized string entry");
    tableForm = StringTableForm::Localized;
    localized.push_back(std::move(value));
    return true;
}
bool SourceParser::parseLine(const std::string& line)
{
    if (line.empty())
        return true;
    if (line[0] == '.')
        return parseDirective(line);
    if (form == Form::Unknown && !selectSharedForm())
        return false;
    LanguageSection* section = current();
    if (section == nullptr)
        return false;
    switch (part)
    {
    case Part::Pages:
    {
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos)
            return compile_error("Invalid page entry");
        const std::string page = trim(line.substr(0, colon));
        const std::string target = trim(line.substr(colon + 1));
        std::uint32_t pageIndex = 0;
        if (!parseIndexName(page, "page_", pageIndex) ||
            pageIndex != pageLabels.size() || !validLabel(target))
        {
            return compile_error("Invalid page entry");
        }
        pageLabels.push_back(target);
        return true;
    }
    case Part::Code:
        if (line.back() != ':')
            return parseInstruction(line);
        else
        {
            const std::string label = trim(line.substr(0, line.size() - 1));
            if (!validLabel(label))
                return compile_error("Invalid label");
            if (!labels.emplace(label,
                                static_cast<std::uint32_t>(section->code.size() * 4)).second)
            {
                return compile_error("Duplicate label");
            }
            return true;
        }
    case Part::Text:
        return parseStringEntry(line, "text_");
    case Part::Resources:
        return parseStringEntry(line, "resource_");
    default:
        return compile_error("Content outside a section");
    }
}
bool assembleScriptBin(std::string_view source, ScriptBin& script)
{
    return SourceParser(source).parse(script);
}
SmsSourceParser::SmsSourceParser(std::string_view input)
    : source(input)
{
}
bool SmsSourceParser::parse(SmsXaf& output)
{
    std::istringstream input(source);
    std::string rawLine;
    while (std::getline(input, rawLine))
    {
        ++lineNumber;
        if (!rawLine.empty() && rawLine.back() == '\r')
            rawLine.pop_back();
        if (!parseLine(trim(stripComment(rawLine))))
            return false;
    }
    if (part != Part::Done)
        return compile_error("Missing .end");
    if (!formatSeen || !recordsSeen || !codeSeen || !stringsSeen)
        return compile_error("Incomplete SMS_XAF source");
    if (sms.code.empty() ||
        (sms.code.front() & 0xFFFF) != EVENT_OP_EVENT)
    {
        return compile_error("SMS code must begin with EVENT");
    }
    sms.scriptId = static_cast<std::uint16_t>(sms.code.front() >> 16);
    const std::uint32_t codeSize =
        static_cast<std::uint32_t>(sms.code.size() * 4);
    std::uint32_t previous = 0;
    for (std::size_t i = 0; i < recordLabels.size(); ++i)
    {
        const auto found = labels.find(recordLabels[i]);
        if (found == labels.end())
            return compile_error("Undefined record label");
        const std::uint32_t offset = found->second;
        if (offset < 4 || offset >= codeSize ||
            (i != 0 && offset <= previous))
        {
            return compile_error("Invalid record entry order");
        }
        sms.recordOffsets.push_back(offset);
        previous = offset;
    }
    output = sms;
    return true;
}
bool SmsSourceParser::compile_error(const char* message) const
{
    printf("ERROR: Line %zu: %s.\n", lineNumber, message);
    return false;
}
bool SmsSourceParser::parseDirective(const std::string& line)
{
    const std::size_t space = line.find_first_of(" \t");
    const std::string directive = line.substr(0, space);
    const std::string argument = space == std::string::npos ?
                                 std::string() : trim(line.substr(space + 1));
    if (directive == ".format")
    {
        if (part != Part::Header || formatSeen || argument != "sms_xaf")
        {
            return compile_error("Invalid .format directive");
        }
        formatSeen = true;
        return true;
    }
    if (directive == ".records")
    {
        if (part != Part::Header || !formatSeen || recordsSeen ||
            !argument.empty())
        {
            return compile_error("Invalid .records directive");
        }
        recordsSeen = true;
        part = Part::Records;
        return true;
    }
    if (directive == ".code")
    {
        if (part != Part::Records || codeSeen || !argument.empty())
        {
            return compile_error("Invalid .code directive");
        }
        codeSeen = true;
        part = Part::Code;
        return true;
    }
    if (directive == ".strings")
    {
        if (part != Part::Code || !argument.empty() || stringsSeen)
            return compile_error("Invalid .strings directive");
        stringsSeen = true;
        part = Part::Strings;
        return true;
    }
    if (directive == ".end")
    {
        if (part != Part::Strings || !argument.empty())
            return compile_error("Invalid .end directive");
        part = Part::Done;
        return true;
    }
    return compile_error("Unknown directive");
}
bool SmsSourceParser::parseRecord(const std::string& line)
{
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos)
        return compile_error("Invalid record entry");
    const std::string name = trim(line.substr(0, colon));
    const std::string target = trim(line.substr(colon + 1));
    std::uint32_t index = 0;
    if (!parseIndexName(name, "record_", index) ||
        index != recordLabels.size() || !validLabel(target) ||
        recordLabels.size() >= 0xFFFF)
    {
        return compile_error("Invalid record entry");
    }
    recordLabels.push_back(target);
    return true;
}
bool SmsSourceParser::parseCode(const std::string& line)
{
    if (line.back() != ':')
    {
        const char* error = appendEventInstruction(
            line, sms.code, EventOpcodeSet::Current,
            0, {}, EVENT_OP_EXF_LOG_MESSAGE,
            "string_", {});
        return error == nullptr ? true : compile_error(error);
    }
    if (sms.code.size() > 0xFFFFFFFF / 4)
        return compile_error("Code is too large");
    const std::string label = trim(line.substr(0, line.size() - 1));
    if (!validLabel(label) ||
        !labels.emplace(label,
                        static_cast<std::uint32_t>(sms.code.size() * 4)).second)
    {
        return compile_error("Invalid or duplicate code label");
    }
    return true;
}
bool SmsSourceParser::parseString(const std::string& line)
{
    const std::size_t colon = line.find(':');
    if (colon == std::string::npos)
        return compile_error("Invalid string entry");
    const std::string name = trim(line.substr(0, colon));
    std::uint32_t index = 0;
    if (!parseIndexName(name, "string_", index) || index != sms.strings.size())
        return compile_error("Invalid string entry");
    std::string value;
    if (!parseQuotedString(trim(line.substr(colon + 1)), value) ||
        value.find('\0') != std::string::npos)
    {
        return compile_error("Invalid quoted string");
    }
    sms.strings.push_back(value);
    return true;
}
bool SmsSourceParser::parseLine(const std::string& line)
{
    if (line.empty())
        return true;
    if (part == Part::Done)
        return compile_error("Content after .end");
    if (line[0] == '.')
        return parseDirective(line);
    if (part == Part::Records)
        return parseRecord(line);
    if (part == Part::Code)
        return parseCode(line);
    if (part == Part::Strings)
        return parseString(line);
    return compile_error("Content outside an SMS section");
}
bool assembleSmsXaf(std::string_view source, SmsXaf& sms)
{
    return SmsSourceParser(source).parse(sms);
}
bool detectEventSourceFormat(std::string_view source, EventScriptFormat& format)
{
    std::istringstream input{std::string(source)};
    std::string rawLine;
    while (std::getline(input, rawLine))
    {
        if (!rawLine.empty() && rawLine.back() == '\r')
            rawLine.pop_back();
        const std::string line = trim(stripComment(rawLine));
        if (line.empty())
            continue;
        const std::size_t space = line.find_first_of(" \t");
        if (line.substr(0, space) != ".format")
        {
            format = EventScriptFormat::Localized;
            return true;
        }
        const std::string argument = space == std::string::npos ?
            std::string() : trim(line.substr(space + 1));
        if (argument == "localized" || argument == "localized_exf50" ||
            argument == "localized_exf51")
            format = EventScriptFormat::Localized;
        else if (argument == "sms_xaf")
            format = EventScriptFormat::SmsXaf;
        else
            return false;
        return true;
    }
    format = EventScriptFormat::Localized;
    return true;
}
bool assembleEventScript(std::string_view source, EventScript& script)
{
    EventScriptFormat format;
    if (!detectEventSourceFormat(source, format))
    {
        printf("ERROR: Unknown Event VM source format.\n");
        return false;
    }
    script = {};
    script.format = format;
    return format == EventScriptFormat::Localized ?
        assembleScriptBin(source, script.localized) :
        assembleSmsXaf(source, script.sms);
}
