#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <sstream>
#include <string>
#include <string_view>
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
    while (begin < text.size())
    {
        const std::size_t comma = text.find(',', begin);
        const std::string operand = trim(text.substr(begin, comma - begin));
        if (operand.empty())
            return false;
        operands.push_back(operand);
        if (comma == std::string::npos)
            break;
        begin = comma + 1;
    }
    return true;
}

SourceParser::SourceParser(std::string_view input)
    : source(input)
{
}

bool SourceParser::parse(LanguageSections& output)
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

    if (currentLanguage >= 0)
        return compile_error("Missing .end");
    if (seen.size() != EVENT_LANGUAGE_COUNT)
        return compile_error("Expected six languages");

    output = sections;
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
    if (currentLanguage < 0)
    {
        compile_error("Directive outside a language block");
        return nullptr;
    }
    return &sections[static_cast<std::size_t>(currentLanguage)];
}

bool SourceParser::beginLanguage(const std::string& code)
{
    if (currentLanguage >= 0)
        return compile_error("Missing .end before the next language");
    const int index = languageIndex(code);
    if (index < 0)
        return compile_error("Unknown language");
    if (!seen.insert(static_cast<std::size_t>(index)).second)
        return compile_error("Duplicate language");

    currentLanguage = index;
    part = Part::None;
    labels.clear();
    pageLabels.clear();
    return true;
}

bool SourceParser::endLanguage()
{
    LanguageSection* section = current();
    if (section == nullptr)
        return false;

    if (!section->empty)
    {
        for (const std::string& label : pageLabels)
        {
            const auto found = labels.find(label);
            if (found == labels.end())
                return compile_error("Undefined page label");
            section->pages.push_back(found->second);
        }
        if (section->pages.size() > 0xFFFF)
            return compile_error("Too many pages");
        if (section->code.empty() ||
            (section->code.front() & 0xFFFF) != EVENT_OP_EVENT)
            return compile_error("Code must begin with EVENT");
        section->eventId = static_cast<std::uint16_t>(section->code.front() >> 16);
    }
    else if (!pageLabels.empty() || !section->code.empty() ||
             !section->text.empty() || !section->resources.empty())
    {
        return compile_error("Empty language contains data");
    }

    currentLanguage = -1;
    part = Part::None;
    return true;
}

bool SourceParser::parseDirective(const std::string& line)
{
    const std::size_t space = line.find_first_of(" \t");
    const std::string directive = line.substr(0, space);
    const std::string argument = space == std::string::npos ?
                                 std::string() : trim(line.substr(space + 1));

    if (directive == ".language")
    {
        if (argument.empty())
            return compile_error(".language requires a code");
        return beginLanguage(argument);
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
    else if (directive == ".end")
    {
        if (!argument.empty())
            return compile_error(".end does not take an argument");
        return endLanguage();
    }
    else if (directive == ".word")
    {
        std::uint32_t value = 0;
        if (part != Part::Code || argument.empty() ||
            !parseNumber(argument, 0xFFFFFFFF, value))
        {
            return compile_error("Invalid .word directive");
        }
        section->code.push_back(value);
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

    const std::size_t space = line.find_first_of(" \t");
    const std::string mnemonic = line.substr(0, space);
    const std::string argumentText = space == std::string::npos ?
                                     std::string() : trim(line.substr(space + 1));
    const EventOpcodeDescriptor* descriptor = findOpcode(mnemonic);
    if (descriptor == nullptr)
        return compile_error("Unknown opcode");

    std::vector<std::string> operands;
    if (!splitOperands(argumentText, operands))
        return compile_error("Invalid operand list");

    const bool head = hasHeadOperand(*descriptor);
    const std::size_t expected = descriptor->wordCount - 1 + (head ? 1 : 0);
    if (operands.size() != expected)
        return compile_error("Invalid operand count");

    std::uint32_t word = static_cast<std::uint16_t>(descriptor->opcode);
    std::size_t operand = 0;
    if (head)
    {
        std::uint32_t value = 0;
        if (hasOperandType(*descriptor, 0, EVENT_OPERAND_PAGE_ID) &&
            parseIndexName(operands[operand], "page_", value))
        {
            if (value >= pageLabels.size())
                return compile_error("Page is outside the page table");
        }
        else if (!parseNumber(operands[operand], 0xFFFF, value))
        {
            return compile_error("Invalid operand");
        }
        word |= value << 16;
        ++operand;
    }
    section->code.push_back(word);

    while (operand < operands.size())
    {
        std::uint32_t value = 0;
        bool parsed = descriptor->opcode == EVENT_OP_CALLFUNC && operand == 0 &&
                      parseCallFunction(operands[operand], value);
        if (!parsed &&
            hasOperandType(*descriptor,
                           static_cast<std::uint8_t>(operand + (head ? 0 : 1)),
                           EVENT_OPERAND_MESSAGE_ID))
        {
            parsed = parseIndexName(operands[operand], "text_", value);
        }
        if (!parsed &&
            hasOperandType(*descriptor,
                           static_cast<std::uint8_t>(operand + (head ? 0 : 1)),
                           EVENT_OPERAND_TEXTURE_RESOURCE_ID))
        {
            parsed = parseIndexName(operands[operand], "resource_", value);
        }
        if (!parsed && !parseNumber(operands[operand], 0xFFFFFFFF, value))
        {
            return compile_error("Invalid operand");
        }

        section->code.push_back(value);
        ++operand;
    }

    return true;
}

bool SourceParser::parseLine(const std::string& line)
{
    if (line.empty())
        return true;
    if (line[0] == '.')
        return parseDirective(line);

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
    {
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos)
            return compile_error("Invalid text entry");
        const std::string name = trim(line.substr(0, colon));
        std::uint32_t index = 0;
        if (!parseIndexName(name, "text_", index) || index != section->text.size())
            return compile_error("Invalid text entry");

        std::string value;
        if (!parseQuotedString(trim(line.substr(colon + 1)), value))
            return compile_error("Invalid quoted string");
        section->text.push_back(value);
        return true;
    }

    case Part::Resources:
    {
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos)
            return compile_error("Invalid resource entry");
        const std::string name = trim(line.substr(0, colon));
        std::uint32_t index = 0;
        if (!parseIndexName(name, "resource_", index) ||
            index != section->resources.size())
        {
            return compile_error("Invalid resource entry");
        }

        std::string value;
        if (!parseQuotedString(trim(line.substr(colon + 1)), value))
            return compile_error("Invalid quoted string");
        section->resources.push_back(value);
        return true;
    }

    default:
        return compile_error("Content outside a section");
    }
}

bool assembleScriptBin(std::string_view source, ScriptBin& script)
{
    return SourceParser(source).parse(script.languages);
}
