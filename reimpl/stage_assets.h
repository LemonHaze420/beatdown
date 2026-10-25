#ifndef STAGE_ASSETS_H
#define STAGE_ASSETS_H

#include "item_world.h"
#include "weapon.h"
#include <string>

namespace rw
{
struct TexDictionary;
struct Clump;
struct Stream;
}

const size_t STAGE_OBJECT_ASSET_CAPACITY = 64;

struct StageObjectAssets
{
    ItemWorldAssets items;
    WeaponObjectLibrary library;
    rw::TexDictionary* dictionary;
    rw::Clump* templates[STAGE_OBJECT_ASSET_CAPACITY];
    bool attempted;

    StageObjectAssets();
    ~StageObjectAssets();
    void release();
    bool load();
    bool load(const char* stageArchive, const char* dataArchive);
    rw::Clump* resolve(int selector);
    static rw::Clump* resolveModel(int8_t kind, int8_t selector, void* userData);

private:
    static rw::Clump* readStandaloneAtomic(rw::Stream* stream);
    std::string stageArchivePath;
    std::string dataArchivePath;
};

#endif
