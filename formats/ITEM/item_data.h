#ifndef ITEM_DATA_H
#define ITEM_DATA_H
#include <stdint.h>
const int ITEM_MAN_RECORD_COUNT = 485;
const int ITEM_WOMAN_RECORD_COUNT = 459;
const int ITEM_CONSUMABLE_COUNT = 16;
const int ITEM_VARIANT_LABEL_COUNT = 10;

const uint32_t ITEM_PACKED_ID_MASK = 0x00000fffu;
const uint32_t ITEM_PACKED_VARIANT_MASK = 0x0000f000u;
const uint32_t ITEM_PACKED_QUANTITY_MASK = 0x03ff0000u;

enum ItemEffectFlags
{
    ITEM_EFFECT_NONE = 0,
    ITEM_EFFECT_RESTORE_HEALTH = 1 << 0,
    ITEM_EFFECT_INCREASE_MAX_HEALTH = 1 << 1,
    ITEM_EFFECT_RESTORE_PRIDE = 1 << 2,
    ITEM_EFFECT_SET_ALL_RELATIONSHIPS = 1 << 3,
    ITEM_EFFECT_MAY_SLOW_PLAYER = 1 << 4,
    ITEM_EFFECT_CONDITIONAL_HEALTH = 1 << 5
};
enum ItemSlowRule
{
    ITEM_SLOW_NEVER = 0,

    ITEM_SLOW_RANDOM15_ABOVE_THRESHOLD = 1,

    ITEM_SLOW_UNLESS_RANDOM16_HEALS = 2
};
struct ItemUseEffect
{
    uint16_t flags;
    uint16_t healthAmount;
    uint16_t maxHealthAmount;
    uint16_t slowThreshold;
    uint16_t healthChanceThreshold;
    uint8_t effectCode;
    uint8_t slowRule;
};

struct ItemTableRecord
{
    uint16_t itemId;
    uint16_t nameTextId;
    uint16_t helpTextId;
    int32_t priceOrVariantCount;
    int32_t unlockThreshold;
    int32_t field_0x14;
    int32_t field_0x18;
    int32_t field_0x1c;
    int32_t field_0x20;
    int32_t field_0x24;
    int32_t variantSelector;
    int32_t menuCategory;
    int32_t categorySlot;
    int32_t modelPart;
    int32_t variantSlot;
    int32_t variantBase;
    int32_t variantCount;
    int32_t field_0x44;
    uint32_t variantLabelIndices[ITEM_VARIANT_LABEL_COUNT];
    ItemUseEffect useEffect;
    uint8_t consumable;
    uint8_t female;
    uint8_t reserved[2];
};
extern const ItemTableRecord ITEM_MAN_RECORDS[ITEM_MAN_RECORD_COUNT];
extern const ItemTableRecord ITEM_WOMAN_RECORDS[ITEM_WOMAN_RECORD_COUNT];
const ItemTableRecord* findItemTableRecord(uint16_t itemId, bool female);
#endif
