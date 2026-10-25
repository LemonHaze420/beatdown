#include "stage/assets.h"
#include "xaf_runtime.h"
#include <rw.h>
#include <string.h>

StageObjectAssets::StageObjectAssets()
    : dictionary(NULL), attempted(false),
      stageArchivePath("raw/XAF/FSTAGE.XAF"), dataArchivePath("raw/XAF/DATA.XAF")
{
    memset(templates, 0, sizeof(templates));
}

StageObjectAssets::~StageObjectAssets()
{
    release();
}

void StageObjectAssets::release()
{
    items.release();
    for(rw::Clump*& model : templates)
    {
        if(model != NULL)
            model->destroy();
        model = NULL;
    }
    if(dictionary != NULL)
    {
        if(rw::TexDictionary::getCurrent() == dictionary)
            rw::TexDictionary::setCurrent(NULL);
        dictionary->destroy();
        dictionary = NULL;
    }
    library.clear();
    attempted = false;
}

bool StageObjectAssets::load(const char* stageArchive, const char* dataArchive)
{
    if(stageArchive == NULL || dataArchive == NULL)
        return false;
    if(stageArchivePath != stageArchive || dataArchivePath != dataArchive)
    {
        release();
        stageArchivePath = stageArchive;
        dataArchivePath = dataArchive;
    }
    return load();
}

bool StageObjectAssets::load()
{
    if(attempted)
        return library.loaded() && dictionary != NULL;
    attempted = true;
    std::vector<uint8_t> data;
    if(!readXafEntry(stageArchivePath.c_str(), "WEAPON.OL", data) ||
       !library.loadMemory(data.data(), data.size()))
        return false;
    const WeaponAssetChunk& texture = library.textureChunk();
    const uint8_t* bytes = library.bytes(texture.offset, texture.end - texture.offset);
    if(bytes == NULL)
        return false;
    rw::StreamMemory stream;
    stream.open(const_cast<uint8_t*>(bytes),
                static_cast<uint32_t>(texture.end - texture.offset));
    if(!rw::findChunk(&stream, rw::ID_TEXDICTIONARY, NULL, NULL))
        return false;
    dictionary = rw::TexDictionary::streamRead(&stream);
    return dictionary != NULL;
}

rw::Clump* StageObjectAssets::readStandaloneAtomic(rw::Stream* stream)
{

    uint32_t size = 0;
    if(!rw::findChunk(stream, rw::ID_STRUCT, &size, NULL) || size != 16)
        return NULL;
    int32_t header[4];
    stream->read32(header, sizeof(header));
    if(!rw::findChunk(stream, rw::ID_GEOMETRY, NULL, NULL))
        return NULL;
    rw::Geometry* geometry = rw::Geometry::streamRead(stream);
    if(geometry == NULL)
        return NULL;
    rw::Atomic* atomic = rw::Atomic::create();
    if(atomic == NULL)
    {
        geometry->destroy();
        return NULL;
    }
    atomic->setGeometry(geometry, 0);
    geometry->destroy();
    atomic->setFlags(static_cast<uint32_t>(header[2]));
    if(!rw::Atomic::s_plglist.streamRead(stream, atomic))
    {
        atomic->destroy();
        return NULL;
    }
    rw::Clump* clump = rw::Clump::create();
    rw::Frame* frame = rw::Frame::create();
    if(clump == NULL || frame == NULL)
    {
        if(clump != NULL)
            clump->destroy();
        if(frame != NULL)
            frame->destroy();
        atomic->destroy();
        return NULL;
    }
    clump->setFrame(frame);
    atomic->setFrame(frame);
    clump->addAtomic(atomic);
    return clump;
}

rw::Clump* StageObjectAssets::resolve(int selector)
{
    if(!load() || selector < 0 || selector >= static_cast<int>(STAGE_OBJECT_ASSET_CAPACITY))
        return NULL;
    if(templates[selector] != NULL)
        return templates[selector];
    const WeaponObjectRecord* object = library.object(selector);
    if(object == NULL || !object->hasPayload || (object->objectType != WEAPON_OBJECT_CLUMP &&
        object->objectType != WEAPON_OBJECT_ATOMIC))
        return NULL;
    const size_t length = object->payload.end - object->payload.offset;
    const uint8_t* bytes = library.bytes(object->payload.offset, length);
    if(bytes == NULL)
        return NULL;
    rw::TexDictionary* previous = rw::TexDictionary::getCurrent();
    rw::TexDictionary::setCurrent(dictionary);
    rw::StreamMemory stream;
    stream.open(const_cast<uint8_t*>(bytes), static_cast<uint32_t>(length));
    if(object->objectType == WEAPON_OBJECT_CLUMP)
    {
        if(rw::findChunk(&stream, rw::ID_CLUMP, NULL, NULL))
            templates[selector] = rw::Clump::streamRead(&stream);
    }
    else if(rw::findChunk(&stream, rw::ID_ATOMIC, NULL, NULL))
        templates[selector] = readStandaloneAtomic(&stream);
    rw::TexDictionary::setCurrent(previous);
    return templates[selector];
}

rw::Clump* StageObjectAssets::resolveModel(int8_t kind, int8_t selector, void* userData)
{
    if(userData == NULL)
        return NULL;
    StageObjectAssets& assets = *static_cast<StageObjectAssets*>(userData);
    if(kind == 2)
        return assets.items.load(assets.dataArchivePath.c_str()) ?
            assets.items.resolve(selector) : NULL;
    if(kind == 3 || kind == 4)
        return assets.resolve(selector);
    return NULL;
}
