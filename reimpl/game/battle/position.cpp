#include "game/game.h"
#include "game/battle/tables.h"
#include "rwworld.h"
#include "game/internal.h"

#include <math.h>

const BattleSpawnRow* battleFollowupSpawnRow(int stage, float x, float y, float z)
{

    if(stage < 0 || stage >= BATTLE_SPAWN_STAGE_COUNT)
        return nullptr;
    const BattleSpawnTable& table = BATTLE_SPAWN_TABLES[stage];
    if(table.rowCount < 2)
        return nullptr;
    const BattleSpawnRow* rows = BATTLE_SPAWN_ROWS + table.firstRow;
    int candidates[256];
    int count = 0;
    candidates[count++] = 1;
    if(stage == 0x1e)
    {
        if(x < -330)
            candidates[count++] = 2;
        else
            for(int index = 4; index < 8 && index < table.rowCount; ++index)
                candidates[count++] = index;
    }
    else if(stage == 0x26)
    {
        if(z > 160)
            return rows + 1;
        for(int index = 3; index < 13 && index < table.rowCount; ++index)
            candidates[count++] = index;
    }
    else
        for(int index = 2; index < table.rowCount; ++index)
            candidates[count++] = index;
    int selected = -1;
    float closest = 0;
    for(int item = 0; item < count; ++item)
    {
        const int index = candidates[item];
        if(index >= table.rowCount)
            continue;
        const BattleSpawnRow& row = rows[index];
        if(stage == 0x13 &&
           (item == 0 ? fabsf(row.y * 0.1f + 5 - y) > 15 :
                        fabsf(row.y * 0.1f + 5 - y) >= 15))
            continue;
        const float dx = x - row.x * 0.1f;
        const float dy = y - row.y * 0.1f;
        const float dz = z - row.z * 0.1f;
        const float distance = dx * dx + dy * dy + dz * dz;
        if(selected < 0 || distance < closest)
        {
            selected = index;
            closest = distance;
        }
    }
    return selected >= 0 ? rows + selected : nullptr;
}

void Game::repositionBattlePair(GameSlotRecord& first, GameSlotRecord& second)
{

    const RwWorldGeometry* geometry =
        static_cast<const RwWorldGeometry*>(stageNavigation.collisionContext);
    if(geometry == nullptr)
        return;
    RwWorldVertex point;
    point.x = first.position.x;
    point.y = first.position.y;
    point.z = first.position.z;
    RwWorldCapsuleContact contact;
    if(!rwWorldCapsuleContact(*geometry, point, point, 20, &contact))
        return;
    const BattleSpawnRow* row = battleFollowupSpawnRow(selectedStageId(),
        first.position.x, first.position.y, first.position.z);
    if(row == nullptr)
        return;
    first.position = { row->x * 0.1f, row->y * 0.1f + 10, row->z * 0.1f };
    first.headingDegrees = row->heading * 0.1f;
    const float radians = first.headingDegrees * 0.01745329251994329577f;
    second.position = { first.position.x + sinf(radians) * 25,
        first.position.y, first.position.z + cosf(radians) * 25 };
    second.headingDegrees = game_detail::normalizeActorDegrees(first.headingDegrees + 180);
}
