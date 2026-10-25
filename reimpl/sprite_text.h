#ifndef SPRITE_TEXT_H
#define SPRITE_TEXT_H

#include <stdint.h>

const int SPRITE_TEXT_CAPACITY = 67;
const int SPRITE_TEXT_INVALID = 0;

const int SPRITE_TEXT_RECORD_BYTES = 0x1a0;

const float SPRITE_TEXT_SHADOW_X = 2.0f;
const float SPRITE_TEXT_SHADOW_Y = 1.0f;

const int SPRITE_TEXT_MAX_LENGTH = 128;

struct SpriteTextColour
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
};

struct SpriteTextSlot
{
    bool used;
    bool visible;
    char text[SPRITE_TEXT_MAX_LENGTH];
    float x;
    float y;
    float scale;
    SpriteTextColour colour;
    bool shadow;
};

struct SpriteTextPool
{

    SpriteTextSlot slots[SPRITE_TEXT_CAPACITY + 1];
};

void spriteTextPoolReset(SpriteTextPool& pool);

int spriteTextCreate(SpriteTextPool& pool);
void spriteTextDestroy(SpriteTextPool& pool, int id);
bool spriteTextValidId(int id);

bool spriteTextSetString(SpriteTextPool& pool, int id, const char* text);
bool spriteTextSetPosition(SpriteTextPool& pool, int id, float x, float y);
bool spriteTextSetScale(SpriteTextPool& pool, int id, float scale);
bool spriteTextSetColour(SpriteTextPool& pool, int id,
                         const SpriteTextColour& colour);
bool spriteTextSetShadow(SpriteTextPool& pool, int id, bool shadow);

struct SpriteTextPass
{
    float x;
    float y;
    float scale;
    SpriteTextColour colour;
    const char* text;
    bool isShadow;
};

typedef void (*SpriteTextEmit)(const SpriteTextPass& pass, void* userData);
int spriteTextShow(const SpriteTextPool& pool, int id, SpriteTextEmit emit,
                   void* userData);

#endif
