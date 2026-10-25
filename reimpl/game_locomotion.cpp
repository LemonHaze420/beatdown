#include "game.h"
#include "game_internal.h"
#include "action_state_handlers.h"

#include <math.h>

using namespace game_detail;

int Game::combatMovementContext(const GameSlotRecord& actor) const
{

    const WeaponInstance* held = (actor.actorActionFlags & 0x4000u) != 0 ?
        weaponManager.equippedBy(actor.slotIndex) : nullptr;
    const WeaponDefinition* definition = held != nullptr ?
        weaponManager.definition(held->objectId) : nullptr;
    int context = 0;
    if(definition != nullptr)
    {
        switch(definition->behaviorId)
        {
        case 0x11: context = 11; break;
        case 0xb: context = (actor.flags & 0x2000u) == 0 ? 10 : 0; break;
        case 6: context = 9; break;
        case 0xa: context = 7; break;
        case 5: context = 6; break;
        case 4: context = 5; break;
        default: break;
        }
    }
    if(context == 0 && (actor.flags & 0x2000u) == 0 &&
       (actor.actorTargetingFlags & 0x40000u) != 0 &&
       actor.currentHealth != actor.maxHealth)
        context = 2;
    if(context == 0 && actor.resourceProfileId == 0x3c)
        context = 12;
    return context;
}

void Game::updateCombatLocomotion(GameSlotRecord& actor, uint32_t pad,
    float worldX, float worldZ, float magnitude)
{
    if(scaledFrameDeltaSeconds == 0.0f ||
       (actor.actorActionFlags & 0x40000u) != 0)
        return;
    const bool movementTask = actor.stateHandler ==
        COMBAT_STATE_UPDATE_COMBATANT_MOVEMENT;
    const bool restingTask = actor.stateHandler == COMBAT_STATE_UPDATE_COMBATANT_DECISION ||
        actor.stateHandler == COMBAT_STATE_UPDATE_COMBATANT_IDLE;
    if(!movementTask && (!restingTask || (actor.actorBehaviorFlags & 0x406u) != 0))
        return;
    if((pad & 15u) == 0 || (actor.actorActionFlags & 4u) != 0)
    {
        if(!movementTask && actor.action != COMBAT_ACTION_MOVE)
            return;
        if((actor.poseStateWord & 8u) != 0)
        {

            const WeaponInstance* held = weaponManager.equippedBy(actor.slotIndex);
            const WeaponDefinition* definition = held != nullptr ?
                weaponManager.definition(held->objectId) : nullptr;
            const int family = definition != nullptr ? definition->behaviorId : 0x12;
            int stop = 0x3b;
            if(family == 0x11) stop = 0x439;
            else if(family == 0xa) stop = 0x2dc;
            else if(family == 5) stop = 0x69c;
            if(stop != 0x3b || combatantActive(actor) || (persistent.story.gameStateFlags & 1u) != 0)
            {
                startCombatMove(actor, stop, COMBAT_ACTION_IDLE);
                runCombatStateTask(actor);
                return;
            }
        }
        startAction(actor, COMBAT_ACTION_IDLE);
        actor.actorUpdateFlags &= ~1u;
        return;
    }

    if(!movementTask)
    {
        const int action = stanceMotion(actor, MOVE_CHARACTER_ACTION_IDLE);
        if(action <= 0)
            return;
        startAction(actor, COMBAT_ACTION_MOVE, false);
        setGameSlotSelectedAction(actor, action);
        runCombatStateTask(actor);
        actor.locomotion.inputSector = -1;
        actor.locomotion.facingSector = -1;
    }
    actor.action = COMBAT_ACTION_MOVE;
    const bool runButton = (pad & CPU_COMBAT_RUN_BUTTON) != 0;
    const bool targeted = combatantActive(actor) ? !runButton : actor.slotIndex != 0;
    const WeaponInstance* held = (actor.actorActionFlags & 0x4000u) != 0 ?
        weaponManager.equippedBy(actor.slotIndex) : nullptr;
    const WeaponDefinition* definition = held != nullptr ?
        weaponManager.definition(held->objectId) : nullptr;
    const int style = combatStyle(actor);
    const int archetype = style >= 0 && style < MOVE_CHARACTER_COUNT ?
        MOVE_CHARACTER_RECORDS[style].archetype : -1;
    const bool canRun = definition == nullptr ||
        ((definition->rawWords[30] & 0x10u) != 0 &&
         (definition->behaviorId != 5 || archetype == 2));
    const bool running = canRun && (targeted ?
        (runButton || (actor.actorUpdateFlags & 4u) != 0) :
        (runButton || magnitude > PLAYER_RUN_THRESHOLD));
    const int context = combatMovementContext(actor);

    if(targeted)
    {
        const float x = ((pad & 1u) != 0 ? 1.0f : 0.0f) -
                        ((pad & 2u) != 0 ? 1.0f : 0.0f);
        const float z = ((pad & 4u) != 0 ? 1.0f : 0.0f) -
                        ((pad & 8u) != 0 ? 1.0f : 0.0f);
        cameraRelativeAxes(x, z, worldX, worldZ);
    }
    const float length = sqrtf(worldX * worldX + worldZ * worldZ);
    if(length == 0.0f)
        return;
    worldX /= length;
    worldZ /= length;
    float speed = LOCOMOTION_FALLBACK_SPEED;
    const PersistentGameRecord* profile = characterRecord(
        static_cast<uint16_t>(actor.persistentRecordId));
    int motion = profile != nullptr && profile->gender != 0 ? MOVE_LOCOMOTION_FREE_WALK_FEMALE :
                                    MOVE_LOCOMOTION_FREE_WALK;
    if(running)
    {
        actor.poseStateWord = (actor.poseStateWord & ~4u) | 8u;
        speed = context == 6 ? 36.0f : PLAYER_RUN_SPEED;
        if(!targeted && (persistent.story.gameStateFlags & 1u) == 0 && context != 6)
            speed = 60.0f;
        motion = COMBAT_RUN_MOTIONS[context];
        if(context != 6 && context != 7 && context != 9 && context != 10 && context != 11)
            motion = targeted ? MOVE_LOCOMOTION_TARGET_RUN : MOVE_LOCOMOTION_FREE_RUN;
        actor.actorUpdateFlags |= 1u;
    }
    else
    {
        actor.poseStateWord = (actor.poseStateWord & ~8u) | 4u;
        if(targeted)
        {
            const float heading = atan2f(worldX, worldZ) * 57.29577951308232f;
            const float relative = normalizeActorDegrees(heading - actor.headingDegrees);
            const int inputSector = moveLocomotionDirection(
                atan2f(((pad & 1u) != 0 ? 1.0f : 0.0f) - ((pad & 2u) != 0 ? 1.0f : 0.0f),
                       ((pad & 4u) != 0 ? 1.0f : 0.0f) - ((pad & 8u) != 0 ? 1.0f : 0.0f)) *
                57.29577951308232f);
            const int facingSector = moveLocomotionDirection(relative);
            bool eightWay = inputSector != actor.locomotion.inputSector;
            if(eightWay)
            {
                actor.actorUpdateFlags |= 8u;
                actor.locomotion.inputSector = static_cast<int8_t>(inputSector);
                actor.locomotion.facingSector = static_cast<int8_t>(facingSector);
            }
            else if((actor.actorUpdateFlags & 8u) != 0 &&
                    facingSector == actor.locomotion.facingSector)
                eightWay = true;
            else
                actor.actorUpdateFlags &= ~8u;
            int direction = COMBAT_WALK_TABLE_DIRECTIONS[facingSector];
            if(!eightWay)
                direction = relative >= -55.0f && relative < 55.0f ? 0 :
                    (relative >= 55.0f && relative < 125.0f ? 2 :
                     (relative >= -125.0f && relative < -55.0f ? 3 : 1));
            const MoveLocomotion* walk = moveLocomotionForCharacter(style, context, direction);
            if(walk != nullptr)
            {
                speed = walk->frameDistance * LOCOMOTION_FRAMES_PER_SECOND;
                motion = walk->moveId;
            }
            actor.actorUpdateFlags &= ~1u;
        }
        else
        {
            if(context == 5 || context == 6 || context == 7 || context == 9 ||
               context == 10 || context == 11)
            {
                const MoveLocomotion* walk = moveLocomotionForCharacter(style, context, 0);
                if(walk != nullptr)
                {
                    speed = walk->frameDistance * LOCOMOTION_FRAMES_PER_SECOND;
                    motion = walk->moveId;
                }
            }
            actor.actorUpdateFlags |= 1u;
        }
    }
    actor.velocity.x = worldX * speed;
    actor.velocity.z = worldZ * speed;
    actor.locomotion.displacementX = actor.velocity.x * scaledFrameDeltaSeconds;
    actor.locomotion.displacementZ = actor.velocity.z * scaledFrameDeltaSeconds;
    actor.combatMotion.turnDuration = COMBAT_LOCOMOTION_TURN_SECONDS;
    actor.combatMotion.elapsed = 0.0f;
    actor.combatMotion.turnBone = 1;
    setLocomotionMove(actor, motion);
    actor.recoveredMoveId = -1;
    actor.scheduledMotionValid = false;
    actor.flags &= ~4u;
}
