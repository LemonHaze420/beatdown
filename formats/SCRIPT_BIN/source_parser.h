#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "script_bin.h"

class SourceParser
{
public:
    explicit SourceParser(std::string_view source);

    bool parse(LanguageSections& output);

private:
    enum class Part
    {
        None,
        Pages,
        Code,
        Text,
        Resources,
    };

    std::string source;
    std::size_t lineNumber = 0;
    int currentLanguage = -1;
    Part part = Part::None;
    LanguageSections sections{};
    std::set<std::size_t> seen;
    std::map<std::string, std::uint32_t> labels;
    std::vector<std::string> pageLabels;

    bool compile_error(const char* message) const;
    static int languageIndex(std::string_view code);
    LanguageSection* current();
    bool beginLanguage(const std::string& code);
    bool endLanguage();
    bool parseDirective(const std::string& line);
    bool parseInstruction(const std::string& line);
    bool parseLine(const std::string& line);
};

bool assembleScriptBin(std::string_view source, ScriptBin& script);
