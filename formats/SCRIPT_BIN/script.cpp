#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include "script.h"
static bool readFile(const std::filesystem::path& path, Bytes& data)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input)
    {
        printf("ERROR: Could not open '%s'.\n", path.string().c_str());
        return false;
    }
    const std::streamoff end = input.tellg();
    if (end < 0 || static_cast<std::uintmax_t>(end) >
                   std::numeric_limits<std::size_t>::max())
    {
        printf("ERROR: Invalid file size.\n");
        return false;
    }
    data.resize(static_cast<std::size_t>(end));
    input.seekg(0, std::ios::beg);
    if (!data.empty())
        input.read(reinterpret_cast<char*>(data.data()),
                   static_cast<std::streamsize>(data.size()));
    if (!input)
    {
        printf("ERROR: Could not read '%s'.\n", path.string().c_str());
        return false;
    }
    return true;
}
static bool writeFile(const std::filesystem::path& path, const Bytes& data)
{
    std::ofstream output(path, std::ios::binary);
    if (!output)
    {
        printf("ERROR: Could not open '%s'.\n", path.string().c_str());
        return false;
    }
    if (!data.empty())
        output.write(reinterpret_cast<const char*>(data.data()),
                     static_cast<std::streamsize>(data.size()));
    if (!output)
    {
        printf("ERROR: Could not write '%s'.\n", path.string().c_str());
        return false;
    }
    return true;
}
static bool disassemble(const std::filesystem::path& inputPath,
                        const std::filesystem::path& outputPath)
{
    Bytes data;
    if (!readFile(inputPath, data))
        return false;
    EventScript script;
    if (!readEventScript(data, script))
        return false;
    const std::string source = disassembleEventScript(script);
    std::ofstream output(outputPath, std::ios::binary);
    if (!output)
    {
        printf("ERROR: Could not open '%s'.\n", outputPath.string().c_str());
        return false;
    }
    output.write(source.data(), static_cast<std::streamsize>(source.size()));
    if (!output)
    {
        printf("ERROR: Could not write '%s'.\n", outputPath.string().c_str());
        return false;
    }
    return true;
}
static bool assemble(const std::filesystem::path& inputPath,
                     const std::filesystem::path& outputPath)
{
    Bytes data;
    if (!readFile(inputPath, data))
        return false;
    const std::string source(reinterpret_cast<const char*>(data.data()), data.size());
    EventScript script;
    if (!assembleEventScript(source, script))
        return false;
    Bytes output;
    if (!writeEventScript(script, output))
        return false;
    return writeFile(outputPath, output);
}
static void printUsage()
{
    printf("./script_bin --disassemble input.event output.script\n");
    printf("./script_bin --assemble input.script output.event\n");
}
static std::filesystem::path defaultOutput(const std::filesystem::path& input,
                                           char mode, EventScriptFormat format)
{
    std::filesystem::path output = input;
    if (mode == 'd')
        output.replace_extension(".script");
    else
        output.replace_extension(format == EventScriptFormat::SmsXaf ?
                                 ".xaf" : ".bin");
    return output;
}
int main(int argc, char** argv)
{
    if (argc == 1)
    {
        printUsage();
        return 0;
    }
    char mode = 0;
    int argument = 1;
    const std::string option = argv[argument];
    if (option == "-d" || option == "--disassemble")
    {
        mode = 'd';
        ++argument;
    }
    else if (option == "-a" || option == "--assemble")
    {
        mode = 'a';
        ++argument;
    }
    else if (option == "-h" || option == "--help")
    {
        printUsage();
        return 0;
    }
    else if (!option.empty() && option[0] == '-')
    {
        printf("ERROR: Unknown option '%s'.\n", option.c_str());
        return 1;
    }
    const int remaining = argc - argument;
    if (remaining < 1 || remaining > 2)
    {
        printUsage();
        return 1;
    }
    const std::filesystem::path inputPath = argv[argument];
    EventScriptFormat format = EventScriptFormat::Localized;
    if (mode == 0)
    {
        Bytes data;
        if (!readFile(inputPath, data))
            return 1;
        if (detectEventScriptFormat(data, format))
        {
            mode = 'd';
        }
        else
        {
            const std::string source(
                reinterpret_cast<const char*>(data.data()), data.size());
            if (!detectEventSourceFormat(source, format))
            {
                printf("ERROR: Unknown Event VM source format.\n");
                return 1;
            }
            mode = 'a';
        }
    }
    else if (mode == 'a' && remaining == 1)
    {
        Bytes data;
        if (!readFile(inputPath, data))
            return 1;
        const std::string source(
            reinterpret_cast<const char*>(data.data()), data.size());
        if (!detectEventSourceFormat(source, format))
        {
            printf("ERROR: Unknown Event VM source format.\n");
            return 1;
        }
    }
    const std::filesystem::path outputPath = remaining == 2 ?
        std::filesystem::path(argv[argument + 1]) :
        defaultOutput(inputPath, mode, format);
    if (inputPath.lexically_normal() == outputPath.lexically_normal())
    {
        printf("ERROR: Input and output paths are the same.\n");
        return 1;
    }
    const bool success = mode == 'd' ? disassemble(inputPath, outputPath) :
                                      assemble(inputPath, outputPath);
    return success ? 0 : 1;
}
