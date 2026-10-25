#include "game/game.h"
#include "game/combat/input.h"
#include "game/combat/links.h"
#include "game/internal.h"
#include <math.h>

using namespace game_detail;

uint8_t combatGroundSurfaceKind(uint16_t property)
{

    return static_cast<uint8_t>((property >> 12) & 0xfu);
}

void Game::updateCombatSurfaceMovement(GameSlotRecord& slot)
{

    if(scaledFrameDeltaSeconds == 0 || !slot.groundNormalValid)
        return;
    float heading = 0;
    if(slot.statePhase == 1)
    {
        heading = normalizeActorDegrees(atan2f(slot.groundNormal.x,
            slot.groundNormal.z) * 57.29578f + 180 - slot.headingDegrees);
        setCombatActorMotion(slot, slot.selectedActionId);
        slot.actorTargetingFlags &= ~0x10u;
        slot.statePhase = 2;
    }
    bool release = false;
    if((slot.animationCueFlags & 2u) != 0)
    {
        if(!slot.groundSurfaceKindValid && (slot.actorRuntimeFlags & 0x20u) == 0)
            return;
        release = (slot.actorRuntimeFlags & 0x20u) != 0 || slot.groundSurfaceKind != 1;
        if((slot.actorRuntimeFlags & 1u) == 0)
            slot.stateWork[0] = 0;
        else
        {
            ++slot.stateWork[0];
            if(slot.stateWork[0] > 4)
                release = true;
        }
    }
    if(release)
        queueCombatPair(slot, 0x33d, 0);
    else
    {
        const Vec3 offset = { 0, 0, scaledFrameDeltaSeconds * -27.6f };
        setCombatMovementTarget(slot, slot, offset, heading, 1);
    }
}

void Game::publishCombatGroundNormal(GameSlotRecord& slot)
{
    slot.groundNormalValid = false;
    slot.groundSurfaceKindValid = false;
    if(!stageNavigation.valid || stageNavigation.groundContact == nullptr)
        return;

    GameCombatSurfaceContact contact = {};
    if(stageNavigation.groundContact(stageNavigation.context, slot.position.x,
                                      slot.position.z, slot.groundHeight, &contact))
    {
        if(fabsf(contact.point.y - slot.groundHeight) > 0.001f)
            return;
        slot.groundNormal = contact.normal;
        const unsigned stage = selectedStageId();
        if(stage < COMBAT_SURFACE_STAGE_COUNT &&
           contact.material < COMBAT_SURFACE_MATERIAL_COUNT)
        {
            slot.groundSurfaceKind = combatGroundSurfaceKind(
                COMBAT_SURFACE_PROPERTIES[stage][contact.material]);
            slot.groundSurfaceKindValid = true;
        }
    }
    else
    {

        slot.groundNormal = { 0, 1, 0 };
        slot.groundSurfaceKind = 0;
        slot.groundSurfaceKindValid = true;
    }
    slot.groundNormalValid = true;
}

bool Game::tryCombatGroundReaction(GameSlotRecord& slot)
{

    if(!slot.groundNormalValid)
        return false;
    const Vec3 normal = normalizedVector(slot.groundNormal);
    if(asinf(normal.y) * 57.29578f >= 80.0f)
        return false;
    if(!stageNavigation.valid || stageNavigation.groundColumn == nullptr)
        return false;
    bool queue = (slot.impactSelectorState & 4u) != 0;
    float height;
    uint16_t material;
    if(stageNavigation.groundColumn(stageNavigation.context, slot.position.x,
                                     slot.position.z, slot.position.y,
                                     &height, &material))
    {
        const unsigned stage = selectedStageId();
        if(stage >= COMBAT_SURFACE_STAGE_COUNT ||
           material >= COMBAT_SURFACE_MATERIAL_COUNT)
            return false;

        if(combatGroundSurfaceKind(COMBAT_SURFACE_PROPERTIES[stage][material]) != 1)
            return false;
        if(slot.position.y - height < 5.0f)
            queue = true;
    }
    if(queue)
        queueCombatPair(slot, 827, 0);
    return queue;
}
