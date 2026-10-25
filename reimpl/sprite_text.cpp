#include "sprite_text.h"

#include <string.h>

namespace
{

SpriteTextSlot* slotFor(SpriteTextPool& pool, int id)
{
    if(!spriteTextValidId(id))
        return NULL;
    SpriteTextSlot* slot = &pool.slots[id];
    return slot->used ? slot : NULL;
}

}

bool
spriteTextValidId(int id)
{

    return id > SPRITE_TEXT_INVALID && id <= SPRITE_TEXT_CAPACITY;
}

void
spriteTextPoolReset(SpriteTextPool& pool)
{
    memset(&pool, 0, sizeof(pool));
    for(int i = 0; i <= SPRITE_TEXT_CAPACITY; ++i)
    {
        pool.slots[i].scale = 1.0f;
        pool.slots[i].colour.r = 0xff;
        pool.slots[i].colour.g = 0xff;
        pool.slots[i].colour.b = 0xff;
        pool.slots[i].colour.a = 0xff;
    }
}

int
spriteTextCreate(SpriteTextPool& pool)
{
    for(int id = 1; id <= SPRITE_TEXT_CAPACITY; ++id)
    {
        if(pool.slots[id].used)
            continue;
        SpriteTextSlot& slot = pool.slots[id];
        slot.used = true;
        slot.visible = true;
        slot.text[0] = 0;
        slot.x = 0.0f;
        slot.y = 0.0f;
        slot.scale = 1.0f;
        slot.colour.r = 0xff;
        slot.colour.g = 0xff;
        slot.colour.b = 0xff;
        slot.colour.a = 0xff;
        slot.shadow = false;
        return id;
    }
    return SPRITE_TEXT_INVALID;
}

void
spriteTextDestroy(SpriteTextPool& pool, int id)
{
    if(!spriteTextValidId(id))
        return;
    pool.slots[id].used = false;
    pool.slots[id].text[0] = 0;
}

bool
spriteTextSetString(SpriteTextPool& pool, int id, const char* text)
{
    SpriteTextSlot* slot = slotFor(pool, id);
    if(slot == NULL || text == NULL)
        return false;
    const size_t length = strlen(text);

    if(length >= (size_t)SPRITE_TEXT_MAX_LENGTH)
        return false;
    memcpy(slot->text, text, length + 1);
    return true;
}

bool
spriteTextSetPosition(SpriteTextPool& pool, int id, float x, float y)
{
    SpriteTextSlot* slot = slotFor(pool, id);
    if(slot == NULL)
        return false;
    slot->x = x;
    slot->y = y;
    return true;
}

bool
spriteTextSetScale(SpriteTextPool& pool, int id, float scale)
{
    SpriteTextSlot* slot = slotFor(pool, id);
    if(slot == NULL)
        return false;
    slot->scale = scale;
    return true;
}

bool
spriteTextSetColour(SpriteTextPool& pool, int id,
                    const SpriteTextColour& colour)
{
    SpriteTextSlot* slot = slotFor(pool, id);
    if(slot == NULL)
        return false;
    slot->colour = colour;
    return true;
}

bool
spriteTextSetShadow(SpriteTextPool& pool, int id, bool shadow)
{
    SpriteTextSlot* slot = slotFor(pool, id);
    if(slot == NULL)
        return false;
    slot->shadow = shadow;
    return true;
}

int
spriteTextShow(const SpriteTextPool& pool, int id, SpriteTextEmit emit,
               void* userData)
{
    if(!spriteTextValidId(id) || emit == NULL)
        return 0;
    const SpriteTextSlot& slot = pool.slots[id];

    if(!slot.used || !slot.visible || slot.text[0] == 0)
        return 0;

    int passes = 0;
    if(slot.shadow)
    {
        SpriteTextPass shadow;
        shadow.x = slot.x + SPRITE_TEXT_SHADOW_X;
        shadow.y = slot.y + SPRITE_TEXT_SHADOW_Y;
        shadow.scale = slot.scale;
        shadow.colour = slot.colour;
        shadow.text = slot.text;
        shadow.isShadow = true;
        emit(shadow, userData);
        ++passes;
    }

    SpriteTextPass pass;
    pass.x = slot.x;
    pass.y = slot.y;
    pass.scale = slot.scale;
    pass.colour = slot.colour;
    pass.text = slot.text;
    pass.isShadow = false;
    emit(pass, userData);
    ++passes;
    return passes;
}
