#include "scene_captions.h"

#include <string.h>

#include "game_data.h"
#include "../formats/XAF/xaf_runtime.h"

namespace
{

const char CAPTION_DIRECTORY[] = "SceneEditor\\data\\caption\\";

bool languageFromCaptionPath(const std::string& path, SceneLanguage& language)
{
    static const struct
    {
        const char* suffix;
        SceneLanguage language;
    } SUFFIXES[] = {
        { "_USA", SceneLanguage::English },
        { "_FRA", SceneLanguage::French },
        { "_GER", SceneLanguage::German },
        { "_ESP", SceneLanguage::Spanish },
        { "_ITA", SceneLanguage::Italian }
    };

    const size_t dot = path.rfind(".txt");
    if(dot == std::string::npos || dot < 4)
        return false;
    const std::string tail = path.substr(dot - 4, 4);
    for(size_t i = 0; i < sizeof(SUFFIXES) / sizeof(SUFFIXES[0]); ++i)
    {
        if(tail == SUFFIXES[i].suffix)
        {
            language = SUFFIXES[i].language;
            return true;
        }
    }
    language = SceneLanguage::Japanese;
    return true;
}

size_t findNextMarker(const std::string& body, size_t from)
{
    const size_t at = body.find("----", from);
    return at == std::string::npos ? body.size() : at;
}

std::string foldCaption(const std::string& raw)
{

    size_t end = raw.size();
    while(end > 0 && (raw[end - 1] == '\r' || raw[end - 1] == '\n'))
        --end;
    size_t begin = 0;
    while(begin < end && (raw[begin] == '\r' || raw[begin] == '\n'))
        ++begin;
    if(begin >= end)
        return std::string();
    const std::string text = normalizeDisplayText(
        reinterpret_cast<const uint8_t*>(raw.data()) + begin, end - begin);

    std::string folded;
    folded.reserve(text.size());
    for(size_t i = 0; i < text.size(); ++i)
    {
        if(text[i] == '\r')
            continue;
        folded.push_back(text[i]);
    }
    return folded;
}

}

bool
SceneCaptionPool::empty(void) const
{
    for(size_t i = 0; i < byLanguage.size(); ++i)
    {
        if(!byLanguage[i].empty())
            return false;
    }
    return true;
}

const char*
SceneCaptionPool::text(SceneLanguage language, int32_t index) const
{
    const size_t slot = static_cast<size_t>(language);
    if(slot >= byLanguage.size() || index < 0)
        return NULL;
    const std::vector<std::string>& lines = byLanguage[slot];
    if(static_cast<size_t>(index) >= lines.size())
        return NULL;
    const std::string& line = lines[static_cast<size_t>(index)];
    return line.empty() ? NULL : line.c_str();
}

bool
parseSceneCaptions(const uint8_t* data, size_t length, SceneCaptionPool& pool)
{
    pool.byLanguage.assign(6, std::vector<std::string>());
    if(data == NULL || length == 0)
        return false;

    const std::string blob(reinterpret_cast<const char*>(data), length);
    const size_t directoryLength = strlen(CAPTION_DIRECTORY);
    size_t search = 0;
    bool any = false;
    while(true)
    {
        const size_t at = blob.find(CAPTION_DIRECTORY, search);
        if(at == std::string::npos)
            break;
        const size_t pathStart = at + directoryLength;
        const size_t pathEnd = blob.find('\0', pathStart);
        if(pathEnd == std::string::npos)
            break;
        search = pathEnd + 1;

        SceneLanguage language = SceneLanguage::Japanese;
        if(!languageFromCaptionPath(blob.substr(pathStart, pathEnd - pathStart),
                                    language))
            continue;

        std::vector<std::string>& lines =
            pool.byLanguage[static_cast<size_t>(language)];

        size_t cursor = pathEnd + 1;
        while(true)
        {
            const size_t marker = blob.find("----", cursor);
            if(marker == std::string::npos)
                break;
            size_t digits = marker + 4;
            size_t count = 0;
            while(digits + count < blob.size() &&
                  blob[digits + count] >= '0' && blob[digits + count] <= '9')
                ++count;
            if(count == 0)
                break;
            const long block = strtol(blob.substr(digits, count).c_str(),
                                      NULL, 10);
            size_t bodyStart = digits + count;
            const size_t bodyEnd = findNextMarker(blob, bodyStart);
            if(block >= 0)
            {
                if(lines.size() <= static_cast<size_t>(block))
                    lines.resize(static_cast<size_t>(block) + 1);
                lines[static_cast<size_t>(block)] =
                    foldCaption(blob.substr(bodyStart, bodyEnd - bodyStart));
                any = true;
            }
            cursor = bodyEnd;
        }
    }
    return any;
}

bool
loadSceneCaptions(const char* archivePath, const char* sceneEntry,
                  SceneCaptionPool& pool)
{
    pool.byLanguage.assign(6, std::vector<std::string>());
    if(archivePath == NULL || sceneEntry == NULL)
        return false;

    std::string companion(sceneEntry);
    const size_t dot = companion.rfind('.');
    if(dot != std::string::npos)
        companion.erase(dot);
    companion += ".RWS";

    std::vector<uint8_t> data;
    if(!readXafEntry(archivePath, companion.c_str(), data))
        return false;
    return parseSceneCaptions(data.data(), data.size(), pool);
}
