#include "game.h"
#include "game_combat_input.h"
#include "game_internal.h"

#include <math.h>

void Game::prepareCombatInputPolicy(GameSlotRecord& actor)
{

    actor.motionControlFlags &= ~0x4000u;
    if(actor.slotIndex == 0 &&
       ((persistent.story.gameStateFlags & 1u) == 0 ||
        ((persistent.story.battleFlags & 0x200u) != 0 &&
         actor.actorBehaviorFlags != 4u)))
        actor.motionControlFlags |= 0x4000u;
}

uint32_t Game::combatHistoryCode(const GameSlotRecord& actor, uint32_t pad,
                                float worldX, float worldZ) const
{
    uint32_t code = pad & ~0x10u;
    if((actor.motionControlFlags & 0x4000u) == 0)
        return code;
    code &= ~0x1fu;
    if((pad & 0xfu) == 0)
        return code;
    const GameSlotRecord* target = combatSlotByIndex(actor.linkedSlotIndex);
    const bool active = activeOpponentCount > 0 &&
        (currentMode != GAME_MODE_STORY || persistent.story.battleActive != 0);
    if(active && target != NULL && target != &actor)
        return code | combatMovementRelation(actor, *target, worldX, worldZ);
    GameSlotRecord forward = {};
    const float heading = actor.headingDegrees * 0.01745329251994329577f;
    forward.position.x = actor.position.x + sinf(heading) * 100.0f;
    forward.position.z = actor.position.z + cosf(heading) * 100.0f;
    return code | combatMovementRelation(actor, forward, worldX, worldZ);
}

void Game::updateCombatFacingFields()
{
    for(int i = -2; i < activeOpponentCount; ++i)
    {
        if(i == -1 && !allyActive)
            continue;
        GameSlotRecord& actor = i == -2 ? persistent.player :
            (i == -1 ? ally : (i == 0 ? enemy : additionalEnemies[i - 1]));
        if((actor.flags & 1u) == 0)
            continue;
        const GameSlotRecord* linked = combatSlotByIndex(actor.linkedSlotIndex);
        const bool active = activeOpponentCount > 0 &&
            (currentMode != GAME_MODE_STORY || persistent.story.battleActive != 0);
        Vec3 target;
        if(active && linked != NULL && linked != &actor)
            target = linked->position;
        else
        {
            const float radians = actor.headingDegrees * 0.01745329251994329577f;
            target = actor.position;
            target.x += sinf(radians) * 50.0f;
            target.z += cosf(radians) * 50.0f;
        }
        publishCombatFacing(actor, target, movementCameraPosition);
    }
}

int Game::rewriteCombatInputCommand(GameSlotRecord& actor, int command)
{

    actor.combatCommand.preserveInputFacing = false;
    if(command < 0)
        return -1;
    GameSlotRecord* target = mutableCombatSlotByIndex(actor.linkedSlotIndex);
    const bool active = activeOpponentCount > 0 && target != NULL &&
        target != &actor && (currentMode != GAME_MODE_STORY ||
                             persistent.story.battleActive != 0);
    const bool armed = (actor.actorActionFlags & 0x4000u) != 0;
    const bool interruptible = actor.appliedMotionId == stanceMotion(actor, 0x1b5) ||
        actor.appliedMotionId == 0x2dd;
    bool markBodyTarget = false;
    if(command == 0xe || command == 0x1c)
    {
        actor.actorActionFlags &= ~8u;
        if((actor.actorBehaviorFlags & 2u) == 0)
        {
            if(armed || !interruptible)
            {
                if((actor.poseStateWord & 8u) != 0)
                    command = command == 0xe ? 0x59 : 0x5a;
                else
                {
                    const bool body = currentMode == GAME_MODE_STORY && active &&
                        (target->impactSelectorState & 4u) != 0 &&
                        target->currentHealth > 0 && !combatTargetInteractable(*target) &&
                        (target->actorRuntimeFlags & 0xcu) == 0 &&
                        actor.groundHeightValid && target->groundHeightValid &&
                        fabsf(target->groundHeight - actor.groundHeight) < 10.0f &&
                        actor.field_0x439c <= (command == 0xe ? 15.0f : 30.0f);
                    if(body)
                    {
                        bool grounded;
                        if(!combatGroundedState(*target, grounded))
                            return -1;
                        const int light = command != 0xe;
                        markBodyTarget = !light;
                        actor.actorActionFlags &= ~0x78u;
                        command = grounded ?
                            (target->facingTarget == 0 ? 0x7a : 0x78) + light :
                            (target->facingTarget == 0 ? 0x74 : 0x76) + light;
                        actor.actorRuntimeFlags |= 0x80u;
                        actor.combatCommand.preserveInputFacing = true;
                    }
                    else if(actor.facingState == 8)
                    {
                        command = command == 0xe ? 0x5c : 0x5d;
                        actor.actorUpdateFlags &= ~1u;
                        actor.combatCommand.preserveInputFacing = true;
                    }
                }
            }
            else
                command = command == 0xe ? 5 : 0x13;
        }
    }
    else if(command == 0xf || command == 0x1d)
    {
        if((actor.actorActionFlags & 0x10u) == 0)
            return -1;
        actor.actorActionFlags &= ~0x10u;
        if((actor.actorActionFlags & 8u) == 0)
        {
            actor.actorActionFlags &= ~0x40u;
            if(actor.actorBehaviorFlags != 2)
                actor.actorActionFlags &= ~0x80u;
            return -1;
        }
        actor.actorActionFlags |= 0x20u;
        actor.actorActionFlags &= ~8u;
    }
    else if(command == 0x46)
    {
        if(interruptible)
            command = 0x40;
        else if((actor.poseStateWord & 8u) != 0 && !armed)
            command = 0x5b;
    }
    else if(command == 0x4f || command == 0x50)
    {
        if(!active)
            return -1;
        actor.combatCommand.preserveInputFacing = true;
    }
    else
    {
        const bool nearBody = currentMode == GAME_MODE_STORY && target != NULL &&
            (target->impactSelectorState & 4u) != 0 && actor.field_0x439c <= 30.0f &&
            (persistent.story.battleFlags & 0x200u) == 0;
        if(!nearBody)
            actor.actorActionFlags &= ~0x78u;
        else if(actor.actorBehaviorFlags != 2)
        {
            if(command >= 0xa && command <= 0xd) command += 0x72;
            else if(command >= 0x18 && command <= 0x1b) command += 0x68;
        }
    }

    const bool ordinary = ((actor.actorBehaviorFlags & 6u) == 0 || command > 0x52) &&
        ((actor.actorActionFlags & 2u) == 0 ||
         (actor.actorTargetingFlags & 0x20000u) != 0) &&
        actor.appliedMotionId != stanceMotion(actor, 0x1e);
    const bool earlyCancel = command == 0x4b &&
        (actor.actorTargetingFlags & 0x40u) != 0 && actor.comboStep <= 1 &&
        actor.actionElapsedSeconds <= 0.33333334f;
    if(!ordinary && !earlyCancel)
        return -1;

    if((actor.actorActionFlags & 2u) != 0 && command != 5 && command != 0x13 &&
       command != 0x40 && command != 4 && command != 0x12)
        return -1;
    if(markBodyTarget)
        target->actorRuntimeFlags |= 8u;
    return command;
}
