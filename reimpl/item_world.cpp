#include "item_world.h"
#include "xaf_runtime.h"
#include <rw.h>
#include <string.h>

ItemWorldAssets::ItemWorldAssets()
{
    memset(models, 0, sizeof(models));
}

ItemWorldAssets::~ItemWorldAssets()
{
    release();
}

bool ItemWorldAssets::load(const char* archivePath)
{
    if(models[ITEM_WORLD_MODEL_COUNT - 1] != NULL)
        return true;
    release();
    std::vector<uint8_t> data;
    if(!readXafEntry(archivePath, "..\\..\\D\\ITEM\\ITEM.BIN", data))
        return false;
    rw::StreamMemory stream;
    stream.open(data.data(), static_cast<uint32_t>(data.size()));
    for(int i = 0; i < ITEM_WORLD_MODEL_COUNT; ++i)
    {
        if(!rw::findChunk(&stream, rw::ID_CLUMP, NULL, NULL) ||
           (models[i] = rw::Clump::streamRead(&stream)) == NULL)
        {
            release();
            return false;
        }
    }
    return true;
}

void ItemWorldAssets::release()
{
    for(int i = 0; i < ITEM_WORLD_MODEL_COUNT; ++i)
    {
        if(models[i] != NULL)
            models[i]->destroy();
        models[i] = NULL;
    }
}

rw::Clump* ItemWorldAssets::resolve(int selector) const
{
    if(selector < 0 || selector >= ITEM_WORLD_SELECTOR_COUNT)
        return NULL;
    const int model = ITEM_WORLD_SELECTOR_MODELS[selector];
    return model < 0 ? NULL : models[model];
}
