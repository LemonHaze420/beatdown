#ifndef GAME_BATTLE_TABLES_H
#define GAME_BATTLE_TABLES_H

#include <stdint.h>

struct BattleResourceList
{
    uint32_t address;
    int firstId;
    int count;
};
struct BattleActionMoves
{
    uint32_t address;
    int16_t moves[3][5];
};
struct BattleFinisherDefinition
{
    uint32_t address;
    float seconds;
    int resourceVariant;
    int16_t moves[5];
};
extern const int32_t BATTLE_RESOURCE_IDS[];
extern const BattleResourceList BATTLE_RESOURCE_LISTS[];
extern const int BATTLE_RESOURCE_LIST_COUNT;
extern const uint32_t BATTLE_RESOURCE_ROWS[24][5];
extern const uint32_t BATTLE_FOLLOWUP_RESOURCES[3][5];
extern const BattleActionMoves BATTLE_ACTION_MOVES[7];
extern const BattleFinisherDefinition BATTLE_FINISHER_DEFINITIONS[16][2];
extern const uint16_t BATTLE_SEQUENCE_TEXT_IDS[17];
const int BATTLE_WEAPON_PRIDE_CLASS_COUNT = 4;
extern const int16_t BATTLE_WEAPON_PRIDE_BONUSES[BATTLE_WEAPON_PRIDE_CLASS_COUNT];
const int BATTLE_DIALOGUE_GROUP_COUNT = 36;
const int BATTLE_DIALOGUE_WORD_COUNT = 49;
extern const uint16_t BATTLE_DIALOGUE_GROUPS[BATTLE_DIALOGUE_GROUP_COUNT][BATTLE_DIALOGUE_WORD_COUNT];
struct BattleSpawnRow
{
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t heading;
};
struct BattleSpawnTable
{
    uint32_t address;
    int firstRow;
    int rowCount;
};
const int BATTLE_SPAWN_STAGE_COUNT = 56;
extern const BattleSpawnRow BATTLE_SPAWN_ROWS[];
extern const BattleSpawnTable BATTLE_SPAWN_TABLES[BATTLE_SPAWN_STAGE_COUNT];
const BattleSpawnRow* battleFollowupSpawnRow(int stage, float x, float y, float z);
const BattleResourceList* battleResourceList(uint32_t address);

#endif
