#ifndef ITEM_WORLD_H
#define ITEM_WORLD_H

namespace rw { struct Clump; }

const int ITEM_WORLD_MODEL_COUNT = 8;
const int ITEM_WORLD_SELECTOR_COUNT = 10;

const int ITEM_WORLD_SELECTOR_MODELS[ITEM_WORLD_SELECTOR_COUNT] =
    { -1, 5, 5, 6, 6, 6, 4, 1, 3, 7 };

class ItemWorldAssets
{
public:
    ItemWorldAssets();
    ~ItemWorldAssets();
    bool load(const char* archivePath);
    void release();
    rw::Clump* resolve(int selector) const;

private:
    rw::Clump* models[ITEM_WORLD_MODEL_COUNT];
};

#endif
