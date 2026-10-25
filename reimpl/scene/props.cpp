#include "scene/props.h"
#include "formats/common.h"
#include "scene/runtime.h"

#include <rw.h>

#include <string.h>

#include "formats/XAF/xaf_runtime.h"

namespace
{

const uint32_t CHUNK_NAME = 0x79032101;

rw::Clump* readClump(const uint8_t* chunk, uint32_t length)
{
    rw::StreamMemory stream;
    stream.open(const_cast<uint8_t*>(chunk), length);
    rw::Clump* clump = NULL;
    if(rw::findChunk(&stream, rw::ID_CLUMP, NULL, NULL))
        clump = rw::Clump::streamRead(&stream);
    stream.close();
    return clump;
}

rw::TexDictionary* readTextureDictionary(const uint8_t* chunk, uint32_t length)
{
    rw::StreamMemory stream;
    stream.open(const_cast<uint8_t*>(chunk), length);
    rw::TexDictionary* dictionary = NULL;
    if(rw::findChunk(&stream, rw::ID_TEXDICTIONARY, NULL, NULL))
        dictionary = rw::TexDictionary::streamRead(&stream);
    stream.close();
    if(dictionary == NULL)
        return NULL;

    FORLIST(textureLink, dictionary->textures)
    {
        rw::Texture* texture = rw::Texture::fromDict(textureLink);
        if(texture == NULL || texture->raster == NULL)
            continue;
        rw::Raster* converted =
            rw::Raster::convertTexToCurrentPlatform(texture->raster);
        if(converted != NULL)
            texture->raster = converted;
    }
    return dictionary;
}

}

SceneProp*
SceneProps::find(const char* authoringPath)
{
    if(authoringPath == NULL)
        return NULL;
    for(size_t i = 0; i < props.size(); ++i)
    {
        if(props[i].path == authoringPath)
            return &props[i];
    }
    return NULL;
}

bool
loadSceneProps(const char* archivePath, const char* sceneEntry,
               rw::World* world, SceneProps& props)
{
    freeSceneProps(props);
    if(archivePath == NULL || sceneEntry == NULL || world == NULL)
        return false;

    std::string companion(sceneEntry);
    const size_t dot = companion.rfind('.');
    if(dot != std::string::npos)
        companion.erase(dot);
    companion += ".RWS";

    std::vector<uint8_t> data;
    if(!readXafEntry(archivePath, companion.c_str(), data))
        return false;

    props.world = world;
    rw::TexDictionary* previous = rw::TexDictionary::getCurrent();

    std::string authored;
    size_t offset = 0;
    while(offset + 12 <= data.size())
    {
        const uint8_t* header = data.data() + offset;
        const uint32_t type = read32(header);
        const uint32_t length = read32(header + 4);
        if(length == 0 ||
           static_cast<uint64_t>(offset) + 12 + length > data.size())
            break;
        const uint8_t* body = header + 12;

        if(type == CHUNK_NAME)
        {
            const uint8_t* end = static_cast<const uint8_t*>(
                memchr(body, 0, length));
            authored.assign(reinterpret_cast<const char*>(body),
                            end != NULL ? static_cast<size_t>(end - body)
                                        : length);
        }
        else if(type == rw::ID_TEXDICTIONARY &&
                props.textureDictionary == NULL)
        {

            props.textureDictionary = readTextureDictionary(header, 12 + length);
            if(props.textureDictionary != NULL)
                rw::TexDictionary::setCurrent(props.textureDictionary);
        }
        else if(type == rw::ID_CLUMP)
        {
            rw::Clump* clump = readClump(header, 12 + length);
            if(clump != NULL)
            {
                SceneProp prop;
                prop.path = authored;
                prop.clump = clump;
                prop.world = world;
                prop.frame = clump->getFrame();
                if(prop.frame == NULL)
                {
                    prop.frame = rw::Frame::create();
                    if(prop.frame != NULL)
                        clump->setFrame(prop.frame);
                }

                props.props.push_back(prop);
            }
        }
        offset += 12 + length;
    }

    rw::TexDictionary::setCurrent(previous);
    return !props.props.empty();
}

void
freeSceneProps(SceneProps& props)
{
    for(size_t i = 0; i < props.props.size(); ++i)
    {
        SceneProp& prop = props.props[i];
        if(prop.clump == NULL)
            continue;
        if(prop.inWorld && prop.world != NULL)
        {
            prop.world->removeClump(prop.clump);
            prop.inWorld = false;
        }
        prop.clump->destroy();
        prop.clump = NULL;
        prop.frame = NULL;
    }
    props.props.clear();
    if(props.textureDictionary != NULL)
    {
        if(rw::TexDictionary::getCurrent() == props.textureDictionary)
            rw::TexDictionary::setCurrent(NULL);
        props.textureDictionary->destroy();
        props.textureDictionary = NULL;
    }
    props.world = NULL;
}

void
setScenePropTransform(SceneProp& prop, bool visible,
                      float x, float y, float z,
                      float pitchDegrees, float yawDegrees,
                      float rollDegrees)
{
    if(prop.clump == NULL || prop.frame == NULL)
        return;

    if(!visible)
    {
        if(prop.inWorld && prop.world != NULL)
        {
            prop.world->removeClump(prop.clump);
            prop.inWorld = false;
        }
        return;
    }

    rw::V3d axisX = { 1.0f, 0.0f, 0.0f };
    rw::V3d axisY = { 0.0f, 1.0f, 0.0f };
    rw::V3d axisZ = { 0.0f, 0.0f, 1.0f };
    rw::Matrix& matrix = prop.frame->matrix;
    matrix.setIdentity();

    matrix.rotate(&axisY, yawDegrees, rw::COMBINEREPLACE);
    matrix.rotate(&axisX, pitchDegrees, rw::COMBINEPOSTCONCAT);
    matrix.rotate(&axisZ, rollDegrees, rw::COMBINEPOSTCONCAT);
    matrix.pos.x = x;
    matrix.pos.y = y;
    matrix.pos.z = z;
    matrix.optimize();
    prop.frame->updateObjects();

    if(!prop.inWorld && prop.world != NULL)
    {
        prop.world->addClump(prop.clump);
        prop.inWorld = true;
    }
}

void
setScenePropBoneTransform(SceneProp& prop, bool visible,
                          const rw::Matrix& boneMatrix,
                          float x, float y, float z,
                          float rotXDeg, float rotYDeg, float rotZDeg)
{
    if(prop.clump == NULL || prop.frame == NULL)
        return;

    if(!visible)
    {
        if(prop.inWorld && prop.world != NULL)
        {
            prop.world->removeClump(prop.clump);
            prop.inWorld = false;
        }
        return;
    }

    SceneMatrix bone;
    bone.position = SceneVec3(boneMatrix.pos.x, boneMatrix.pos.y,
                              boneMatrix.pos.z);
    bone.right = SceneVec3(boneMatrix.right.x, boneMatrix.right.y,
                           boneMatrix.right.z);
    bone.up = SceneVec3(boneMatrix.up.x, boneMatrix.up.y, boneMatrix.up.z);
    bone.at = SceneVec3(boneMatrix.at.x, boneMatrix.at.y, boneMatrix.at.z);
    const SceneMatrix world = composeSceneModelBoneTransform(
        bone, SceneVec3(x, y, z), SceneVec3(rotXDeg, rotYDeg, rotZDeg));

    rw::Matrix& matrix = prop.frame->matrix;
    matrix.setIdentity();
    matrix.right.x = world.right.x;
    matrix.right.y = world.right.y;
    matrix.right.z = world.right.z;
    matrix.up.x = world.up.x;
    matrix.up.y = world.up.y;
    matrix.up.z = world.up.z;
    matrix.at.x = world.at.x;
    matrix.at.y = world.at.y;
    matrix.at.z = world.at.z;
    matrix.pos.x = world.position.x;
    matrix.pos.y = world.position.y;
    matrix.pos.z = world.position.z;
    matrix.optimize();
    prop.frame->updateObjects();

    if(!prop.inWorld && prop.world != NULL)
    {
        prop.world->addClump(prop.clump);
        prop.inWorld = true;
    }
}
