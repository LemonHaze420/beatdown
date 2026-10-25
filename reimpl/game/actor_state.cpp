#include "game/actor_state.h"
#include "game/game.h"
#include "game/internal.h"
#include "move_tables.h"
#include "character_records.h"

#include <math.h>
#include <string.h>

using game_detail::normalizeCombatDegrees;

namespace
{

void setMove(GameSlotRecord& actor, ActorActionRuntime& runtime,
               int moveId, const ActorActionCallbacks& callbacks)
{
    callbacks.setMove(callbacks.userData, actor, moveId);
    runtime.motionId = static_cast<int16_t>(moveId);
}

bool setIdle(GameSlotRecord& actor, ActorActionRuntime& runtime,
               const ActorActionInput& input,
               const ActorActionCallbacks& callbacks)
{
    int moveId = input.defaultMotionId;
    if(input.playerRoster)
    {
        if(input.gender == 1)
            moveId = 0x248;
        else
        {
            if(callbacks.random == NULL)
                return false;
            moveId = ACTOR_PARTY_IDLE_MOTIONS[
                callbacks.random(callbacks.userData) % 6u];
        }
    }
    setMove(actor, runtime, moveId, callbacks);
    return true;
}

bool turnActor(GameSlotRecord& actor, float targetHeading)
{
    const float delta = normalizeCombatDegrees(
        targetHeading - actor.headingDegrees);
    const bool done = fabsf(delta) < 10.0f;
    const float step = done ? delta : (delta < 0.0f ? -10.0f : 10.0f);
    actor.headingDegrees = normalizeCombatDegrees(actor.headingDegrees + step);
    return done;
}

bool moveActor(GameSlotRecord& actor, ActorActionRuntime& runtime,
                 float distance, float maxTurn)
{
    const float x = runtime.targetPosition[0] - actor.position.x;
    const float y = runtime.targetPosition[1] - actor.position.y;
    const float z = runtime.targetPosition[2] - actor.position.z;
    const bool reached = x * x + z * z <= runtime.radiusSquared;
    if(reached)
        runtime.flags |= 4u;
    const float length = sqrtf(x * x + y * y + z * z);
    const float scale = length > 0.0f ? distance / length : 0.0f;
    const float heading = atan2f(x, z) * 57.29577951308232f;
    float delta = normalizeCombatDegrees(heading - actor.headingDegrees);
    if(delta > maxTurn) delta = maxTurn;
    if(delta < -maxTurn) delta = -maxTurn;
    actor.headingDegrees = normalizeCombatDegrees(actor.headingDegrees + delta);
    actor.position.x += x * scale;
    actor.position.y += y * scale;
    actor.position.z += z * scale;
    return reached;
}

void finishState(ActorActionRuntime& runtime)
{
    runtime.previousState = runtime.state;
    runtime.state = runtime.nextState;
    runtime.nextState = 0xff;
    if(runtime.state != 0xff)
        requestActorActionState(runtime, runtime.state);
}

}

void initializeActorActionRuntime(ActorActionRuntime& runtime)
{
    memset(&runtime, 0, sizeof(runtime));
    runtime.state = 0xff;
    runtime.nextState = 0xff;
    runtime.previousState = 0xff;
    runtime.motionId = -1;
}

bool requestActorActionState(ActorActionRuntime& runtime, uint8_t state)
{

    if(runtime.combatControl != 0 || (runtime.flags & 0x2000u) != 0)
        return false;
    return requestActorActionTargetState(runtime, 0, state);
}

bool requestActorActionTargetState(ActorActionRuntime& runtime,
                                    uint8_t activation, uint8_t state)
{
    if(activation != 0)
        runtime.flags &= ~1u;
    if((runtime.flags & 0x2000u) != 0)
        return false;
    runtime.previousState = runtime.state;
    runtime.state = state;
    runtime.activation = activation;
    runtime.phase = 0;
    if(activation == 0)
        runtime.flags |= 1u;
    return true;
}

bool clearActorActionState(ActorActionRuntime& runtime)
{
    return requestActorActionTargetState(runtime, 0, 0);
}

void queueActorActionState(ActorActionRuntime& runtime, uint8_t state)
{
    runtime.nextState = state;
}

void setActorActionTarget(ActorActionRuntime& runtime, const Vec3& target,
                           float radius)
{
    runtime.targetPosition[0] = target.x;
    runtime.targetPosition[1] = target.y;
    runtime.targetPosition[2] = target.z;
    runtime.radiusSquared = radius * radius;
}

ActorActionTickResult tickActorActionRuntime(
    GameSlotRecord& actor, ActorActionRuntime& runtime,
    const ActorActionInput& input, const ActorActionCallbacks& callbacks)
{
    if(runtime.combatControl != 0)
        return ACTOR_ACTION_INACTIVE;
    if(runtime.activation == 0 ||
       (runtime.activation == 1 && (input.runtimeFlags & 8u) != 0) ||
       (runtime.activation == 2 && (runtime.flags & 0x100u) != 0))
        runtime.flags |= 1u;
    if(runtime.state == 0xff || (runtime.flags & 1u) == 0)
        return ACTOR_ACTION_INACTIVE;
    if(callbacks.setMove == NULL)
        return ACTOR_ACTION_UNRECOVERED;

    if((actor.actorActionFlags & 0x100000u) != 0)
    {
        if(input.linkedActor == NULL)
            return ACTOR_ACTION_UNRECOVERED;
        const GameSlotRecord& linked = *input.linkedActor;
        if(linked.actorBehaviorFlags != 4 && actor.appliedMotionId != 0x255 &&
           actor.appliedMotionId != 0x612 && actor.appliedMotionId != 0x63f)
        {
            actor.actorActionFlags &= ~0x100000u;
            requestActorActionState(runtime, 0);
        }

        return ACTOR_ACTION_UNRECOVERED;
    }

    switch(runtime.state)
    {
    case 0:
        if(!setIdle(actor, runtime, input, callbacks))
            return ACTOR_ACTION_UNRECOVERED;
        runtime.phase = 0xff;
        break;
    case 1:
    case 2:
    {
        const bool run = runtime.state == 2;
        if(runtime.phase == 0)
        {
            const int motion = run ? 0x254 : ACTOR_WALK_MOTIONS[input.gender == 1];
            setMove(actor, runtime, motion, callbacks);
            runtime.elapsedSeconds = 0.0f;
            runtime.phase = 1;
        }
        if(runtime.phase == 1)
        {
            const float step = input.deltaSeconds *
                (run ? 65.454544f : 13.333333f);
            if(step > 0.0f && moveActor(actor, runtime, step, run ? 10.0f : 7.5f))
                runtime.phase = 2;
            break;
        }
        if(runtime.phase == 2)
        {
            runtime.elapsedSeconds += input.deltaSeconds;
            if(runtime.elapsedSeconds > 0.3f)
                runtime.phase = 3;

            if(!run)
                break;
        }
        if(runtime.phase == 3 || (run && runtime.phase == 2))
        {
            if(!setIdle(actor, runtime, input, callbacks))
                return ACTOR_ACTION_UNRECOVERED;
            runtime.phase = 0xff;
        }
        break;
    }
    case 8:
        if(runtime.phase == 0)
        {
            setMove(actor, runtime, ACTOR_TRANSITION_MOTIONS[input.gender == 1], callbacks);
            runtime.phase = 1;
        }
        else if(runtime.phase == 1 && runtime.motionId != actor.appliedMotionId)
            runtime.phase = 2;
        else if(runtime.phase == 2)
        {
            if(!setIdle(actor, runtime, input, callbacks))
                return ACTOR_ACTION_UNRECOVERED;
            runtime.phase = 0xff;
        }
        break;
    case 9:
        if(turnActor(actor, runtime.targetHeading))
            runtime.phase = 0xff;
        break;
    case 10:
    case 11:
    case 12:
    case 13:
        if(runtime.phase == 0)
        {
            int moveId = runtime.state == 10 ? 0x6b4 :
                (runtime.state == 11 ? 0x241 :
                 (runtime.state == 12 ? 0x26a : 0x269));
            if(runtime.state == 11 && input.impactOverride &&
               (actor.impactSelectorState & 0x10u) != 0)
                moveId = 0x78e;
            setMove(actor, runtime, moveId, callbacks);
            runtime.phase = 1;
        }
        else if(runtime.phase == 1 && runtime.motionId != actor.appliedMotionId)
        {
            if(runtime.state >= 12 && !setIdle(actor, runtime, input, callbacks))
                return ACTOR_ACTION_UNRECOVERED;
            runtime.phase = 0xff;
        }
        break;
    case 18:
        if(runtime.motionId != actor.appliedMotionId)
        {
            if((runtime.flags & 0x200u) != 0)
                setMove(actor, runtime, runtime.motionId, callbacks);
            else
            {
                if(!setIdle(actor, runtime, input, callbacks))
                    return ACTOR_ACTION_UNRECOVERED;
                runtime.phase = 0xff;
            }
        }
        break;
    case 19:
        if(runtime.phase == 0)
        {
            setMove(actor, runtime, 0x23b, callbacks);
            runtime.phase = 1;
        }
        break;
    default:
        return ACTOR_ACTION_UNRECOVERED;
    }
    if(runtime.phase == 0xff)
        finishState(runtime);
    return ACTOR_ACTION_HANDLED;
}

namespace
{

uint32_t actorCommandBits(const EventVmCommand& command,
                           EventOperandTypeCode type)
{
    for(uint8_t i = 0; i < command.operandCount; ++i)
        if(command.operands[i].type == type)
            return command.operands[i].bits;
    return 0;
}

float actorCommandFloat(const EventVmCommand& command,
                          EventOperandTypeCode type)
{
    const uint32_t bits = actorCommandBits(command, type);
    float value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

void publishActorMove(GameSlotRecord& slot, StoryActorState* story, int moveId)
{
    const CharacterRecord* character =
        characterRecordForId(static_cast<uint16_t>(slot.characterId));
    const int selected = resolveCharacterMove(
        character != NULL ? character->combatStyleId : -1, moveId);
    setGameSlotSelectedAction(slot, selected, true);
    slot.appliedMotionId = static_cast<int16_t>(selected);
    slot.recoveredMoveId = static_cast<int16_t>(selected);
    MoveMotion motion;
    if(story != NULL && moveMotions(selected, &motion, 1) == 1)
        story->motionId = static_cast<uint16_t>(motion.motionId);
}

}

bool Game::applyActorActionCommand(const EventVmCommand& command)
{
    const bool walkPosition = command.opcode == EVENT_OP_EXF_NPC_WALK_POS;
    const bool runPosition = command.opcode == EVENT_OP_EXF_NPC_RUN_POS;
    const bool walkTarget = command.opcode == EVENT_OP_EXF_NPC_WALK_TARGET;
    const bool runTarget = command.opcode == EVENT_OP_EXF_NPC_RUN_TARGET;
    const bool rotate = command.opcode == EVENT_OP_EXF_NPC_ROT;
    const bool motion = command.opcode == EVENT_OP_EXF_SET_MOTION;
    if(!walkPosition && !runPosition && !walkTarget && !runTarget &&
       !rotate && !motion)
        return false;

    uint32_t actorId = actorCommandBits(
        command, rotate ? EVENT_OPERAND_EXF_VALUE : EVENT_OPERAND_ACTOR_ID);
    const uint32_t registerId =
        actorCommandBits(command, EVENT_OPERAND_REGISTER_ID);
    if(registerId > 0 && registerId < STORY_REGISTER_COUNT)
        actorId = persistent.story.registers[registerId];
    StageActorRuntime* runtime = actorId == 0 ? NULL :
        findStageActor(static_cast<uint16_t>(actorId));
    GameSlotRecord* slot = actorId == 0 ? &persistent.player :
        (runtime != NULL ? &runtime->slot : NULL);
    StoryActorState* story =
        findStoryActor(persistent.story, static_cast<uint16_t>(actorId), false);
    if(slot == NULL || story == NULL)
        return false;

    ActorActionRuntime& action = slot->actorAction;
    if(rotate)
    {
        const float heading = actorCommandFloat(command, EVENT_OPERAND_WORLD_X);
        const uint32_t immediate =
            actorCommandBits(command, EVENT_OPERAND_EXF_PARAMETER);
        if(actorId == 0 || immediate != 0)
        {
            slot->headingDegrees = heading;
            story->headingDegrees = heading;
        }
        else
        {

            story->headingDegrees = slot->headingDegrees;
            requestActorActionState(action, 9);
            action.targetHeading = heading;
        }
        return true;
    }

    if(motion)
    {
        const uint32_t mode = actorCommandBits(command, EVENT_OPERAND_EXF_VALUE);
        if(mode < 2)
        {
            requestActorActionState(action, 18);
            const int moveId = static_cast<int16_t>(
                actorCommandBits(command, EVENT_OPERAND_EXF_PARAMETER));
            publishActorMove(*slot, story, moveId);
            action.motionId = static_cast<int16_t>(moveId);
            if(mode == 0) action.flags &= ~0x200u;
            else action.flags |= 0x200u;
            story->movementMode = 0;
        }
        else
        {
            if(runtime != NULL)
                story->motionId = static_cast<uint16_t>(runtime->activeMotionId);
            if(mode == 2)
                action.combatControl = 1;
            else if(mode == 3)
                action.combatControl = 0;
        }
        return true;
    }

    Vec3 target;
    float radius;
    if(walkTarget || runTarget)
    {
        const uint32_t targetId =
            actorCommandBits(command, EVENT_OPERAND_TARGET_ACTOR_ID);
        const StoryActorState* targetActor =
            findStoryActor(persistent.story, static_cast<uint16_t>(targetId));
        if(targetActor == NULL)
        {
            story->movementMode = 0;
            story->movementDurationFrames = 0.0f;
            return true;
        }
        target = { targetActor->position[0], targetActor->position[1],
                   targetActor->position[2] };

        radius = static_cast<float>(static_cast<int32_t>(
            actorCommandBits(command, EVENT_OPERAND_DURATION)));
    }
    else
    {
        target = { actorCommandFloat(command, EVENT_OPERAND_WORLD_X),
                   actorCommandFloat(command, EVENT_OPERAND_WORLD_Y),
                   actorCommandFloat(command, EVENT_OPERAND_WORLD_Z) };

        radius = actorCommandFloat(command, EVENT_OPERAND_DURATION);
    }
    requestActorActionState(action, (runPosition || runTarget) ? 2 : 1);
    setActorActionTarget(action, target, radius);

    story->movementMode = 0;
    story->movementDurationFrames = 0.0f;
    story->targetActorId = 0xffff;
    return true;
}

void Game::advanceActorActions(float deltaSeconds)
{
    if(currentScreen != GAME_SCREEN_GAMEPLAY || deltaSeconds <= 0.0f ||
       !isfinite(deltaSeconds))
        return;
    ActorActionCallbacks callbacks = {};
    callbacks.userData = this;
    callbacks.random = [](void* userData) -> uint32_t
    {
        return nextGameRandom(static_cast<Game*>(userData)->random);
    };
    callbacks.setMove = [](void* userData, GameSlotRecord& slot, int moveId)
    {
        Game& game = *static_cast<Game*>(userData);
        const uint16_t actorId = &slot == &game.persistent.player ? 0 :
            static_cast<uint16_t>(slot.characterId);
        publishActorMove(slot, findStoryActor(game.persistent.story, actorId, false), moveId);
    };

    for(int i = -1; i < GAME_STAGE_ACTOR_CAPACITY; ++i)
    {
        StageActorRuntime* runtime = i < 0 ? NULL : &stageActors[i];
        GameSlotRecord& slot = i < 0 ? persistent.player : runtime->slot;
        if(runtime != NULL && (!runtime->occupied || runtime->battleActive ||
                              runtime->battleAdmitted))
            continue;
        if((slot.flags & 1u) == 0 || slot.currentHealth <= 0 ||
           slot.actorAction.combatControl != 0 || slot.actorAction.state == 0xff)
            continue;
        const uint16_t actorId = i < 0 ? 0 : runtime->authoredCharacterId;
        StoryActorState* story = findStoryActor(persistent.story, actorId, false);
        if(story == NULL || !story->active)
            continue;
        ActorActionInput input = {};
        input.deltaSeconds = deltaSeconds;
        input.defaultMotionId = runtime != NULL ? runtime->defaultMotionId : 1;
        input.playerRoster = stageActorIsActiveRoster(
            static_cast<uint16_t>(slot.persistentRecordId));
        const PersistentGameRecord* record = i < 0 ?
            characterRecord(static_cast<uint16_t>(slot.persistentRecordId)) :
            stageActorPersistentRecord(i);
        input.gender = record != NULL ? record->gender : 0;
        if(tickActorActionRuntime(slot, slot.actorAction, input, callbacks) !=
           ACTOR_ACTION_HANDLED)
            continue;
        story->position[0] = slot.position.x;
        story->position[1] = slot.position.y;
        story->position[2] = slot.position.z;
        story->headingDegrees = slot.headingDegrees;
        if(runtime != NULL)
            runtime->activeMotionId = static_cast<int16_t>(story->motionId);
    }
}
