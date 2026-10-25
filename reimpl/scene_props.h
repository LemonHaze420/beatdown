#ifndef SCENE_PROPS_H
#define SCENE_PROPS_H

#include <stdint.h>

#include <string>
#include <vector>

namespace rw
{
struct Clump;
struct Matrix;
struct Frame;
struct TexDictionary;
struct World;
}

struct SceneProp
{

    std::string path;
    rw::Clump* clump;
    rw::Frame* frame;

    rw::World* world;
    bool inWorld;

    SceneProp()
        : path(), clump(NULL), frame(NULL), world(NULL), inWorld(false)
    {
    }
};

struct SceneProps
{
    std::vector<SceneProp> props;
    rw::TexDictionary* textureDictionary;
    rw::World* world;

    SceneProps()
        : props(), textureDictionary(NULL), world(NULL)
    {
    }

    SceneProp* find(const char* authoringPath);
};

bool loadSceneProps(const char* archivePath, const char* sceneEntry,
                    rw::World* world, SceneProps& props);

void freeSceneProps(SceneProps& props);

void setScenePropTransform(SceneProp& prop, bool visible,
                           float x, float y, float z,
                           float pitchDegrees, float yawDegrees,
                           float rollDegrees);

void setScenePropBoneTransform(SceneProp& prop, bool visible,
                               const rw::Matrix& boneMatrix,
                               float x, float y, float z,
                               float rotXDeg, float rotYDeg, float rotZDeg);
#endif
