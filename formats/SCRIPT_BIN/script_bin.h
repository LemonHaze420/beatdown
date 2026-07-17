#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "event_isa.h"

using Bytes = std::vector<std::uint8_t>;

struct LanguageSection
{
    bool empty = false;
    std::uint16_t eventId = 0;
    std::vector<std::uint32_t> code;
    std::vector<std::uint32_t> pages;
    std::vector<std::string> text;
    std::vector<std::string> resources;
};

using LanguageSections =
    std::array<LanguageSection, EVENT_LANGUAGE_COUNT>;

struct ScriptBin
{
    LanguageSections languages;
};

bool readScriptBin(const Bytes& data, ScriptBin& script);
bool writeScriptBin(const ScriptBin& script, Bytes& data);
std::string disassembleScriptBin(const ScriptBin& script);
