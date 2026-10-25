#include "story_state.h"
#include "game/save.h"
#include "formats/STORY_FLOW/story_flow_tables.h"

#include <math.h>
#include <string.h>

void
initializeStoryState(StoryState& state)
{

    memset(&state, 0, sizeof(state));
    state.eventTypeMask = -1;
    for(int i = 0; i < STORY_PARTY_COUNT; ++i)
        state.party[i] = 0xffff;
    state.removedActorId = 0xffff;
    state.lastPunchedActorId = 0xffff;
    state.lastPunchingActorId = 0xffff;
    state.messageActorId = 0xffff;
    state.activeOpponentRosterIndex = -1;
    state.activeOpponentCharacterId = 0xffff;

    state.eventSelectorPrimary = -1;
    state.eventSelectorSecondary = -1;

    state.storyOverlayRoot2Eligibility = 100;
    state.storyOverlayRoot1Eligibility = 100;

    state.activeStoryEventKinds[0] = -1;
    state.activeStoryEventKinds[1] = -1;
    state.recruitmentPlayerResourceProfile = -1;
    state.progression.phase = STORY_PROGRESSION_PHASE_PROLOGUE;
    state.progression.selectedPartnerProfileId = -1;
    state.progression.scratch005f3d5c = -1;
    state.progression.scratch005f3d58 = -1;

    state.progression.scratch005f3d60 = 0;
    state.progression.dynamicMessageStageId = -1;
    state.progression.dynamicMessageId = -1;
    state.progression.lastPromptStageId = -1;
    for(int i = 0; i < 2; ++i)
    {
        state.progression.hudRadar.relationshipTargets[i].characterId = -1;
        state.progression.hudRadar.relationshipTargets[i].stageId = -1;
        state.progression.hudRadar.specialRelationshipTargets[i].characterId =
            -1;
        state.progression.hudRadar.specialRelationshipTargets[i].stageId = -1;
    }
    state.progression.hudRadar.objective.primary.characterId = -1;
    state.progression.hudRadar.objective.primary.stageId = -1;
    state.progression.hudRadar.objective.alternate.characterId = -1;
    state.progression.hudRadar.objective.alternate.stageId = -1;
    state.stageTransition.queuedEventId = 0xffff;
    state.stageTransition.actorProfileId = 0xffff;
    state.stageTransition.arrivalSelector = -1;
    for(int i = 0; i < STORY_CHARACTER_COUNT; ++i)
        state.characters[i].profileRevealFlags = 0;
}

namespace
{

bool finiteStoryFloat(float value)
{
    return isfinite(value) != 0;
}

bool validateStoryCameraTransition(const StoryCameraTransition& transition)
{
    for(int i = 0; i < 3; ++i)
    {
        if(!finiteStoryFloat(transition.start[i]) ||
           !finiteStoryFloat(transition.end[i]))
            return false;
    }
    return true;
}

}

bool
validateStoryState(const StoryState& state)
{
    for(int i = 0; i < STORY_CHARACTER_COUNT; ++i)
    {
        const StoryCharacterState& character = state.characters[i];
        if(character.reserved != 0)
            return false;
    }

    for(int i = 0; i < STORY_DRESS_SLOT_COUNT; ++i)
    {
        if(state.dressInventory[i].reserved != 0)
            return false;
    }

    for(int i = 0; i < STORY_NPC_SLOT_COUNT; ++i)
    {
        if(state.npcSlots[i].reserved[0] != 0 ||
           state.npcSlots[i].reserved[1] != 0 ||
           state.npcSlots[i].reserved[2] != 0)
            return false;
    }

    for(int i = 0; i < STORY_GENERATOR_COUNT; ++i)
    {
        const StoryGeneratorState& generator = state.generators[i];
        if(!finiteStoryFloat(generator.position[0]) ||
           !finiteStoryFloat(generator.position[1]) ||
           !finiteStoryFloat(generator.position[2]) ||
           !finiteStoryFloat(generator.headingDegrees) ||
           !finiteStoryFloat(generator.timerSeconds))
            return false;
    }

    if(!validateStoryCameraTransition(state.cameraPosition) ||
       !validateStoryCameraTransition(state.cameraTarget))
        return false;

    for(int i = 0; i < STORY_ACTOR_COUNT; ++i)
    {
        const StoryActorState& actor = state.actors[i];
        for(int component = 0; component < 3; ++component)
        {
            if(!finiteStoryFloat(actor.position[component]) ||
               !finiteStoryFloat(actor.destination[component]))
                return false;
        }
        if(!finiteStoryFloat(actor.headingDegrees) ||
           !finiteStoryFloat(actor.movementDurationFrames))
            return false;
    }

    for(int i = 0; i < STORY_PARTY_COUNT; ++i)
    {
        if(state.party[i] == 0xffff)
            continue;
        for(int previous = 0; previous < i; ++previous)
        {
            if(state.party[previous] == state.party[i])
                return false;
        }
    }

    const StoryStageTransition& transition = state.stageTransition;
    if(state.progression.phase < STORY_PROGRESSION_PHASE_BOOT ||
       state.progression.phase >= STORY_PROGRESSION_PHASE_COUNT ||
       transition.lifecycle > STORY_TRANSITION_FIELD_RESUME ||
       transition.loadState > STORY_STAGE_LOAD_BIND_EFFECTS ||
       transition.authoredTransformCount > 2 ||
       state.progression.fullHealCount > 8 ||
       !finiteStoryFloat(transition.headingDegrees) ||
       !finiteStoryFloat(transition.arrival[0]) ||
       !finiteStoryFloat(transition.arrival[1]) ||
       !finiteStoryFloat(transition.arrival[2]) ||
       !finiteStoryFloat(transition.elapsedSeconds))
        return false;
    for(int i = 0; i < transition.authoredTransformCount; ++i)
    {
        const StoryAuthoredTransform& authored = transition.actorTransforms[i];
        if(!finiteStoryFloat(authored.headingDegrees) ||
           !finiteStoryFloat(authored.position[0]) ||
           !finiteStoryFloat(authored.position[1]) ||
           !finiteStoryFloat(authored.position[2]))
            return false;
    }

    return true;
}

bool
getStorySaveFlag(const StoryState& state, uint16_t flagId)
{
    if(flagId >= STORY_SAVE_FLAG_COUNT)
        return false;
    return (state.saveFlags[flagId >> 5] & (1u << (flagId & 31))) != 0;
}

void
setStorySaveFlag(StoryState& state, uint16_t flagId, bool enabled)
{
    if(flagId >= STORY_SAVE_FLAG_COUNT)
        return;
    const uint32_t mask = 1u << (flagId & 31);
    if(enabled)
        state.saveFlags[flagId >> 5] |= mask;
    else
        state.saveFlags[flagId >> 5] &= ~mask;
}

StoryActorState*
findStoryActor(StoryState& state, uint16_t actorId, bool create)
{
    StoryActorState* freeSlot = NULL;
    for(int i = 0; i < STORY_ACTOR_COUNT; ++i)
    {
        StoryActorState& actor = state.actors[i];
        if(actor.active && actor.actorId == actorId)
            return &actor;
        if(!actor.active && freeSlot == NULL)
            freeSlot = &actor;
    }
    if(!create || freeSlot == NULL)
        return NULL;
    memset(freeSlot, 0, sizeof(*freeSlot));
    freeSlot->targetActorId = 0xffff;
    freeSlot->actorId = actorId;
    freeSlot->currentHealth = 100;
    freeSlot->active = 1;
    freeSlot->visible = 1;
    return freeSlot;
}

const StoryActorState*
findStoryActor(const StoryState& state, uint16_t actorId)
{
    for(int i = 0; i < STORY_ACTOR_COUNT; ++i)
    {
        if(state.actors[i].active && state.actors[i].actorId == actorId)
            return &state.actors[i];
    }
    return NULL;
}

bool
addStoryPartyMember(StoryState& state, uint16_t actorId, bool friendly)
{
    for(int i = 0; i < STORY_PARTY_COUNT; ++i)
    {
        if(state.party[i] == actorId)
            return true;
    }
    for(int i = 0; i < STORY_PARTY_COUNT; ++i)
    {
        if(state.party[i] != 0xffff)
            continue;
        state.party[i] = actorId;
        StoryActorState* actor = findStoryActor(state, actorId, true);
        if(actor != NULL)
        {
            actor->inParty = 1;
            actor->friendly = friendly ? 1 : 0;
        }
        return true;
    }
    return false;
}

bool
removeStoryPartyMember(StoryState& state, uint16_t actorId)
{
    for(int i = 0; i < STORY_PARTY_COUNT; ++i)
    {
        if(state.party[i] != actorId)
            continue;
        state.party[i] = 0xffff;
        StoryActorState* actor = findStoryActor(state, actorId, false);
        if(actor != NULL)
        {
            actor->inParty = 0;
            actor->friendly = 0;
        }
        return true;
    }
    return false;
}

void
advanceStoryActor(StoryState& state, StoryActorState& actor,
                  float deltaSeconds)
{
    if(!actor.active || actor.movementMode < 1 ||
       actor.movementMode > 2 || !isfinite(deltaSeconds) ||
       deltaSeconds <= 0.0f)
        return;

    float destination[3] =
    {
        actor.destination[0],
        actor.destination[1],
        actor.destination[2]
    };
    if(actor.targetActorId != 0xffff &&
       actor.targetActorId != actor.actorId)
    {
        const StoryActorState* target =
            findStoryActor(state, actor.targetActorId);
        if(target != NULL && target->active)
        {
            destination[0] = target->position[0];
            destination[1] = target->position[1];
            destination[2] = target->position[2];
        }
    }

    const float dx = destination[0] - actor.position[0];
    const float dz = destination[2] - actor.position[2];
    const float distanceSquared = dx * dx + dz * dz;
    const float arrivalRadius = actor.movementMode == 1 ?
        4.4f : 10.0f;
    const float frameDelta = deltaSeconds * 60.0f;
    if(distanceSquared <= arrivalRadius * arrivalRadius ||
       (actor.movementDurationFrames > 0.0f &&
        frameDelta >= actor.movementDurationFrames))
    {
        actor.position[0] = destination[0];
        actor.position[1] = destination[1];
        actor.position[2] = destination[2];
        actor.movementDurationFrames = 0.0f;
        actor.movementMode = 0;
        return;
    }

    const float speed = actor.movementMode == 1 ?
        13.333333f : 65.454544f;
    const float distance = sqrtf(distanceSquared);
    if(distance <= 0.000001f)
    {
        actor.position[1] = destination[1];
        actor.movementDurationFrames = 0.0f;
        actor.movementMode = 0;
        return;
    }

    const float step = speed * deltaSeconds;
    const float scale = step < distance ? step / distance : 1.0f;
    actor.position[0] += dx * scale;
    actor.position[1] += (destination[1] - actor.position[1]) * scale;
    actor.position[2] += dz * scale;
    actor.headingDegrees = atan2f(dx, dz) * 57.29577951308232f;
    if(actor.movementDurationFrames > 0.0f)
    {
        actor.movementDurationFrames -= frameDelta;
        if(actor.movementDurationFrames <= 0.0f)
        {
            actor.position[0] = destination[0];
            actor.position[1] = destination[1];
            actor.position[2] = destination[2];
            actor.movementDurationFrames = 0.0f;
            actor.movementMode = 0;
        }
    }
}

void
advanceStoryActors(StoryState& state, float deltaSeconds)
{
    if(!isfinite(deltaSeconds) || deltaSeconds <= 0.0f)
        return;
    for(int i = 0; i < STORY_ACTOR_COUNT; ++i)
        advanceStoryActor(state, state.actors[i], deltaSeconds);
}

namespace
{

bool storyFlag(const StoryState& state, uint16_t flagId)
{
    return getStorySaveFlag(state, flagId);
}

void setStoryFlag(StoryState& state, uint16_t flagId)
{
    setStorySaveFlag(state, flagId, true);
}

void clearStoryFlag(StoryState& state, uint16_t flagId)
{
    setStorySaveFlag(state, flagId, false);
}

bool npcBaseSelectorSkipsCountdown(int selector)
{
    switch(selector)
    {
    case 0x140:
    case 0x44:
    case 0x9e:
    case 0x35:
    case 0x9f:
    case 0x6b:
    case 0x39:
    case 0x4a:
    case 0x4d:
    case 0x3f:
    case 0x6e:
    case 0x4c:
        return true;
    default:
        return false;
    }
}

void markNpcBaseSelectorUnavailable(StoryState& state, int selector)
{
    if(selector < 0 || selector >= STORY_NPC_BASE_SELECTOR_COUNT ||
       (state.gameStateFlags & 0x2u) != 0)
        return;

    state.npcBaseSelectorCountdown[selector] =
        npcBaseSelectorSkipsCountdown(selector) ? 0 : 3;
}

void tickNpcBaseSelectorCountdowns(StoryState& state)
{
    for(int selector = 0; selector < STORY_NPC_BASE_SELECTOR_COUNT;
        ++selector)
    {
        if(state.npcBaseSelectorCountdown[selector] == 0)
            continue;

        --state.npcBaseSelectorCountdown[selector];
    }
}

bool insideStoryRectangle(float x, float z,
                          float minimumX, float maximumX,
                          float minimumZ, float maximumZ)
{
    return minimumX <= x && x <= maximumX &&
           minimumZ <= z && z <= maximumZ;
}

void activateStoryTask(StoryProgressionTask& task, int state, int mode)
{
    task.active = 1;
    task.state = static_cast<int16_t>(state);
    task.mode = static_cast<int16_t>(mode);
    task.work = 0;
    task.elapsedSeconds = 0.0f;
}

void setStoryPromptPhase(StoryState& state, int phase)
{
    StoryProgressionRuntime& runtime = state.progression;
    runtime.promptPhase = static_cast<int16_t>(phase);
    runtime.promptPhaseSeenMask |= 1u << (phase & 31);
    if(phase != 1 || storyFlag(state, 0x03e8))
        runtime.promptStep = static_cast<int16_t>(phase * 3 - 2);
    else
        runtime.promptStep = 0;

    setStoryFlag(state, static_cast<uint16_t>(0x0558 + phase));
}

void queueStoryRoute(StoryState& state, int eventId,
                     int routeMode, int destinationStageId,
                     uint32_t actionFlags =
                         STORY_TRANSITION_ACTION_QUEUED_ROUTE)
{
    StoryStageTransition& transition = state.stageTransition;
    const uint8_t postBattleFlagsApplied =
        transition.postBattleFlagsApplied;
    memset(&transition, 0, sizeof(transition));
    transition.postBattleFlagsApplied = postBattleFlagsApplied;
    transition.actionFlags = actionFlags;
    transition.resultStatus = 0;
    transition.queuedEventId = static_cast<uint16_t>(eventId);
    transition.routeMode = static_cast<uint16_t>(routeMode);
    transition.destinationStageId =
        static_cast<uint16_t>(destinationStageId);
    transition.currentStageId = state.stageId;
    transition.actorProfileId = 0xffff;
    transition.arrivalSelector = -1;
    transition.lifecycle = STORY_TRANSITION_EVENT_QUEUED;
}

void setStoryTransitionTransform(StoryStageTransition& transition,
                                 float heading,
                                 float x, float y, float z)
{
    transition.actionFlags |= STORY_TRANSITION_ACTION_EXPLICIT_TRANSFORM;
    transition.headingDegrees = heading;
    transition.arrival[0] = x;
    transition.arrival[1] = y;
    transition.arrival[2] = z;
    transition.authoredTransformCount = 1;
    transition.actorTransforms[0].headingDegrees = heading;
    transition.actorTransforms[0].position[0] = x;
    transition.actorTransforms[0].position[1] = y;
    transition.actorTransforms[0].position[2] = z;
}

void setStoryTransitionSecondTransform(StoryStageTransition& transition,
                                       float heading,
                                       float x, float y, float z)
{
    transition.authoredTransformCount = 2;
    transition.actorTransforms[1].headingDegrees = heading;
    transition.actorTransforms[1].position[0] = x;
    transition.actorTransforms[1].position[1] = y;
    transition.actorTransforms[1].position[2] = z;
}

void requestStoryFullHeal(StoryState& state, uint16_t id,
                          bool resourceProfile)
{
    StoryProgressionRuntime& runtime = state.progression;
    if(runtime.fullHealCount >= 8)
        return;
    const uint8_t index = runtime.fullHealCount++;
    runtime.fullHealIds[index] = id;
    if(resourceProfile)
        runtime.fullHealResourceProfileMask |=
            static_cast<uint8_t>(1u << index);
}

void callStoryMessageHelper00395e30(StoryState& state)
{
    ++state.progression.messageHelper00395e30Count;
    setStoryProgressionMessage(state, 0, false);
}

bool isStoryGeneric0775Profile(uint16_t resourceProfileId)
{
    return resourceProfileId == 0x0158 ||
           resourceProfileId == 0x00c7 ||
           resourceProfileId == 0x0157;
}

void clearStoryTrackedScratch(StoryState& state)
{

    state.progression.scratch005f3d60 = 0;
    state.progression.scratch005f3d64 = 0;
    state.progression.globalSlots[0x14] = 0;
    state.progression.globalSlots[0x15] = 0;
}

void applyStoryPostBattleTable(StoryState& state,
                               const StoryFlowPostBattleRow* rows,
                               uint32_t& rowMask,
                               int16_t& completedCount,
                               uint16_t characterId,
                               bool encounterMode,
                               bool chapter5)
{
    for(int i = 0; i < STORY_FLOW_POST_BATTLE_ROW_COUNT; ++i)
    {
        const StoryFlowPostBattleRow& row = rows[i];
        if(row.characterId != characterId)
            continue;

        const bool hadPrimary = storyFlag(state, row.primaryFlag);
        if(!hadPrimary)
        {
            if(encounterMode)
                setStoryFlag(state, row.primaryFlag);
            const uint32_t bit = 1u << i;
            if((rowMask & bit) == 0)
            {
                rowMask |= bit;
                ++completedCount;
            }
        }

        if(chapter5)
        {
            const StoryCharacterState& character =
                state.characters[characterId < STORY_CHARACTER_COUNT ?
                    characterId : 0];
            if((!encounterMode || character.persistentStatus == 1) &&
               !storyFlag(state, row.resultFlag))
            {
                setStoryFlag(state, row.resultFlag);
                targetListMan(state, 0, row.targetListIndex);
            }
        }
        else
        {
            if(!encounterMode)
                setStoryFlag(state, row.resultFlag);
            if(!hadPrimary)
                targetListMan(state, 0, row.targetListIndex);
        }
    }
}

}

void
beginStoryEventSelection(StoryState& state, int eventId)
{

    if(eventId == 100 || eventId == 101)
        state.eventSelectorSecondary = eventId;
    else
        state.eventSelectorPrimary = eventId;
}

void
endStoryEventSelection(StoryState& state, int eventId)
{

    if(eventId == 100 || eventId == 101)
        state.eventSelectorSecondary = -1;
    else
        state.eventSelectorPrimary = -1;
}

bool
storyNpcBaseSelectorAvailable(const StoryState& state, int16_t selector)
{

    return selector >= 0 && selector < STORY_NPC_BASE_SELECTOR_COUNT &&
        state.npcBaseSelectorCountdown[selector] == 0;
}

namespace
{

bool
storyEventNpcSelectorAvailable(const StoryState& state, int16_t selector)
{

    for(int i = 0; i < STORY_NPC_HIDE_SLOT_COUNT; ++i)
    {
        const StoryNpcSlot& slot = state.npcSlots[i];
        const int16_t slotSelector = slot.active != 0 ?
            static_cast<int16_t>(slot.actorId) : 0;
        if(slotSelector == selector)
            return slot.option == 0;
    }

    return true;
}

bool
storyNpcSelectorIsActive(int16_t selector,
                         const int16_t activeRecordIds[3])
{
    return activeRecordIds != NULL &&
        (selector == activeRecordIds[0] ||
         selector == activeRecordIds[1] ||
         selector == activeRecordIds[2]);
}

}

StoryNpcOverlayDecision
storyNpcOverlayDecision(StoryState& state, int16_t selector,
                        bool indexedPersistentValue6,
                        bool suppressPostBattleOverlays,
                        const int16_t activeRecordIds[3],
                        StoryPersistentRecordQuery recordQuery,
                        void* recordUserData, int16_t* resolvedSelector)
{
    if(resolvedSelector != NULL)
        *resolvedSelector = selector;

    if(indexedPersistentValue6)
    {
        int16_t resolved = selector;
        if(selector == 0x11e || selector == 0x11f)
        {

            const int target = selector == 0x11e ? 0 : 1;
            resolved = state.progression.hudRadar
                .specialRelationshipTargets[target].characterId;

            PersistentGameRecord* const record = recordQuery != NULL ?
                recordQuery(resolved, recordUserData) : NULL;
            if(record != NULL)
                record->relationshipState = 1;
            if(resolved >= 0 && resolved < STORY_CHARACTER_COUNT)
                state.characters[resolved].relationshipState = 1;
        }

        if(resolved < 0 ||
           storyNpcSelectorIsActive(resolved, activeRecordIds))
            return STORY_NPC_OVERLAY_CLEAR;

        if(resolvedSelector != NULL)
            *resolvedSelector = resolved;
        return STORY_NPC_OVERLAY_COPY;
    }

    if(!suppressPostBattleOverlays &&
       !storyNpcBaseSelectorAvailable(state, selector))
        return STORY_NPC_OVERLAY_KEEP;

    if(selector < STORY_CHARACTER_COUNT)
    {

        PersistentGameRecord* const record = recordQuery != NULL ?
            recordQuery(selector, recordUserData) : NULL;
        if(record == NULL || record->currentHealth <= 0 ||
           storyNpcSelectorIsActive(selector, activeRecordIds))
            return STORY_NPC_OVERLAY_KEEP;

        if((record->characterFlags & 1u) == 0)
            return STORY_NPC_OVERLAY_COPY;

        if(storyEffectiveEventId(state) != 0 &&
           !storyEventNpcSelectorAvailable(state, selector))
            return STORY_NPC_OVERLAY_KEEP;
        return STORY_NPC_OVERLAY_COPY;
    }

    if(storyEffectiveEventId(state) != 0 &&
       !storyEventNpcSelectorAvailable(state, selector))
        return STORY_NPC_OVERLAY_KEEP;
    return STORY_NPC_OVERLAY_COPY;
}

bool
storySceneDefersProgression(uint16_t sceneId)
{

    for(int i = 0; i < STORY_FLOW_DEFERRED_SCENE_COUNT; ++i)
    {
        if(STORY_FLOW_DEFERRED_SCENE_IDS[i] == sceneId)
            return true;
    }
    return false;
}

bool
storyMoveStageUsesRegister(uint16_t scriptId)
{

    for(int i = 0; i < STORY_FLOW_REGISTER_OVERRIDE_SCRIPT_COUNT; ++i)
    {
        if(STORY_FLOW_REGISTER_OVERRIDE_SCRIPT_IDS[i] == scriptId)
            return true;
    }
    return false;
}

int
storyEffectiveEventId(const StoryState& state)
{
    int selected = state.eventSelectorPrimary;
    if(selected < 0)
        selected = state.eventSelectorSecondary;
    return selected < 0 ? 0 : selected;
}

int
storyAlternateOverlayMode(const StoryState& state)
{
    int selected = state.eventSelectorPrimary;
    if(state.eventSelectorSecondary > 0)
        selected = state.eventSelectorSecondary;
    return selected < 0 ? 0 : selected;
}

int
storyTransitionModeValue(const StoryState& state)
{
    const int eventId = storyEffectiveEventId(state);
    return storySceneDefersProgression(static_cast<uint16_t>(eventId)) ? 1 : 0;
}

int16_t
storyNpcBeforeRoot(const StoryState& state, uint16_t currentStageId)
{

    int16_t root = -1;
    for(int i = 0; i < STORY_NPC_BEFORE_RECORD_COUNT; ++i)
    {
        const StoryNpcBeforeRecord& record = state.npcBeforeRecords[i];
        if(record.stageId == 0 && record.transitionMode == 0)
            continue;
        const uint16_t transitionMode =
            state.stageTransition.destinationStageId;
        int matched = 0;
        if(record.transitionMode == 0 ||
           transitionMode == record.transitionMode)
            matched = 1;
        if(record.stageId == 0 || currentStageId == record.stageId)
            matched += 2;
        if(matched != 3)
            continue;
        root = record.root != 0 ? record.root : -1;
    }
    return root;
}

namespace
{

uint32_t drawStoryOverlayRandom(StoryRandomSource randomSource,
                                void* randomUserData)
{
    return randomSource == NULL ? 0 : randomSource(randomUserData);
}

bool storyOverlayThresholdRoll(const StoryState& state,
                               StoryRandomSource randomSource,
                               void* randomUserData)
{
    int threshold = 0;
    if(state.progression.promptPhase >= 1 &&
       state.progression.promptPhase <= 4)
        threshold = 40;
    else if(state.progression.promptPhase >= 5 &&
            state.progression.promptPhase <= 7)
        threshold = 60;
    return static_cast<int>(
               drawStoryOverlayRandom(randomSource, randomUserData) % 100u) <
           threshold;
}

bool storyRollOverlayState(StoryState& state,
                           StoryRandomSource randomSource,
                           void* randomUserData)
{
    state.storyOverlayState = 0;
    if(state.storyOverlayRoot2Eligibility >= 100 &&
       storyOverlayThresholdRoll(state, randomSource, randomUserData))
    {

        if(state.storyOverlayRoot2Blocked == 0)
        {
            state.storyOverlayState = 2;
            return true;
        }
        return false;
    }
    if(state.storyOverlayRoot1Eligibility >= 100 &&
       storyOverlayThresholdRoll(state, randomSource, randomUserData))
    {
        state.storyOverlayState = 1;
        return true;
    }
    return false;
}

bool storyStageEligible(const StoryState& state, int16_t stageArgument)
{
    if(stageArgument < 1 || stageArgument > 11)
        return false;

    const uint16_t destination = state.stageTransition.destinationStageId;
    switch(state.progression.phase)
    {
    case STORY_PROGRESSION_PHASE_RECRUITMENT:
        return (state.storyStageEligibilityFlags & 0x2u) == 0 &&
               destination == 1;
    case STORY_PROGRESSION_PHASE_CHAPTER_1:
        return getStorySaveFlag(state, 0x03fa) && destination == 0x2f;
    case STORY_PROGRESSION_PHASE_CHAPTER_3:
        return destination == 0x13 || destination == 0x14 ||
               destination == 0x2f;
    case STORY_PROGRESSION_PHASE_CHAPTER_4:
        return true;
    case STORY_PROGRESSION_PHASE_CHAPTER_5:
        return (!getStorySaveFlag(state, 0x1f54) && destination == 7) ||
               destination == 0x0d;
    case STORY_PROGRESSION_PHASE_CHAPTER_6:
        return !getStorySaveFlag(state, 0x1f54) &&
               (destination == 7 || destination == 0x13);
    case STORY_PROGRESSION_PHASE_CHAPTER_7:
        return !getStorySaveFlag(state, 0x1f54) && destination == 7;
    default:
        return false;
    }
}

bool storyTransitionOverlayPair(StoryState& state,
                                bool rollSucceeded,
                                StoryRandomSource randomSource,
                                void* randomUserData)
{
    const uint16_t destination = state.stageTransition.destinationStageId;
    const uint16_t source = state.stageTransition.currentStageId;
    uint16_t firstFlag = 0;
    uint16_t secondFlag = 0;
    bool chooseVariant = false;

    if(destination == 1 && source == 0x2f)
        firstFlag = 0x03d4;
    else if(destination == 1 && source == 7)
        firstFlag = 0x03d5;
    else if(destination == 0x0d && source == 7)
        firstFlag = 0x03d7;
    else if(destination == 0x13 && source == 1)
    {
        firstFlag = 0x03d8;
        secondFlag = 0x03d9;
        chooseVariant = true;
    }
    else if(destination == 0x13 && source == 0x27)
    {
        firstFlag = 0x03da;
        secondFlag = 0x03db;
        chooseVariant = true;
    }
    else if(destination == 0x2f && source == 1)
    {
        firstFlag = 0x03dc;
        secondFlag = 0x03dd;
        chooseVariant = true;
    }
    else if(destination == 0x2f && source == 0x25)
    {
        firstFlag = 0x03de;
        secondFlag = 0x03df;
        chooseVariant = true;
    }
    else
        return false;

    if(rollSucceeded)
    {
        uint16_t selectedFlag = firstFlag;
        if(chooseVariant &&
           (drawStoryOverlayRandom(randomSource, randomUserData) & 1u) == 0)
            selectedFlag = secondFlag;
        setStorySaveFlag(state, selectedFlag, true);
        return true;
    }
    return getStorySaveFlag(state, firstFlag) ||
           (secondFlag != 0 && getStorySaveFlag(state, secondFlag));
}

int16_t storyDebouncedNpcBattleRoot(StoryState& state, uint32_t tick)
{
    const int32_t elapsed =
        static_cast<int32_t>(tick - state.npcBattleRootDebounceTick);
    if(elapsed < 6)
        return 0;
    state.npcBattleRootDebounceTick = tick;
    return 2;
}

}

bool
storyResolveOverlayState(StoryState& state,
                         StoryRandomSource randomSource,
                         void* randomUserData)
{

    static const uint16_t routeFlags[] =
    {
        0x03dc, 0x03dd, 0x03de, 0x03df, 0x03d8, 0x03d9,
        0x03da, 0x03db, 0x03d7, 0x03d4, 0x03d5
    };
    for(size_t i = 0; i < sizeof(routeFlags) / sizeof(routeFlags[0]); ++i)
        setStorySaveFlag(state, routeFlags[i], false);

    if(state.storyOverlayGlobalCondition11 == 0 ||
       state.gameplayEventId != 0 ||
       state.battleContextFlowActive != 0 ||
       state.battleLetterboxFlowActive != 0 ||
       storyStageEligible(state, 3))
        return false;

    const bool oneShotWasPrimed = state.scratch005f2220 != 0;
    state.scratch005f2220 = 1;
    if(!oneShotWasPrimed ||
       state.storyOverlayScriptLoadActive != 0 ||
       state.progression.phase >= STORY_PROGRESSION_PHASE_FINALE)
        return false;

    const bool rollSucceeded =
        storyRollOverlayState(state, randomSource, randomUserData);
    return storyTransitionOverlayPair(
        state, rollSucceeded, randomSource, randomUserData);
}

void
storyResolveNpcBattleRoot(StoryState& state,
                          uint16_t currentStageId,
                          uint32_t tick,
                          bool activePartyBattleFlag,
                          bool indexedValue6Suppressor,
                          StoryRandomSource randomSource,
                          void* randomUserData,
                          int16_t* battleRoot,
                          bool* suppressPostBattleOverlays)
{
    int16_t root = 0;
    bool suppress = false;
    if(storyResolveOverlayState(state, randomSource, randomUserData))
    {

        if(state.storyOverlayState == 1 || state.storyOverlayState == 2)
        {
            root = static_cast<int16_t>(state.storyOverlayState);
            suppress = true;
        }
    }
    else
    {

        if(currentStageId == 37 || currentStageId == 38)
            root = storyDebouncedNpcBattleRoot(state, tick);
        else if(currentStageId == 30 ||
                currentStageId == 39 ||
                currentStageId == 7)
        {
            root = activePartyBattleFlag ?
                storyDebouncedNpcBattleRoot(state, tick) : 1;
        }

        if(state.battleContextFlowActive != 0 ||
           state.battleLetterboxFlowActive != 0 ||
           indexedValue6Suppressor)
            root = 0;
    }

    if(!suppress &&
       state.progression.phase > STORY_PROGRESSION_PHASE_CHAPTER_7 &&
       !getStorySaveFlag(state, 0x1f50))
        root = 3;

    if(battleRoot != NULL)
        *battleRoot = root;
    if(suppressPostBattleOverlays != NULL)
        *suppressPostBattleOverlays = suppress;
}

namespace
{

int storyRecruitmentRatingBucket(uint16_t rating)
{

    if(rating <= 20 || rating >= 256)
        return 0;
    if(rating < 61)
        return 1;
    if(rating < 101)
        return 2;
    if(rating < 141)
        return 3;
    if(rating < 191)
        return 4;
    if(rating < 241)
        return 5;
    return 6;
}

bool storyHasActiveEventKind(const StoryState& state, int8_t eventKind)
{

    return state.activeStoryEventKinds[0] == eventKind ||
           state.activeStoryEventKinds[1] == eventKind;
}

bool storyPersistentCharacterRecruited(const StoryState& state,
                                       uint16_t characterId)
{
    if(characterId >= STORY_CHARACTER_COUNT)
        return false;

    return (state.characters[characterId].characterFlags &
            STORY_CHARACTER_RECRUITED) != 0;
}

}

bool
canRecruitStoryCharacter(const StoryState& state, uint16_t phase,
                         uint16_t characterId)
{
    const int profile = state.recruitmentPlayerResourceProfile;
    const uint16_t destination = state.stageTransition.destinationStageId;
    switch(characterId)
    {
    case 0x44:

        if(storyRecruitmentRatingBucket(state.recruitmentRatings[2]) <= 1 ||
           storyRecruitmentRatingBucket(state.recruitmentRatings[1]) <= 1)
            return false;
        if(profile != 0x44 && destination == 0x1f &&
           !getStorySaveFlag(state, 0x1f42))
            return true;
        return getStorySaveFlag(state, 0x05f2) &&
               destination == 0x1f &&
               !storyPersistentCharacterRecruited(state, 0x44);
    case 0x6c:

        if(storyRecruitmentRatingBucket(state.recruitmentRatings[2]) <= 1 ||
           storyRecruitmentRatingBucket(state.recruitmentRatings[0]) <= 1)
            return false;
        if(profile != 0x6c && destination == 0x11 &&
           !getStorySaveFlag(state, 0x1f41))
            return true;
        return getStorySaveFlag(state, 0x05f6) &&
               destination == 0x11 &&
               !storyPersistentCharacterRecruited(state, 0x6c);
    case 0x6b:
    {

        const bool routePrerequisite =
            (getStorySaveFlag(state, 0x03fd) &&
             getStorySaveFlag(state, 0x03fe)) ||
            getStorySaveFlag(state, 0x03ff);
        if(phase == STORY_PROGRESSION_PHASE_CHAPTER_4 ||
           (profile == 0x35 && !getStorySaveFlag(state, 0x1f52)) ||
           storyHasActiveEventKind(state, 4) ||
           storyRecruitmentRatingBucket(state.recruitmentRatings[2]) <= 1 ||
           getStorySaveFlag(state, 0x04ce) || !routePrerequisite)
            return false;

        if(profile != 0x6b && destination == 4 &&
           !getStorySaveFlag(state, 0x1f40))
            return true;
        return getStorySaveFlag(state, 0x05f1) && destination == 4 &&
               !storyPersistentCharacterRecruited(state, 0x6b);
    }
    case 0x35:
    {

        const bool routePrerequisite =
            (getStorySaveFlag(state, 0x03fd) &&
             getStorySaveFlag(state, 0x03fe)) ||
            getStorySaveFlag(state, 0x03ff);
        if(destination != 7 || storyHasActiveEventKind(state, 7) ||
           storyRecruitmentRatingBucket(state.recruitmentRatings[0]) <= 1 ||
           getStorySaveFlag(state, 0x04ce) || !routePrerequisite)
            return false;
        const bool retryRoute =
            profile == 0x35 ||
            (getStorySaveFlag(state, 0x1f3f) &&
             getStorySaveFlag(state, 0x1f54));

        if(!retryRoute)
            return true;
        return getStorySaveFlag(state, 0x05f4) &&
               !storyPersistentCharacterRecruited(state, 0x35);
    }
    case 0x16:

        if(state.stageId != 0x33 ||
           storyRecruitmentRatingBucket(state.recruitmentRatings[1]) <= 1)
            return false;
        if(profile != 0x16 && !getStorySaveFlag(state, 0x1f3e))
            return true;
        return getStorySaveFlag(state, 0x05f5) &&
               !storyPersistentCharacterRecruited(state, 0x16);
    default:

        return false;
    }
}

int16_t
storyRecruitmentState(const StoryState& state, uint16_t phase,
                      uint16_t characterId)
{

    int16_t result =
        canRecruitStoryCharacter(state, phase, characterId) ? 1 : 0;
    if(result != 0 && characterId == 0x35)
    {

        if(phase == STORY_PROGRESSION_PHASE_CHAPTER_6 &&
           !getStorySaveFlag(state, 0x0724) &&
           !getStorySaveFlag(state, 0x0757))
            return 0;
        if(getStorySaveFlag(state, 0x1f3f))
            result = 2;
    }
    else if(result != 0 && characterId == 0x44 &&
            state.recruitmentPlayerResourceProfile == 0x44)
    {

        result = 3;
    }
    if(result == 0 && characterId == 0x44 &&
       state.stageTransition.destinationStageId == 0x1f &&
       !getStorySaveFlag(state, 0x1f42))
    {

        result = 3;
    }
    return result;
}

void
selectStoryStageArrival(uint16_t destinationStageId,
                        uint16_t sceneId,
                        float& headingDegrees,
                        float arrival[3])
{

    int index = 0;
    headingDegrees = 96.1f;
    switch(destinationStageId)
    {
    case 0x2f: index = 1; break;
    case 0x19: index = 2; headingDegrees = 117.9f; break;
    case 0x2a: index = 3; break;
    case 0x0c: index = 4; headingDegrees = 0.0f; break;
    case 0x15: index = 5; headingDegrees = 58.8f; break;
    case 0x0f: index = 6; headingDegrees = 7.4f; break;
    case 0x07: index = 7; break;
    case 0x1d: index = 8; break;
    case 0x01:
        if(sceneId == 0x07)
        {
            index = 10;
            headingDegrees = 17.0f;
        }
        else
        {
            index = 9;
            headingDegrees = -80.0f;
        }
        break;
    case 0x1a:
        if(sceneId == 0x92)
        {
            index = 11;
            headingDegrees = 2.1f;
        }
        else
        {
            index = 12;
            headingDegrees = -86.7f;
        }
        break;
    default:
        break;
    }

    arrival[0] = STORY_FLOW_ARRIVAL_VECTORS[index].x;
    arrival[1] = STORY_FLOW_ARRIVAL_VECTORS[index].y;
    arrival[2] = STORY_FLOW_ARRIVAL_VECTORS[index].z;
}

void
queueStoryStageTransition(StoryState& state,
                          uint16_t destinationStageId,
                          uint16_t sourceScriptId,
                          int16_t arrivalSelector)
{
    StoryStageTransition& transition = state.stageTransition;
    const bool routedEvent =
        transition.lifecycle == STORY_TRANSITION_EVENT_QUEUED ||
        transition.lifecycle == STORY_TRANSITION_EVENT_RUNNING;
    if(!routedEvent)
    {
        memset(&transition, 0, sizeof(transition));
        transition.queuedEventId = 0xffff;
        transition.actorProfileId = 0xffff;
        transition.arrivalSelector = -1;
    }

    transition.currentStageId = state.stageId;
    transition.destinationStageId = destinationStageId;
    transition.sourceScriptId = sourceScriptId;
    transition.arrivalSelector = arrivalSelector;
    transition.elapsedSeconds = 0.0f;
    transition.postBattleState = 0;
    transition.loadState = STORY_STAGE_LOAD_OL_WEAPON;
    if((transition.actionFlags &
        STORY_TRANSITION_ACTION_EXPLICIT_TRANSFORM) == 0)
    {
        transition.authoredTransformCount = 0;
        selectStoryStageArrival(destinationStageId, sourceScriptId,
                                transition.headingDegrees,
                                transition.arrival);
    }
    transition.lifecycle = state.resultDisplayActive ?
        STORY_TRANSITION_POST_BATTLE : STORY_TRANSITION_FIELD_UNLOAD;
    state.eventStageId = destinationStageId;
    state.stageTransitionPending = 1;
}

void
targetListMan(StoryState& state, int listType, int index)
{

    if(listType < 0 || listType > 1 || index < 0 || index > 23)
    {
        listType = 0;
        index = 0;
    }
    state.progression.targetLists[listType][index] = 0xc9;
}

void
setStoryProgressionMessage(StoryState& state, int messageId,
                           bool pauseBattleSlots)
{
    StoryProgressionRuntime& runtime = state.progression;
    if(messageId < 0 || messageId >= STORY_FLOW_MESSAGE_COUNT)
        messageId = 0;

    runtime.messageId = static_cast<int16_t>(messageId);
    uint16_t stageId = STORY_FLOW_MESSAGES[messageId].stageId;
    uint16_t textId = STORY_FLOW_MESSAGES[messageId].messageId;
    if(messageId == 15)
    {
        stageId = static_cast<uint16_t>(runtime.dynamicMessageStageId);
        textId = static_cast<uint16_t>(runtime.dynamicMessageId);
    }
    else if(messageId == 16)
    {
        stageId = static_cast<uint16_t>(runtime.objectiveStageId);
        textId = 0x004a;
    }
    runtime.messageStageId = static_cast<int16_t>(stageId);
    runtime.messageTextId = static_cast<int16_t>(textId);
    runtime.messageActive = messageId > 0;
    setStoryHudObjective(
        state, static_cast<int16_t>(textId),
        static_cast<int16_t>(stageId), false);
    if(pauseBattleSlots)
        runtime.controlFlags |= 0x00000200u;
}

void
startStoryIntroFade(StoryState& state, int mode)
{

    StoryProgressionTask& task = state.progression.introFadeTask;
    activateStoryTask(task, 0, mode);
    switch(mode)
    {
    case 4: task.work = 3; break;
    case 5:
    case 6:
    case 7: task.work = 1; break;
    case 8: task.work = 10000; break;
    default: task.work = 0; break;
    }
}

void
startStoryDialogueGate(StoryState& state, int mode,
                       const StoryProgressionInput& input)
{

    bool accepted = false;
    switch(mode)
    {
    case 1:
        setStoryFlag(state, 0x04ee);
        clearStoryFlag(state, 0x034c);
        accepted = true;
        break;
    case 3:
        if(storyFlag(state, 0x040b))
        {
            setStoryFlag(state, 0x04ef);
            state.progression.promptStep = 5;
            accepted = true;
        }
        break;
    case 4:
        setStoryFlag(state, 0x04f5);
        accepted = true;
        break;
    case 5:
        if(state.progression.scratch005f3d28 != 0 &&
           insideStoryRectangle(input.playerX, input.playerZ,
                                -108.1f, 100.6f, 105.5f, 131.7f))
        {
            setStoryFlag(state, 0x04e8);
            accepted = true;
        }
        break;
    case 6:
        if(state.progression.scratch005f3d28 != 0 &&
           insideStoryRectangle(input.playerX, input.playerZ,
                                -296.2f, 214.61f, 80.1f, 105.4f))
        {
            setStoryFlag(state, 0x04e7);
            accepted = true;
        }
        break;
    case 7:
        if(input.currentStageId == 0x2b &&
           insideStoryRectangle(input.playerX, input.playerZ,
                                -33.9f, 38.5f, -14.0f, 4.4f))
        {
            setStoryFlag(state, 0x04f9);
            accepted = true;
        }
        break;
    case 8:
        if(storyFlag(state, 0x0fa6) &&
           insideStoryRectangle(input.playerX, input.playerZ,
                                -39.5f, -17.2f, -68.9f, 99.7f))
        {
            setStoryFlag(state, 0x0500);
            accepted = true;
        }
        break;
    case 9:
        setStoryFlag(state, 0x0502);
        accepted = true;
        break;
    case 10:
        if(insideStoryRectangle(input.playerX, input.playerZ,
                                14.4f, 37.4f, -30.0f, 19.2f))
        {
            setStoryFlag(state, 0x0504);
            accepted = true;
        }
        break;
    case 11:
        setStoryFlag(state, 0x04f2);
        accepted = true;
        break;
    case 12:
        setStoryFlag(state, 0x0526);
        accepted = true;
        break;
    default:
        break;
    }

    if(accepted)
        activateStoryTask(state.progression.dialogueTask, 1, mode);
}

void
refreshStoryProgressionFlags(StoryState& state, bool clearOnExit)
{

    const StoryProgressionPhase phase = state.progression.phase;
    const uint16_t destination = state.eventStageId;
    switch(phase)
    {
    case STORY_PROGRESSION_PHASE_PROLOGUE:
        if(storyFlag(state, 0x0f43))
            setStoryFlag(state, 0x0331);
        else
        {
            if(destination == 0x0d)
                setStoryFlag(state, 0x02be);
            clearStoryFlag(state, 0x0331);
        }
        if(!clearOnExit)
        {
            clearStoryFlag(state, 0x0331);
            clearStoryFlag(state, 0x02be);
        }
        break;
    case STORY_PROGRESSION_PHASE_RECRUITMENT:
        if(!storyFlag(state, 0x04ed))
        {
            setStoryFlag(state, 0x04ed);
            setStoryFlag(state, 0x03e6);
        }
        if(!storyFlag(state, 0x03ec))
            setStoryFlag(state, 0x034c);
        if(destination == 0x19)
            setStoryFlag(state, 0x03d1);
        if(!clearOnExit)
        {
            clearStoryFlag(state, 0x034c);
            clearStoryFlag(state, 0x03d1);
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_1:
        if(!storyFlag(state, 0x03ec))
            setStoryFlag(state, 0x034c);
        if(!storyFlag(state, 0x0f46) && storyFlag(state, 0x0f45))
        {
            setStoryFlag(state, 0x0387);
            setStoryFlag(state, 0x03a2);
        }
        if(destination == 0x19)
            setStoryFlag(state, 0x03d1);
        if(!clearOnExit)
        {
            clearStoryFlag(state, 0x034c);
            clearStoryFlag(state, 0x0387);
            clearStoryFlag(state, 0x03a2);
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_2:
        clearStoryFlag(state, 0x03b7);
        clearStoryFlag(state, 0x03b8);
        clearStoryFlag(state, 0x03b9);
        clearStoryFlag(state, 0x03ba);
        if(state.stageId == 0x33 && storyFlag(state, 0x0f55))
            setStoryFlag(state, 0x03b7);
        if(destination == 0x19)
        {
            setStoryFlag(state, 0x03d2);
            setStoryFlag(state, 0x03d3);
        }
        if(!clearOnExit)
            clearStoryFlag(state, 0x03b7);
        break;
    case STORY_PROGRESSION_PHASE_INTERLUDE:
        if(!storyFlag(state, 0x0407))
            setStoryFlag(state, 0x034c);
        if(destination == 0x19)
        {
            setStoryFlag(state, 0x03d2);
            setStoryFlag(state, 0x03d3);
        }
        if(!clearOnExit)
            clearStoryFlag(state, 0x034c);
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_3:
        if(!storyFlag(state, 0x040b))
            setStoryFlag(state, 0x034c);
        if(storyFlag(state, 0x0415) || storyFlag(state, 0x06f0))
        {
            clearStoryFlag(state, 0x0341);
            clearStoryFlag(state, 0x03c7);
        }
        if(storyFlag(state, 0x04e0) || storyFlag(state, 0x0411))
        {
            clearStoryFlag(state, 0x0345);
            clearStoryFlag(state, 0x0346);
            clearStoryFlag(state, 0x0347);
        }
        if(!clearOnExit)
        {
            clearStoryFlag(state, 0x034c);
            clearStoryFlag(state, 0x034d);
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_4:
        if(!storyFlag(state, 0x0453))
            setStoryFlag(state, 0x034c);
        if(storyFlag(state, 0x0f48))
        {
            setStoryFlag(state, 0x0328);
            setStoryFlag(state, 0x0329);
        }
        else
        {
            setStoryFlag(state, 0x03a6);
            setStoryFlag(state, 0x03a7);
            setStoryFlag(state, 0x03a8);
        }
        setStoryFlag(state, 0x039b);
        clearStoryFlag(state, 0x0386);
        clearStoryFlag(state, 0x037f);
        if(destination == 6)
            setStoryFlag(state, 0x03bf);
        if(!clearOnExit)
        {
            clearStoryFlag(state, 0x0328);
            clearStoryFlag(state, 0x0329);
            clearStoryFlag(state, 0x034c);
            clearStoryFlag(state, 0x039b);
            clearStoryFlag(state, 0x03a6);
            clearStoryFlag(state, 0x03a7);
            clearStoryFlag(state, 0x03a8);
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_5:
        if(!storyFlag(state, 0x0460))
            setStoryFlag(state, 0x034c);
        if(storyFlag(state, 0x0779) || storyFlag(state, 0x052c))
        {
            setStoryFlag(state, 0x03bb);
            setStoryFlag(state, 0x0390);
        }
        if(storyFlag(state, 0x0477))
            clearStoryFlag(state, 0x0334);
        if(!clearOnExit)
        {
            clearStoryFlag(state, 0x034c);
            clearStoryFlag(state, 0x03bb);
            clearStoryFlag(state, 0x0390);
            setStoryFlag(state, 0x0334);
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_6:
        if(!storyFlag(state, 0x0fa7))
            setStoryFlag(state, 0x034c);
        if(storyFlag(state, 0x04d8) && !storyFlag(state, 0x0f4b))
            setStoryFlag(state, 0x0396);
        if(storyFlag(state, 0x03f2))
            setStoryFlag(state, 0x039a);
        if(!clearOnExit)
        {
            clearStoryFlag(state, 0x034c);
            clearStoryFlag(state, 0x0396);
            clearStoryFlag(state, 0x039a);
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_7:
        if(!storyFlag(state, 0x049c))
            setStoryFlag(state, 0x034c);
        if(storyFlag(state, 0x0fa6))
        {
            clearStoryFlag(state, 0x035c);
            targetListMan(state, 0, 0);
        }
        if(!clearOnExit)
        {
            clearStoryFlag(state, 0x034c);
            setStoryFlag(state, 0x035c);
        }
        break;
    case STORY_PROGRESSION_PHASE_FINALE:
        if(storyFlag(state, 0x0f4e))
        {
            clearStoryFlag(state, 0x037e);
            clearStoryFlag(state, 0x037f);
            clearStoryFlag(state, 0x0386);
            setStoryFlag(state, 0x034c);
            setStoryFlag(state, 0x039b);
        }
        if(storyFlag(state, 0x04e3))
        {
            clearStoryFlag(state, 0x034c);
            setStoryFlag(state, 0x039c);
            setStoryFlag(state, 0x039d);
            setStoryFlag(state, 0x039e);
        }
        setStoryFlag(state, 0x03b5);
        setStoryFlag(state, 0x03be);
        if(destination == 0x19)
        {
            setStoryFlag(state, 0x03d1);
            setStoryFlag(state, 0x03d2);
            setStoryFlag(state, 0x03d3);
        }
        if(state.progression.finaleCleanupStarted)
            setStoryFlag(state, 0x037e);
        if(!clearOnExit)
        {
            clearStoryFlag(state, 0x034c);
            clearStoryFlag(state, 0x037e);
            clearStoryFlag(state, 0x039b);
            clearStoryFlag(state, 0x039c);
            clearStoryFlag(state, 0x039d);
            clearStoryFlag(state, 0x039e);
            clearStoryFlag(state, 0x03b5);
        }
        break;
    default:
        break;
    }
}

int
refreshStoryProgressionPrompt(StoryState& state)
{

    StoryProgressionRuntime& runtime = state.progression;

    state.recruitmentMode = 0;
    runtime.chapter3RowMask = 0;
    runtime.chapter5RowMask = 0;
    runtime.chapter6RowMask = 0;
    runtime.chapter3TargetCount = 0;
    runtime.chapter5ObjectiveCount = 0;
    runtime.chapter6TargetCount = 0;
    runtime.messageActive = 0;
    runtime.messageId = 0;
    runtime.promptPhase = 0;
    runtime.promptStep = 0;

    switch(runtime.phase)
    {
    case STORY_PROGRESSION_PHASE_RECRUITMENT:
    case STORY_PROGRESSION_PHASE_CHAPTER_1:
    case STORY_PROGRESSION_PHASE_CHAPTER_2:
    case STORY_PROGRESSION_PHASE_INTERLUDE:
        if(storyFlag(state, 0x0f55))
        {
            setStoryPromptPhase(state, 2);
            if(!storyFlag(state, 0x0407))
                runtime.promptStep = 4;
        }
        else
        {
            setStoryPromptPhase(state, 1);
            if(storyFlag(state, 0x03fa))
            {
                setStoryProgressionMessage(state, 3, false);
                runtime.promptStep = 2;
            }
            else if(storyFlag(state, 0x03ec))
            {
                runtime.promptStep = 1;
                setStoryProgressionMessage(state, 0, false);
            }
            else if(state.eventStageId != 0x19 &&
                    storyFlag(state, 0x03e8))
            {
                runtime.promptStep = 1;
                setStoryProgressionMessage(state, 2, false);
            }
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_3:
        setStoryPromptPhase(state, 2);
        runtime.promptStep = storyFlag(state, 0x040b) ? 5 : 4;
        for(int i = 0; i < STORY_FLOW_POST_BATTLE_ROW_COUNT; ++i)
        {
            const StoryFlowPostBattleRow& row =
                STORY_FLOW_CHAPTER_3_ROWS[i];
            if(storyFlag(state, row.primaryFlag) ||
               storyFlag(state, row.resultFlag))
            {
                runtime.chapter3RowMask |= 1u << i;
                ++runtime.chapter3TargetCount;
            }
        }
        if(storyFlag(state, 0x0442) && !storyFlag(state, 0x0f47) &&
           runtime.promptPhase == 2)
        {
            setStoryProgressionMessage(state, 4, false);
            runtime.promptStep = 6;
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_4:
        setStoryPromptPhase(state, 3);
        runtime.promptStep = 7;
        if(storyFlag(state, 0x0453))
        {
            runtime.promptStep = 8;
            if(storyFlag(state, 0x0463))
            {
                runtime.promptStep = 9;
                if(storyFlag(state, 0x0462))
                {

                    state.recruitmentMode = 3;
                    setStoryProgressionMessage(state, 6, false);
                }
                else
                {

                    for(int i = 0; i < 3; ++i)
                    {
                        if(storyFlag(state,
                                     static_cast<uint16_t>(0x0457 + i)))
                            ++state.recruitmentMode;
                        if(storyFlag(state,
                                     static_cast<uint16_t>(0x045c + i)))
                            ++state.recruitmentMode;
                    }
                }
            }
            else if(!storyFlag(state, 0x04f0) &&
                    runtime.promptPhase == 3)
                setStoryProgressionMessage(state, 5, false);
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_5:
        setStoryPromptPhase(state, 4);
        runtime.promptStep = 10;
        if(storyFlag(state, 0x0460) && runtime.promptPhase == 4)
            runtime.promptStep = 11;
        for(int i = 0; i < STORY_FLOW_POST_BATTLE_ROW_COUNT; ++i)
        {
            const StoryFlowPostBattleRow& row =
                STORY_FLOW_CHAPTER_5_ROWS[i];
            if(storyFlag(state, row.primaryFlag) ||
               storyFlag(state, row.resultFlag))
            {
                runtime.chapter5RowMask |= 1u << i;
                ++runtime.chapter5ObjectiveCount;
            }
        }
        if(!storyFlag(state, 0x0f4a) &&
           (storyFlag(state, 0x03f7) || storyFlag(state, 0x052c)))
        {
            setStoryProgressionMessage(state, 7, false);
            runtime.promptStep = 12;
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_6:
        setStoryPromptPhase(state, 5);
        runtime.promptStep = 13;
        if(storyFlag(state, 0x0f4b))
            runtime.promptStep = 15;
        else if(storyFlag(state, 0x0f72))
        {
            if(storyFlag(state, 0x0f73))
            {
                setStoryProgressionMessage(state, 9, false);
                runtime.promptStep = 15;
            }
            else
            {
                setStoryProgressionMessage(state, 13, false);
                runtime.promptStep = 14;
            }
        }
        else if(storyFlag(state, 0x0fa7))
        {
            setStoryProgressionMessage(state, 8, false);
            runtime.promptStep = 14;
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_7:
        setStoryPromptPhase(state, 6);
        runtime.promptStep = 16;
        if(!storyFlag(state, 0x049c) || storyFlag(state, 0x0f4c))
        {
            if(storyFlag(state, 0x0fa6) && !storyFlag(state, 0x0f4d))
            {
                setStoryProgressionMessage(state, 11, false);
                runtime.promptStep = 18;
            }
        }
        else
        {
            setStoryProgressionMessage(state, 10, false);
            runtime.promptStep = 17;
        }
        break;
    case STORY_PROGRESSION_PHASE_FINALE:
        setStoryPromptPhase(state, 7);
        runtime.promptStep = 19;
        if(!storyFlag(state, 0x0f7e))
            setStoryProgressionMessage(state, 2, false);
        else
        {
            setStoryProgressionMessage(state, 12, false);
            runtime.promptStep = state.eventStageId == 0x30 ? 21 : 20;
            if(state.eventStageId == 0x30)
                runtime.finaleCleanupStarted = 1;
        }
        break;
    default:
        break;
    }

    const int group = runtime.promptPhase;
    const uint32_t bit = 1u << (group & 31);
    const bool firstPresentation =
        (runtime.adviceShownMask & bit) == 0;
    runtime.adviceShownMask |= bit;
    return firstPresentation ? group : -1;
}

void
applyStoryChapter3BattleFlags(StoryState& state,
                              uint16_t characterId,
                              bool encounterMode)
{

    const bool hadPrimary = characterId == 1 &&
        storyFlag(state, STORY_FLOW_CHAPTER_3_ROWS[0].primaryFlag);
    applyStoryPostBattleTable(
        state, STORY_FLOW_CHAPTER_3_ROWS,
        state.progression.chapter3RowMask,
        state.progression.chapter3TargetCount,
        characterId, encounterMode, false);
    if(characterId == 1 && !hadPrimary)
        clearStoryFlag(state, 0x0341);
    if(state.stageId == 0x14 && encounterMode)
    {
        clearStoryFlag(state, 0x0345);
        clearStoryFlag(state, 0x0346);
        clearStoryFlag(state, 0x0347);
        setStoryFlag(state, 0x04e0);
    }
    if(state.progression.chapter3TargetCount >= 5)
        setStoryFlag(state, 0x0411);
}

void
applyStoryChapter5BattleFlags(StoryState& state,
                              uint16_t characterId,
                              bool encounterMode)
{

    bool hadPrimary = true;
    if(characterId == 4)
        hadPrimary = storyFlag(state, STORY_FLOW_CHAPTER_5_ROWS[0].primaryFlag);
    applyStoryPostBattleTable(
        state, STORY_FLOW_CHAPTER_5_ROWS,
        state.progression.chapter5RowMask,
        state.progression.chapter5ObjectiveCount,
        characterId, encounterMode, true);
    if(characterId == 4 && !hadPrimary)
    {
        setStoryFlag(state, 0x0477);
        clearStoryFlag(state, 0x0334);
    }

    int completed = 0;
    for(uint16_t flag = 0x0528; flag <= 0x052c; ++flag)
        completed += storyFlag(state, flag) ? 1 : 0;
    state.progression.globalSlots[0x0b] =
        static_cast<uint16_t>(completed);
    if(state.progression.chapter5ObjectiveCount > 4)
        setStoryFlag(state, 0x0779);
    if(state.progression.chapter5ObjectiveCount > 6)
        setStoryFlag(state, 0x0785);
}

void
applyStoryChapter6BattleFlags(StoryState& state,
                              uint16_t characterId,
                              bool encounterMode)
{

    applyStoryPostBattleTable(
        state, STORY_FLOW_CHAPTER_6_ROWS,
        state.progression.chapter6RowMask,
        state.progression.chapter6TargetCount,
        characterId, encounterMode, false);
    if(characterId == 0x30 &&
       (encounterMode || storyFlag(state, 0x0020)))
    {
        setStoryFlag(state, 0x04d8);
        setStoryFlag(state, 0x0396);
    }
}

void
applyStoryPostBattleFlags(StoryState& state, uint16_t opponentCharacterId)
{

    switch(state.progression.phase)
    {
    case STORY_PROGRESSION_PHASE_CHAPTER_6:
        if(storyFlag(state, 0x001d) || storyFlag(state, 0x0020))
            applyStoryChapter6BattleFlags(
                state, opponentCharacterId, false);
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_5:
        if(storyFlag(state, 0x0021) || storyFlag(state, 0x0020))
            applyStoryChapter5BattleFlags(
                state, opponentCharacterId, false);
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_4:
        if((storyFlag(state, 0x0021) || storyFlag(state, 0x0020)) &&
           (opponentCharacterId == 0x3a ||
            opponentCharacterId == 0x3b))
            clearStoryFlag(state, 0x0341);
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_3:
        if(storyFlag(state, 0x0021) || storyFlag(state, 0x0020))
            applyStoryChapter3BattleFlags(
                state, opponentCharacterId, false);
        break;
    default:
        break;
    }
}

void
processStoryEncounterCompletion(StoryState& state,
                                uint16_t resourceProfileId,
                                uint8_t persistentStatus,
                                bool recruited)
{

    if(state.progression.controlFlags != 0)
        return;

    bool complete = false;
    switch(state.progression.phase)
    {
    case STORY_PROGRESSION_PHASE_CHAPTER_1:
        if(resourceProfileId == 0x4d)
        {
            setStoryFlag(state, 0x0f55);
            ++state.progression.thunk001becd0Count;
            clearStoryFlag(state, 0x0387);
            clearStoryFlag(state, 0x0388);
            clearStoryFlag(state, 0x03a2);
            clearStoryFlag(state, 0x039c);
            clearStoryFlag(state, 0x039d);
            clearStoryFlag(state, 0x039e);
            clearStoryFlag(state, 0x039f);
            complete = true;
        }
        break;
    case STORY_PROGRESSION_PHASE_INTERLUDE:
        if(resourceProfileId == 0x19)
        {
            if(persistentStatus == 1)
                setStoryFlag(state, 0x040c);
            else if(recruited)
                setStoryFlag(state, 0x040d);
            targetListMan(state, 0, 3);
            callStoryMessageHelper00395e30(state);
            setStoryFlag(state, 0x0407);
            complete = true;
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_3:
        applyStoryChapter3BattleFlags(state, resourceProfileId, true);
        if(resourceProfileId == 0x3f)
        {
            clearStoryFlag(state, 0x034d);
            clearStoryFlag(state, 0x0341);
            clearStoryFlag(state, 0x0345);
            clearStoryFlag(state, 0x0346);
            clearStoryFlag(state, 0x0347);
            callStoryMessageHelper00395e30(state);
            callStoryMessageHelper00395e30(state);
            state.scratch005f24bc = 0;
            complete = true;
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_4:
        if(resourceProfileId == 0x3a || resourceProfileId == 0x3b)
        {
            setStoryFlag(state, 0x04e9);
            clearStoryFlag(state, 0x0341);
        }
        else if(resourceProfileId == 0x4a)
        {
            clearStoryFlag(state, 0x0328);
            clearStoryFlag(state, 0x0329);
            callStoryMessageHelper00395e30(state);
            complete = true;
        }
        else if(isStoryGeneric0775Profile(resourceProfileId))
            setStoryFlag(state, 0x0775);
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_5:
        applyStoryChapter5BattleFlags(state, resourceProfileId, true);
        if(state.stageId == 1 && resourceProfileId == 0x42)
            setStoryFlag(state, 0x0466);
        else if(state.stageId == 0x0d && resourceProfileId == 4)
        {
            setStoryFlag(state, 0x0477);
            clearStoryFlag(state, 0x0334);
        }
        if(resourceProfileId == 0x4b)
        {
            clearStoryFlag(state, 0x0338);
            callStoryMessageHelper00395e30(state);
            complete = true;
        }
        else if(isStoryGeneric0775Profile(resourceProfileId))
            setStoryFlag(state, 0x0775);
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_6:
        applyStoryChapter6BattleFlags(state, resourceProfileId, true);
        if(resourceProfileId == 0x6e)
        {
            requestStoryFullHeal(state, 0x6e, true);
            if(state.stageId == 0x13)
            {
                setStoryFlag(state, 0x077a);
                state.scratch005f2220 = 0;
            }
        }
        else if(resourceProfileId == 0x30)
        {
            setStoryFlag(state, 0x04d8);
            setStoryFlag(state, 0x0396);
        }
        else if(resourceProfileId == 0x39)
        {
            clearStoryFlag(state, 0x0374);
            callStoryMessageHelper00395e30(state);
            complete = true;
        }
        else if(isStoryGeneric0775Profile(resourceProfileId))
            setStoryFlag(state, 0x0775);
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_7:
        if(resourceProfileId == 0x4c)
        {
            callStoryMessageHelper00395e30(state);
            clearStoryFlag(state, 0x035e);
            clearStoryFlag(state, 0x035f);
            clearStoryFlag(state, 0x0360);
            clearStoryFlag(state, 0x0361);
            requestStoryFullHeal(state, 0x4c, true);
        }
        if(resourceProfileId == 0x3c)
        {
            clearStoryFlag(state, 0x035a);
            complete = true;
        }
        else if(isStoryGeneric0775Profile(resourceProfileId))
            setStoryFlag(state, 0x0775);
        break;
    case STORY_PROGRESSION_PHASE_FINALE:
        if(resourceProfileId == 0x4d || resourceProfileId == 0x48)
            setStoryFlag(state, 0x0f50);
        complete = false;
        if(resourceProfileId == 0x4e)
        {
            callStoryMessageHelper00395e30(state);
            complete = true;
        }
        break;
    default:
        break;
    }
    if(complete)
        state.progression.controlFlags |= 1u;
}

int
handleStoryCharacterAction(StoryState& state, uint16_t resourceProfileId)
{

    if((state.progression.controlFlags & 1u) == 0)
        return 0;

    switch(state.progression.phase)
    {
    case STORY_PROGRESSION_PHASE_CHAPTER_1:
        state.progression.controlFlags = 0;
        state.progression.phase = STORY_PROGRESSION_PHASE_CHAPTER_2;
        setStoryFlag(state, 0x03b7);
        setStoryFlag(state, 0x031e);
        activateStoryTask(state.progression.sequenceTask, 1, 0);
        return 5;
    case STORY_PROGRESSION_PHASE_INTERLUDE:
    {
        state.progression.controlFlags = 0;
        state.progression.phase = STORY_PROGRESSION_PHASE_CHAPTER_3;
        StoryActorState* player = findStoryActor(state, 0, true);
        if(player != NULL)
        {
            player->position[0] = 23.6f;
            player->position[1] = 0.0f;
            player->position[2] = -25.1f;
            player->headingDegrees = -165.03f;
        }
        return 2;
    }
    case STORY_PROGRESSION_PHASE_CHAPTER_3:
        state.progression.controlFlags = 0;
        state.progression.phase = STORY_PROGRESSION_PHASE_CHAPTER_4;
        state.progression.globalSlots[0] = resourceProfileId;
        return 5;
    case STORY_PROGRESSION_PHASE_CHAPTER_4:
        if(resourceProfileId == 0x4a && !storyFlag(state, 0x0770))
        {
            state.progression.controlFlags = 0;
            state.progression.phase = STORY_PROGRESSION_PHASE_CHAPTER_5;
            setStoryProgressionMessage(state, 0, false);
            state.progression.globalSlots[0] = resourceProfileId;
            return 5;
        }
        break;
    default:
        break;
    }
    return 0;
}

bool
applyStoryEncounterTransition(
    StoryState& state, const StoryEncounterTransitionInput& input)
{

    StoryProgressionRuntime& runtime = state.progression;
    StoryStageTransition& transition = state.stageTransition;
    const uint16_t opponent = input.opponentResourceProfileId;
    const uint16_t player = input.playerResourceProfileId;

    switch(runtime.phase)
    {
    case STORY_PROGRESSION_PHASE_PROLOGUE:
        if(opponent == 0x4b)
        {
            queueStoryRoute(state, 0x4c, 0x0d, 0x07);
            setStoryFlag(state, 0x04ec);
            return true;
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_4:
        if(opponent == 0x4a && storyFlag(state, 0x0770))
        {
            queueStoryRoute(state, 0x18, 1, 5);
            runtime.controlFlags = 0;
            runtime.phase = STORY_PROGRESSION_PHASE_CHAPTER_5;
            clearStoryFlag(state, 0x0328);
            clearStoryFlag(state, 0x0329);
            return true;
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_5:
        if(opponent == 0x4b)
        {
            queueStoryRoute(
                state, storyFlag(state, 0x0773) ? 0x1d : 0x1c,
                1, 0x19);
            runtime.controlFlags = 0;
            runtime.phase = STORY_PROGRESSION_PHASE_CHAPTER_6;
            clearStoryFlag(state, 0x03bb);
            callStoryMessageHelper00395e30(state);
            return true;
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_7:
        if(opponent == 0x3c)
        {
            queueStoryRoute(
                state, storyFlag(state, 0x0772) ? 0x29 : 0x28,
                0x20, 0x22, 0x18);
            setStoryTransitionTransform(
                transition, -92.2f, 89.1f, 0.0f, -9.8f);
            runtime.controlFlags = 0;
            runtime.phase = STORY_PROGRESSION_PHASE_FINALE;
            return true;
        }
        if(!storyFlag(state, 0x0772))
        {
            if(opponent == 0x44)
            {
                setStoryFlag(state, 0x05f2);
                transition.currentStageId = 0x20;
                transition.destinationStageId = 0x1f;
                return true;
            }
            if(player == 0x44)
            {
                setStoryFlag(state, 0x05f3);
                transition.currentStageId = 0x20;
                transition.destinationStageId = 0x1f;
                setStoryTransitionTransform(
                    transition, -74.3f, -18.8f, 10.4f, 68.5f);
                state.scratch005f3d04 = 0;
                state.gameStateFlags &= ~0x20u;
                return true;
            }
        }
        break;
    default:
        break;
    }

    if(runtime.phase == STORY_PROGRESSION_PHASE_FINALE &&
       state.scratch005f3d24 != 0)
    {
        if(opponent == 0x98 && (runtime.controlFlags & 4u) != 0)
        {
            state.scratch005f33f4 = 0;
            state.scratch005f3d24 = 1;
            state.scratch005f333c = 1;
            return true;
        }

        const bool finaleTracked =
            opponent == 0x44 || opponent == 0x16 ||
            opponent == 0x6c || opponent == 0x35 ||
            opponent == 0x6b;
        if(finaleTracked && runtime.scratch005f3d28 == 3 &&
           runtime.scratch005f3d5c == static_cast<int16_t>(opponent))
        {
            queueStoryRoute(state, 0x39, 0x31, 0x2f);
            state.scratch005f3d24 = 1;
            clearStoryFlag(state, 0x038f);
            state.scratch005f3d04 = 0;
            state.scratch005f24bc = 1;
            return true;
        }

        if(opponent == 0x4e)
        {
            const int result = runtime.scratch005f3d28;
            if(result == 4)
            {
                queueStoryRoute(state, 0x39, 0x31, 0x2f);
                clearStoryFlag(state, 0x038f);
                state.scratch005f24bc = 1;
                state.scratch005f3d24 = 1;
                state.scratch005f3d04 = 0;
                state.gameStateFlags &= ~0x20u;
                runtime.controlFlags |= 2u;

                state.phase11ModeFlags |= 2u;
                return true;
            }
            if(result == 3)
            {
                uint16_t partner = 0x6b;
                uint16_t eventId = 0x44;
                requestStoryFullHeal(state, 0x6b, true);
                switch(runtime.selectedPartnerProfileId)
                {
                case 0x44: partner = 0x44; eventId = 0x48; break;
                case 0x16: partner = 0x16; eventId = 0x47; break;
                case 0x6c: partner = 0x6c; eventId = 0x46; break;
                case 0x35: partner = 0x35; eventId = 0x45; break;
                default: break;
                }
                if(runtime.selectedPartnerProfileId >= 0)
                {
                    requestStoryFullHeal(
                        state,
                        static_cast<uint16_t>(
                            runtime.selectedPartnerProfileId),
                        true);
                    const uint16_t selected = static_cast<uint16_t>(
                        runtime.selectedPartnerProfileId);
                    if(selected < STORY_CHARACTER_COUNT)
                    {
                        state.characters[selected].characterFlags &=
                            ~STORY_CHARACTER_RECRUITED;
                    }
                }
                queueStoryRoute(state, eventId, 0x29, 0x36, 0x14);
                transition.actorProfileId = partner;
                setStoryTransitionTransform(
                    transition, 147.8f, -10.8f, 0.0f, 50.9f);
                setStoryTransitionSecondTransform(
                    transition, -32.2f, 22.4f, 0.0f, -1.3f);
                runtime.scratch005f3d58 =
                    static_cast<int16_t>(player);
                runtime.scratch005f3d5c =
                    runtime.selectedPartnerProfileId;
                state.scratch005f3d04 = 0;
                setStoryFlag(state, 0x038f);
                return true;
            }

            if(result == 0)
                setStoryFlag(state, 0x0552);
            else if(result == 1)
                setStoryFlag(state, 0x0553);
            else if(result == 2)
            {
                if(storyFlag(state, 0x0771))
                    setStoryFlag(state, 0x0554);
                else if(storyFlag(state, 0x0770))
                    setStoryFlag(state, 0x0555);
                else if(storyFlag(state, 0x0772))
                    setStoryFlag(state, 0x0556);
                else if(storyFlag(state, 0x0773))
                    setStoryFlag(state, 0x0557);
                else if(storyFlag(state, 0x0774))
                    setStoryFlag(state, 0x0558);
            }
            queueStoryRoute(state, 0x49, 0x0d, 0x0c);
            clearStoryFlag(state, 0x038f);
            state.scratch005f24bc = 1;
            state.scratch005f3d04 = 0;
            state.gameStateFlags &= ~0x20u;
            runtime.controlFlags |= 4u;

            state.phase11ModeFlags |= 4u;
            return true;
        }
    }

    if(runtime.phase < STORY_PROGRESSION_PHASE_FINALE &&
       runtime.scratch005f3d64 != 0)
    {
        const uint16_t tracked =
            static_cast<uint16_t>(runtime.scratch005f3d60);
        if(tracked == opponent)
        {
            bool routed = true;
            switch(opponent)
            {
            case 0x6b:
                queueStoryRoute(state, 0x32, 0x2f, 0x04, 0x18);
                setStoryTransitionTransform(
                    transition, -166.5f, -8.3f, 0.0f, -5.7f);
                setStoryTransitionSecondTransform(
                    transition, -1.3f, -3.0f, 0.0f, -62.6f);
                requestStoryFullHeal(
                    state, input.opponentCharacterId, false);
                setStoryFlag(state, 0x0800);
                setStoryFlag(state, 0x05f1);
                break;
            case 0x44:
                queueStoryRoute(state, 0x36, 0x20, 0x1f, 0x18);
                setStoryTransitionTransform(
                    transition, -71.6f, 4.5f, 0.0f, 67.3f);
                setStoryTransitionSecondTransform(
                    transition, 109.0f, 28.9f, 0.0f, 45.6f);
                requestStoryFullHeal(
                    state, input.opponentCharacterId, false);
                setStoryFlag(state, 0x0f42);
                setStoryFlag(state, 0x05f2);
                clearStoryFlag(state, 0x03c6);
                break;
            case 0x35:
                setStoryFlag(state, 0x0f54);
                queueStoryRoute(state, 0x30, 0x0d, 0x07, 0x18);
                setStoryTransitionTransform(
                    transition, -3.8f, 2.0f, 0.0f, -56.6f);
                setStoryTransitionSecondTransform(
                    transition, 155.8f, 77.3f, 0.0f, -39.8f);
                requestStoryFullHeal(
                    state, input.opponentCharacterId, false);
                setStoryFlag(state, 0x05f4);
                clearStoryFlag(state, 0x03c8);
                break;
            default:
                routed = false;
                break;
            }
            if(routed)
            {
                state.scratch005f3c50 = opponent;
                state.scratch005f3c54 = 0;
                state.gameStateFlags &= ~0x20u;
                state.scratch005f3d04 = 0;
            }
            clearStoryTrackedScratch(state);
            return true;
        }
        if(tracked == player)
        {
            switch(player)
            {
            case 0x6b:
                state.activeOpponentSlotFlags &= ~1u;
                state.battleFlags &= ~0x2000u;
                setStoryFlag(state, 0x0800);
                queueStoryRoute(state, 0x33, 0x2f, 0x04, 0x18);
                state.scratch005f3d04 = 0;
                state.scratch005f24bc = 1;
                clearStoryFlag(state, 0x03c6);
                requestStoryFullHeal(state, 0, false);
                state.gameStateFlags &= ~0x20u;
                break;
            case 0x44:
                setStoryFlag(state, 0x05f3);
                break;
            case 0x35:
                setStoryFlag(state, 0x0f54);
                clearStoryFlag(state, 0x03c8);
                state.gameStateFlags &= ~0x20u;
                requestStoryFullHeal(state, 0, false);
                state.scratch005f3d08 = 1;
                runtime.cameraResetRequested = 1;
                break;
            default:
                break;
            }
            clearStoryTrackedScratch(state);
            return true;
        }
    }
    return false;
}

void
advanceStoryProgressionPhase(StoryState& state)
{

    if(state.progression.phase < STORY_PROGRESSION_PHASE_FINALE)
    {
        state.progression.phase = static_cast<StoryProgressionPhase>(
            state.progression.phase + 1);
    }
}

void
finalizeStoryEncounterFlow(StoryState& state)
{

    StoryProgressionRuntime& runtime = state.progression;
    if(runtime.phase == STORY_PROGRESSION_PHASE_FINALE)
    {
        if((runtime.controlFlags & 8u) != 0 &&
           runtime.scratch005f3d28 == 2)
            runtime.manualCameraDisabled = 1;
        if(state.scratch005f3d24 != 0 && !storyFlag(state, 0x0505))
        {
            setStoryFlag(state, 0x0505);
            runtime.finaleCleanupStarted = 1;
            runtime.cameraResetRequested = 1;
        }
    }
    runtime.cameraResetRequested = 1;
}

namespace
{

void resolveStoryChapter4Target(StoryState& state)
{

    StoryProgressionRuntime& runtime = state.progression;
    const int resolvedCount = runtime.globalSlots[0x1c];
    if(resolvedCount == 3 && !storyFlag(state, 0x0462))
    {
        if(storyFlag(state, 0x04f4) && !runtime.battleFlowTask.active)
        {
            activateStoryTask(runtime.battleFlowTask, 0, 4);
            setStoryFlag(state, 0x0462);
        }
        return;
    }

    const uint16_t targetId =
        static_cast<uint16_t>(runtime.dynamicMessageId);
    if(targetId != 0 && runtime.globalSlots[0x1b] == 0 &&
       !runtime.battleFlowTask.active)
    {
        activateStoryTask(runtime.battleFlowTask, 0, 5);
        return;
    }

    if(!storyFlag(state, 0x04e5) || targetId == 0)
        return;

    int targetSlot = -1;
    for(int i = 0; i < 3; ++i)
    {
        if(runtime.globalSlots[i] == targetId)
        {
            targetSlot = i;
            break;
        }
    }
    if(targetSlot >= 0)
    {
        runtime.globalSlots[0x1c] =
            static_cast<uint16_t>(targetSlot + 1);
        const uint16_t flagBase =
            runtime.globalSlots[0x0a] == 0 ? 0x0457 : 0x045c;
        setStoryFlag(state,
            static_cast<uint16_t>(flagBase + targetSlot));
    }
    setStoryProgressionMessage(state, 0, false);

    if(runtime.globalSlots[0x0a] != 0 &&
       runtime.globalSlots[0x1d] == 0 &&
       !runtime.battleFlowTask.active)
    {
        activateStoryTask(runtime.battleFlowTask, 0, 2);
        setStoryFlag(state, 0x04c4);
        if(targetId < STORY_CHARACTER_COUNT)
        {
            StoryCharacterState& character = state.characters[targetId];
            character.battleStatusFlags |= 1u;
            character.persistentStatus = 3;
        }
        StoryActorState* actor = findStoryActor(state, targetId, false);
        if(actor != NULL)
        {
            actor->currentHealth = 0;
            actor->defeated = 1;
            actor->active = 0;
        }
    }

    runtime.dynamicMessageId = 0;
    runtime.dynamicMessageStageId = 0;
    runtime.globalSlots[0x0c] = 0;
    runtime.globalSlots[0x0a] = 0;
    runtime.globalSlots[0x1b] = 0;
}

void updateStoryPhaseBoundaries(StoryState& state,
                                const StoryProgressionInput& input)
{
    StoryProgressionRuntime& runtime = state.progression;
    switch(runtime.phase)
    {
    case STORY_PROGRESSION_PHASE_PROLOGUE:
        if(input.destinationStageId == 0x0c)
        {
            static const uint16_t RECRUITS[] = { 0x5d, 0x87, 0x8c };
            for(int i = 0; i < 3; ++i)
            {
                StoryCharacterState& character =
                    state.characters[RECRUITS[i]];
                if((character.characterFlags &
                    STORY_CHARACTER_RECRUITED) == 0)
                    ++runtime.recruitedCharacterCount;
                character.characterFlags |= STORY_CHARACTER_RECRUITED |
                                            STORY_CHARACTER_ENCOUNTERED;
                character.profileRevealFlags = 0xffff;
            }
        }
        else if(input.destinationStageId == 0x0d)
        {
            if(!storyFlag(state, 0x0f43))
            {
                setStoryFlag(state, 0x0f43);
                queueStoryRoute(state, 2, 0, 0x0d);
            }
            else if(input.currentStageId == input.destinationStageId &&
                    state.stageTransition.lifecycle ==
                        STORY_TRANSITION_IDLE)
            {
                uint16_t eventId = 3;
                switch(input.playerResourceProfileId)
                {
                case 0x44: eventId = 7; break;
                case 0x35: eventId = 4; break;
                case 0x6c: eventId = 5; break;
                case 0x16: eventId = 6; break;
                default: break;
                }
                queueStoryRoute(state, eventId, 0, 0x0d);
                runtime.phase = STORY_PROGRESSION_PHASE_RECRUITMENT;
            }
        }
        break;
    case STORY_PROGRESSION_PHASE_RECRUITMENT:
        if(input.destinationStageId == 0x19 &&
           !storyFlag(state, 0x0f44))
        {
            setStoryFlag(state, 0x0f44);
            setStoryFlag(state, 0x034c);
            queueStoryRoute(state, 8, 0, 0x19);
            runtime.phase = STORY_PROGRESSION_PHASE_CHAPTER_1;
            setStoryProgressionMessage(state, 0, false);
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_2:
        if(storyFlag(state, 0x0f55) &&
           (input.destinationStageId == 0x2f ||
            input.destinationStageId == 0x19))
        {
            runtime.phase = STORY_PROGRESSION_PHASE_INTERLUDE;
            if((runtime.promptPhaseSeenMask & (1u << 2)) == 0)
            {
                setStoryPromptPhase(state, 2);
                runtime.promptStep = 4;
            }
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_6:

        state.encounterSubstate = 0;
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_7:

        state.encounterSubstate =
            input.destinationStageId == 0x22 &&
            input.currentStageId == 0x20 ? 1 : 0;
        break;
    case STORY_PROGRESSION_PHASE_FINALE:
        if(storyFlag(state, 0x0f4e))
        {
            if(input.destinationStageId == 0x36 &&
               !storyFlag(state, 0x0f4f))
            {

                state.phase11ModeFlags |= 1u;
            }
            else if((state.phase11ModeFlags & 1u) != 0 &&
                    !storyFlag(state, 0x0505) &&
                    (state.phase11ModeFlags & 4u) != 0 &&
                    (state.phase11ModeFlags & 8u) == 0)
            {

                state.phase11ModeFlags |= 8u;
            }
        }
        break;
    default:
        break;
    }
}

}

void
updateStoryProgressionTasks(StoryState& state,
                            const StoryProgressionInput& input,
                            float deltaSeconds)
{
    if(!isfinite(deltaSeconds) || deltaSeconds < 0.0f)
        deltaSeconds = 0.0f;
    StoryProgressionRuntime& runtime = state.progression;

    StoryProgressionTask& battleFlow = runtime.battleFlowTask;
    if(battleFlow.active)
    {
        switch(battleFlow.state)
        {
        case 0:
            battleFlow.state = 1;
            break;
        case 1:
            if(!input.battleFlowBlocked)
                battleFlow.state = 2;
            break;
        case 2:
            if(input.fieldReady)
            {
                switch(battleFlow.mode)
                {
                case 5: setStoryProgressionMessage(state, 15, false); break;
                case 3: setStoryProgressionMessage(state, 16, false); break;
                case 4: setStoryProgressionMessage(state, 6, false); break;
                case 6: setStoryProgressionMessage(state, 2, true); break;
                default: break;
                }
                battleFlow.state = 3;
            }
            break;
        case 3:
            if(!input.battleFlowBlocked)
            {
                if(input.currentStageId == 4)
                    startStoryDialogueGate(state, 11, input);
                memset(&battleFlow, 0, sizeof(battleFlow));
            }
            break;
        default:
            break;
        }
    }

    StoryProgressionTask& prompt = runtime.promptTask;
    if(prompt.active)
    {
        switch(prompt.state)
        {
        case 1:
            prompt.state = 2;
            break;
        case 2:
            if(input.promptConfirmed && prompt.work == 0)
            {
                prompt.work = 1;
                prompt.elapsedSeconds = 0.0f;
            }
            if(prompt.work != 0)
            {
                prompt.elapsedSeconds += deltaSeconds;
                if(prompt.elapsedSeconds >= 0.5f)
                    prompt.state = 4;
            }
            break;
        case 5:
            prompt.state = 6;
            break;
        case 6:
            memset(&prompt, 0, sizeof(prompt));
            break;
        default:
            break;
        }
    }

    StoryProgressionTask& sequence = runtime.sequenceTask;
    if(sequence.active)
    {
        switch(sequence.state)
        {
        case 1:
            sequence.state = 3;
            break;
        case 3:
            if(input.fieldReady)
                sequence.state = 2;
            break;
        case 2:
            if(input.promptConfirmed)
                sequence.state = 5;
            break;
        case 5:
            sequence.state = 6;
            break;
        case 6:
            memset(&sequence, 0, sizeof(sequence));
            break;
        default:
            break;
        }
    }

    StoryProgressionTask& dialogue = runtime.dialogueTask;
    if(dialogue.active)
    {
        switch(dialogue.state)
        {
        case 1:
            dialogue.state = 3;
            break;
        case 3:
            if(input.fieldReady)
                dialogue.state = 2;
            break;
        case 2:
            if(input.promptConfirmed)
                dialogue.state = 5;
            break;
        case 5:
            setStoryFlag(state, 0x0028);
            dialogue.state = 6;
            break;
        case 6:
            memset(&dialogue, 0, sizeof(dialogue));
            break;
        default:
            break;
        }
    }

    StoryProgressionTask& battleStart = runtime.battleStartTask;
    if(battleStart.active)
    {
        StoryActorState* player = findStoryActor(state, 0, true);
        switch(battleStart.state)
        {
        case 0:
            runtime.controlFlags |= 0x00000100u;
            if(player != NULL)
            {
                player->position[0] = 51.8f;
                player->position[1] = 10.5f;
                player->position[2] = 0.0f;
                player->headingDegrees = 90.0f;
                player->motionId =
                    storyFlag(state, 0x076c) ? 0x025a : 0x0249;
            }
            battleStart.state = 1;
            break;
        case 1:
            if(player != NULL)
            {
                player->position[0] += 12.0f * deltaSeconds;
                if(player->position[0] >= 70.0f)
                {
                    player->position[0] = 70.0f;
                    startStoryIntroFade(state, 8);
                    battleStart.elapsedSeconds = 0.0f;
                    battleStart.state = 2;
                }
            }
            break;
        case 2:
            battleStart.elapsedSeconds += deltaSeconds;
            if(battleStart.elapsedSeconds >= 0.5f)
            {
                if(player != NULL)
                    player->motionId = 0x03d2;
                battleStart.elapsedSeconds = 0.0f;
                battleStart.state = 3;
            }
            break;
        case 3:
            battleStart.elapsedSeconds += deltaSeconds;
            if(battleStart.elapsedSeconds >= 0.5f)
            {
                setStoryFlag(state, 0x03bc);
                activateStoryTask(runtime.sequenceTask, 1, 2);
                battleStart.state = 4;
            }
            break;
        case 4:
            memset(&runtime.introFadeTask, 0,
                   sizeof(runtime.introFadeTask));
            battleStart.state = 5;
            break;
        case 5:
            memset(&battleStart, 0, sizeof(battleStart));
            break;
        default:
            break;
        }
    }
}

bool
updateStoryProgressionFlow(StoryState& state,
                           const StoryProgressionInput& input)
{

    tickNpcBaseSelectorCountdowns(state);
    if(input.encounterCompleted)
    {

        markNpcBaseSelectorUnavailable(state, input.opponentCharacterId);
    }
    StoryProgressionRuntime& runtime = state.progression;
    runtime.transitionMode =
        storySceneDefersProgression(input.sceneId) ? 1 : 0;
    if(runtime.transitionMode != 0 &&
       !(runtime.phase == STORY_PROGRESSION_PHASE_FINALE &&
         input.destinationStageId == 0x30))
    {
        runtime.deferred = 1;
        refreshStoryProgressionFlags(state, false);
        return false;
    }
    if(runtime.deferred)
    {
        runtime.deferred = 0;
        refreshStoryProgressionFlags(state, false);
        setStoryProgressionMessage(state, runtime.messageId, false);
    }

    updateStoryPhaseBoundaries(state, input);
    if(input.encounterCompleted && input.encounterProfileId >= 0)
    {
        const uint16_t profile =
            static_cast<uint16_t>(input.encounterProfileId);
        processStoryEncounterCompletion(
            state, profile, input.encounterStatus,
            input.encounterRecruited != 0);
        StoryEncounterTransitionInput transitionInput = {};
        transitionInput.opponentResourceProfileId = profile;
        transitionInput.playerResourceProfileId =
            input.playerResourceProfileId;
        transitionInput.opponentCharacterId = input.opponentCharacterId;
        transitionInput.playerCharacterId = input.playerCharacterId;
        if(!applyStoryEncounterTransition(state, transitionInput))
            handleStoryCharacterAction(state, profile);
    }

    bool completed = false;
    switch(runtime.phase)
    {
    case STORY_PROGRESSION_PHASE_RECRUITMENT:
        if(storyFlag(state, 0x03e8))
        {
            runtime.promptStep = 1;
            targetListMan(state, 0, 0);
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_1:
        if(storyFlag(state, 0x03ec))
        {
            if(storyFlag(state, 0x04ee) && !input.updateTaskActive)
                runtime.controlFlags &= ~1u;
            startStoryDialogueGate(state, 1, input);
        }
        break;
    case STORY_PROGRESSION_PHASE_INTERLUDE:
        runtime.controlFlags &= ~6u;
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_3:
        runtime.controlFlags &= ~2u;
        if(storyFlag(state, 0x0407) &&
           (state.characters[0x19].characterFlags &
            STORY_CHARACTER_RECRUITED) != 0 &&
           !storyFlag(state, 0x040d))
            setStoryFlag(state, 0x040d);

        if(input.currentStageId == 0x19 && storyFlag(state, 0x040b))
        {
            if(storyFlag(state, 0x04ef) && !input.updateTaskActive)
                runtime.controlFlags &= ~8u;
            startStoryDialogueGate(state, 3, input);
            clearStoryFlag(state, 0x034c);
        }
        if(storyFlag(state, 0x0411) &&
           input.currentStageId == 0x1a)
        {
            bool found = false;
            for(int i = 0; i < input.activeCharacterCount && i < 11; ++i)
            {
                if(input.activeCharacterIds[i] == 0x37 ||
                   input.activeCharacterIds[i] == 0x38)
                {
                    found = true;
                    break;
                }
            }
            if(found && (runtime.globalSlots[0x0e] & 0x10) == 0)
            {
                runtime.globalSlots[0x0e] |= 0x10;
                startStoryIntroFade(state, 4);
            }
        }
        if(storyFlag(state, 0x0442) && !storyFlag(state, 0x0f47))
            runtime.promptStep = 6;
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_4:
        runtime.controlFlags &= ~0x0au;
        if(input.currentStageId == 0x19 && storyFlag(state, 0x0453))
        {
            clearStoryFlag(state, 0x034c);
            if(runtime.promptStep < 8)
                runtime.promptStep = 8;
        }
        if(!input.battleFlowBlocked)
        {
            if(storyFlag(state, 0x0463))
                resolveStoryChapter4Target(state);
            else if(storyFlag(state, 0x04de) &&
                    !storyFlag(state, 0x04f0))
            {
                if(!storyFlag(state, 0x04df))
                {
                    targetListMan(state, 0, 0);
                    setStoryFlag(state, 0x04f0);
                }
            }
            else
            {
                if(input.currentStageId == 4 &&
                   storyFlag(state, 0x04df))
                {
                    runtime.promptStep = 9;
                    if(!storyFlag(state, 0x04f1))
                    {
                        setStoryFlag(state, 0x04f1);
                        setStoryProgressionMessage(state, 0, false);
                        return false;
                    }
                }
                if(!input.interactionStateBit0 &&
                   storyFlag(state, 0x04f1))
                {
                    if(!runtime.battleFlowTask.active)
                        activateStoryTask(runtime.battleFlowTask, 0, 1);
                    setStoryFlag(state, 0x0463);
                }
            }
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_5:
        runtime.controlFlags &= ~2u;
        if(storyFlag(state, 0x0460) && !storyFlag(state, 0x0779))
        {
            clearStoryFlag(state, 0x034c);
            runtime.promptStep = 11;
            if(storyFlag(state, 0x0526) && !input.updateTaskActive)
                runtime.controlFlags &= ~8u;
            startStoryDialogueGate(state, 12, input);
        }
        if((storyFlag(state, 0x052c) ||
            storyFlag(state, 0x03f7)) &&
           !storyFlag(state, 0x0f4a))
        {
            if(!runtime.messageActive)
                setStoryProgressionMessage(state, 7, false);
            runtime.promptStep = 12;
        }
        if(input.currentStageId == 0x0d && storyFlag(state, 0x0475))
        {
            setStoryFlag(state, 0x0477);
            clearStoryFlag(state, 0x0334);
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_6:
        runtime.controlFlags &= ~2u;
        if(input.currentStageId == 0x19 && storyFlag(state, 0x0fa7))
        {
            clearStoryFlag(state, 0x034c);
            if(!storyFlag(state, 0x0f72))
                runtime.promptStep = 14;
            if(storyFlag(state, 0x04f5) && !input.updateTaskActive)
                runtime.controlFlags &= ~8u;
            startStoryDialogueGate(state, 4, input);
        }
        if(storyFlag(state, 0x0f72) && !storyFlag(state, 0x04fd) &&
           runtime.globalSlots[0x1e] == 0)
        {
            runtime.globalSlots[0x1e] = 0x69;
            setStoryProgressionMessage(state, 0, false);
        }
        if(storyFlag(state, 0x0f72) && !storyFlag(state, 0x0f4b))
        {
            if(storyFlag(state, 0x077a))
            {
                if(storyFlag(state, 0x0f73))
                {
                    if(!runtime.messageActive)
                    {
                        setStoryProgressionMessage(state, 9, false);
                        runtime.promptStep = 15;
                        runtime.globalSlots[0x1f] = 0x6b;
                    }
                }
                else
                    setStoryProgressionMessage(state, 0, false);
            }
            else if(!runtime.messageActive)
                setStoryProgressionMessage(state, 13, false);
        }

        if(input.currentStageId == 0x27 &&
           !storyFlag(state, 0x0f4b) &&
           storyFlag(state, 0x0f73) &&
           !storyFlag(state, 0x03f2))
        {
            const bool inside = insideStoryRectangle(
                input.playerX, input.playerZ,
                -278.0f, -266.0f, -37.0f, -26.0f);
            if(!inside)
            {
                if(runtime.promptTask.active &&
                   runtime.promptTask.work == 0)
                    runtime.promptTask.state = 5;
            }
            else if(!runtime.promptTask.active)
                activateStoryTask(runtime.promptTask, 1, 0);
            else if(runtime.promptTask.state == 4)
            {
                setStoryFlag(state, 0x03f2);
                runtime.promptTask.state = 5;
            }
        }
        if(input.currentStageId == 0x2d &&
           (runtime.globalSlots[0x0e] & 0x40) == 0)
        {
            runtime.globalSlots[0x0e] |= 0x40;
            startStoryIntroFade(state, 6);
        }
        if(!storyFlag(state, 0x0f4b))
        {
            if(input.currentStageId == 0x27)
                startStoryDialogueGate(state, 6, input);
            else if(input.currentStageId == 0x26)
                startStoryDialogueGate(state, 5, input);
            else
                startStoryDialogueGate(state, 7, input);
        }

        if(input.currentStageId == 0x2d &&
           storyFlag(state, 0x0f72))
        {
            if(!storyFlag(state, 0x04fc))
            {
                setStoryProgressionMessage(state, 0, false);
                setStoryFlag(state, 0x04fc);
                activateStoryTask(runtime.sequenceTask, 1, 0);
                return false;
            }
            if(storyFlag(state, 0x04fd) &&
               !storyFlag(state, 0x0f4b) &&
               !runtime.battleStartTask.active &&
               !runtime.sequenceTask.active)
            {
                setStoryFlag(state, 0x0f4b);
                queueStoryRoute(state, 0x21, 0, input.currentStageId);
                setStoryFlag(state, 0x0374);
                setStoryProgressionMessage(state, 0, false);
                return false;
            }
            if(!storyFlag(state, 0x04fd) &&
               insideStoryRectangle(input.playerX, input.playerZ,
                                    51.0f, 61.0f, -50.0f, 10.0f))
            {
                activateStoryTask(runtime.battleStartTask, 0, 0);
                setStoryFlag(state, 0x04fd);
            }
        }

        if(storyFlag(state, 0x0f4b) &&
           (input.destinationStageId == 0x27 ||
            input.currentStageId == 0x26) &&
           !storyFlag(state, 0x04ff))
        {
            const bool boundary =
                (input.destinationStageId == 0x27 &&
                 insideStoryRectangle(input.playerX, input.playerZ,
                                      -285.0f, -266.0f,
                                      -37.0f, -29.5f)) ||
                (input.currentStageId == 0x26 &&
                 insideStoryRectangle(input.playerX, input.playerZ,
                                      -60.7f, 57.0f,
                                      179.5f, 195.0f));
            if(boundary)
            {
                runtime.controlFlags = 0;
                runtime.phase = STORY_PROGRESSION_PHASE_CHAPTER_7;
                queueStoryRoute(state, 0x22, 1, 0x19);
                setStoryProgressionMessage(state, 0, false);
                setStoryFlag(state, 0x04ff);
            }
        }
        break;
    case STORY_PROGRESSION_PHASE_CHAPTER_7:
        runtime.controlFlags &= ~0x0au;
        if(!storyFlag(state, 0x049c) && !storyFlag(state, 0x0f4c))
        {
            if(!runtime.messageActive)
            {
                setStoryProgressionMessage(state, 10, false);
                runtime.promptStep = 17;
                clearStoryFlag(state, 0x034c);
            }
        }
        else if(storyFlag(state, 0x0fa6) &&
                !storyFlag(state, 0x0f4d) &&
                !runtime.messageActive)
        {
            runtime.promptStep = 18;
            setStoryProgressionMessage(state, 11, false);
        }
        if(input.currentStageId == 0x22)
            startStoryDialogueGate(state, 8, input);
        if(storyFlag(state, 0x05f0) && storyFlag(state, 0x05f2))
        {

            state.phase10Stage31Suppress = 1;
            clearStoryFlag(state, 0x05f0);
            state.gameStateFlags &= ~0x20u;
            completed = true;
        }
        if(storyFlag(state, 0x05f0) && storyFlag(state, 0x05f3))
        {
            clearStoryFlag(state, 0x05f0);
            completed = true;
        }
        break;
    case STORY_PROGRESSION_PHASE_FINALE:
        if(state.scratch005f3d24 == 0)
        {
            if(input.currentStageId != 0x22 ||
               !storyFlag(state, 0x0f4d) ||
               storyFlag(state, 0x0501) ||
               runtime.battleFlowTask.active)
            {
                if(input.currentStageId == 0x19)
                {
                    if(input.activeCharacterCount == 3)
                        setStoryFlag(state, 0x04e2);
                    else
                        clearStoryFlag(state, 0x04e2);
                }
                if(storyFlag(state, 0x04e3))
                {
                    if(input.currentStageId == 0x19)
                    {
                        if(storyFlag(state, 0x0502))
                        {
                            if(!input.updateTaskActive &&
                               !runtime.messageActive)
                            {
                                setStoryProgressionMessage(
                                    state, 12, false);
                                runtime.promptStep = 20;
                                clearStoryFlag(state, 0x034c);
                            }
                        }
                        else
                            startStoryDialogueGate(state, 9, input);
                    }
                    else if(input.currentStageId == 0x32)
                        startStoryDialogueGate(state, 10, input);
                }
            }
            else
            {
                activateStoryTask(runtime.battleFlowTask, 0, 6);
                setStoryFlag(state, 0x0501);
            }
        }
        else
        {
            if(!storyFlag(state, 0x0505))
            {
                setStoryFlag(state, 0x0505);
                runtime.finaleCleanupStarted = 1;
                runtime.cameraResetRequested = 1;
            }
            completed = true;
        }
        break;
    default:
        break;
    }
    return completed;
}

bool
selectStoryRelationshipTarget(StoryState& state, uint16_t characterId,
                              int16_t stageId, bool specialMode)
{
    if(characterId == 0 || characterId >= STORY_CHARACTER_COUNT ||
       stageId < 0 || stageId > 100)
        return false;

    StoryHudRadarState& radar = state.progression.hudRadar;
    if(specialMode)
    {
        radar.specialRelationshipMode = 1;
        for(int i = 0; i < 2; ++i)
        {
            StoryHudTarget& target = radar.specialRelationshipTargets[i];
            if(target.characterId >= 0)
                continue;
            target.characterId = static_cast<int16_t>(characterId);
            target.stageId = 0x30;
            return true;
        }

    }

    for(int i = 0; i < 2; ++i)
    {
        StoryHudTarget& target = radar.relationshipTargets[i];
        if(target.characterId >= 0)
            continue;
        target.characterId = static_cast<int16_t>(characterId);
        target.stageId = stageId;
        state.characters[characterId].persistentStatus = 3;
        return true;
    }
    return false;
}

bool
clearStoryRelationshipTarget(StoryState& state, uint16_t characterId,
                             bool specialMode, bool completed)
{
    if(characterId == 0 || characterId >= STORY_CHARACTER_COUNT)
        return false;

    StoryHudRadarState& radar = state.progression.hudRadar;
    StoryHudTarget* targets = specialMode ?
        radar.specialRelationshipTargets : radar.relationshipTargets;
    if(!specialMode && state.characters[characterId].persistentStatus != 3)
        return false;

    int slot = -1;
    for(int i = 0; i < 2; ++i)
        if(targets[i].characterId == static_cast<int16_t>(characterId))
        {
            slot = i;
            break;
        }
    if(slot < 0)
        return false;

    targets[slot].characterId = -1;
    targets[slot].stageId = -1;
    if(slot == 0 && targets[1].characterId >= 0)
    {
        targets[0] = targets[1];
        targets[1].characterId = -1;
        targets[1].stageId = -1;
    }
    if(!specialMode)
        state.characters[characterId].persistentStatus = completed ? 4 : 0;
    return true;
}

const StoryHudTarget*
storyRelationshipTargets(const StoryState& state)
{
    const StoryHudRadarState& radar = state.progression.hudRadar;
    return radar.specialRelationshipMode ?
        radar.specialRelationshipTargets : radar.relationshipTargets;
}

void
setStoryHudObjective(StoryState& state, int16_t characterId,
                     int16_t stageId, bool alternate)
{
    StoryHudObjective& objective = state.progression.hudRadar.objective;
    StoryHudTarget& target = alternate ?
        objective.alternate : objective.primary;
    if(stageId != 0)
    {
        target.characterId = characterId;
        target.stageId = stageId;
    }
    else
        target.stageId = 0;
    if(alternate)
        objective.alternateActive = stageId != 0 ? 1 : 0;
    else
        objective.primaryActive = stageId != 0 ? 1 : 0;
    if(alternate)
        objective.useAlternate = stageId != 0 ? 1 : 0;
}
