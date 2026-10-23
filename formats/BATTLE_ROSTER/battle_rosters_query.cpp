#include "battle_rosters.h"
#include <stddef.h>
int
battleRosterFor(int16_t scene, int16_t characterId)
{
    for(int i = 0; i < BATTLE_ROSTER_SCENE_COUNT; ++i)
    {
        const BattleRosterScene& row = BATTLE_ROSTER_SCENES[i];
        if(row.scene != scene)
            continue;

        for(int g = 0; g < row.groupCount; ++g)
        {
            const BattleRosterGroup& group =
                BATTLE_ROSTER_GROUPS[row.firstGroup + g];
            if(group.memberCount > 0 &&
               BATTLE_ROSTER_MEMBERS[group.firstMember] == characterId)
                return row.firstGroup + g;
        }

        return -1;
    }
    return -1;
}
int
battleRosterMembers(int16_t scene, int16_t characterId,
                    int16_t* members, int capacity)
{
    if(members == NULL || capacity <= 0)
        return 0;
    const int index = battleRosterFor(scene, characterId);
    if(index < 0)
        return 0;
    const BattleRosterGroup& group = BATTLE_ROSTER_GROUPS[index];
    int written = 0;
    for(int i = 0; i < group.memberCount && written < capacity; ++i)
        members[written++] = BATTLE_ROSTER_MEMBERS[group.firstMember + i];
    return written;
}
