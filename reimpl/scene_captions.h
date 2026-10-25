#ifndef SCENE_CAPTIONS_H
#define SCENE_CAPTIONS_H

#include <stdint.h>

#include <string>
#include <vector>

#include "scene_runtime.h"

struct SceneCaptionPool
{

    std::vector<std::vector<std::string> > byLanguage;

    SceneCaptionPool()
        : byLanguage(6)
    {
    }

    bool empty(void) const;

    const char* text(SceneLanguage language, int32_t index) const;
};

bool parseSceneCaptions(const uint8_t* data, size_t length,
                        SceneCaptionPool& pool);

bool loadSceneCaptions(const char* archivePath, const char* sceneEntry,
                       SceneCaptionPool& pool);

#endif
