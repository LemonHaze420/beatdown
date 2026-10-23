#include "character_records.h"
const CharacterRecord*
characterRecordForId(uint16_t characterId)
{
    if(characterId >= CHARACTER_DB_RECORD_COUNT)
        return NULL;
    return &CHARACTER_RECORDS[characterId];
}
int
characterRecordResourceId(uint16_t characterId)
{
    const CharacterRecord* const record = characterRecordForId(characterId);
    if(record == NULL || record->resourceId <= 0)
        return -1;
    return record->resourceId;
}
