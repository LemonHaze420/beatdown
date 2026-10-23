#ifndef CHARACTER_RECORDS_H
#define CHARACTER_RECORDS_H
#include <stddef.h>
#include <stdint.h>

const int CHARACTER_DB_RECORD_COUNT = 602;
struct CharacterDefaultMoveSlot
{
    int32_t type;
    int16_t id;
    uint8_t field_0x06[2];
};
static_assert(sizeof(CharacterDefaultMoveSlot) == 8,
              "default move-slot ABI");
struct CharacterRecord
{
    uint8_t category;
    int16_t field_0x02;

    int16_t resourceId;
    int16_t combatStyleId;
    uint8_t aiProfileId;
    uint8_t groupId;

    uint8_t encounterMode;
    uint8_t initialRelationshipState;

    uint8_t profileCategory;
    int8_t level;
    int16_t maxHealth;
    int16_t initialPrideGauge;
    uint8_t attackPowerClass;
    int16_t initialDefeatCount;
    int32_t cash;
    uint32_t flags;
    CharacterDefaultMoveSlot moveSlots[2];
    int16_t profileIndex;
    uint16_t profileRevealFlags;

    int8_t weaponDropModifier;

    int8_t outcomePrideModifiers[2];
};
extern const CharacterRecord CHARACTER_RECORDS[CHARACTER_DB_RECORD_COUNT];

const CharacterRecord* characterRecordForId(uint16_t characterId);

int characterRecordResourceId(uint16_t characterId);
#endif
