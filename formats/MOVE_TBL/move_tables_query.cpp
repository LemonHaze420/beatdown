#include "move_tables.h"
#include <stddef.h>
namespace
{
bool
movePersonalRequirementsMet(
    const uint8_t requirements[MOVE_LIST_REQUIREMENT_COUNT],
    const uint8_t* levels)
{
    bool hasAlternative = false;
    for(int i = 3; i < MOVE_LIST_REQUIREMENT_COUNT; ++i)
    {
        if(requirements[i] == 0)
            continue;
        hasAlternative = true;
        if(levels != NULL && levels[i] >= requirements[i])
            return true;
    }

    int primaryTotal = 0;
    for(int i = 0; i < 3; ++i)
    {
        primaryTotal += requirements[i];
        if(requirements[i] != 0 &&
           (levels == NULL || levels[i] < requirements[i]))
            return false;
    }
    return !hasAlternative || primaryTotal != 0;
}

bool
movePersonalConditionMet(
    const MovePersonalCondition& condition,
    const MovePersonalActionContext& context)
{
    switch(condition.opcode)
    {
    case 0x89:
        return context.archetype == condition.argument;
    case 0x8a:
        return context.archetype != condition.argument;
    case 0x8b:
        return true;
    case 0x8c:
        return context.characterIdZero;
    case 0x8d:
        return context.playerIndex > 1;
    case 0x8e:
        return context.resourceProfileId == condition.argument;
    case 0x8f:
        return !context.secondaryGameState;
    case 0x90:
        return context.secondaryGameState;
    case 0x92:
        return condition.argument == 0 ?
            context.storySpecial : !context.storySpecial;
    case 0x93:
        return context.interactionRecovery;
    default:
        return true;
    }
}
}

bool
movePersonalActionEligible(
    const uint8_t requirements[MOVE_LIST_REQUIREMENT_COUNT],
    const MovePersonalCondition* conditions, int conditionCount,
    const MovePersonalActionContext& context)
{
    if(!movePersonalRequirementsMet(requirements, context.levels))
        return false;
    for(int i = 0; i < conditionCount; ++i)
        if(!movePersonalConditionMet(conditions[i], context))
            return false;
    return true;
}

const MoveRecord*
moveRecord(int moveId)
{
    if(moveId < 0 || moveId >= MOVE_RECORD_COUNT)
        return NULL;
    return &MOVE_RECORDS[moveId];
}
const MoveListStyle*
moveListStyle(int personalMovesetId)
{
    if(personalMovesetId <= 0 ||
       personalMovesetId > MOVE_LIST_STYLE_COUNT)
        return NULL;
    return &MOVE_LIST_STYLES[personalMovesetId - 1];
}
bool
moveListEntryVisible(
    int personalMovesetId, int entryIndex,
    const uint8_t levels[MOVE_LIST_REQUIREMENT_COUNT])
{
    const MoveListStyle* style = moveListStyle(personalMovesetId);
    if(style == NULL || entryIndex < 0 || entryIndex >= style->entryCount)
        return false;
    const MoveListGate* gate = NULL;
    for(int i = 0; i < style->gateCount; ++i)
    {
        const MoveListGate& candidate =
            MOVE_LIST_GATES[style->firstGate + i];
        if(candidate.entryIndex == entryIndex)
        {
            gate = &candidate;
            break;
        }
    }
    if(gate == NULL)
        return true;
    if(!gate->predicatePass)
        return false;
    bool hasAlternative = false;
    for(int i = 3; i < MOVE_LIST_REQUIREMENT_COUNT; ++i)
    {
        const uint8_t requirement = gate->requirements[i];
        if(requirement == 0)
            continue;
        hasAlternative = true;
        if(levels != NULL && levels[i] >= requirement)
            return true;
    }
    int primaryTotal = 0;
    for(int i = 0; i < 3; ++i)
    {
        const uint8_t requirement = gate->requirements[i];
        primaryTotal += requirement;
        if(requirement != 0 &&
           (levels == NULL || levels[i] < requirement))
            return false;
    }
    return !hasAlternative || primaryTotal != 0;
}
int
moveListVisibleEntries(
    int personalMovesetId,
    const uint8_t levels[MOVE_LIST_REQUIREMENT_COUNT],
    uint8_t* output, int capacity)
{
    const MoveListStyle* style = moveListStyle(personalMovesetId);
    if(style == NULL)
        return 0;
    int count = 0;
    for(int entry = 0; entry < style->entryCount; ++entry)
    {
        if(!moveListEntryVisible(personalMovesetId, entry, levels))
            continue;
        if(output != NULL && count < capacity)
            output[count] = static_cast<uint8_t>(entry);
        ++count;
    }
    return count;
}
int
resolveCharacterMove(int characterIndex, int moveId)
{

    if(moveId == 0)
        moveId = 1;
    if(moveId > 0)
        return moveId;
    if(characterIndex < 0 || characterIndex >= MOVE_CHARACTER_COUNT)
        return -1;
    int index = -moveId;
    if(index > MOVE_REMAP_LIMIT)
        index = 1;
    const int archetype = MOVE_CHARACTER_RECORDS[characterIndex].archetype;
    if(archetype < 0 || archetype >= MOVE_ARCHETYPE_COUNT ||
       index < 0 || index >= MOVE_REMAP_COUNT)
        return -1;
    return MOVE_ARCHETYPE_REMAP[archetype][index];
}
int
moveLocomotionDirection(float relativeHeadingDegrees)
{
    while(relativeHeadingDegrees < -180.0f)
        relativeHeadingDegrees += 360.0f;
    while(relativeHeadingDegrees >= 180.0f)
        relativeHeadingDegrees -= 360.0f;
    if(relativeHeadingDegrees >= -25.0f &&
       relativeHeadingDegrees < 25.0f)
        return 0;
    if(relativeHeadingDegrees >= 25.0f &&
       relativeHeadingDegrees < 65.0f)
        return 1;
    if(relativeHeadingDegrees >= 65.0f &&
       relativeHeadingDegrees < 115.0f)
        return 2;
    if(relativeHeadingDegrees >= 115.0f &&
       relativeHeadingDegrees < 155.0f)
        return 3;
    if(relativeHeadingDegrees >= 155.0f ||
       relativeHeadingDegrees < -155.0f)
        return 4;
    if(relativeHeadingDegrees >= -155.0f &&
       relativeHeadingDegrees < -115.0f)
        return 5;
    if(relativeHeadingDegrees >= -115.0f &&
       relativeHeadingDegrees < -65.0f)
        return 6;
    return 7;
}
const MoveLocomotion*
moveLocomotion(int mode, int variant, int direction)
{
    if(mode < 0 || mode >= MOVE_LOCOMOTION_MODE_COUNT ||
       variant < 0 || variant >= MOVE_LOCOMOTION_VARIANT_COUNT ||
       direction < 0 || direction >= MOVE_LOCOMOTION_DIRECTION_COUNT)
        return NULL;
    const MoveLocomotion& entry =
        MOVE_LOCOMOTION[mode][variant][direction];
    return entry.moveId > 0 ? &entry : NULL;
}
const MoveLocomotion*
moveLocomotionForCharacter(int characterIndex, int mode, int direction)
{
    if(characterIndex < 0 || characterIndex >= MOVE_CHARACTER_COUNT)
        return NULL;
    return moveLocomotion(
        mode, MOVE_CHARACTER_RECORDS[characterIndex].locomotionVariant,
        direction);
}
bool
moveIsAttack(int moveId)
{
    const MoveRecord* record = moveRecord(moveId);
    return record != NULL && record->handler == MOVE_HANDLER_ATTACK;
}
int
movePairedAction(int moveId)
{
    const MoveRecord* record = moveRecord(moveId);
    return record != NULL ? record->linkedMoveId : 0;
}
uint32_t
moveTargetingBits(int moveId)
{
    const MoveRecord* record = moveRecord(moveId);
    if(record == NULL)
        return 0;
    if((record->behaviorFlags & MOVE_BEHAVIOR_HIT_BLOCKS) != 0 &&
       record->alternateLinkedMoveId != 0)
        return 0x40u;
    if((record->behaviorFlags & MOVE_BEHAVIOR_LINKED) != 0 &&
       record->linkedMoveId == 0)
        return 0x80u;
    return 0;
}
const MoveReaction*
moveReaction(int set, int reaction, int direction)
{
    if(set < 0 || set >= MOVE_REACTION_SET_COUNT)
        return NULL;
    if(reaction < 0 || reaction >= MOVE_REACTION_COUNT)
        return NULL;
    if(direction < 0 || direction >= MOVE_REACTION_DIRECTIONS)
        return NULL;
    return &MOVE_REACTIONS[set][reaction][direction];
}
int
moveStreamInstructionWords(const int16_t* words, int available)
{
    if(words == NULL || available <= 0)
        return 0;
    switch(words[0])
    {
    case MOVE_STREAM_PLAY_MOTION:
    case MOVE_STREAM_EFFECT:
        return 5;
    case MOVE_STREAM_SKIP_3:
        return 3;
    case MOVE_STREAM_LOOP_BEGIN:
    case MOVE_STREAM_SET_TIME_SCALE:
    case MOVE_STREAM_CALL:
    case MOVE_STREAM_SET_ACTION:
    case MOVE_STREAM_COMBO_GATE:
        return 2;
    case MOVE_STREAM_RANDOM_CALL:
        return available < 2 ? 2 : 2 + words[1];
    default:

        return 1;
    }
}
int
moveMotions(int moveId, MoveMotion* out, int capacity)
{
    if(out == NULL || capacity <= 0 ||
       moveId < 0 || moveId >= MOVE_RECORD_COUNT)
        return 0;
    const MoveStream& stream = MOVE_STREAMS[moveId];
    int count = 0;
    int offset = 0;
    while(offset < stream.wordCount && count < capacity)
    {
        const int16_t* words = &MOVE_STREAM_WORDS[stream.firstWord + offset];
        const int step = moveStreamInstructionWords(
            words, stream.wordCount - offset);
        if(step <= 0)
            break;
        if(words[0] == MOVE_STREAM_PLAY_MOTION)
        {
            MoveMotion& motion = out[count++];
            motion.motionId = words[1];
            motion.blendFrames = words[2];
            motion.maxDurationFrames = words[3];
            motion.transitionFrames = words[4];
        }
        offset += step;
    }
    return count;
}
float
moveMotionDuration(const MoveMotion& motion, float clipSeconds)
{
    if(motion.maxDurationFrames == -1)
        return clipSeconds;
    const float clamped = static_cast<float>(motion.maxDurationFrames) / 60.0f;
    return clamped <= clipSeconds ? clamped : clipSeconds;
}
float
moveMotionTransition(const MoveMotion& motion, float clipSeconds)
{
    const float transition =
        static_cast<float>(motion.transitionFrames) / 60.0f;
    const float blend = static_cast<float>(motion.blendFrames) / 60.0f;
    if(moveMotionDuration(motion, clipSeconds) - blend < transition)
        return 0.0f;
    return transition;
}
int
moveMotionBundle(int motionId, int* positionInBundle)
{
    for(int bundle = 0; bundle < MOVE_ANIMATION_BUNDLE_COUNT; ++bundle)
    {
        const MoveAnimationBundle& record = MOVE_ANIMATION_BUNDLES[bundle];
        for(int i = 0; i < record.motionCount; ++i)
            if(MOVE_BUNDLE_MOTIONS[record.firstMotion + i] == motionId)
            {
                if(positionInBundle != NULL)
                    *positionInBundle = i;
                return bundle;
            }
    }
    if(positionInBundle != NULL)
        *positionInBundle = -1;
    return -1;
}
int
moveMotionBundleForCharacter(int characterIndex, int motionId,
                             int* positionInBundle)
{
    if(characterIndex < 0 || characterIndex >= MOVE_CHARACTER_COUNT)
    {
        if(positionInBundle != NULL)
            *positionInBundle = -1;
        return -1;
    }
    const MoveCharacterRecord& character =
        MOVE_CHARACTER_RECORDS[characterIndex];
    for(int resource = 0; resource < character.animationResourceCount;
        ++resource)
    {
        const int bundle = MOVE_CHARACTER_ANIMATION_RESOURCES[
            character.firstAnimationResource + resource];
        if(bundle < 0 || bundle >= MOVE_ANIMATION_BUNDLE_COUNT)
            continue;
        const MoveAnimationBundle& record = MOVE_ANIMATION_BUNDLES[bundle];
        for(int position = 0; position < record.motionCount; ++position)
        {
            if(MOVE_BUNDLE_MOTIONS[record.firstMotion + position] != motionId)
                continue;
            if(positionInBundle != NULL)
                *positionInBundle = position;
            return bundle;
        }
    }
    if(positionInBundle != NULL)
        *positionInBundle = -1;
    return -1;
}
int
moveHitDamage(const MoveHitBlock& block, float attackerPower,
              float victimMultiplier, float situational)
{
    if(block.damage == 0)
        return 0;

    unsigned value = static_cast<unsigned>(
        static_cast<float>(block.damage) * attackerPower + 0.01f) & 0xffffu;
    value = static_cast<unsigned>(
        static_cast<float>(value) * victimMultiplier + 0.01f) & 0xffffu;
    value = static_cast<unsigned>(
        static_cast<float>(value) * situational + 0.01f) & 0xffffu;
    return value == 0 ? 1 : static_cast<int>(value);
}
int
moveGuardDamage(int appliedDamage)
{
    if(appliedDamage == 0)
        return 0;
    const int value = static_cast<int>(
        static_cast<float>(appliedDamage) * 0.333f) & 0xffff;
    return value == 0 ? 1 : value;
}
int
moveGuardGauge(int partSelector)
{
    switch(partSelector)
    {
    case 0:
    case 4:
        return 2;
    case 1:
    case 5:
        return 1;
    case 2:
    case 6:
        return 4;
    case 3:
    case 7:
        return 3;
    case 8:
    case 9:
        return 5;
    case 10:
        return 0;
    default:
        return -1;
    }
}
int
moveCommandFollowUp(int commandIndex)
{
    if(commandIndex < 0 || commandIndex >= MOVE_COMMAND_COUNT)
        return -1;
    const MoveCommand& command = MOVE_COMMANDS[commandIndex];
    if(command.kind != MOVE_COMMAND_KIND_FOLLOWUP)
        return -1;
    if(command.selector < 0 || command.selector >= MOVE_COMMAND_MOVE_COUNT)
        return -1;
    const int16_t move = MOVE_COMMAND_MOVES[command.selector];
    return move > 0 && move < MOVE_RECORD_COUNT ? move : -1;
}
int
moveDispatchCommand(int kind, int selector)
{
    if(kind == 2)
    {
        switch(selector)
        {
        case 1: return 91;
        case 2: return 92;
        case 3: return 93;
        case 4: return 94;
        case 5: return 95;
        case 6: return 1;
        case 7: return 2;
        case 8: return 4;
        default:

            return -1;
        }
    }
    if(kind == 3)
    {
        if(selector >= 1 && selector <= 37)
            return selector + 4;
        if(selector >= 39 && selector <= 45)
            return selector + 3;
    }
    return -1;
}
int
moveEquippedTypeOne(int slotId)
{

    if(slotId < 0 || slotId >= MOVE_COMMAND_MOVE_COUNT)
        return -1;
    const int moveId = MOVE_COMMAND_MOVES[slotId];
    return moveId > 0 && moveId < MOVE_RECORD_COUNT ? moveId : -1;
}
int
moveForAction(int actionId)
{
    if(actionId < 0 || actionId >= MOVE_ACTION_COUNT)
        return -1;
    const int16_t move = MOVE_DEFAULT_ACTIONS[actionId];
    return move > 0 && move < MOVE_RECORD_COUNT ? move : -1;
}
int
moveBehaviourInstructionWords(const int16_t* words, int available)
{
    if(words == NULL || available <= 0)
        return 0;
    switch(words[0] & 0xffff)
    {
    case 0x0e:
        return 7;
    case MOVE_BEHAVIOUR_EFFECT:
        return 6;
    case 0x17:
        return 6;
    case 0x0c:
    case 0x13:
    case 0x32:
    case 0x38:
        return 5;
    case 0x0b:
    case 0x10:
    case 0x14:
    case 0x18:
    case 0x19:
    case 0x2d:
    case 0x2e:
        return 4;
    case 0x1d:
    case 0x23:
    case 0x2f:
    case 0x33:
    case 0x3b:
    case 0x3c:
    case 0x3d:
    case 0x41:
    case 0x46:
    case 0x49:
    case 0x4d:
    case 0x4e:
        return 3;
    case 0x02:
    case 0x05:
    case 0x0a:
    case 0x11:
    case 0x16:
    case 0x1a:
    case 0x1b:
    case 0x1e:
    case 0x1f:
    case 0x20:
    case 0x21:
    case 0x22:
    case 0x25:
    case 0x26:
    case 0x27:
    case 0x29:
    case 0x2b:
    case 0x30:
    case 0x31:
    case 0x35:
    case 0x37:
    case 0x39:
    case 0x3a:
    case 0x3e:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x45:
    case 0x47:
    case 0x4a:
    case 0x4b:
    case 0x4f:
    case 0x50:
        return 2;
    case MOVE_BEHAVIOUR_EFFECT_LIST:
    {
        int end = 1;
        while(end < available && (words[end] & 0xffff) != 0xffff)
            ++end;
        return end + 1;
    }
    case MOVE_BEHAVIOUR_ARM_HIT_WINDOW:
    {
        int end = 4;
        while(end < available && (words[end] & 0xffff) != 0xffff)
            ++end;
        return end + 1;
    }
    case MOVE_BEHAVIOUR_CAMERA:
        if(available > 1 && words[1] == 0)
            return 2;
        return (available > 2 && (words[2] & 0xffff) == 0xffff) ? 4 : 6;
    default:

        return 1;
    }
}
bool
moveHitTransitionApplies(uint32_t attackerBehaviourFlags,
                         uint32_t victimBehaviourFlags)
{
    return (attackerBehaviourFlags & MOVE_ACTOR_DRIVES_HITS) != 0 &&
           (victimBehaviourFlags & MOVE_ACTOR_TAKES_TRANSITIONS) != 0;
}
bool
moveInputCommandEligible(const MoveInputCommand& command,
                         uint32_t actorBehaviour, uint32_t linkedBehaviour,
                         uint32_t actorField0x4390,
                         uint32_t linkedField0x4390,
                         uint32_t actorTargeting, uint32_t linkedTargeting)
{

    if(command.actorBehaviour != 0 &&
       (actorBehaviour & command.actorBehaviour) == 0)
        return false;
    if(command.linkedBehaviour != 0 &&
       (linkedBehaviour & command.linkedBehaviour) == 0)
        return false;
    if(command.actorField0x4390 != 0 &&
       (actorField0x4390 & command.actorField0x4390) == 0)
        return false;
    if(command.linkedField0x4390 != 0 &&
       (linkedField0x4390 & command.linkedField0x4390) == 0)
        return false;
    if(command.actorTargeting != 0 &&
       (actorTargeting & command.actorTargeting) == 0)
        return false;
    if(command.linkedTargeting != 0 &&
       (linkedTargeting & command.linkedTargeting) == 0)
        return false;
    return true;
}
int
moveCommandActionIndex(int command, bool facingTarget, float chargeSeconds)
{

    if(command >= 0x3c && command <= 0x3f &&
       chargeSeconds <= MOVE_COMMAND_CHARGE_SECONDS)
        command = MOVE_ACTION_INDEX_UNCHARGED;
    switch(command)
    {
    case 0x29:
    case 0x2a:
    case 0x2b:
    case 0x2c:
    case 0x2d:
    case 0x2e:
    case 0x2f:

        return facingTarget ? MOVE_ACTION_INDEX_FACING_ALT
                            : MOVE_ACTION_INDEX_RESOLVED;
    case 0x30:
    case 0x31:
        return facingTarget ? MOVE_ACTION_INDEX_FACING
                            : MOVE_ACTION_INDEX_RESOLVED;
    case 0x32:
        return facingTarget ? MOVE_ACTION_INDEX_FACING_STRONG
                            : MOVE_ACTION_INDEX_RESOLVED;
    case 0x33:

        return facingTarget ? MOVE_ACTION_INDEX_FACING : command;
    case 0x3b:

        return facingTarget ? MOVE_ACTION_INDEX_FACING
                            : MOVE_ACTION_INDEX_NEUTRAL;
    case 0x3c:
    case 0x3d:
    case 0x3e:
    case 0x3f:

        return command;
    default:
        return facingTarget ? MOVE_ACTION_INDEX_FACING
                            : MOVE_ACTION_INDEX_NEUTRAL;
    }
}
int
moveHitWindows(int moveId, MoveHitWindow* out, int capacity)
{
    if(out == NULL || capacity <= 0 ||
       moveId <= 0 || moveId >= MOVE_RECORD_COUNT)
        return 0;
    const MoveStream& stream = MOVE_BEHAVIOURS[moveId];
    int written = 0;
    int offset = 0;
    while(offset < stream.wordCount && written < capacity)
    {
        const int16_t* words = &MOVE_BEHAVIOUR_WORDS[stream.firstWord + offset];
        const int step = moveBehaviourInstructionWords(
            words, stream.wordCount - offset);
        if(step <= 0)
            break;
        if((words[0] & 0xffff) == MOVE_BEHAVIOUR_ARM_HIT_WINDOW && step >= 5)
        {
            MoveHitWindow& window = out[written++];
            window.centreFrame = words[1];
            window.startFrame = static_cast<int16_t>(words[1] - words[2]);
            window.endFrame = static_cast<int16_t>(words[1] + words[3]);
            window.channelMask = 0;

            for(int i = 4; i < step - 1; ++i)
            {
                const int channel = words[i];
                if(channel >= 0 && channel < MOVE_HIT_CHANNEL_COUNT)
                    window.channelMask |= 1u << channel;
            }
        }
        offset += step;
    }
    return written;
}
bool
moveHitWindowActive(const MoveHitWindow& window, int animationFrame)
{
    return animationFrame >= window.startFrame &&
           animationFrame <= window.endFrame;
}
float
moveAttackPowerForClass(int attackPowerClass)
{
    if(attackPowerClass < 0 ||
       attackPowerClass >= MOVE_ATTACK_POWER_CLASS_COUNT)
        return 1.0f;
    const float power = MOVE_ATTACK_POWER_CLASSES[attackPowerClass];

    return power > 0.0f ? power : 1.0f;
}
int
moveCommandFallbackAction(int command, bool facingTarget)
{

    const bool hasFallback = (command >= 0x29 && command <= 0x33);
    if(!hasFallback)
        return MOVE_ACTION_INDEX_RESOLVED;
    return facingTarget ? MOVE_ACTION_INDEX_FACING
                        : MOVE_ACTION_INDEX_NEUTRAL;
}
int
moveCommandResolvedAction(int command, bool facingTarget, float chargeSeconds)
{
    const int first =
        moveCommandActionIndex(command, facingTarget, chargeSeconds);
    if(first >= 0 && moveForAction(first) > 0)
        return first;
    const int fallback = moveCommandFallbackAction(command, facingTarget);
    if(fallback >= 0 && moveForAction(fallback) > 0)
        return fallback;
    return first >= 0 ? first : MOVE_ACTION_INDEX_RESOLVED;
}
int
movePersonalActionForStyle(
    int combatStyleId, int action,
    const MovePersonalActionContext& context)
{
    if(combatStyleId < 0 || combatStyleId >= MOVE_CHARACTER_COUNT)
        return -1;
    const MoveListStyle* const style = moveListStyle(
        MOVE_CHARACTER_RECORDS[combatStyleId].personalMovesetId);
    if(style == NULL)
        return -1;

    for(int depth = 0;
        action >= 0 && depth < MOVE_ACTION_COUNT;
        action = moveActionParent(action), ++depth)
    {
        for(int i = 0; i < style->actionCount; ++i)
        {
            const MovePersonalAction& candidate =
                MOVE_PERSONAL_ACTIONS[style->firstAction + i];
            if(candidate.actionId == action &&
               movePersonalActionEligible(
                   candidate.requirements,
                   &MOVE_PERSONAL_CONDITIONS[candidate.firstCondition],
                   candidate.conditionCount, context))
                return candidate.moveId;
        }
    }
    return -1;
}
int
moveActionParent(int actionId)
{
    if(actionId < 0 || actionId >= MOVE_ACTION_COUNT)
        return -1;
    const int parent = MOVE_ACTION_PARENTS[actionId];
    if(parent < 0 || parent >= MOVE_ACTION_COUNT)
        return -1;
    return parent;
}
int
moveActionResolveChain(int actionId)
{

    int steps = 0;
    for(int action = actionId;
        action >= 0 && steps < MOVE_ACTION_COUNT;
        action = moveActionParent(action), ++steps)
    {
        if(moveForAction(action) > 0)
            return action;
    }
    return -1;
}
MoveGrabActions
moveGrabActionsFor(uint32_t hitDirection)
{
    MoveGrabActions actions;
    actions.primary = -1;
    actions.secondary = -1;

    if((hitDirection & MOVE_HIT_DIRECTION_FRONT) != 0)
    {
        actions.primary = 0x63;
        actions.secondary = 0x67;
    }
    else if((hitDirection & MOVE_HIT_DIRECTION_BACK) != 0)
    {
        actions.primary = 0x64;
        actions.secondary = 0x68;
    }
    else if((hitDirection & MOVE_HIT_DIRECTION_SIDE_A) != 0)
    {
        actions.primary = 0x65;
        actions.secondary = 0x67;
    }
    else if((hitDirection & MOVE_HIT_DIRECTION_SIDE_B) != 0)
    {
        actions.primary = 0x66;
        actions.secondary = 0x67;
    }
    return actions;
}
int
moveGrabMoveFor(uint32_t hitDirection)
{
    const MoveGrabActions actions = moveGrabActionsFor(hitDirection);
    if(actions.primary >= 0)
    {
        const int move = moveForAction(actions.primary);
        if(move > 0)
            return move;
    }
    if(actions.secondary >= 0)
    {
        const int move = moveForAction(actions.secondary);
        if(move > 0)
            return move;
    }
    return -1;
}
MovePairedOutcome
moveGrabPairedOutcome(int attackerStyle, int victimStyle,
                      uint32_t hitDirection)
{
    MovePairedOutcome outcome = {};
    if(attackerStyle < 0 || attackerStyle >= MOVE_CHARACTER_COUNT ||
       victimStyle < 0 || victimStyle >= MOVE_CHARACTER_COUNT)
        return outcome;
    const int attackerSize =
        MOVE_CHARACTER_RECORDS[attackerStyle].archetype;
    const int victimSize =
        MOVE_CHARACTER_RECORDS[victimStyle].archetype;
    if(attackerSize < 0 || attackerSize >= MOVE_ARCHETYPE_COUNT ||
       victimSize < 0 || victimSize >= MOVE_ARCHETYPE_COUNT)
        return outcome;
    const bool matrixOutcome =
        (hitDirection & MOVE_HIT_DIRECTION_FRONT) != 0 ||
        ((hitDirection & MOVE_HIT_DIRECTION_BACK) == 0 &&
         (hitDirection &
          (MOVE_HIT_DIRECTION_SIDE_A | MOVE_HIT_DIRECTION_SIDE_B)) != 0);
    if(matrixOutcome)
    {
        static const int16_t PRIMARY[MOVE_ARCHETYPE_COUNT]
                                    [MOVE_ARCHETYPE_COUNT] =
        {
            { 126, 126, 519 },
            { 507, 126, 481 },
            { 513, 507, 126 }
        };
        static const int16_t SECONDARY[MOVE_ARCHETYPE_COUNT]
                                      [MOVE_ARCHETYPE_COUNT] =
        {
            { 124, 483, 521 },
            { 509, 124, 483 },
            { 515, 509, 124 }
        };
        outcome.secondaryMove = SECONDARY[attackerSize][victimSize];
        outcome.secondaryLinkedMove = 125;
        outcome.primaryMove = PRIMARY[attackerSize][victimSize];
        outcome.primaryLinkedMove =
            static_cast<int16_t>(movePairedAction(outcome.primaryMove));
        return outcome;
    }
    if((hitDirection & MOVE_HIT_DIRECTION_BACK) != 0)
    {
        outcome.secondaryMove = 281;
        outcome.secondaryLinkedMove =
            static_cast<int16_t>(movePairedAction(281));
        outcome.primaryMove = 279;
        outcome.primaryLinkedMove =
            static_cast<int16_t>(movePairedAction(279));
    }
    return outcome;
}
MovePairedOutcome
moveWeaponGrabOutcome(uint8_t weaponCategory)
{
    MovePairedOutcome outcome = {};
    switch(weaponCategory)
    {
    case 0x11:
    case 0x0a:
    case 0x03:
        outcome.primaryMove = 427;
        outcome.primaryLinkedMove = 428;
        break;
    case 0x0d:
    case 0x06:
    case 0x02:
    case 0x01:
        outcome.primaryMove = 425;
        outcome.primaryLinkedMove = 426;
        break;
    default:
        break;
    }
    return outcome;
}
bool
moveIsGuardBreak(int moveId)
{
    if(moveId <= 0 || moveId >= MOVE_RECORD_COUNT)
        return false;
    return (MOVE_RECORDS[moveId].hitBlocks[0].flagsA & MOVE_HIT_GUARD_BREAK) != 0;
}
