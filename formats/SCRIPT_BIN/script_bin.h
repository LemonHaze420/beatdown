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
    bool headerless = false;
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
    EventOpcodeSet opcodeSet = EventOpcodeSet::Current;
    LanguageSections languages;
};
struct SmsXaf
{
    std::uint16_t scriptId = 0;
    std::vector<std::uint32_t> code;
    std::vector<std::uint32_t> recordOffsets;
    std::vector<std::string> strings;
};
enum class EventScriptFormat
{
    Localized,
    SmsXaf,
};
struct EventScript
{
    EventScriptFormat format = EventScriptFormat::Localized;
    ScriptBin localized;
    SmsXaf sms;
};
bool readScriptBin(const Bytes& data, ScriptBin& script);
bool writeScriptBin(const ScriptBin& script, Bytes& data);
std::string disassembleScriptBin(const ScriptBin& script);
bool readSmsXaf(const Bytes& data, SmsXaf& sms);
bool writeSmsXaf(const SmsXaf& sms, Bytes& data);
std::string disassembleSmsXaf(const SmsXaf& sms);
bool detectEventScriptFormat(const Bytes& data, EventScriptFormat& format);
bool readEventScript(const Bytes& data, EventScript& script);
bool writeEventScript(const EventScript& script, Bytes& data);
std::string disassembleEventScript(const EventScript& script);
