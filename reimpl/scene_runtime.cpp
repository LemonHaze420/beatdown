#include "scene_runtime.h"

#include "xaf_runtime.h"
#include "../formats/MOVE_TBL/move_tables.h"

#include <array>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <utility>
#include <vector>

SceneClip::SceneClip()
    : name(), hasTime(false), hasDtime(false), resourceName(),
      startTime(0.0f), endTime(0.0f), dtimeStart(0.0f), dtimeEnd(0.0f),
      hasOffsetPosition(false), offsetPosition(), hasOffsetRotation(false),
      offsetRotation(), offsetRotationCount(0), hasMatrixStart(false),
      matrixStart(), hasMatrixEnd(false), matrixEnd(), hasViewAngle(false),
      viewAngleStart(0.0f), viewAngleEnd(0.0f), hasClipPlanes(false),
      nearClip(0.0f), farClip(0.0f), hasModelCheck(false), modelCheck(0),
      hasTarget(false), targetId(0), targetName(), hasCurve(false), curve(0),
      hasAcceleration(false), acceleration(0), hasDeceleration(false),
      deceleration(0), hasColor(false), color(), hasPosition(false),
      position(), hasRotation(false), rotationStart(0.0f), rotationEnd(0.0f),
      hasRadius(false), radius(0.0f), hasAngle(false), angle(0.0f),
      hasSound(false), sound(), hasMusic(false), music(), hasSwitch(false),
      switchValues(), hasVoice(false), voice(), hasLip(false), lip(0),
      lipDuration(0.0f), hasInterpolate(false), interpolate(0),
      hasNoStageCheckSwitch(false), noStageCheckSwitch(0),
      hasOffsetSwitch(false), offsetSwitch(0), hasCaption(false), caption(0),
      hasVolume(false), volumeStart(0), volumeEnd(0), hasMorph(false), morph(),
      hasVibrationDirection(false), vibrationDirection(0),
      hasVibrationAmplify(false), vibrationAmplify(0.0f),
      hasVibrationFrequency(false), vibrationFrequency(0.0f),
      hasVibrationAcceleration(false), vibrationAcceleration(0),
      hasVibrationDeceleration(false), vibrationDeceleration(0),
      hasBlur(false), blur(0), hasTimeScale(false), timeScale(1.0f), children()
{
}

namespace
{

struct SceneLine
{
    std::string text;
    std::size_t number = 0;
};

struct SceneToken
{
    std::string text;
    bool quoted = false;
};

struct SceneDirective
{
    std::string name;
    std::vector<SceneToken> arguments;
    std::size_t line = 0;
};

bool isShiftJisLead(uint8_t value)
{
    return (value >= 0x81 && value <= 0x9f) ||
           (value >= 0xe0 && value <= 0xfc);
}

bool isShiftJisTrail(uint8_t value)
{
    return (value >= 0x40 && value <= 0x7e) ||
           (value >= 0x80 && value <= 0xfc);
}

std::string trim(const std::string& value)
{
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && (value[begin] == ' ' || value[begin] == '\t'))
        ++begin;
    while (end > begin && (value[end - 1] == ' ' || value[end - 1] == '\t'))
        --end;
    return value.substr(begin, end - begin);
}

bool setError(std::string* error,
              std::size_t line,
              std::string message)
{
    if (error != nullptr)
    {
        *error = "line " + std::to_string(line) + ": " + std::string(message);
    }
    return false;
}

bool parseFloatToken(const SceneToken& token, float& value)
{
    if (token.text.empty())
        return false;
    char* end = nullptr;
    errno = 0;
    const float parsed = std::strtof(token.text.c_str(), &end);
    if (errno == ERANGE || end == token.text.c_str() || *end != '\0' ||
        !std::isfinite(parsed))
        return false;
    value = parsed;
    return true;
}

bool parseIntToken(const SceneToken& token, int32_t& value)
{
    if (token.text.empty())
        return false;
    char* end = nullptr;
    errno = 0;
    const long parsed = std::strtol(token.text.c_str(), &end, 10);
    if (errno == ERANGE || end == token.text.c_str() || *end != '\0' ||
        parsed < std::numeric_limits<int32_t>::min() ||
        parsed > std::numeric_limits<int32_t>::max())
        return false;
    value = static_cast<int32_t>(parsed);
    return true;
}

bool parseFloatArguments(const std::vector<SceneToken>& arguments,
                         std::size_t count,
                         float* values)
{
    if (arguments.size() != count)
        return false;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!parseFloatToken(arguments[i], values[i]))
            return false;
    }
    return true;
}

bool parseIntArguments(const std::vector<SceneToken>& arguments,
                       std::size_t count,
                       int32_t* values)
{
    if (arguments.size() != count)
        return false;
    for (std::size_t i = 0; i < count; ++i)
    {
        if (!parseIntToken(arguments[i], values[i]))
            return false;
    }
    return true;
}

bool tokenize(std::string input,
              std::vector<SceneToken>& tokens)
{
    tokens.clear();
    std::size_t cursor = 0;
    while (cursor < input.size())
    {
        while (cursor < input.size() &&
               (input[cursor] == ' ' || input[cursor] == '\t' ||
                input[cursor] == ','))
            ++cursor;
        if (cursor == input.size())
            break;

        SceneToken token;
        if (input[cursor] == '"')
        {
            token.quoted = true;
            ++cursor;
            const std::size_t begin = cursor;
            std::string value;
            while (cursor < input.size())
            {
                const uint8_t byte = static_cast<uint8_t>(input[cursor]);
                if (isShiftJisLead(byte) && cursor + 1 < input.size())
                {
                    value.push_back(input[cursor++]);
                    value.push_back(input[cursor++]);
                    continue;
                }
                if (input[cursor] == '"')
                    break;
                value.push_back(input[cursor++]);
            }
            if (cursor >= input.size() || input[cursor] != '"')
                return false;
            ++cursor;
            token.text = std::move(value);
            (void)begin;
        }
        else
        {
            const std::size_t begin = cursor;
            while (cursor < input.size() && input[cursor] != ' ' &&
                   input[cursor] != '\t' && input[cursor] != ',')
                ++cursor;
            token.text.assign(input.substr(begin, cursor - begin));
        }
        tokens.push_back(std::move(token));
    }
    return true;
}

bool parseDirective(std::string input,
                    std::size_t line,
                    SceneDirective& directive,
                    std::string* error)
{
    input = trim(input);
    if (input.empty() || input.front() != '@')
        return setError(error, line, "expected directive");

    std::size_t nameEnd = 0;
    while (nameEnd < input.size() && input[nameEnd] != ' ' &&
           input[nameEnd] != '\t' && input[nameEnd] != ',')
        ++nameEnd;
    if (nameEnd <= 1)
        return setError(error, line, "empty directive name");

    directive.name.assign(input.substr(0, nameEnd));
    directive.line = line;
    if (!tokenize(input.substr(nameEnd), directive.arguments))
        return setError(error, line, "unbalanced quoted argument");
    return true;
}

bool isKnownProperty(std::string name)
{
    static const char* names[] =
    {
        "@ACCELE", "@ANGLE", "@BLUR", "@CAPTION", "@CLIP", "@COLOR",
        "@CURVE", "@DECELE", "@DTIME", "@INTERPOLATE", "@LIP",
        "@MATEND", "@MATSTART", "@MODELCHK", "@MORPH", "@MUSIC",
        "@NOSTGCHKSW", "@OFFSETPOS", "@OFFSETROT", "@OFFSETSW", "@POS",
        "@RADIUS", "@ROT", "@SOUND", "@SWITCH", "@TARGET", "@TIME",
        "@TIMESCALE", "@VIBACCELE", "@VIBAMPLIFY", "@VIBDECELE",
        "@VIBDIR", "@VIBFREQUENCY", "@VIEWANGLE", "@VOICE", "@VOLUME"
    };
    const std::size_t count = sizeof(names) / sizeof(names[0]);
    for (std::size_t i = 0; i < count; ++i)
    {
        if (name == names[i])
            return true;
    }
    return false;
}

bool isRootDirective(std::string name)
{
    return name == "@CONSOLE" || name == "@VERSION" || name == "@STAGE" ||
           name == "@MOTION" || name == "@SOUNDBANK" || name == "@CAPTEXT" ||
           name == "@FADETEXT" || name == "@TRACK" || name == "@END";
}

bool isLightResourceName(std::string value)
{
    return value == "AMBIENT" || value == "DIRECTIONAL" || value == "POINT" ||
           value == "SPOT" || value == "SPOTSOFT";
}

bool isNoResourceName(std::string value)
{
    return value.empty() || value == "NONE" || value == "LERP" ||
           isLightResourceName(value);
}

SceneTrackKind trackKindFromName(std::string name,
                                 bool& valid)
{
    valid = true;
    if (name == "CAM") return SceneTrackKind::Camera;
    if (name == "LIT") return SceneTrackKind::Light;
    if (name == "MDL") return SceneTrackKind::Model;
    if (name == "MOT") return SceneTrackKind::Motion;
    if (name == "SND") return SceneTrackKind::Sound;
    if (name == "BGM") return SceneTrackKind::Music;
    if (name == "PLY") return SceneTrackKind::Player;
    if (name == "PTK") return SceneTrackKind::PlayerTask;
    if (name == "CAP") return SceneTrackKind::Caption;
    if (name == "VOL") return SceneTrackKind::Volume;
    if (name == "MPH") return SceneTrackKind::Morph;
    if (name == "VOI") return SceneTrackKind::Voice;
    if (name == "VIB") return SceneTrackKind::Vibration;
    if (name == "EFF") return SceneTrackKind::Effect;
    valid = false;
    return SceneTrackKind::Camera;
}

class SceneParser
{
public:
    SceneParser(const uint8_t* data, std::size_t size)
        : data_(data), size_(size)
    {
    }

    bool run(SceneDefinition& scene, std::string* error)
    {
        if (!validateEncoding(error) || !splitLines(error))
            return false;

        SceneDefinition parsed;
        if (!parseRoot(parsed, error))
            return false;
        if (parsed.console != "PS2")
            return setError(error, 1, "@CONSOLE must be PS2");
        if (parsed.version != "1.000")
            return setError(error, 1, "@VERSION must be 1.000");

        bool hasDuration = false;
        parsed.duration = 0.0f;
        for (const SceneTrack& track : parsed.tracks)
            updateDuration(track, parsed.duration, hasDuration);
        if (!hasDuration)
            parsed.duration = 0.0f;
        scene = std::move(parsed);
        return true;
    }

private:
    const uint8_t* data_;
    std::size_t size_;
    std::vector<SceneLine> lines_;
    std::size_t cursor_ = 0;

    bool validateEncoding(std::string* error)
    {
        for (std::size_t i = 0; i < size_; ++i)
        {
            const uint8_t value = data_[i];
            if (value == 0)
                return setError(error, 0, "embedded NUL byte");
            if (value < 0x80 || (value >= 0xa1 && value <= 0xdf))
                continue;
            if (!isShiftJisLead(value))
                return setError(error, 0, "invalid Shift-JIS lead byte");
            if (i + 1 >= size_ || !isShiftJisTrail(data_[i + 1]))
                return setError(error, 0, "invalid Shift-JIS trail byte");
            ++i;
        }
        return true;
    }

    bool splitLines(std::string* error)
    {
        std::size_t begin = 0;
        std::size_t i = 0;
        while (i < size_)
        {
            if (data_[i] != '\r' && data_[i] != '\n')
            {
                ++i;
                continue;
            }
            lines_.push_back({
                std::string(reinterpret_cast<const char*>(data_ + begin),
                            i - begin),
                lines_.size() + 1
            });
            if (data_[i] == '\r' && i + 1 < size_ &&
                data_[i + 1] == '\n')
                i += 2;
            else
                ++i;
            begin = i;
        }
        if (begin < size_)
        {
            lines_.push_back({
                std::string(reinterpret_cast<const char*>(data_ + begin),
                            size_ - begin),
                lines_.size() + 1
            });
        }
        if (lines_.empty() && size_ != 0)
            return setError(error, 1, "scene has no lines");
        return true;
    }

    bool nextContent(std::string& text, std::size_t& line)
    {
        while (cursor_ < lines_.size())
        {
            const SceneLine& source = lines_[cursor_];
            ++cursor_;
            text = trim(source.text);
            line = source.number;
            if (!text.empty())
                return true;
        }
        return false;
    }

    bool openBlock(std::string* error)
    {
        std::string text;
        std::size_t line = 0;
        if (!nextContent(text, line) || text != "{")
            return setError(error, line, "expected opening brace");
        return true;
    }

    bool parseRoot(SceneDefinition& scene, std::string* error)
    {
        bool sawEnd = false;
        while (true)
        {
            std::string text;
            std::size_t line = 0;
            if (!nextContent(text, line))
                break;
            if (text == "{" || text == "}")
                return setError(error, line, "orphan brace");

            SceneDirective directive;
            if (!parseDirective(text, line, directive, error))
                return false;
            if (directive.name == "@END")
            {
                if (!directive.arguments.empty())
                    return setError(error, line, "@END takes no arguments");
                sawEnd = true;
                break;
            }
            if (!isRootDirective(directive.name))
                return setError(error, line, "unknown root directive " + directive.name);

            if (directive.name == "@TRACK")
            {
                if (!parseTrack(directive, scene.tracks, error))
                    return false;
            }
            else if (!parseRootResource(directive, scene, error))
                return false;
        }

        if (!sawEnd)
            return setError(error, lines_.empty() ? 1 : lines_.back().number,
                            "missing @END");

        int trailingDepth = 0;
        while (cursor_ < lines_.size())
        {
            const std::string trailing = trim(lines_[cursor_].text);
            ++cursor_;
            if (trailing == "{")
            {
                ++trailingDepth;
                continue;
            }
            if (trailing == "}" && trailingDepth > 0)
            {
                --trailingDepth;
                continue;
            }
            if (!trailing.empty())
                return setError(error, lines_[cursor_ - 1].number,
                                "content after @END");
        }
        if (trailingDepth != 0)
            return setError(error, lines_.back().number,
                            "unterminated block after @END");
        return true;
    }

    bool parseRootResource(const SceneDirective& directive,
                           SceneDefinition& scene,
                           std::string* error)
    {
        if (directive.name == "@CONSOLE" || directive.name == "@VERSION")
        {
            if (directive.arguments.size() != 1)
                return setError(error, directive.line,
                                directive.name + " expects one value");
            if (directive.name == "@CONSOLE")
                scene.console = directive.arguments[0].text;
            else
                scene.version = directive.arguments[0].text;
            return parseEmptyBlock(error);
        }

        if (directive.name == "@STAGE")
        {
            if (directive.arguments.size() != 1)
                return setError(error, directive.line, "@STAGE expects one value");
            if (!parseStageValue(directive.arguments[0].text, scene, error,
                                 directive.line))
                return false;
            if (!openBlock(error))
                return false;
            while (true)
            {
                std::string text;
                std::size_t line = 0;
                if (!nextContent(text, line))
                    return setError(error, directive.line,
                                    "unterminated @STAGE block");
                if (text == "}")
                    break;
                SceneDirective property;
                if (!parseDirective(text, line, property, error))
                    return false;
                if (property.name != "@STAGELIGHT")
                    return setError(error, line,
                                    "unexpected @STAGE property " + property.name);
                if (property.arguments.size() != 1 ||
                    (property.arguments[0].text != "ON" &&
                     property.arguments[0].text != "OFF"))
                    return setError(error, line,
                                    "@STAGELIGHT expects ON or OFF");
                scene.hasStageLight = true;
                scene.stageLight = property.arguments[0].text == "ON";
            }
            return true;
        }

        if (directive.name == "@MOTION")
        {
            if (directive.arguments.size() != 1)
                return setError(error, directive.line,
                                "@MOTION expects one value");
            scene.motionResource = directive.arguments[0].text;
            if (scene.motionResource == "NONE")
                scene.motionId = -1;
            else
            {
                int32_t id = 0;
                if (!parseIntToken(directive.arguments[0], id) || id <= 0)
                    return setError(error, directive.line,
                                    "invalid @MOTION id");
                scene.motionId = id - 1;
            }
            return parseEmptyBlock(error);
        }

        if (directive.name == "@SOUNDBANK" ||
            directive.name == "@FADETEXT")
        {
            if (directive.arguments.size() != 1)
                return setError(error, directive.line,
                                directive.name + " expects one value");
            const std::string value = directive.arguments[0].text == "NONE" ?
                                      std::string() : directive.arguments[0].text;
            if (directive.name == "@SOUNDBANK")
                scene.soundbank = value;
            else
                scene.fadeText = value;
            return parseEmptyBlock(error);
        }

        if (directive.name == "@CAPTEXT")
        {
            if (directive.arguments.size() != 1)
                return setError(error, directive.line,
                                "@CAPTEXT expects one value");
            SceneCaption caption;
            caption.path = directive.arguments[0].text == "NONE" ?
                           std::string() : directive.arguments[0].text;
            if (!openBlock(error))
                return false;
            bool hasLanguage = false;
            while (true)
            {
                std::string text;
                std::size_t line = 0;
                if (!nextContent(text, line))
                    return setError(error, directive.line,
                                    "unterminated @CAPTEXT block");
                if (text == "}")
                    break;
                SceneDirective property;
                if (!parseDirective(text, line, property, error))
                    return false;
                if (property.name != "@LANGUAGE" || property.arguments.size() != 1)
                    return setError(error, line,
                                    "@CAPTEXT expects one @LANGUAGE");
                if (!parseLanguage(property.arguments[0].text,
                                   caption.language))
                    return setError(error, line, "unknown caption language");
                hasLanguage = true;
            }
            if (!hasLanguage)
                return setError(error, directive.line,
                                "@CAPTEXT is missing @LANGUAGE");
            scene.captions.push_back(std::move(caption));
            return true;
        }

        return setError(error, directive.line, "unsupported root directive");
    }

    bool parseEmptyBlock(std::string* error)
    {
        if (!openBlock(error))
            return false;
        std::string text;
        std::size_t line = 0;
        if (!nextContent(text, line) || text != "}")
            return setError(error, line, "expected empty block");
        return true;
    }

    bool parseStageValue(const std::string& value,
                         SceneDefinition& scene,
                         std::string* error,
                         std::size_t line)
    {
        if (value.empty())
            return setError(error, line, "empty stage value");
        char* end = nullptr;
        errno = 0;
        const long parsed = std::strtol(value.c_str(), &end, 10);
        if (errno == ERANGE || end == value.c_str() || parsed < 0 ||
            parsed > std::numeric_limits<int32_t>::max())
            return setError(error, line, "invalid stage id");
        scene.stageId = static_cast<int32_t>(parsed);
        while (*end == ' ' || *end == '\t')
            ++end;
        scene.stageName = end;
        return true;
    }

    bool parseLanguage(const std::string& value, SceneLanguage& language)
    {
        if (value == "JAPANESE") language = SceneLanguage::Japanese;
        else if (value == "ENGLISH") language = SceneLanguage::English;
        else if (value == "FRENCH") language = SceneLanguage::French;
        else if (value == "GERMAN") language = SceneLanguage::German;
        else if (value == "SPANISH") language = SceneLanguage::Spanish;
        else if (value == "ITALIAN") language = SceneLanguage::Italian;
        else return false;
        return true;
    }

    bool parseTrack(const SceneDirective& directive,
                    std::vector<SceneTrack>& destination,
                    std::string* error)
    {
        if (directive.arguments.size() != 2 ||
            (directive.arguments[0].text != "ON" &&
             directive.arguments[0].text != "OFF"))
            return setError(error, directive.line,
                            "@TRACK expects ON|OFF and a kind");
        bool validKind = false;
        const SceneTrackKind kind =
            trackKindFromName(directive.arguments[1].text, validKind);
        if (!validKind)
            return setError(error, directive.line,
                            "unknown @TRACK kind " + directive.arguments[1].text);

        SceneTrack track;
        track.kind = kind;
        track.enabled = directive.arguments[0].text == "ON";
        if (!openBlock(error))
            return false;
        while (true)
        {
            std::string text;
            std::size_t line = 0;
            if (!nextContent(text, line))
                return setError(error, directive.line, "unterminated @TRACK block");
            if (text == "}")
                break;
            SceneDirective item;
            if (!parseDirective(text, line, item, error))
                return false;
            if (item.name == "@NAME")
            {
                if (item.arguments.size() != 1)
                    return setError(error, line, "@NAME expects one value");
                track.name = item.arguments[0].text;
            }
            else if (item.name == "@CUT")
            {
                if (!parseClip(item, track, error))
                    return false;
            }
            else if (item.name == "@TRACK")
            {
                if (!parseTrack(item, track.children, error))
                    return false;
            }
            else
                return setError(error, line,
                                "unexpected @TRACK property " + item.name);
        }
        destination.push_back(std::move(track));
        return true;
    }

    bool parseClip(const SceneDirective& directive,
                   SceneTrack& track,
                   std::string* error)
    {
        SceneClip clip;
        if (!directive.arguments.empty())
        {
            if (directive.arguments[0].text == "FILE")
            {
                if (directive.arguments.size() != 2)
                    return setError(error, directive.line,
                                    "@CUT FILE expects one path");
                clip.resourceName = directive.arguments[1].text;
            }
            else if (directive.arguments.size() == 1)
                clip.resourceName = directive.arguments[0].text;
            else
                return setError(error, directive.line, "invalid @CUT arguments");
        }
        if (!openBlock(error))
            return false;

        while (true)
        {
            std::string text;
            std::size_t line = 0;
            if (!nextContent(text, line))
                return setError(error, directive.line, "unterminated @CUT block");
            if (text == "}")
                break;
            SceneDirective item;
            if (!parseDirective(text, line, item, error))
                return false;
            if (item.name == "@NAME")
            {
                if (item.arguments.size() != 1)
                    return setError(error, line, "@NAME expects one value");
                clip.name = item.arguments[0].text;
            }
            else if (item.name == "@TRACK")
            {
                if (!parseTrack(item, clip.children, error))
                    return false;
            }
            else if (isKnownProperty(item.name))
            {
                if (!parseProperty(track.kind, item, clip, error))
                    return false;
            }
            else
                return setError(error, line,
                                "unexpected @CUT property " + item.name);
        }

        if (!clip.hasTime)
            return setError(error, directive.line, "@CUT is missing @TIME");
        if (clip.endTime < clip.startTime)
            return setError(error, directive.line, "@TIME end precedes start");
        track.clips.push_back(std::move(clip));
        return true;
    }

    bool parseProperty(SceneTrackKind kind,
                       const SceneDirective& directive,
                       SceneClip& clip,
                       std::string* error)
    {
        const std::vector<SceneToken>& a = directive.arguments;
        float f[12]{};
        int32_t i[4]{};

        if (directive.name == "@TIME")
        {
            if (!parseFloatArguments(a, 2, f) || f[0] < 0.0f || f[1] < f[0])
                return setError(error, directive.line, "invalid @TIME values");
            clip.hasTime = true;
            clip.startTime = f[0];
            clip.endTime = f[1];
            return true;
        }
        if (directive.name == "@DTIME")
        {
            if (!parseFloatArguments(a, 2, f))
                return setError(error, directive.line, "invalid @DTIME values");
            clip.hasDtime = true;
            clip.dtimeStart = f[0];
            clip.dtimeEnd = f[1];
            return true;
        }
        if (directive.name == "@OFFSETPOS")
        {
            if (kind != SceneTrackKind::Camera && kind != SceneTrackKind::Light &&
                kind != SceneTrackKind::Model && kind != SceneTrackKind::Player &&
                kind != SceneTrackKind::PlayerTask)
                return true;
            if (!parseFloatArguments(a, 3, f))
                return setError(error, directive.line, "invalid @OFFSETPOS values");
            clip.hasOffsetPosition = true;
            clip.offsetPosition = SceneVec3(f[0], f[1], f[2]);
            return true;
        }
        if (directive.name == "@OFFSETROT")
        {
            const bool three = kind == SceneTrackKind::Model;
            const bool accepted = three || kind == SceneTrackKind::Camera ||
                                  kind == SceneTrackKind::Light ||
                                  kind == SceneTrackKind::Player ||
                                  kind == SceneTrackKind::PlayerTask;
            if (!accepted)
                return true;
            if (!parseFloatArguments(a, three ? 3 : 1, f))
                return setError(error, directive.line, "invalid @OFFSETROT values");
            clip.hasOffsetRotation = true;
            clip.offsetRotation = SceneVec3(f[0], three ? f[1] : 0.0f,
                                   three ? f[2] : 0.0f);
            clip.offsetRotationCount = three ? 3 : 1;
            return true;
        }
        if (directive.name == "@MATSTART" || directive.name == "@MATEND")
        {
            if (kind != SceneTrackKind::Camera)
                return true;
            if (!parseFloatArguments(a, 12, f))
                return setError(error, directive.line,
                                "invalid camera matrix values");

            SceneMatrix matrix;
            matrix.position = SceneVec3(f[0], f[1], f[2]);
            matrix.at = SceneVec3(f[3], f[4], f[5]);
            matrix.up = SceneVec3(f[6], f[7], f[8]);
            matrix.right = SceneVec3(f[9], f[10], f[11]);
            if (directive.name == "@MATSTART")
            {
                clip.hasMatrixStart = true;
                clip.matrixStart = matrix;
            }
            else
            {
                clip.hasMatrixEnd = true;
                clip.matrixEnd = matrix;
            }
            return true;
        }
        if (directive.name == "@VIEWANGLE")
        {
            if (kind != SceneTrackKind::Camera)
                return true;
            if (!parseFloatArguments(a, 2, f))
                return setError(error, directive.line, "invalid @VIEWANGLE values");
            clip.hasViewAngle = true;
            clip.viewAngleStart = f[0];
            clip.viewAngleEnd = f[1];
            return true;
        }
        if (directive.name == "@CLIP")
        {
            if (kind != SceneTrackKind::Camera)
                return true;
            if (!parseFloatArguments(a, 2, f))
                return setError(error, directive.line, "invalid @CLIP values");
            clip.hasClipPlanes = true;
            clip.nearClip = f[0];
            clip.farClip = f[1];
            return true;
        }
        if (directive.name == "@MODELCHK")
        {
            if (kind != SceneTrackKind::Camera)
                return true;
            if (!parseIntArguments(a, 1, i))
                return setError(error, directive.line, "invalid @MODELCHK value");
            clip.hasModelCheck = true;
            clip.modelCheck = i[0];
            return true;
        }
        if (directive.name == "@TARGET")
        {
            if (kind != SceneTrackKind::Camera && kind != SceneTrackKind::Model)
                return true;
            if (a.size() != 2 || !parseIntToken(a[0], i[0]))
                return setError(error, directive.line, "invalid @TARGET values");
            clip.hasTarget = true;
            clip.targetId = i[0];
            clip.targetName = a[1].text;
            return true;
        }
        if (directive.name == "@CURVE" || directive.name == "@ACCELE" ||
            directive.name == "@DECELE")
        {
            if (kind != SceneTrackKind::Camera)
                return true;
            if (!parseIntArguments(a, 1, i))
                return setError(error, directive.line,
                                "invalid camera curve value");
            if (directive.name == "@CURVE")
            {
                clip.hasCurve = true;
                clip.curve = i[0];
            }
            else if (directive.name == "@ACCELE")
            {
                clip.hasAcceleration = true;
                clip.acceleration = i[0];
            }
            else
            {
                clip.hasDeceleration = true;
                clip.deceleration = i[0];
            }
            return true;
        }
        if (directive.name == "@COLOR")
        {
            if (kind != SceneTrackKind::Light)
                return true;
            if (!parseFloatArguments(a, 3, f))
                return setError(error, directive.line, "invalid @COLOR values");
            clip.hasColor = true;
            clip.color = SceneVec3(f[0], f[1], f[2]);
            return true;
        }
        if (directive.name == "@POS")
        {
            if (kind != SceneTrackKind::Light && kind != SceneTrackKind::Sound)
                return true;
            if (!parseFloatArguments(a, 3, f))
                return setError(error, directive.line, "invalid @POS values");
            clip.hasPosition = true;
            clip.position = SceneVec3(f[0], f[1], f[2]);
            return true;
        }
        if (directive.name == "@ROT")
        {
            if (kind != SceneTrackKind::Light)
                return true;
            if (!parseFloatArguments(a, 2, f))
                return setError(error, directive.line, "invalid @ROT values");
            clip.hasRotation = true;
            clip.rotationStart = f[0];
            clip.rotationEnd = f[1];
            return true;
        }
        if (directive.name == "@RADIUS" || directive.name == "@ANGLE")
        {
            if (kind != SceneTrackKind::Light)
                return true;
            if (!parseFloatArguments(a, 1, f))
                return setError(error, directive.line,
                                "invalid light scalar value");
            if (directive.name == "@RADIUS")
            {
                clip.hasRadius = true;
                clip.radius = f[0];
            }
            else
            {
                clip.hasAngle = true;
                clip.angle = f[0];
            }
            return true;
        }
        if (directive.name == "@SOUND")
        {
            if (kind != SceneTrackKind::Sound)
                return true;
            if (!parseIntArguments(a, 4, i))
                return setError(error, directive.line, "invalid @SOUND values");
            clip.hasSound = true;
            for (int n = 0; n < 4; ++n)
                clip.sound[static_cast<std::size_t>(n)] = i[n];
            return true;
        }
        if (directive.name == "@MUSIC")
        {
            if (kind != SceneTrackKind::Music)
                return true;
            if (!parseIntArguments(a, 3, i))
                return setError(error, directive.line, "invalid @MUSIC values");
            clip.hasMusic = true;
            for (int n = 0; n < 3; ++n)
                clip.music[static_cast<std::size_t>(n)] = i[n];
            return true;
        }
        if (directive.name == "@SWITCH")
        {
            if (kind != SceneTrackKind::Music)
                return true;
            if (!parseIntArguments(a, 2, i))
                return setError(error, directive.line, "invalid @SWITCH values");
            clip.hasSwitch = true;
            clip.switchValues[0] = i[0];
            clip.switchValues[1] = i[1];
            return true;
        }
        if (directive.name == "@VOICE")
        {
            if (kind != SceneTrackKind::Voice)
                return true;
            if (!parseIntArguments(a, 2, i))
                return setError(error, directive.line, "invalid @VOICE values");
            clip.hasVoice = true;
            clip.voice[0] = i[0];
            clip.voice[1] = i[1];
            return true;
        }
        if (directive.name == "@LIP")
        {
            if (kind != SceneTrackKind::Voice)
                return true;
            if (a.size() != 2 || !parseIntToken(a[0], i[0]) ||
                !parseFloatToken(a[1], f[0]))
                return setError(error, directive.line, "invalid @LIP values");
            clip.hasLip = true;
            clip.lip = i[0];
            clip.lipDuration = f[0];
            return true;
        }
        if (directive.name == "@INTERPOLATE" ||
            directive.name == "@NOSTGCHKSW" || directive.name == "@OFFSETSW")
        {
            if (kind != SceneTrackKind::PlayerTask)
                return true;
            if (!parseIntArguments(a, 1, i))
                return setError(error, directive.line,
                                "invalid player task value");
            if (directive.name == "@INTERPOLATE")
            {
                clip.hasInterpolate = true;
                clip.interpolate = i[0];
            }
            else if (directive.name == "@NOSTGCHKSW")
            {
                clip.hasNoStageCheckSwitch = true;
                clip.noStageCheckSwitch = i[0];
            }
            else
            {
                clip.hasOffsetSwitch = true;
                clip.offsetSwitch = i[0];
            }
            return true;
        }
        if (directive.name == "@CAPTION")
        {
            if (kind != SceneTrackKind::Caption)
                return true;
            if (!parseIntArguments(a, 1, i))
                return setError(error, directive.line, "invalid @CAPTION value");
            clip.hasCaption = true;
            clip.caption = i[0];
            return true;
        }
        if (directive.name == "@VOLUME")
        {
            if (kind != SceneTrackKind::Volume)
                return true;
            if (!parseIntArguments(a, 2, i))
                return setError(error, directive.line, "invalid @VOLUME values");
            clip.hasVolume = true;
            clip.volumeStart = i[0];
            clip.volumeEnd = i[1];
            return true;
        }
        if (directive.name == "@MORPH")
        {
            if (kind != SceneTrackKind::Morph)
                return true;
            if (!parseIntArguments(a, 3, i))
                return setError(error, directive.line, "invalid @MORPH values");
            clip.hasMorph = true;
            clip.morph[0] = i[0];
            clip.morph[1] = i[1];
            clip.morph[2] = i[2];
            return true;
        }
        if (directive.name == "@VIBDIR" || directive.name == "@VIBACCELE" ||
            directive.name == "@VIBDECELE")
        {
            if (kind != SceneTrackKind::Vibration)
                return true;
            if (!parseIntArguments(a, 1, i))
                return setError(error, directive.line,
                                "invalid vibration integer value");
            if (directive.name == "@VIBDIR")
            {
                clip.hasVibrationDirection = true;
                clip.vibrationDirection = i[0];
            }
            else if (directive.name == "@VIBACCELE")
            {
                clip.hasVibrationAcceleration = true;
                clip.vibrationAcceleration = i[0];
            }
            else
            {
                clip.hasVibrationDeceleration = true;
                clip.vibrationDeceleration = i[0];
            }
            return true;
        }
        if (directive.name == "@VIBAMPLIFY" ||
            directive.name == "@VIBFREQUENCY")
        {
            if (kind != SceneTrackKind::Vibration)
                return true;
            if (!parseFloatArguments(a, 1, f))
                return setError(error, directive.line,
                                "invalid vibration float value");
            if (directive.name == "@VIBAMPLIFY")
            {
                clip.hasVibrationAmplify = true;
                clip.vibrationAmplify = f[0];
            }
            else
            {
                clip.hasVibrationFrequency = true;
                clip.vibrationFrequency = f[0];
            }
            return true;
        }
        if (directive.name == "@BLUR")
        {
            if (kind != SceneTrackKind::Effect)
                return true;
            if (!parseIntArguments(a, 1, i))
                return setError(error, directive.line, "invalid @BLUR value");
            clip.hasBlur = true;
            clip.blur = i[0];
            return true;
        }
        if (directive.name == "@TIMESCALE")
        {
            if (kind != SceneTrackKind::Effect)
                return true;
            if (!parseFloatArguments(a, 1, f))
                return setError(error, directive.line, "invalid @TIMESCALE value");
            clip.hasTimeScale = true;
            clip.timeScale = f[0];
            return true;
        }

        return setError(error, directive.line, "unhandled SCN property");
    }

    static void updateDuration(const SceneTrack& track,
                               float& duration,
                               bool& hasDuration)
    {
        for (const SceneClip& clip : track.clips)
        {
            duration = std::max(duration, clip.endTime);
            hasDuration = true;
            for (const SceneTrack& child : clip.children)
                updateDuration(child, duration, hasDuration);
        }
        for (const SceneTrack& child : track.children)
            updateDuration(child, duration, hasDuration);
    }
};

SceneVec3 add(SceneVec3 a, SceneVec3 b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

SceneVec3 lerp(SceneVec3 a, SceneVec3 b, float amount)
{
    return {
        a.x + (b.x - a.x) * amount,
        a.y + (b.y - a.y) * amount,
        a.z + (b.z - a.z) * amount
    };
}

SceneMatrix lerpMatrix(const SceneMatrix& a,
                       const SceneMatrix& b,
                       float amount)
{
    SceneMatrix result;
    result.position = lerp(a.position, b.position, amount);
    result.right = lerp(a.right, b.right, amount);
    result.up = lerp(a.up, b.up, amount);
    result.at = lerp(a.at, b.at, amount);
    return result;
}

SceneVec3 transformDirection(const SceneMatrix& matrix, SceneVec3 value)
{
    return {
        matrix.right.x * value.x + matrix.up.x * value.y +
            matrix.at.x * value.z,
        matrix.right.y * value.x + matrix.up.y * value.y +
            matrix.at.y * value.z,
        matrix.right.z * value.x + matrix.up.z * value.y +
            matrix.at.z * value.z
    };
}

SceneMatrix multiplyTransform(const SceneMatrix& parent,
                              const SceneMatrix& local)
{
    SceneMatrix result;
    result.right = transformDirection(parent, local.right);
    result.up = transformDirection(parent, local.up);
    result.at = transformDirection(parent, local.at);
    result.position = add(parent.position,
                          transformDirection(parent, local.position));
    return result;
}

SceneMatrix rotationX(float degrees)
{
    const float radians = degrees * 0.017453292f;
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);
    SceneMatrix result;
    result.right = SceneVec3(1.0f, 0.0f, 0.0f);
    result.up = SceneVec3(0.0f, cosine, sine);
    result.at = SceneVec3(0.0f, -sine, cosine);
    return result;
}

SceneMatrix rotationY(float degrees)
{
    const float radians = degrees * 0.017453292f;
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);
    SceneMatrix result;
    result.right = SceneVec3(cosine, 0.0f, -sine);
    result.up = SceneVec3(0.0f, 1.0f, 0.0f);
    result.at = SceneVec3(sine, 0.0f, cosine);
    return result;
}

SceneMatrix rotationZ(float degrees)
{
    const float radians = degrees * 0.017453292f;
    const float sine = std::sin(radians);
    const float cosine = std::cos(radians);
    SceneMatrix result;
    result.right = SceneVec3(cosine, sine, 0.0f);
    result.up = SceneVec3(-sine, cosine, 0.0f);
    result.at = SceneVec3(0.0f, 0.0f, 1.0f);
    return result;
}

bool resolveSceneModelTargetTracks(const std::vector<SceneTrack>& tracks,
                                   const std::string& targetName,
                                   int32_t matrixIndex,
                                   const std::string* inheritedActorName,
                                   SceneModelTarget& target)
{
    for (const SceneTrack& track : tracks)
    {
        const std::string* actorName = inheritedActorName;
        if (track.kind == SceneTrackKind::Player && !track.name.empty())
            actorName = &track.name;

        if (track.kind == SceneTrackKind::Player ||
            track.kind == SceneTrackKind::Model)
        {
            for (const SceneClip& clip : track.clips)
            {
                const char* name = clip.name.empty() ? "NONE" :
                                   clip.name.c_str();
                if (targetName == name)
                {
                    target.kind = track.kind == SceneTrackKind::Player ?
                        SceneModelTargetKind::ActorBone :
                        SceneModelTargetKind::ModelFrame;
                    target.track = &track;
                    target.clip = &clip;
                    target.actorTrackName = actorName;
                    target.matrixIndex = matrixIndex;
                    return true;
                }
            }
        }

        for (const SceneClip& clip : track.clips)
        {
            if (resolveSceneModelTargetTracks(clip.children, targetName,
                                              matrixIndex, actorName, target))
                return true;
        }
        if (resolveSceneModelTargetTracks(track.children, targetName,
                                          matrixIndex, actorName, target))
            return true;
    }
    return false;
}

float clipAmount(const SceneClip& clip, float time)
{
    const float duration = clip.endTime - clip.startTime;
    if (duration <= 0.0f)
        return 1.0f;

    const float elapsed = time - clip.startTime;
    if (elapsed <= 0.0f)
        return 0.0f;
    if (elapsed >= duration)
        return 1.0f;

    const float accelerationTime = clip.hasAcceleration ?
        duration * static_cast<float>(clip.acceleration) / 100.0f : 0.0f;
    const float decelerationTime = clip.hasDeceleration ?
        duration * static_cast<float>(clip.deceleration) / 100.0f : 0.0f;
    const float constantSpeedTime =
        duration - accelerationTime - decelerationTime;
    const float speed = 2.0f / (duration + constantSpeedTime);

    float amount;
    if (elapsed < accelerationTime)
    {
        amount = elapsed * elapsed * speed / (2.0f * accelerationTime);
    }
    else if (elapsed > duration - decelerationTime)
    {
        const float remaining = duration - elapsed;
        amount = speed * constantSpeedTime +
                 accelerationTime * speed * 0.5f +
                 decelerationTime * speed * 0.5f -
                 remaining * remaining * speed /
                     (2.0f * decelerationTime);
    }
    else
    {
        amount = speed * (elapsed - accelerationTime) +
                 accelerationTime * speed * 0.5f;
    }
    return std::max(0.0f, std::min(1.0f, amount));
}

}

bool parseScene(const uint8_t* data,
                std::size_t size,
                SceneDefinition& scene,
                std::string* error)
{
    scene = SceneDefinition();
    if (data == NULL && size != 0)
    {
        if (error != NULL)
            *error = "scene buffer is null";
        return false;
    }
    SceneParser parser(data, size);
    return parser.run(scene, error);
}

bool loadSceneFromXaf(const char* archivePath,
                      const char* entryName,
                      SceneDefinition& scene,
                      std::string* error)
{
    if (archivePath == nullptr || entryName == nullptr)
    {
        if (error != nullptr)
            *error = "archive and entry paths are required";
        return false;
    }
    std::vector<uint8_t> data;
    if (!readXafEntry(archivePath, entryName, data))
    {
        if (error != nullptr)
            *error = "could not read XAF scene entry";
        return false;
    }
    return parseScene(data.empty() ? NULL : &data[0], data.size(),
                      scene, error);
}

bool loadSceneFromXaf(std::string archivePath,
                      std::string entryName,
                      SceneDefinition& scene,
                      std::string* error)
{
    const std::string archive(archivePath);
    const std::string entry(entryName);
    return loadSceneFromXaf(archive.c_str(), entry.c_str(), scene, error);
}

bool resolveSceneModelTarget(const SceneDefinition& scene,
                             const SceneClip& modelClip,
                             SceneModelTarget& target)
{
    target = SceneModelTarget();
    if (!modelClip.hasTarget || modelClip.targetName == "(null)")
        return false;

    return resolveSceneModelTargetTracks(scene.tracks, modelClip.targetName,
                                         modelClip.targetId, nullptr, target);
}

SceneMatrix composeSceneModelBoneTransform(const SceneMatrix& boneMatrix,
                                           const SceneVec3& offset,
                                           const SceneVec3& rotationDegrees)
{

    SceneMatrix local =
        multiplyTransform(rotationZ(rotationDegrees.z),
                          multiplyTransform(rotationY(rotationDegrees.y),
                                            rotationX(rotationDegrees.x)));
    local.position = offset;
    return multiplyTransform(boneMatrix, local);
}

namespace
{

const uint32_t SCENE_CHUNK_NAME = 0x79032101;
const uint32_t SCENE_CHUNK_PTASK = 0x79032104;
const size_t SCENE_PTASK_HEADER_SIZE = 36;

uint16_t readScenePtask16(const uint8_t* data)
{
    return static_cast<uint16_t>(data[0]) |
           static_cast<uint16_t>(data[1] << 8);
}

uint32_t readScenePtask32(const uint8_t* data)
{
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

void copyScenePtaskWords(const uint8_t* data, size_t size,
                         std::vector<int16_t>& words)
{
    words.resize(size / 2);
    for(size_t i = 0; i < words.size(); ++i)
        words[i] = static_cast<int16_t>(readScenePtask16(data + i * 2));
}

bool appendScenePtaskProgram(const ScenePtaskPool& pool,
                             const int16_t* words, int wordCount,
                             ScenePtaskMotionProgram& program, int depth)
{
    if(words == nullptr || wordCount <= 0 || depth > 8)
        return false;

    int cursor = 0;
    int loopReturn = -1;
    int loopCounter = 0;
    int loopMotionStart = -1;
    int instructions = 0;
    while(cursor < wordCount && ++instructions <= 10000 &&
          program.motions.size() <= 4096)
    {
        const int step =
            moveStreamInstructionWords(words + cursor, wordCount - cursor);
        if(step <= 0 || step > wordCount - cursor)
            return false;

        const int16_t opcode = words[cursor];
        if(opcode == MOVE_STREAM_PLAY_MOTION)
        {
            ScenePtaskMotion motion;
            motion.motionId = words[cursor + 1];
            motion.blendFrames = words[cursor + 2];
            motion.maxDurationFrames = words[cursor + 3];
            motion.transitionFrames = words[cursor + 4];
            program.motions.push_back(motion);
        }
        else if(opcode == MOVE_STREAM_END)
        {
            return true;
        }
        else if(opcode == MOVE_STREAM_LOOP_BEGIN)
        {
            loopReturn = cursor + step;
            loopCounter = words[cursor + 1] - 1;
            loopMotionStart = static_cast<int>(program.motions.size());
        }
        else if(opcode == MOVE_STREAM_LOOP_END && loopCounter != 0)
        {
            if(loopReturn < 0 || loopMotionStart < 0)
                return false;
            if(loopCounter < 0)
            {
                if(loopMotionStart >= static_cast<int>(program.motions.size()))
                    return false;
                program.loopStart = loopMotionStart;
                return true;
            }
            --loopCounter;
            cursor = loopReturn;
            continue;
        }
        else if(opcode == MOVE_STREAM_CALL && words[cursor + 1] > 0)
        {
            const int calledMove = words[cursor + 1];
            const ScenePtaskRecord* called =
                findScenePtaskRecord(pool, calledMove);
            if(called != nullptr)
                return appendScenePtaskProgram(
                    pool, called->commandWords.data(),
                    static_cast<int>(called->commandWords.size()),
                    program, depth + 1);
            if(calledMove >= MOVE_RECORD_COUNT)
                return false;
            const MoveStream& stream = MOVE_STREAMS[calledMove];
            if(stream.firstWord < 0 || stream.wordCount <= 0)
                return true;
            return appendScenePtaskProgram(
                pool, &MOVE_STREAM_WORDS[stream.firstWord],
                stream.wordCount, program, depth + 1);
        }
        cursor += step;
    }
    return false;
}

}

const ScenePtaskRecord*
findScenePtaskRecord(const ScenePtaskPool& pool, int moveId)
{
    for(size_t i = 0; i < pool.records.size(); ++i)
        if(pool.records[i].moveId == moveId)
            return &pool.records[i];
    return nullptr;
}

bool
loadScenePtaskPool(const char* archivePath, const char* sceneEntry,
                   ScenePtaskPool& pool)
{
    pool.records.clear();
    if(archivePath == nullptr || sceneEntry == nullptr)
        return false;

    std::string companion(sceneEntry);
    const size_t dot = companion.rfind('.');
    if(dot != std::string::npos)
        companion.erase(dot);
    companion += ".RWS";

    std::vector<uint8_t> data;
    if(!readXafEntry(archivePath, companion.c_str(), data))
        return false;

    std::string authoringPath;
    for(size_t offset = 0; offset + 12 <= data.size();)
    {
        const uint8_t* header = data.data() + offset;
        const uint32_t type = readScenePtask32(header);
        const uint32_t length = readScenePtask32(header + 4);
        const uint64_t end = static_cast<uint64_t>(offset) + 12 + length;
        if(length == 0 || end > data.size())
            return false;
        const uint8_t* body = header + 12;

        if(type == SCENE_CHUNK_NAME)
        {
            const uint8_t* terminator = static_cast<const uint8_t*>(
                memchr(body, 0, length));
            authoringPath.assign(
                reinterpret_cast<const char*>(body),
                terminator != nullptr ?
                    static_cast<size_t>(terminator - body) : length);
        }
        else if(type == SCENE_CHUNK_PTASK)
        {
            if(length < SCENE_PTASK_HEADER_SIZE)
                return false;
            const uint32_t moveId = readScenePtask32(body);
            if(moveId > UINT16_MAX ||
               findScenePtaskRecord(pool, static_cast<int>(moveId)) != nullptr)
            {
                offset = static_cast<size_t>(end);
                continue;
            }

            uint32_t sectionOffset[4];
            uint32_t sectionSize[4];
            uint32_t expected = static_cast<uint32_t>(
                SCENE_PTASK_HEADER_SIZE);
            bool valid = true;
            for(int section = 0; section < 4; ++section)
            {
                sectionOffset[section] =
                    readScenePtask32(body + 4 + section * 8);
                sectionSize[section] =
                    readScenePtask32(body + 8 + section * 8);
                if(sectionOffset[section] != expected ||
                   static_cast<uint64_t>(sectionOffset[section]) +
                       sectionSize[section] > length)
                    valid = false;
                expected = sectionOffset[section] + sectionSize[section];
            }
            if(!valid || length - expected > 3 ||
               (sectionSize[0] & 1) != 0 || (sectionSize[1] & 1) != 0 ||
               (sectionSize[2] != 0x3c && sectionSize[2] != 0x40) ||
               sectionSize[3] != 0x10)
                return false;

            ScenePtaskRecord record;
            record.moveId = static_cast<uint16_t>(moveId);
            record.authoringPath = authoringPath;
            copyScenePtaskWords(
                body + sectionOffset[0], sectionSize[0],
                record.commandWords);
            copyScenePtaskWords(
                body + sectionOffset[1], sectionSize[1],
                record.behaviourWords);
            record.attributeBytes.assign(
                body + sectionOffset[2],
                body + sectionOffset[2] + sectionSize[2]);
            record.moveBehaviourBytes.assign(
                body + sectionOffset[3],
                body + sectionOffset[3] + sectionSize[3]);
            pool.records.push_back(record);
        }
        offset = static_cast<size_t>(end);
    }
    return !pool.records.empty();
}

bool
buildScenePtaskMotionProgram(const ScenePtaskPool& pool, int moveId,
                             ScenePtaskMotionProgram& program)
{
    program = ScenePtaskMotionProgram();
    const ScenePtaskRecord* record = findScenePtaskRecord(pool, moveId);
    if(record != nullptr)
        return appendScenePtaskProgram(
                   pool, record->commandWords.data(),
                   static_cast<int>(record->commandWords.size()), program, 0) &&
               !program.motions.empty();
    if(moveId < 0 || moveId >= MOVE_RECORD_COUNT)
        return false;
    const MoveStream& stream = MOVE_STREAMS[moveId];
    if(stream.firstWord < 0 || stream.wordCount <= 0)
        return false;
    return appendScenePtaskProgram(
               pool, &MOVE_STREAM_WORDS[stream.firstWord], stream.wordCount,
               program, 0) && !program.motions.empty();
}

bool retainSceneActorPlacement(SceneActorPlacementState& retained,
                               const SceneActorState& state)
{
    if (state.kind == SceneTrackKind::PlayerTask)
        retained.noStageCheck = state.noStageCheck;

    if (!state.applyPlacement ||
        (state.kind != SceneTrackKind::Player &&
         state.kind != SceneTrackKind::PlayerTask))
        return false;

    retained.logicalPosition = state.position;
    retained.heading = state.rotation.x;
    retained.renderedRootY = state.position.y;
    retained.placed = true;
    return true;
}
bool advanceSceneMotionPlayback(SceneMotionPlaybackState& state,
                                 float elapsedSeconds, int motionCount,
                                 int loopStart)
{
    if(state.clipSeconds < 0.0f ||
       elapsedSeconds - state.startSeconds < state.clipSeconds)
        return false;
    int next = state.index + 1;
    if(next >= motionCount)
    {
        if(loopStart < 0 || loopStart >= motionCount)
            return false;
        next = loopStart;
        ++state.sequenceGeneration;
    }
    state.index = next;
    state.startSeconds = elapsedSeconds;
    state.clipSeconds = -1.0f;
    return true;
}

void beginSceneActorRootMotion(SceneActorRootMotionState& retained,
                               const SceneVec3& logicalPosition,
                               const SceneVec3& rootWorldPosition)
{
    retained.logicalPosition = logicalPosition;
    retained.startLogicalPosition = logicalPosition;
    retained.startRootWorldPosition = rootWorldPosition;
    retained.terminalRootWorldPosition = rootWorldPosition;
    retained.active = true;
    retained.committed = false;
}

bool commitSceneActorRootMotion(SceneActorRootMotionState& retained,
                                const SceneVec3& rootWorldPosition)
{
    if (!retained.active || retained.committed)
        return false;

    retained.logicalPosition.x = retained.startLogicalPosition.x +
        rootWorldPosition.x - retained.startRootWorldPosition.x;
    retained.logicalPosition.y = retained.startLogicalPosition.y +
        rootWorldPosition.y - retained.startRootWorldPosition.y;
    retained.logicalPosition.z = retained.startLogicalPosition.z +
        rootWorldPosition.z - retained.startRootWorldPosition.z;
    retained.terminalRootWorldPosition = rootWorldPosition;
    retained.committed = true;
    return true;
}

bool resolveSceneActorRootY(SceneActorPlacementState& retained,
                            float currentRootY,
                            float scaledDeltaSeconds,
                            SceneGroundHeightCallback groundHeight,
                            void* userData,
                            float* rootY)
{
    if (!retained.placed || rootY == nullptr)
        return false;

    retained.renderedRootY = currentRootY;
    if (retained.noStageCheck)
    {
        retained.renderedRootY = retained.logicalPosition.y;
        *rootY = retained.renderedRootY;
        return true;
    }
    if (scaledDeltaSeconds == 0.0f || groundHeight == nullptr)
        return false;

    float height = 0.0f;
    if (!groundHeight(userData, retained.logicalPosition.x,
                      retained.logicalPosition.z,
                      currentRootY + 16.0f, &height))
        return false;

    retained.renderedRootY = height;
    *rootY = height;
    return true;
}

struct SceneRuntime::RuntimeClip
{
    const SceneTrack* track = nullptr;
    const SceneClip* clip = nullptr;
    const std::string* actorName = nullptr;
    SceneTrackKind parentKind = SceneTrackKind::Camera;

    const std::vector<SceneClip>* siblings = nullptr;
    std::size_t siblingIndex = 0;
};

namespace
{

bool isCurveCameraClip(const SceneClip& clip)
{
    return clip.hasCurve && clip.curve != 0;
}

bool curveGroupBounds(const std::vector<SceneClip>* siblings,
                      std::size_t index,
                      std::size_t& begin,
                      std::size_t& end)
{
    if (siblings == nullptr || index >= siblings->size() ||
        !isCurveCameraClip((*siblings)[index]))
        return false;

    begin = index;
    while (begin != 0 && isCurveCameraClip((*siblings)[begin - 1]))
        --begin;

    end = index;
    while (end + 1 < siblings->size() &&
           isCurveCameraClip((*siblings)[end + 1]))
        ++end;
    return true;
}

SceneMatrix curveMatrixStart(const SceneClip& clip)
{
    return clip.hasMatrixStart ? clip.matrixStart : SceneMatrix();
}

SceneMatrix curveMatrixEnd(const SceneClip& clip)
{
    return clip.hasMatrixEnd ? clip.matrixEnd : curveMatrixStart(clip);
}

SceneVec3 cubicBSpline(SceneVec3 p0,
                       SceneVec3 p1,
                       SceneVec3 p2,
                       SceneVec3 p3,
                       float t)
{

    const float t2 = t * t;
    const float t3 = t2 * t;
    const float b0 = (1.0f - 3.0f * t + 3.0f * t2 - t3) / 6.0f;
    const float b1 = (4.0f - 6.0f * t2 + 3.0f * t3) / 6.0f;
    const float b2 = (1.0f + 3.0f * t + 3.0f * t2 - 3.0f * t3) / 6.0f;
    const float b3 = t3 / 6.0f;
    return SceneVec3(
        p0.x * b0 + p1.x * b1 + p2.x * b2 + p3.x * b3,
        p0.y * b0 + p1.y * b1 + p2.y * b2 + p3.y * b3,
        p0.z * b0 + p1.z * b1 + p2.z * b2 + p3.z * b3);
}

SceneMatrix splineWeightedMatrix(const std::vector<SceneMatrix>& controls,
                                 const std::array<float, 100>& coefficients,
                                 std::size_t dimension,
                                 std::size_t row,
                                 std::size_t sourceBegin)
{
    SceneMatrix result;
    result.position = SceneVec3(0.0f, 0.0f, 0.0f);
    result.right = SceneVec3(0.0f, 0.0f, 0.0f);
    result.up = SceneVec3(0.0f, 0.0f, 0.0f);
    result.at = SceneVec3(0.0f, 0.0f, 0.0f);
    for (std::size_t j = 0; j < dimension; ++j)
    {
        const float weight = coefficients[row * dimension + j];
        const SceneMatrix& source = controls[sourceBegin + j];
        result.position.x += source.position.x * weight;
        result.position.y += source.position.y * weight;
        result.position.z += source.position.z * weight;
        result.right.x += source.right.x * weight;
        result.right.y += source.right.y * weight;
        result.right.z += source.right.z * weight;
        result.up.x += source.up.x * weight;
        result.up.y += source.up.y * weight;
        result.up.z += source.up.z * weight;
        result.at.x += source.at.x * weight;
        result.at.y += source.at.y * weight;
        result.at.z += source.at.z * weight;
    }
    return result;
}

std::vector<SceneMatrix> generateSplinePoints(
    const std::vector<SceneMatrix>& controls)
{

    const std::size_t dimension = std::min<std::size_t>(controls.size(), 10);
    std::array<float, 100> coefficients = {};
    std::array<int, 10> series = {};
    std::array<int, 10> secondSeries = {};
    series[0] = 1;
    series[1] = 4;
    for (std::size_t i = 2; i < dimension - 1; ++i)
        series[i] = 4 * series[i - 1] - series[i - 2];
    const int divisor = series[dimension - 2];
    secondSeries[0] = 1;
    secondSeries[1] = 2;
    for (std::size_t i = 2; i < dimension; ++i)
        secondSeries[i] = 4 * secondSeries[i - 1] - secondSeries[i - 2];
    for (int i = static_cast<int>(dimension) - 2; i >= 0; i -= 2)
        secondSeries[static_cast<std::size_t>(i)] =
            -secondSeries[static_cast<std::size_t>(i)];
    const int multiplier = (dimension & 1) != 0 ? 2 : -2;
    for (std::size_t i = 0; i < dimension; ++i)
    {
        coefficients[dimension * i] =
            static_cast<float>(secondSeries[dimension - 1 - i]) /
            static_cast<float>(divisor);
        for (std::size_t j = 1; j <= i; ++j)
        {
            coefficients[dimension * i + j] =
                static_cast<float>(
                    secondSeries[j] * secondSeries[dimension - 1 - i] *
                    multiplier) /
                static_cast<float>(divisor);
        }
    }
    coefficients[dimension * dimension - 1] = coefficients[0];
    for (std::size_t i = 0; i < dimension; ++i)
    {
        for (std::size_t j = dimension - 1; j > i; --j)
        {
            coefficients[i * dimension + j] =
                coefficients[dimension * (dimension - i) - 1 - j];
        }
    }

    const std::size_t significant = dimension;
    std::vector<SceneMatrix> generated;
    generated.reserve(controls.size());
    const std::size_t half = significant / 2;
    for (std::size_t i = 0; i < controls.size(); ++i)
    {
        std::size_t row = 0;
        std::size_t sourceBegin = 0;
        if (i < half)
        {
            row = i;
        }
        else if (i < controls.size() - half)
        {
            row = half;
            sourceBegin = i - half;
        }
        else
        {
            row = significant - controls.size() + i;
            sourceBegin = controls.size() - significant;
        }
        generated.push_back(splineWeightedMatrix(
            controls, coefficients, significant, row, sourceBegin));
    }
    return generated;
}

SceneMatrix sampleCurveSpline(const std::vector<SceneClip>& siblings,
                              std::size_t begin,
                              std::size_t end,
                              float progress)
{
    const std::size_t cutCount = end - begin + 1;
    std::vector<SceneMatrix> controls;
    controls.reserve(cutCount + 1);
    controls.push_back(curveMatrixStart(siblings[begin]));
    for (std::size_t i = begin; i <= end; ++i)
        controls.push_back(curveMatrixEnd(siblings[i]));

    const std::vector<SceneMatrix> generated =
        generateSplinePoints(controls);
    auto endpointWindow = [&](std::size_t index) -> const SceneMatrix&
    {
        if (index == 0)
            return generated.back();
        return generated[(index - 1) % generated.size()];
    };

    const float scaled = progress * static_cast<float>(cutCount);
    const std::size_t segment = std::min(
        cutCount - 1, static_cast<std::size_t>(std::floor(scaled)));
    float local = scaled - static_cast<float>(segment);
    local = std::max(0.0f, std::min(1.0f, local));
    const SceneMatrix& p0 = endpointWindow(segment);
    const SceneMatrix& p1 = endpointWindow(segment + 1);
    const SceneMatrix& p2 = endpointWindow(segment + 2);
    const SceneMatrix& p3 = endpointWindow(segment + 3);
    SceneMatrix result;
    result.position = cubicBSpline(p0.position, p1.position,
                                   p2.position, p3.position, local);
    result.right = cubicBSpline(p0.right, p1.right, p2.right,
                                p3.right, local);
    result.up = cubicBSpline(p0.up, p1.up, p2.up, p3.up, local);
    result.at = cubicBSpline(p0.at, p1.at, p2.at, p3.at, local);
    return result;
}

float curveGroupProgress(const std::vector<SceneClip>& siblings,
                         std::size_t begin,
                         std::size_t end,
                         float time)
{
    const float weight = 1.0f /
        static_cast<float>(end - begin + 1);
    float progress = 0.0f;
    for (std::size_t i = begin; i <= end; ++i)
    {
        const SceneClip& clip = siblings[i];
        const float duration = clip.endTime - clip.startTime;
        if (time >= clip.startTime)
        {
            if (time <= clip.endTime)
            {
                const float local = duration > 0.0f ?
                    (time - clip.startTime) / duration : 1.0f;
                progress += weight *
                    std::max(0.0f, std::min(1.0f, local));
            }
            else
            {
                progress += weight;
            }
        }
    }
    return std::max(0.0f, std::min(1.0f, progress));
}

}

struct SceneRuntime::RuntimeClipState
{
    bool started = false;
    bool stopped = false;

    bool applied = false;
};

struct SceneRuntime::ResourceState
{
    SceneTrackKind kind = SceneTrackKind::Model;
    std::string name;
    SceneResourceHandle handle = 0;
};

SceneRuntime::SceneRuntime()
    : scene_(), bindings_(), clips_(), clipStates_(), resources_(),
      status_(SceneStatus::Empty), time_(0.0f), skipRequested_(false),
      restored_(false)
{
}

SceneRuntime::~SceneRuntime()
{
    stop();
}

bool SceneRuntime::load(const SceneDefinition& scene,
                        const ScenePresentationBindings& bindings,
                        std::string* error)
{
    stop();
    scene_ = scene;
    bindings_ = bindings;
    clips_.clear();
    clipStates_.clear();
    resources_.clear();
    time_ = 0.0f;
    skipRequested_ = false;
    restored_ = false;
    if (!std::isfinite(scene_.duration) || scene_.duration < 0.0f)
    {
        status_ = SceneStatus::Failed;
        if (error != nullptr)
            *error = "scene has an invalid duration";
        return false;
    }
    flattenTracks(scene_.tracks, nullptr, nullptr);
    clipStates_.resize(clips_.size());
    if (!acquireResources(error))
    {
        status_ = SceneStatus::Failed;
        releaseResources();
        return false;
    }
    status_ = SceneStatus::Loaded;
    return true;
}

bool SceneRuntime::load(const uint8_t* data,
                        std::size_t size,
                        const ScenePresentationBindings& bindings,
                        std::string* error)
{
    SceneDefinition scene;
    if (!parseScene(data, size, scene, error))
        return false;
    return load(scene, bindings, error);
}

bool SceneRuntime::load(const std::vector<uint8_t>& data,
                        const ScenePresentationBindings& bindings,
                        std::string* error)
{
    return load(data.empty() ? NULL : &data[0], data.size(), bindings, error);
}

bool SceneRuntime::loadFromXaf(const char* archivePath,
                               const char* entryName,
                               const ScenePresentationBindings& bindings,
                               std::string* error)
{
    SceneDefinition scene;
    if (!loadSceneFromXaf(archivePath, entryName, scene, error))
        return false;
    return load(scene, bindings, error);
}

bool SceneRuntime::start()
{
    if (status_ != SceneStatus::Loaded)
        return false;
    for (RuntimeClipState& state : clipStates_)
        state = RuntimeClipState();
    time_ = 0.0f;
    skipRequested_ = false;
    restored_ = false;
    status_ = SceneStatus::Playing;
    return true;
}

SceneUpdateResult SceneRuntime::update(float scaledDeltaSeconds,
                                       bool skipRequested)
{
    SceneUpdateResult result;
    result.status = status_;
    result.time = time_;
    if (status_ != SceneStatus::Playing)
    {
        result.finished = status_ == SceneStatus::Completed ||
                          status_ == SceneStatus::Skipped ||
                          status_ == SceneStatus::Failed;
        result.skipped = status_ == SceneStatus::Skipped;
        return result;
    }

    if (!std::isfinite(scaledDeltaSeconds) || scaledDeltaSeconds < 0.0f)
    {
        status_ = SceneStatus::Failed;
        restore(false, true);
        result.status = status_;
        result.finished = true;
        return result;
    }

    if (skipRequested || skipRequested_)
    {
        for (std::size_t i = 0; i < clips_.size(); ++i)
        {
            if (clipStates_[i].started && !clipStates_[i].stopped)
                stopClip(i);
        }
        time_ = scene_.duration;
        status_ = SceneStatus::Skipped;
        restore(true, false);
        result.status = status_;
        result.time = time_;
        result.skipped = true;
        result.finished = true;
        return result;
    }

    const float sampleTime = time_;
    for (std::size_t i = 0; i < clips_.size(); ++i)
    {
        const SceneClip& clip = *clips_[i].clip;
        RuntimeClipState& state = clipStates_[i];
        if (state.started && !state.stopped && sampleTime >= clip.endTime)
            stopClip(i);
    }

    for (std::size_t i = 0; i < clips_.size(); ++i)
    {
        const SceneClip& clip = *clips_[i].clip;
        RuntimeClipState& state = clipStates_[i];
        if (state.started || sampleTime < clip.startTime)
            continue;
        state.started = true;
        if (sampleTime < clip.endTime)
            startClip(i);
        else
            state.stopped = true;
    }

    applyAtTime(sampleTime);

    const float oldTime = time_;
    time_ = std::min(scene_.duration, time_ + scaledDeltaSeconds);
    result.advanced = time_ != oldTime;
    if (time_ >= scene_.duration)
    {
        for (std::size_t i = 0; i < clips_.size(); ++i)
        {
            if (clipStates_[i].started && !clipStates_[i].stopped)
                stopClip(i);
        }
        status_ = SceneStatus::Completed;
        restore(false, false);
    }
    result.status = status_;
    result.time = time_;
    result.finished = status_ == SceneStatus::Completed;
    return result;
}

void SceneRuntime::requestSkip()
{
    skipRequested_ = true;
}

void SceneRuntime::stop()
{
    if (status_ == SceneStatus::Playing)
    {
        for (std::size_t i = 0; i < clips_.size(); ++i)
        {
            if (clipStates_[i].started && !clipStates_[i].stopped)
                stopClip(i);
        }
        restore(false, false);
    }
    else
        releaseResources();
    clips_.clear();
    clipStates_.clear();
    resources_.clear();
    scene_ = SceneDefinition();
    bindings_ = ScenePresentationBindings();
    status_ = SceneStatus::Empty;
    time_ = 0.0f;
    skipRequested_ = false;
    restored_ = false;
}

SceneStatus SceneRuntime::status() const
{
    return status_;
}

float SceneRuntime::time() const
{
    return time_;
}

float SceneRuntime::duration() const
{
    return scene_.duration;
}

const SceneDefinition* SceneRuntime::definition() const
{
    return status_ == SceneStatus::Empty ? nullptr : &scene_;
}

void SceneRuntime::flattenTracks(const std::vector<SceneTrack>& tracks,
                                 const SceneTrack* parent,
                                 const std::string* inheritedName)
{
    const SceneTrackKind parentKind = parent != nullptr ?
        parent->kind : SceneTrackKind::Camera;
    for (const SceneTrack& track : tracks)
    {
        const std::string* actorName = inheritedName;
        if((track.kind == SceneTrackKind::Model ||
            track.kind == SceneTrackKind::Player) &&
           !track.name.empty())
            actorName = &track.name;

        for (std::size_t clipIndex = 0; clipIndex < track.clips.size();
             ++clipIndex)
        {
            const SceneClip& clip = track.clips[clipIndex];
            clips_.push_back({&track, &clip, actorName, parentKind,
                              &track.clips, clipIndex});
            flattenTracks(clip.children, &track, actorName);
        }
        flattenTracks(track.children, &track, actorName);
    }
}

bool SceneRuntime::acquireResources(std::string* error)
{
    auto acquire = [this, error](SceneTrackKind kind,
                                 const std::string& name) -> bool
    {
        if (isNoResourceName(name))
            return true;
        for (const ResourceState& resource : resources_)
        {
            if (resource.kind == kind && resource.name == name)
                return true;
        }
        SceneResourceHandle handle = 0;
        if (bindings_.acquireResource != nullptr &&
            !bindings_.acquireResource(bindings_.userData, kind,
                                       name.c_str(), &handle))
        {
            if (error != nullptr)
                *error = "resource acquisition failed: " + name;
            return false;
        }
        resources_.push_back({kind, name, handle});
        return true;
    };

    if (!acquire(SceneTrackKind::Motion, scene_.motionResource) ||
        !acquire(SceneTrackKind::Music, scene_.soundbank) ||
        !acquire(SceneTrackKind::Caption, scene_.fadeText))
        return false;
    for (const SceneCaption& caption : scene_.captions)
    {
        if (!acquire(SceneTrackKind::Caption, caption.path))
            return false;
    }
    for (const RuntimeClip& runtimeClip : clips_)
    {
        if (!acquire(runtimeClip.track->kind, runtimeClip.clip->resourceName))
            return false;
    }
    return true;
}

void SceneRuntime::releaseResources()
{
    if (bindings_.releaseResource != nullptr)
    {
        for (auto it = resources_.rbegin(); it != resources_.rend(); ++it)
        {
            bindings_.releaseResource(bindings_.userData, it->kind,
                                       it->name.c_str(), it->handle);
        }
    }
    resources_.clear();
}

void SceneRuntime::startClip(std::size_t index)
{
    const RuntimeClip& runtimeClip = clips_[index];
    const SceneTrack& track = *runtimeClip.track;
    const SceneClip& clip = *runtimeClip.clip;
    if (!track.enabled)
        return;

    if (track.kind == SceneTrackKind::Sound ||
        track.kind == SceneTrackKind::Music ||
        track.kind == SceneTrackKind::Voice ||
        track.kind == SceneTrackKind::Vibration)
    {
        if (bindings_.audio == nullptr)
            return;
        SceneAudioEvent event;
        event.active = true;
        event.trackName = runtimeClip.actorName != nullptr ?
            runtimeClip.actorName->c_str() : track.name.c_str();
        event.resourceName = clip.resourceName.c_str();
        if (track.kind == SceneTrackKind::Sound)
        {
            event.kind = SceneAudioKind::Sound;
            event.values = clip.sound;
        }
        else if (track.kind == SceneTrackKind::Music)
        {
            event.kind = SceneAudioKind::Music;
            if (clip.hasMusic)
            {
                event.values[0] = clip.music[0];
                event.values[1] = clip.music[1];
                event.values[2] = clip.music[2];
                event.values[3] = 0;
            }
            else if (clip.hasSwitch)
            {
                event.values[0] = clip.switchValues[0];
                event.values[1] = clip.switchValues[1];
                event.values[2] = 0;
                event.values[3] = 0;
            }
        }
        else if (track.kind == SceneTrackKind::Voice)
        {
            event.kind = SceneAudioKind::Voice;
            event.values[0] = clip.voice[0];
            event.values[1] = clip.voice[1];
            event.values[2] = clip.lip;
            event.values[3] = 0;
            event.lip = clip.lip;
            event.lipRate = clip.lipDuration;
        }
        else
        {
            event.kind = SceneAudioKind::Vibration;
            event.values[0] = clip.vibrationDirection;
            event.values[1] = clip.vibrationAcceleration;
            event.values[2] = clip.vibrationDeceleration;
            event.values[3] = 0;
            event.vibrationDirection = clip.vibrationDirection;
            event.vibrationAmplify = clip.vibrationAmplify;
            event.vibrationFrequency = clip.vibrationFrequency;
            event.vibrationAcceleration = clip.vibrationAcceleration;
            event.vibrationDeceleration = clip.vibrationDeceleration;
        }
        bindings_.audio(bindings_.userData, event);
    }
    else if (track.kind == SceneTrackKind::Caption &&
             bindings_.caption != nullptr)
    {
        SceneCaptionEvent event;
        event.active = true;
        event.caption = clip.caption;
        event.trackName = track.name.c_str();
        event.language = bindings_.language;
        for (const SceneCaption& caption : scene_.captions)
        {
            if (caption.language == bindings_.language)
            {
                event.path = caption.path.c_str();
                break;
            }
        }
        if ((event.path == NULL || event.path[0] == '\0') &&
            !scene_.captions.empty())
        {
            event.language = scene_.captions.front().language;
            event.path = scene_.captions.front().path.c_str();
        }
        bindings_.caption(bindings_.userData, event);
    }
    else if (track.kind == SceneTrackKind::Effect &&
             bindings_.effect != nullptr)
    {
        SceneEffectState state;
        state.active = true;
        state.trackName = track.name.c_str();
        state.blur = clip.blur;
        state.timeScale = clip.timeScale;
        bindings_.effect(bindings_.userData, state);
    }
}

void SceneRuntime::stopClip(std::size_t index)
{
    RuntimeClipState& clipState = clipStates_[index];
    if (clipState.stopped)
        return;
    clipState.stopped = true;
    const RuntimeClip& runtimeClip = clips_[index];
    const SceneTrack& track = *runtimeClip.track;
    const SceneClip& clip = *runtimeClip.clip;
    if (!track.enabled)
        return;

    if ((track.kind == SceneTrackKind::Sound ||
         track.kind == SceneTrackKind::Music ||
         track.kind == SceneTrackKind::Voice ||
         track.kind == SceneTrackKind::Vibration ||
         track.kind == SceneTrackKind::Volume) &&
        bindings_.audio != nullptr)
    {
        SceneAudioEvent event;
        event.active = false;
        event.trackName = track.name.c_str();
        event.resourceName = clip.resourceName.c_str();
        if (track.kind == SceneTrackKind::Sound)
        {
            event.kind = SceneAudioKind::Sound;
            event.values = clip.sound;
        }
        else if (track.kind == SceneTrackKind::Music)
        {
            event.kind = SceneAudioKind::Music;
            event.values[0] = clip.music[0];
                event.values[1] = clip.music[1];
                event.values[2] = clip.music[2];
                event.values[3] = 0;
        }
        else if (track.kind == SceneTrackKind::Voice)
        {
            event.kind = SceneAudioKind::Voice;
            event.trackName = runtimeClip.actorName != nullptr ?
                runtimeClip.actorName->c_str() : track.name.c_str();
            event.values[0] = clip.voice[0];
            event.values[1] = clip.voice[1];
            event.values[2] = clip.lip;
            event.values[3] = 0;
            event.lip = clip.lip;
            event.lipRate = clip.lipDuration;
        }
        else if(track.kind == SceneTrackKind::Vibration)
        {
            event.kind = SceneAudioKind::Vibration;
            event.values[0] = clip.vibrationDirection;
            event.values[1] = clip.vibrationAcceleration;
            event.values[2] = clip.vibrationDeceleration;
            event.values[3] = 0;
            event.vibrationDirection = clip.vibrationDirection;
            event.vibrationAmplify = clip.vibrationAmplify;
            event.vibrationFrequency = clip.vibrationFrequency;
            event.vibrationAcceleration = clip.vibrationAcceleration;
            event.vibrationDeceleration = clip.vibrationDeceleration;
        }
        else
        {
            event.kind = runtimeClip.parentKind == SceneTrackKind::Music ?
                SceneAudioKind::Music : SceneAudioKind::Voice;
            event.volumeControl = true;
            event.volume = static_cast<float>(clip.volumeEnd) / 100.0f;
        }
        bindings_.audio(bindings_.userData, event);
    }
    else if(track.kind == SceneTrackKind::Effect &&
            bindings_.effect != nullptr)
    {
        SceneEffectState state;
        state.active = false;
        state.trackName = track.name.c_str();
        state.blur = clip.blur;
        state.timeScale = clip.timeScale;
        bindings_.effect(bindings_.userData, state);
    }
    else if (track.kind == SceneTrackKind::Caption &&
             bindings_.caption != nullptr)
    {
        SceneCaptionEvent event;
        event.active = false;
        event.caption = clip.caption;
        event.trackName = track.name.c_str();
        event.language = bindings_.language;
        bindings_.caption(bindings_.userData, event);
    }
}

void SceneRuntime::applyAtTime(float time)
{
    for (std::size_t i = 0; i < clips_.size(); ++i)
    {
        const RuntimeClipState& state = clipStates_[i];
        const SceneClip& clip = *clips_[i].clip;
        if (!state.started || state.stopped || time < clip.startTime ||
            time >= clip.endTime)
            continue;
        if (!clips_[i].track->enabled)
            continue;
        applyClip(i, time);
    }
}

namespace
{

bool findLightTrackOrdinal(const std::vector<SceneTrack>& tracks,
                           const SceneTrack* target, int32_t& ordinal)
{
    for (std::size_t i = 0; i < tracks.size(); ++i)
    {
        const SceneTrack& candidate = tracks[i];
        if (&candidate == target)
            return true;
        if (candidate.kind == SceneTrackKind::Light)
            ++ordinal;
        if (findLightTrackOrdinal(candidate.children, target, ordinal))
            return true;
    }
    return false;
}

}

int32_t SceneRuntime::lightTrackIndex(const SceneTrack* track) const
{
    if (track == nullptr)
        return -1;
    int32_t ordinal = 0;
    if (!findLightTrackOrdinal(scene_.tracks, track, ordinal))
        return -1;
    return ordinal;
}

void SceneRuntime::applyClip(std::size_t index, float time)
{
    const RuntimeClip& runtimeClip = clips_[index];
    const SceneTrack& track = *runtimeClip.track;
    const SceneClip& clip = *runtimeClip.clip;
    const float amount = clipAmount(clip, time);
    const bool firstApply = !clipStates_[index].applied;
    clipStates_[index].applied = true;

    if (track.kind == SceneTrackKind::Camera && bindings_.applyCamera != nullptr)
    {

        SceneMatrix start = clip.hasMatrixStart ? clip.matrixStart :
                             SceneMatrix();
        SceneMatrix end = clip.hasMatrixEnd ? clip.matrixEnd : start;
        float cameraAmount = amount;
        float viewAmount = amount;
        std::size_t groupBegin = 0;
        std::size_t groupEnd = 0;
        const bool groupedCurve =
            track.kind == SceneTrackKind::Camera &&
            curveGroupBounds(runtimeClip.siblings, runtimeClip.siblingIndex,
                             groupBegin, groupEnd);
        if (groupedCurve)
        {
            const std::vector<SceneClip>& siblings = *runtimeClip.siblings;
            const std::size_t cutCount = groupEnd - groupBegin + 1;
            const float duration = clip.endTime - clip.startTime;
            const float elapsed = time - clip.startTime;
            const float localAmount = duration > 0.0f ?
                std::max(0.0f, std::min(1.0f, elapsed / duration)) : 1.0f;
            viewAmount = localAmount;
            if (cutCount < 3)
            {
                if (runtimeClip.siblingIndex > groupBegin)
                {
                    const SceneClip& previous =
                        siblings[runtimeClip.siblingIndex - 1];
                    start = curveMatrixEnd(previous);
                }
                cameraAmount = localAmount;
                end = curveMatrixEnd(clip);
            }
            else
            {
                const float progress = curveGroupProgress(
                    siblings, groupBegin, groupEnd, time);
                cameraAmount = progress;
                start = sampleCurveSpline(
                    siblings, groupBegin, groupEnd, progress);
                end = start;
            }
        }
        SceneCameraState state;
        state.matrix = groupedCurve && (groupEnd - groupBegin + 1) >= 3 ?
            start : lerpMatrix(start, end, cameraAmount);
        state.eye = state.matrix.position;
        state.target = add(state.matrix.position, state.matrix.at);
        state.fieldOfView = clip.hasViewAngle ?
            clip.viewAngleStart + (clip.viewAngleEnd - clip.viewAngleStart) *
                viewAmount :
            60.0f;
        state.nearClip = clip.nearClip;
        state.farClip = clip.farClip;
        state.targetId = clip.targetId;
        state.targetName = clip.targetName == "(null)" ?
                           NULL : clip.targetName.c_str();
        state.resourceName = clip.resourceName.c_str();
        bindings_.applyCamera(bindings_.userData, state);
        return;
    }

    if (track.kind == SceneTrackKind::Model ||
        track.kind == SceneTrackKind::Player ||
        track.kind == SceneTrackKind::PlayerTask)
    {
        if (bindings_.applyActor == nullptr)
            return;
        SceneActorState state;
        state.kind = track.kind;
        state.trackName = runtimeClip.actorName != nullptr ?
            runtimeClip.actorName->c_str() : track.name.c_str();
        state.resourceName = clip.resourceName.c_str();
        state.position = clip.hasOffsetPosition ? clip.offsetPosition : SceneVec3();
        state.rotation = clip.hasOffsetRotation ? clip.offsetRotation : SceneVec3();
        state.targetId = clip.targetId;
        state.targetName = clip.targetName == "(null)" ?
                           NULL : clip.targetName.c_str();
        state.visible = true;

        state.noStageCheck = track.kind == SceneTrackKind::PlayerTask &&
                             clip.noStageCheckSwitch != 0;
        state.clipElapsed = time - clip.startTime;
        state.clipDuration = clip.endTime - clip.startTime;
        state.sampleTime = time;

        state.applyPlacement =
            firstApply &&
            (track.kind != SceneTrackKind::PlayerTask ||
             (clip.hasOffsetSwitch && clip.offsetSwitch != 0));
        bindings_.applyActor(bindings_.userData, state);
        return;
    }

    if (track.kind == SceneTrackKind::Motion &&
        bindings_.applyMotion != nullptr)
    {
        SceneActorState state;
        state.kind = track.kind;
        state.trackName = track.name.c_str();
        state.motionName = clip.resourceName.c_str();
        state.resourceName = clip.resourceName.c_str();
        state.position = clip.hasOffsetPosition ? clip.offsetPosition : SceneVec3();
        state.rotation = clip.hasOffsetRotation ? clip.offsetRotation : SceneVec3();
        bindings_.applyMotion(bindings_.userData, state);
        return;
    }

    if (track.kind == SceneTrackKind::Light && bindings_.applyLight != nullptr)
    {
        SceneLightState state;
        state.trackName = track.name.c_str();
        state.resourceName = clip.resourceName.c_str();
        state.color = clip.hasColor ? clip.color : SceneVec3(1.0f, 1.0f, 1.0f);
        state.position = clip.hasPosition ? clip.position : SceneVec3();
        state.rotationStart = clip.hasRotation ? clip.rotationStart : 0.0f;
        state.rotationEnd = clip.hasRotation ? clip.rotationEnd : state.rotationStart;
        state.rotationStart += clip.hasOffsetRotation ? clip.offsetRotation.x : 0.0f;
        state.rotationEnd += clip.hasOffsetRotation ? clip.offsetRotation.x : 0.0f;
        state.radius = clip.radius;
        state.angle = clip.angle;
        state.visible = true;
        state.index = lightTrackIndex(runtimeClip.track);
        bindings_.applyLight(bindings_.userData, state);
        return;
    }

    if (track.kind == SceneTrackKind::Morph &&
        bindings_.applyActor != nullptr)
    {
        SceneActorState state;
        state.kind = track.kind;
        state.trackName = runtimeClip.actorName != nullptr ?
            runtimeClip.actorName->c_str() : track.name.c_str();
        state.morphStartTarget = clip.hasMorph ? clip.morph[1] : 0;
        state.morphEndTarget = clip.hasMorph ? clip.morph[2] : 0;
        state.morphAmount = amount;
        state.clipElapsed = time - clip.startTime;
        state.clipDuration = clip.endTime - clip.startTime;
        state.sampleTime = time;
        bindings_.applyActor(bindings_.userData, state);
        return;
    }

    if(track.kind == SceneTrackKind::Voice &&
       bindings_.audio != nullptr)
    {
        SceneAudioEvent event;
        event.kind = SceneAudioKind::Voice;
        event.active = true;
        event.continuous = true;
        event.trackName = runtimeClip.actorName != nullptr ?
            runtimeClip.actorName->c_str() : track.name.c_str();
        event.resourceName = clip.resourceName.c_str();
        event.values[0] = clip.voice[0];
        event.values[1] = clip.voice[1];
        event.lip = clip.lip;
        event.lipRate = clip.lipDuration;
        event.clipElapsed = time - clip.startTime;
        bindings_.audio(bindings_.userData, event);
        return;
    }

    if(track.kind == SceneTrackKind::Vibration &&
       bindings_.audio != nullptr)
    {
        SceneAudioEvent event;
        event.kind = SceneAudioKind::Vibration;
        event.active = true;
        event.continuous = true;
        event.vibrationDirection = clip.vibrationDirection;
        event.vibrationAmplify = clip.vibrationAmplify;
        event.vibrationFrequency = clip.vibrationFrequency;
        event.vibrationAcceleration = clip.vibrationAcceleration;
        event.vibrationDeceleration = clip.vibrationDeceleration;
        event.clipElapsed = time - clip.startTime;
        bindings_.audio(bindings_.userData, event);
        return;
    }

    if(track.kind == SceneTrackKind::Volume &&
       bindings_.audio != nullptr)
    {
        SceneAudioEvent event;
        event.kind = runtimeClip.parentKind == SceneTrackKind::Music ?
            SceneAudioKind::Music : SceneAudioKind::Voice;
        event.active = true;
        event.continuous = true;
        event.volumeControl = true;
        event.trackName = track.name.c_str();
        event.volume =
            (static_cast<float>(clip.volumeStart) +
             static_cast<float>(clip.volumeEnd - clip.volumeStart) * amount) /
            100.0f;
        bindings_.audio(bindings_.userData, event);
        return;
    }

    if (track.kind == SceneTrackKind::Effect &&
        bindings_.effect != nullptr)
    {
        SceneEffectState state;
        state.active = true;
        state.trackName = track.name.c_str();
        state.blur = clip.blur;
        state.timeScale = clip.timeScale;
        bindings_.effect(bindings_.userData, state);
        return;
    }
}

void SceneRuntime::restore(bool skipped, bool failed)
{
    if (restored_)
        return;
    restored_ = true;
    releaseResources();
    if (bindings_.restore != nullptr)
        bindings_.restore(bindings_.userData, skipped, failed);
}
