#include "game.h"
#include "game_combat_input.h"
#include "game_input_directions.h"
#include "menu_inventory.h"
#include "scene_task.h"
#include "character_records.h"
#include "combat_styles.h"
#include "versus_tables.h"
#include "xaf_runtime.h"
#include "move_tables.h"
#include "action_state_handlers.h"
#include "options_menu.h"
#include "camera.h"
#include "sound_cues.h"
#include "stage.h"
#include "hud_advice.h"
#include "../formats/MENU_LAYOUT/pause_results_layout.h"
#include "../formats/BATTLE_ROSTER/battle_rosters.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "game_internal.h"

using namespace game_detail;

namespace game_detail
{

void setLocomotionMove(GameSlotRecord& slot, int moveId)
{

    slot.appliedMotionId = static_cast<int16_t>(moveId);
    if(slot.locomotionMoveId == moveId)
        return;
    slot.locomotionMoveId = static_cast<int16_t>(moveId);
    slot.playingAnimationFrame = 0.0f;
    slot.scheduledMotionValid = false;
    slot.scheduledMotionComplete = false;
    slot.combatPosePublished = false;
}

int16_t defaultCharacterHealth(CharacterId characterId)
{

    const CharacterRecord* const record =
        characterRecordForId(static_cast<uint16_t>(characterId));
    return record == NULL ? 0 : record->maxHealth;
}

float horizontalDistanceSquared(const GameSlotRecord& first,
                                const GameSlotRecord& second)
{
    const float x = second.position.x - first.position.x;
    const float z = second.position.z - first.position.z;
    return x * x + z * z;
}

void faceSlot(GameSlotRecord& slot, const GameSlotRecord& target)
{

    if(slot.combatMotion.turnDuration != 0.0f || slot.combatMotion.duration != 0.0f)
        return;
    const float x = target.position.x - slot.position.x;
    const float z = target.position.z - slot.position.z;
    if(x * x + z * z > 0.0001f)
        slot.headingDegrees = atan2f(x, z) * 57.29577951308232f;
}

float signedHeadingDelta(const GameSlotRecord& actor,
                         const GameSlotRecord& target)
{
    const float dx = target.position.x - actor.position.x;
    const float dz = target.position.z - actor.position.z;
    if(dx * dx + dz * dz < 0.0001f)
        return 0.0f;
    const float toTarget = atan2f(dx, dz) * 57.29577951308232f;
    return normalizeCombatDegrees(toTarget - actor.headingDegrees);
}

std::int16_t combatSelfDirectionClass(const GameSlotRecord& actor,
                                      const GameSlotRecord& target)
{
    const float delta = signedHeadingDelta(actor, target);
    if(delta > 45.0f && delta <= 135.0f)
        return 0;
    if(delta > -135.0f && delta <= -45.0f)
        return 1;
    if(delta > -45.0f && delta <= 45.0f)
        return 2;
    return 3;
}

}

void
Game::separateFighters(GameSlotRecord& opponent)
{
    const float x = opponent.position.x - persistent.player.position.x;
    const float z = opponent.position.z - persistent.player.position.z;
    const float distanceSquared = x * x + z * z;
    if(distanceSquared >= 5.0f * 5.0f || distanceSquared < 0.0001f)
        return;

    const float distance = sqrtf(distanceSquared);
    const float correction = (5.0f - distance) * 0.5f / distance;
    persistent.player.position.x -= x * correction;
    persistent.player.position.z -= z * correction;
    opponent.position.x += x * correction;
    opponent.position.z += z * correction;
}

void
Game::clampFighterToArena(GameSlotRecord& slot)
{
    if(stageNavigation.valid)
    {

        const float margin = 2.0f;
        float lowerX = stageNavigation.minX + margin;
        float upperX = stageNavigation.maxX - margin;
        float lowerZ = stageNavigation.minZ + margin;
        float upperZ = stageNavigation.maxZ - margin;
        if(lowerX > upperX)
            lowerX = upperX = (stageNavigation.minX + stageNavigation.maxX) * 0.5f;
        if(lowerZ > upperZ)
            lowerZ = upperZ = (stageNavigation.minZ + stageNavigation.maxZ) * 0.5f;
        if(slot.position.x < lowerX)
            slot.position.x = lowerX;
        else if(slot.position.x > upperX)
            slot.position.x = upperX;
        if(slot.position.z < lowerZ)
            slot.position.z = lowerZ;
        else if(slot.position.z > upperZ)
            slot.position.z = upperZ;
        return;
    }
    if(currentMode == GAME_MODE_STORY)
        return;
    const float distanceSquared =
        slot.position.x * slot.position.x + slot.position.z * slot.position.z;
    if(distanceSquared <= arenaRadius * arenaRadius)
        return;

    const float scale = arenaRadius / sqrtf(distanceSquared);
    slot.position.x *= scale;
    slot.position.z *= scale;
}

void
Game::groundFighter(GameSlotRecord& slot)
{
    if(!stageNavigation.valid || stageNavigation.groundHeight == NULL)
        return;

    const float feet = slot.position.y - GAME_ACTOR_GROUND_OFFSET;
    float height = 0.0f;
    if(stageNavigation.groundHeight(stageNavigation.context, slot.position.x,
                                    slot.position.z,
                                    feet + GAME_GROUND_STEP_UP, &height))
    {
        slot.groundHeight = height;
        slot.groundHeightValid = true;
        slot.position.y = height + GAME_ACTOR_GROUND_OFFSET;
        publishCombatGroundNormal(slot);
        return;
    }

    if(stageNavigation.nearestFloor == NULL)
        return;
    float x = slot.position.x;
    float y = slot.position.y;
    float z = slot.position.z;
    if(!stageNavigation.nearestFloor(stageNavigation.context, x, z, &x, &y,
                                     &z))
        return;
    slot.position.x = x;
    slot.position.y = y + GAME_ACTOR_GROUND_OFFSET;
    slot.groundHeight = y;
    slot.groundHeightValid = true;
    slot.position.z = z;
    publishCombatGroundNormal(slot);
}

bool
Game::placeFighter(GameSlotRecord& slot, float x, float z)
{
    const float feet = slot.position.y - GAME_ACTOR_GROUND_OFFSET;
    float height = 0.0f;
    if(!stageNavigation.groundHeight(stageNavigation.context, x, z,
                                     feet + GAME_GROUND_STEP_UP, &height))
        return false;

    const RwWorldGeometry* const collision =
        static_cast<const RwWorldGeometry*>(
            stageNavigation.collisionContext != NULL ?
                stageNavigation.collisionContext :
                stageNavigation.context);
    if(collision != NULL && !collision->triangles.empty())
    {
        const float actorRadius = 5.0f;
        RwWorldVertex start;
        start.x = slot.position.x;
        start.y = slot.position.y;
        start.z = slot.position.z;
        RwWorldVertex desired;
        desired.x = x;
        desired.y = height + GAME_ACTOR_GROUND_OFFSET;
        desired.z = z;
        for(int pass = 0; pass < 4; ++pass)
        {
            RwWorldCapsuleContact contact;
            if(!rwWorldCapsuleContact(
                   *collision, start, desired, actorRadius, &contact))
                break;

            slot.actorRuntimeFlags |= 1u;
            const float clearance = actorRadius * 1.001f;
            desired.x = contact.point.x +
                contact.normal.x * clearance;
            desired.y = contact.point.y +
                contact.normal.y * clearance;
            desired.z = contact.point.z +
                contact.normal.z * clearance;
        }
        x = desired.x;
        z = desired.z;
        height = desired.y - GAME_ACTOR_GROUND_OFFSET;
    }
    slot.position.x = x;
    slot.position.z = z;
    slot.position.y = height + GAME_ACTOR_GROUND_OFFSET;
    slot.groundHeight = height;
    slot.groundHeightValid = true;
    publishCombatGroundNormal(slot);
    return true;
}

void
Game::moveFighter(GameSlotRecord& slot, float deltaX, float deltaZ)
{
    slot.actorRuntimeFlags &= ~1u;
    if(!stageNavigation.valid || stageNavigation.groundHeight == NULL)
    {
        slot.position.x += deltaX;
        slot.position.z += deltaZ;
        return;
    }

    const float startX = slot.position.x;
    const float startZ = slot.position.z;
    if(placeFighter(slot, startX + deltaX, startZ + deltaZ))
        return;

    if(deltaX != 0.0f && placeFighter(slot, startX + deltaX, startZ))
        return;
    if(deltaZ != 0.0f)
        placeFighter(slot, startX, startZ + deltaZ);
}

int Game::stageSelection(void) const
{
    return selectedStage;
}

uint16_t Game::selectedStageId(void) const
{
    if(currentMode != GAME_MODE_STORY &&
       selectedStage >= 0 && selectedStage < GAME_VERSUS_STAGE_COUNT)
        return VERSUS_STAGE_IDS[selectedStage];
    return static_cast<uint16_t>(selectedStage);
}

void Game::setArenaRadius(float radius)
{
    if(radius < 35.0f)
        radius = 35.0f;
    if(radius > 100.0f)
        radius = 100.0f;
    arenaRadius = radius;
}

void Game::setStageNavigation(const GameStageNavigation& navigation)
{
    stageNavigation = navigation;
    stageNavigation.valid = navigation.valid &&
        navigation.groundHeight != NULL &&
        navigation.maxX > navigation.minX && navigation.maxZ > navigation.minZ;
}

void Game::clearStageNavigation(void)
{
    stageNavigation = GameStageNavigation();
}

GameScreen Game::screen(void) const { return currentScreen; }
