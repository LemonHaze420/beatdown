#ifndef BATTLE_ROSTERS_H
#define BATTLE_ROSTERS_H
#include <stdint.h>

const int BATTLE_ROSTER_SCENE_COUNT = 11;
const int BATTLE_ROSTER_GROUP_COUNT = 26;
struct BattleRosterGroup
{
    int firstMember;
    int memberCount;
};
struct BattleRosterScene
{
    int16_t scene;
    int firstGroup;
    int groupCount;
};
extern const int16_t BATTLE_ROSTER_MEMBERS[];
extern const BattleRosterGroup BATTLE_ROSTER_GROUPS[BATTLE_ROSTER_GROUP_COUNT];
extern const BattleRosterScene BATTLE_ROSTER_SCENES[BATTLE_ROSTER_SCENE_COUNT];

int battleRosterFor(int16_t scene, int16_t characterId);

int battleRosterMembers(int16_t scene, int16_t characterId,
                        int16_t* members, int capacity);

const int16_t BATTLE_ROSTER_SWAP_LEAD = 0x6e;

const int BATTLE_ROSTER_CANDIDATE_LIMIT = 3;
const int BATTLE_ROSTER_MAX_OPPONENTS = 3;
#endif
