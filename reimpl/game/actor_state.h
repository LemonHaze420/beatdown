#ifndef GAME_ACTOR_STATE_H
#define GAME_ACTOR_STATE_H

#include <stdint.h>

inline const int16_t ACTOR_WALK_MOTIONS[2] = { 0x249, 0x25a };
inline const int16_t ACTOR_TRANSITION_MOTIONS[2] = { 0x218, 0x237 };
inline const int16_t ACTOR_PARTY_IDLE_MOTIONS[6] =
    { 0x23b, 0x23c, 0x240, 0x6a6, 0x301, 0x302 };

struct GameSlotRecord;
struct Vec3;

struct ActorActionRuntime
{
    uint8_t combatControl;
    uint8_t state;
    uint8_t nextState;
    uint8_t previousState;
    uint8_t activation;
    uint8_t phase;
    int16_t motionId;
    uint32_t flags;
    float targetHeading;
    float targetPosition[3];
    float radiusSquared;
    float elapsedSeconds;
};

struct ActorActionInput
{
    float deltaSeconds;
    uint32_t runtimeFlags;
    int16_t defaultMotionId;
    uint8_t gender;
    bool playerRoster;
    bool impactOverride;
    const GameSlotRecord* linkedActor;
};

struct ActorActionCallbacks
{
    void (*setMove)(void* userData, GameSlotRecord& actor, int moveId);
    uint32_t (*random)(void* userData);
    void* userData;
};

enum ActorActionTickResult
{
    ACTOR_ACTION_INACTIVE,
    ACTOR_ACTION_HANDLED,
    ACTOR_ACTION_UNRECOVERED
};

void initializeActorActionRuntime(ActorActionRuntime& runtime);
bool requestActorActionState(ActorActionRuntime& runtime, uint8_t state);
bool requestActorActionTargetState(ActorActionRuntime& runtime,
                                    uint8_t activation, uint8_t state);
bool clearActorActionState(ActorActionRuntime& runtime);
void queueActorActionState(ActorActionRuntime& runtime, uint8_t state);
void setActorActionTarget(ActorActionRuntime& runtime, const Vec3& target,
                           float radius);
ActorActionTickResult tickActorActionRuntime(
    GameSlotRecord& actor, ActorActionRuntime& runtime,
    const ActorActionInput& input, const ActorActionCallbacks& callbacks);

#endif
