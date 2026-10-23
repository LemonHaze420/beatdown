#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>
#include "script_bin.h"
class SourceParser
{
public:
    explicit SourceParser(std::string_view source);
    bool parse(ScriptBin& output);
private:
    enum class Form
    {
        Unknown,
        Shared,
        Explicit,
    };
    enum class Part
    {
        None,
        Pages,
        Code,
        Text,
        Resources,
    };
    enum class StringTableForm
    {
        None,
        Shared,
        Localized,
    };
    std::string source;
    std::size_t lineNumber = 0;
    Form form = Form::Unknown;
    int currentLanguage = -1;
    Part part = Part::None;
    LanguageSections sections{};
    LanguageSection sharedSection;
    std::array<bool, EVENT_LANGUAGE_COUNT> seen{};
    std::array<bool, EVENT_LANGUAGE_COUNT> activeLanguages{};
    std::map<std::string, std::uint32_t> labels;
    std::vector<std::string> pageLabels;
    bool formatSeen = false;
    EventOpcodeSet opcodeSet = EventOpcodeSet::Current;
    bool activeLanguagesSeen = false;
    bool sharedEnded = false;
    StringTableForm textForm = StringTableForm::None;
    StringTableForm resourceForm = StringTableForm::None;
    bool compile_error(const char* message) const;
    static int languageIndex(std::string_view code);
    LanguageSection* current();
    bool selectSharedForm();
    bool beginLanguage(const std::string& code);
    bool finishSection(LanguageSection& section);
    bool endLanguage();
    bool endShared();
    bool parseLanguages(const std::string& argument);
    bool parseDirective(const std::string& line);
    bool parseInstruction(const std::string& line);
    bool parseStringEntry(const std::string& line,
                          std::string_view prefix);
    bool parseLine(const std::string& line);
};
class SmsSourceParser
{
public:
    explicit SmsSourceParser(std::string_view source);
    bool parse(SmsXaf& output);
private:
    enum class Part
    {
        Header,
        Records,
        Code,
        Strings,
        Done,
    };
    std::string source;
    std::size_t lineNumber = 0;
    Part part = Part::Header;
    SmsXaf sms;
    bool formatSeen = false;
    bool recordsSeen = false;
    bool codeSeen = false;
    bool stringsSeen = false;
    std::map<std::string, std::uint32_t> labels;
    std::vector<std::string> recordLabels;
    bool compile_error(const char* message) const;
    bool parseDirective(const std::string& line);
    bool parseRecord(const std::string& line);
    bool parseCode(const std::string& line);
    bool parseString(const std::string& line);
    bool parseLine(const std::string& line);
};
bool assembleScriptBin(std::string_view source, ScriptBin& script);
bool assembleSmsXaf(std::string_view source, SmsXaf& sms);
bool detectEventSourceFormat(std::string_view source,
                             EventScriptFormat& format);
bool assembleEventScript(std::string_view source, EventScript& script);
