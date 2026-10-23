#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
#include "../tool_common.hpp"
struct ScnLine
{
    std::uint64_t offset;
    std::uint64_t length;
    std::uint8_t terminatorLength;
};
struct ScnNode
{
    std::string name;
    std::uint64_t offset;
    std::uint64_t argumentOffset;
    std::uint64_t argumentLength;
    std::size_t line;
    std::size_t column;
    std::optional<std::size_t> parent;
    std::size_t depth;
    bool known;
};
class ScnFile
{
public:
    explicit ScnFile(const std::string& path)
        : path(path), data(tool::readFile(path))
    {
        validateEncoding();
        splitLines();
        parseStructure();
    }
    void inspect() const
    {
        std::size_t roots = 0;
        for (const auto& node : nodes)
            roots += !node.parent.has_value();
        std::cout << "file: " << path << '\n'
                  << "size: " << data.size() << " bytes\n"
                  << "encoding: " << encoding << '\n'
                  << "lines: " << lines.size() << '\n'
                  << "line endings: CRLF=" << crlfCount
                  << " LF=" << lfCount << " CR=" << crCount << '\n'
                  << "directives: " << nodes.size() << '\n'
                  << "root directives: " << roots << '\n'
                  << "maximum block depth: " << maximumDepth << '\n'
                  << "warnings: " << warnings.size() << '\n';
        for (const auto& warning : warnings)
            std::cout << "warning: " << warning << '\n';
        for (const auto& node : nodes)
        {
            std::cout << std::string(node.depth * 2, ' ')
                      << node.name
                      << " line=" << node.line
                      << " column=" << node.column
                      << " offset=" << node.offset
                      << " known=" << (node.known ? "yes" : "no");
            if (node.argumentLength != 0)
                std::cout << " arguments=" << escape(node.argumentOffset, node.argumentLength);
            std::cout << '\n';
        }
    }
private:
    static const std::set<std::string_view>& knownDirectives()
    {
        static const std::set<std::string_view> names{
            "@ACCELE", "@ANGLE", "@BLUR", "@CAPTEXT", "@CAPTION", "@CLIP", "@COLOR",
            "@CONSOLE", "@CURVE", "@CUT", "@DECELE", "@DTIME", "@END", "@FADETEXT",
            "@INTERPOLATE", "@LANGUAGE", "@LIP", "@MATEND", "@MATSTART", "@MODELCHK",
            "@MORPH", "@MOTION", "@MUSIC", "@NAME", "@NOSTGCHKSW", "@OFFSETPOS",
            "@OFFSETROT", "@OFFSETSW", "@POS", "@RADIUS", "@ROT", "@SOUND",
            "@SOUNDBANK", "@STAGE", "@STAGELIGHT", "@SWITCH", "@TARGET", "@TIME",
            "@TIMESCALE", "@TRACK", "@VERSION", "@VIBACCELE", "@VIBAMPLIFY",
            "@VIBDECELE", "@VIBDIR", "@VIBFREQUENCY", "@VIEWANGLE", "@VOICE", "@VOLUME"
        };
        return names;
    }
    std::string path;
    tool::Bytes data;
    std::string encoding;
    std::vector<ScnLine> lines;
    std::vector<ScnNode> nodes;
    std::vector<std::string> warnings;
    std::size_t crlfCount{};
    std::size_t lfCount{};
    std::size_t crCount{};
    std::size_t maximumDepth{};
    void validateEncoding()
    {
        bool ascii = true;
        for (std::size_t i = 0; i < data.size(); ++i)
        {
            if (data[i] == 0)
                fail("embedded NUL byte at offset " + std::to_string(i));
            ascii &= data[i] < 0x80;
        }
        if (ascii)
        {
            encoding = "ASCII";
            return;
        }
        std::size_t i = 0;
        while (i < data.size())
        {
            const std::uint8_t value = data[i];
            if (value < 0x80 || (value >= 0xA1 && value <= 0xDF))
            {
                ++i;
                continue;
            }
            const bool lead = (value >= 0x81 && value <= 0x9F) ||
                              (value >= 0xE0 && value <= 0xFC);
            if (!lead)
                fail("invalid Shift-JIS byte at offset " + std::to_string(i));
            if (i + 1 >= data.size())
                fail("truncated Shift-JIS sequence at offset " + std::to_string(i));
            const std::uint8_t trail = data[i + 1];
            const bool validTrail = (trail >= 0x40 && trail <= 0x7E) ||
                                    (trail >= 0x80 && trail <= 0xFC);
            if (!validTrail)
                fail("invalid Shift-JIS trail byte at offset " + std::to_string(i + 1));
            i += 2;
        }
        encoding = "Shift-JIS";
    }
    void splitLines()
    {
        std::uint64_t lineStart = 0;
        std::uint64_t cursor = 0;
        while (cursor < data.size())
        {
            const std::uint8_t value = data[static_cast<std::size_t>(cursor)];
            if (value != '\r' && value != '\n')
            {
                ++cursor;
                continue;
            }
            std::uint8_t terminatorLength = 1;
            if (value == '\r' && cursor + 1 < data.size() &&
                data[static_cast<std::size_t>(cursor + 1)] == '\n')
            {
                terminatorLength = 2;
                ++crlfCount;
            }
            else if (value == '\r')
                ++crCount;
            else
                ++lfCount;
            lines.push_back({lineStart, cursor - lineStart, terminatorLength});
            cursor += terminatorLength;
            lineStart = cursor;
        }
        if (lineStart < data.size())
        {
            lines.push_back({lineStart, data.size() - lineStart, 0});
            warnings.push_back("final line has no line terminator");
        }
    }
    void parseStructure()
    {
        std::vector<std::size_t> stack;
        std::optional<std::size_t> lastDirective;
        for (std::size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex)
        {
            const auto& line = lines[lineIndex];
            std::uint64_t begin = line.offset;
            std::uint64_t end = line.offset + line.length;
            while (begin < end && isIndent(data[static_cast<std::size_t>(begin)]))
                ++begin;
            while (end > begin && isIndent(data[static_cast<std::size_t>(end - 1)]))
                --end;
            if (begin == end)
                continue;
            if (end - begin == 1 && data[static_cast<std::size_t>(begin)] == '{')
            {
                if (!lastDirective.has_value())
                    fail("orphan opening brace on line " + std::to_string(lineIndex + 1));
                stack.push_back(*lastDirective);
                maximumDepth = std::max(maximumDepth, stack.size());
                lastDirective.reset();
                continue;
            }
            if (end - begin == 1 && data[static_cast<std::size_t>(begin)] == '}')
            {
                if (stack.empty())
                    fail("orphan closing brace on line " + std::to_string(lineIndex + 1));
                stack.pop_back();
                lastDirective.reset();
                continue;
            }
            if (data[static_cast<std::size_t>(begin)] != '@')
            {
                warnings.push_back("unrecognized content on line " + std::to_string(lineIndex + 1));
                lastDirective.reset();
                continue;
            }
            std::uint64_t nameEnd = begin;
            while (nameEnd < end)
            {
                const std::uint8_t value = data[static_cast<std::size_t>(nameEnd)];
                if (value == ' ' || value == '\t' || value == ',' || value == '"')
                    break;
                ++nameEnd;
            }
            const std::string name(reinterpret_cast<const char*>(data.data() + begin),
                                   static_cast<std::size_t>(nameEnd - begin));
            std::uint64_t argumentBegin = nameEnd;
            while (argumentBegin < end && isIndent(data[static_cast<std::size_t>(argumentBegin)]))
                ++argumentBegin;
            const bool known = knownDirectives().contains(name);
            if (!known)
                warnings.push_back("unknown directive " + name + " on line " + std::to_string(lineIndex + 1));
            if (!quotesBalanced(argumentBegin, end))
                warnings.push_back("unbalanced quote bytes on line " + std::to_string(lineIndex + 1));
            const std::optional<std::size_t> parent =
                stack.empty() ? std::nullopt : std::optional<std::size_t>(stack.back());
            nodes.push_back({
                name,
                begin,
                argumentBegin,
                end - argumentBegin,
                lineIndex + 1,
                static_cast<std::size_t>(begin - line.offset + 1),
                parent,
                stack.size(),
                known
            });
            lastDirective = nodes.size() - 1;
        }
        if (!stack.empty())
            fail(std::to_string(stack.size()) + " directive block(s) remain unclosed at end of file");
    }
    bool quotesBalanced(std::uint64_t begin, std::uint64_t end) const
    {
        bool quoted = false;
        std::uint64_t cursor = begin;
        while (cursor < end)
        {
            const std::uint8_t value = data[static_cast<std::size_t>(cursor)];
            const bool lead = (value >= 0x81 && value <= 0x9F) ||
                              (value >= 0xE0 && value <= 0xFC);
            if (lead)
            {
                cursor += 2;
                continue;
            }
            if (value == '"')
                quoted = !quoted;
            ++cursor;
        }
        return !quoted;
    }
    static bool isIndent(std::uint8_t value)
    {
        return value == ' ' || value == '\t';
    }
    std::string escape(std::uint64_t offset, std::uint64_t length) const
    {
        std::ostringstream output;
        output << '"';
        for (std::uint64_t i = 0; i < length; ++i)
        {
            const std::uint8_t value = data[static_cast<std::size_t>(offset + i)];
            if (value == '\\')
                output << "\\\\";
            else if (value == '"')
                output << "\\\"";
            else if (value >= 0x20 && value < 0x7F)
                output << static_cast<char>(value);
            else
                output << "\\x" << std::hex << std::uppercase << std::setw(2)
                       << std::setfill('0') << static_cast<unsigned int>(value);
        }
        output << '"';
        return output.str();
    }
    [[noreturn]] void fail(const std::string& message) const
    {
        throw std::runtime_error(path + ": " + message);
    }
};
static void printUsage(const char* argv0)
{
    std::cerr << "usage:\n"
              << "  " << argv0 << " inspect <input.scn>\n"
              << "  " << argv0 << " validate <input.scn>\n";
}
int main(int argc, char** argv)
{
    if (argc != 3)
    {
        printUsage(argv[0]);
        return 1;
    }
    const std::string command = argv[1];
    if (command != "inspect" && command != "validate")
    {
        printUsage(argv[0]);
        return 1;
    }
    try
    {
        const ScnFile scn(argv[2]);
        if (command == "inspect")
            scn.inspect();
        else
            std::cout << "valid: " << argv[2] << '\n';
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
