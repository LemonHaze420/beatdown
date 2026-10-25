#include "game.h"
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

void
Game::beginSceneInspection(void)
{
    installStageSceneSequenceCaption();
    sceneSequencer.setCaptionShadowEnabled(false);
    currentScreen = GAME_SCREEN_GAMEPLAY;
    switchGameFlowTaskHandler(updateGameplaySessionFlow, 1);
}

void
Game::clearStageActorRuntime(void)
{
    for(int i = 0; i < GAME_STAGE_ACTOR_CAPACITY; ++i)
    {
        stageActors[i] = {};
        stageResourcePreloads[i] = -1;
    }
    memset(stageActorOverflowRecords, 0,
           sizeof(stageActorOverflowRecords));
    for(std::size_t i = 0; i < STAGE_AI_CONTEXT_COUNT; ++i)
        stageAiContexts[i] = {};
    stageActorOccupiedCount = 0;
    stageResourcePreloadCountValue = 0;
    stageActorStageId = 0xffff;
}

void
Game::preloadStageActor(const FstageNpcPositionRow& row,
                        int rowIndex, int actorOrdinal)
{
    if(actorOrdinal < 0 || actorOrdinal >= GAME_STAGE_ACTOR_CAPACITY ||
       rowIndex < 0 || rowIndex >= GAME_STAGE_ACTOR_CAPACITY ||
       row.kind() != NPC_POSITION_CHARACTER || row.selector() < 0)
        return;

    StageActorRuntime& actor = stageActors[actorOrdinal];
    actor = {};
    actor.authoredCharacterId =
        static_cast<uint16_t>(row.selector());
    actor.secondaryCharacterId = actor.authoredCharacterId < 200 ?
        static_cast<int16_t>(actor.authoredCharacterId) :
        static_cast<int16_t>(200 + actorOrdinal);
    actor.sourceRowIndex = static_cast<int16_t>(rowIndex);
    actor.rowKind = static_cast<uint8_t>(row.kind());
    actor.alternateMotionId = row.values[2];
    actor.interactionSelector = row.values[3];
    actor.defaultMotionId = row.values[8];
    actor.activeMotionId = actor.defaultMotionId != 0 ?
        actor.defaultMotionId : actor.alternateMotionId;
    actor.occupied = true;
    actor.visible = true;

    initializeGameSlot(
        actor.slot, static_cast<CharacterId>(actor.authoredCharacterId),
        static_cast<uint8_t>(actorOrdinal + 2));
    actor.slot.playerIndex = static_cast<uint8_t>(actorOrdinal + 3);
    actor.slot.persistentRecordId = actor.secondaryCharacterId;
    actor.slot.npcPositionRowIndex = actor.sourceRowIndex;
    if(actor.authoredCharacterId < 200)
    {
        projectPersistentRecordToGameSlot(
            actor.authoredCharacterId, actor.slot);
    }
    else
    {
        PersistentGameRecord& overflow =
            stageActorOverflowRecords[actorOrdinal];
        initializePersistentRecord(overflow, actor.authoredCharacterId);
        overflow.aiProfileId = combatAiProfileForCharacter(
            actor.authoredCharacterId, COMBAT_AI_PRIMARY_OPPONENT);
        actor.slot.currentHealth = overflow.currentHealth;
        actor.slot.maxHealth = overflow.maxHealth;
        actor.slot.prideGauge = overflow.prideGauge;
        actor.slot.maxPrideGauge = overflow.prideGauge;
    }
    const PersistentGameRecord* record =
        stageActorPersistentRecord(actorOrdinal);
    actor.slot.resourceProfileId = resolveBattleResourceProfile(
        static_cast<int16_t>(actor.authoredCharacterId),
        record != NULL ? record->resourceId : -1);

    FstageSpawnTransform transform;
    if(getFstageSpawnTransform(row, &transform))
    {
        actor.slot.position = {
            transform.position.x, transform.position.y,
            transform.position.z};
        actor.slot.headingDegrees = transform.headingDegrees;
    }
    actor.slot.flags |= 1u;
    initializeActorActionRuntime(actor.slot.actorAction);
    ++stageActorOccupiedCount;

    StoryActorState* storyActor = findStoryActor(
        persistent.story, actor.authoredCharacterId, true);
    if(storyActor != NULL)
    {
        storyActor->position[0] = actor.slot.position.x;
        storyActor->position[1] = actor.slot.position.y;
        storyActor->position[2] = actor.slot.position.z;
        storyActor->headingDegrees = actor.slot.headingDegrees;
        storyActor->motionId =
            static_cast<uint16_t>(actor.activeMotionId);
        storyActor->currentHealth = actor.slot.currentHealth;
        storyActor->active = 1;
        storyActor->visible = 1;
    }
}

int Game::stageActorCount(void) const
{
    return stageActorOccupiedCount;
}

int Game::fstageNpcPositionRowCount(void) const
{
    return static_cast<int>(fstageNpcPositionRows.size());
}

const FstageNpcPositionRow* Game::fstageNpcPositionRow(int index) const
{
    return index >= 0 &&
           index < static_cast<int>(fstageNpcPositionRows.size()) ?
        &fstageNpcPositionRows[index] : NULL;
}

StageActorRuntime* Game::stageActor(int index)
{
    return index >= 0 && index < GAME_STAGE_ACTOR_CAPACITY ?
        &stageActors[index] : NULL;
}

const StageActorRuntime* Game::stageActor(int index) const
{
    return index >= 0 && index < GAME_STAGE_ACTOR_CAPACITY ?
        &stageActors[index] : NULL;
}

StageActorRuntime* Game::findStageActor(uint16_t authoredCharacterId)
{
    for(int i = 0; i < GAME_STAGE_ACTOR_CAPACITY; ++i)
    {
        StageActorRuntime& actor = stageActors[i];
        if(actor.occupied && (actor.slot.flags & 1u) != 0 &&
           actor.authoredCharacterId == authoredCharacterId)
            return &actor;
    }
    return NULL;
}

const StageActorRuntime* Game::findStageActor(
    uint16_t authoredCharacterId) const
{
    for(int i = 0; i < GAME_STAGE_ACTOR_CAPACITY; ++i)
    {
        const StageActorRuntime& actor = stageActors[i];
        if(actor.occupied && (actor.slot.flags & 1u) != 0 &&
           actor.authoredCharacterId == authoredCharacterId)
            return &actor;
    }
    return NULL;
}

bool Game::setStageActorSecondaryIdentity(
    int index, int16_t persistentRecordId)
{
    StageActorRuntime* actor = stageActor(index);
    if(actor == NULL || !actor->occupied || persistentRecordId < 0 ||
       persistentRecordId >= GAME_SAVE_RECORD_COUNT)
        return false;
    actor->secondaryCharacterId = persistentRecordId;
    actor->slot.persistentRecordId = persistentRecordId;
    const PersistentGameRecord* record = characterRecord(
        static_cast<uint16_t>(persistentRecordId));
    actor->slot.resourceProfileId = resolveBattleResourceProfile(
        static_cast<int16_t>(actor->authoredCharacterId),
        record != NULL ? record->resourceId : -1);
    return true;
}

const PersistentGameRecord* Game::stageActorPersistentRecord(int index) const
{
    const StageActorRuntime* actor = stageActor(index);
    if(actor == NULL || !actor->occupied)
        return NULL;
    if(actor->secondaryCharacterId >= 200 &&
       actor->secondaryCharacterId <
           200 + GAME_STAGE_ACTOR_OVERFLOW_RECORD_COUNT)
    {
        return &stageActorOverflowRecords[
            actor->secondaryCharacterId - 200];
    }
    return actor->secondaryCharacterId >= 0 ?
        characterRecord(
            static_cast<uint16_t>(actor->secondaryCharacterId)) : NULL;
}

PersistentGameRecord* Game::stageActorPersistentRecord(int index)
{
    return const_cast<PersistentGameRecord*>(
        static_cast<const Game*>(this)->stageActorPersistentRecord(index));
}

int Game::stageResourcePreloadCount(void) const
{
    return stageResourcePreloadCountValue;
}

int16_t Game::stageResourcePreload(int index) const
{
    return index >= 0 && index < stageResourcePreloadCountValue ?
        stageResourcePreloads[index] : -1;
}

bool
Game::startStageActorEncounter(
    uint16_t characterId, uint16_t eventId, bool plural)
{
    if(currentMode != GAME_MODE_STORY)
        return false;
    persistent.story.battleId = characterId;
    persistent.story.battleEventId = eventId;
    persistent.story.battlePlural = plural ? 1 : 0;
    persistent.story.battleActive = 1;
    initializeBattle(true);
    return persistent.story.battleActive != 0 &&
           activeOpponentCount > 0;
}

StageAiAsyncContext* Game::mutableStageAiContext(int playerIndex)
{
    return playerIndex >= 0 &&
        playerIndex < static_cast<int>(STAGE_AI_CONTEXT_COUNT) ?
            &stageAiContexts[playerIndex] : NULL;
}

const StageAiAsyncContext* Game::stageAiContext(int playerIndex) const
{
    return playerIndex >= 0 &&
        playerIndex < static_cast<int>(STAGE_AI_CONTEXT_COUNT) ?
            &stageAiContexts[playerIndex] : NULL;
}

bool Game::stageActorIsActiveRoster(uint16_t characterId) const
{
    for(int i = 0; i < STORY_PARTY_COUNT; ++i)
        if(persistent.story.party[i] == characterId)
            return true;
    return false;
}

void
Game::updateInteraction(void)
{
    if(!fstageNpcPositionRows.empty())
    {
        updateFstageInteraction();
        if(currentMode == GAME_MODE_STORY &&
           persistent.story.pickpocketMode != 0 &&
           persistent.story.battleActive == 0 &&
           focusedInteractionSlot >= 0)
        {
            for(int i = 0; i < GAME_STAGE_ACTOR_CAPACITY; ++i)
            {
                StageActorRuntime& actor = stageActors[i];
                if(!actor.occupied || !actor.visible ||
                   actor.slot.slotIndex != focusedInteractionSlot ||
                   actor.slot.currentHealth <= 0)
                    continue;

                startStageActorEncounter(
                    actor.authoredCharacterId,
                    static_cast<uint16_t>(actor.interactionSelector), false);
                break;
            }
        }
        return;
    }

    const GameSlotRecord& player = persistent.player;
    int nearest = -1;
    float nearestDistance = 0.0f;
    int nearestSlot = -1;
    for(int i = 0; i < stageInteractionCount; ++i)
    {
        GameInteraction& item = stageInteractions[i];
        if(item.kind == GAME_INTERACTION_NONE || item.used != 0)
        {
            item.focusSeconds = 0.0f;
            continue;
        }
        const float dy = item.position.y - player.position.y;
        const float dx = item.position.x - player.position.x;
        const float dz = item.position.z - player.position.z;
        const float distanceSquared = dx * dx + dz * dz;
        if(fabsf(dy) >= GAME_INTERACTION_VERTICAL_LIMIT ||
           distanceSquared > item.range * item.range)
        {
            item.focusSeconds = 0.0f;
            continue;
        }
        if(nearest < 0 || distanceSquared < nearestDistance)
        {
            nearest = i;
            nearestDistance = distanceSquared;
        }
    }
    for(int i = 0; i < activeOpponentCount; ++i)
    {
        GameSlotRecord& actor = i == 0 ? enemy : additionalEnemies[i - 1];
        if(actor.currentHealth <= 0)
        {
            actor.interactionElapsed = 0.0f;
            continue;
        }
        const float dy = actor.position.y - player.position.y;
        const float dx = actor.position.x - player.position.x;
        const float dz = actor.position.z - player.position.z;
        const float distanceSquared = dx * dx + dz * dz;
        if(fabsf(dy) >= GAME_INTERACTION_VERTICAL_LIMIT ||
           distanceSquared >
               actor.interactionRange * actor.interactionRange)
        {
            actor.interactionElapsed = 0.0f;
            continue;
        }
        if(nearestSlot < 0 && (nearest < 0 ||
                              distanceSquared < nearestDistance))
        {
            nearest = -1;
            nearestDistance = distanceSquared;
            nearestSlot = actor.slotIndex;
        }
        else if(nearestSlot >= 0 && distanceSquared < nearestDistance)
        {
            nearestDistance = distanceSquared;
            nearestSlot = actor.slotIndex;
        }
        else
            actor.interactionElapsed = 0.0f;
    }
    for(int i = 0; i < stageInteractionCount; ++i)
        if(i != nearest)
            stageInteractions[i].focusSeconds = 0.0f;
    focusedInteractionIndex = nearest;
    focusedInteractionSlot = nearestSlot;
    if(nearest >= 0)
        stageInteractions[nearest].focusSeconds += scaledFrameDeltaSeconds;
    if(nearestSlot >= 0)
    {
        GameSlotRecord& actor = nearestSlot == 1 ?
            enemy : additionalEnemies[nearestSlot - 2];
        actor.interactionElapsed += scaledFrameDeltaSeconds;
    }
    if((pressedInput & GAME_INPUT_CONFIRM) == 0)
        return;
    if(nearestSlot >= 0)
    {
        persistent.player.linkedSlotIndex =
            static_cast<uint8_t>(nearestSlot);
        persistent.player.actorTargetingFlags |= 0x1000;
        return;
    }
    if(nearest < 0)
        return;
    GameInteraction& item = stageInteractions[nearest];
    item.used = 1;
    item.focusSeconds = 0.0f;
    lastInteractionSelectorId = item.selectorId;
    focusedInteractionIndex = -1;
}

void
Game::bindNpcPositionRow(GameSlotRecord& slot)
{
    slot.npcPositionRowIndex = -1;
    for(size_t i = 0;
        i < fstageNpcPositionRows.size() && i < FSTAGE_NPC_ROW_COUNT; ++i)
    {
        const FstageNpcPositionRow& row = fstageNpcPositionRows[i];
        if(row.kind() == GAME_INTERACTION_ACTOR &&
           row.selector() == slot.characterId)
        {
            slot.npcPositionRowIndex = static_cast<int16_t>(i);
            return;
        }
    }
}

void
Game::updateFstageInteraction(void)
{
    FstageNpcActorBinding actors[GAME_STAGE_ACTOR_CAPACITY];
    int actorCount = 0;
    for(int i = 0; i < GAME_STAGE_ACTOR_CAPACITY; ++i)
    {
        StageActorRuntime& actor = stageActors[i];
        if(!actor.occupied || !actor.visible ||
           (actor.slot.flags & 1u) == 0)
            continue;
        FstageNpcActorBinding& binding = actors[actorCount++];
        binding.actorSlot = actor.slot.slotIndex;
        binding.npcPositionRowIndex = actor.sourceRowIndex;
        binding.characterId = actor.authoredCharacterId;
        binding.position = {
            actor.slot.position.x, actor.slot.position.y,
            actor.slot.position.z};
        binding.active = actor.slot.currentHealth > 0;
    }

    const rw::V3d playerPosition = {
        persistent.player.position.x, persistent.player.position.y,
        persistent.player.position.z};
    FstageNpcInteractionCandidate candidates[FSTAGE_NPC_ROW_COUNT];
    const int count = collectFstageNpcInteractionCandidates(
        fstageNpcPositionRows, actors, actorCount, playerPosition,
        candidates, static_cast<int>(FSTAGE_NPC_ROW_COUNT));
    int nearestCandidate = -1;
    int nearestInteraction = -1;
    for(int i = 0; i < count; ++i)
    {
        int interaction = -1;
        for(int j = 0; j < stageInteractionCount; ++j)
        {
            if(stageInteractions[j].npcPositionRowIndex ==
                   candidates[i].rowIndex &&
               stageInteractions[j].used == 0)
            {
                interaction = j;
                break;
            }
        }
        if(interaction < 0)
            continue;
        if(nearestCandidate < 0 ||
           candidates[i].distanceSquared <
               candidates[nearestCandidate].distanceSquared)
        {
            nearestCandidate = i;
            nearestInteraction = interaction;
        }
    }

    int nearestSlot = -1;
    if(nearestCandidate >= 0)
    {
        const FstageNpcInteractionCandidate& candidate =
            candidates[nearestCandidate];
        GameInteraction& item = stageInteractions[nearestInteraction];
        item.position = {
            candidate.position.x, candidate.position.y,
            candidate.position.z};
        item.range = candidate.radius;
        nearestSlot = candidate.actorSlot;
    }
    for(int i = 0; i < stageInteractionCount; ++i)
        if(i != nearestInteraction)
            stageInteractions[i].focusSeconds = 0.0f;
    focusedInteractionIndex = nearestInteraction;
    focusedInteractionSlot = nearestSlot;
    if(nearestInteraction < 0)
        return;
    GameInteraction& item = stageInteractions[nearestInteraction];
    item.focusSeconds += scaledFrameDeltaSeconds;
    if((pressedInput & GAME_INPUT_CONFIRM) == 0)
        return;
    if(nearestSlot >= 0)
    {
        persistent.player.linkedSlotIndex =
            static_cast<uint8_t>(nearestSlot);
        persistent.player.actorTargetingFlags |= 0x1000;
        return;
    }
    item.used = 1;
    item.focusSeconds = 0.0f;
    lastInteractionSelectorId = item.selectorId;
    focusedInteractionIndex = -1;
}

void Game::setStageInteractions(const GameInteraction* items, int count)
{
    clearStageInteractions();
    if(items == NULL || count <= 0)
        return;
    if(count > GAME_INTERACTION_CAPACITY)
        count = GAME_INTERACTION_CAPACITY;
    for(int i = 0; i < count; ++i)
        stageInteractions[i] = items[i];
    stageInteractionCount = count;
}

void Game::clearStageInteractions(void)
{
    for(int i = 0; i < GAME_STAGE_ACTOR_CAPACITY; ++i)
    {
        if(!stageActors[i].occupied)
            continue;
        StoryActorState* actor = findStoryActor(
            persistent.story, stageActors[i].authoredCharacterId, false);
        if(actor != NULL && !actor->inParty)
        {
            actor->active = 0;
            actor->visible = 0;
        }
    }
    clearStageActorRuntime();
    fstageNpcPositionRows.clear();
    persistent.player.npcPositionRowIndex = -1;
    enemy.npcPositionRowIndex = -1;
    ally.npcPositionRowIndex = -1;
    for(int i = 0; i < MAX_GAME_OPPONENTS - 1; ++i)
        additionalEnemies[i].npcPositionRowIndex = -1;
    focusedInteractionSlot = -1;
    for(int i = 0; i < GAME_INTERACTION_CAPACITY; ++i)
        stageInteractions[i] = GameInteraction();
    stageInteractionCount = 0;
    focusedInteractionIndex = -1;
    lastInteractionSelectorId = 0;
}

void
Game::setFstageNpcPositionRows(
    const std::vector<FstageNpcPositionRow>& rows)
{
    setFstageNpcPositionRows(rows, persistent.story.stageId);
}

void
Game::setFstageNpcPositionRows(
    const std::vector<FstageNpcPositionRow>& rows,
    uint16_t liveStageId)
{
    clearStageInteractions();
    stageActorStageId = liveStageId;
    fstageNpcPositionRows.assign(
        rows.begin(),
        rows.begin() + (rows.size() < FSTAGE_NPC_ROW_COUNT ?
                        rows.size() : FSTAGE_NPC_ROW_COUNT));
    int actorOrdinal = 0;
    for(size_t i = 0; i < fstageNpcPositionRows.size(); ++i)
    {
        const FstageNpcPositionRow& row = fstageNpcPositionRows[i];
        if(row.kind() == NPC_POSITION_CHARACTER &&
           actorOrdinal < GAME_STAGE_ACTOR_CAPACITY)
        {
            preloadStageActor(row, static_cast<int>(i), actorOrdinal);
            ++actorOrdinal;
        }
        else if(row.kind() == NPC_POSITION_CONDITIONAL &&
                stageResourcePreloadCountValue <
                    GAME_STAGE_ACTOR_CAPACITY)
        {
            stageResourcePreloads[stageResourcePreloadCountValue++] =
                row.selector();
        }

        if(row.kind() != GAME_INTERACTION_ACTOR &&
           row.kind() != GAME_INTERACTION_OBJECT)
            continue;
        GameInteraction& item =
            stageInteractions[stageInteractionCount++];
        item.kind = static_cast<uint8_t>(row.kind());
        item.npcPositionRowIndex = static_cast<int16_t>(i);
        item.selectorId = row.interactionSelector();
        item.promptKind = row.kind();
        FstageSpawnTransform transform;
        if(getFstageSpawnTransform(row, &transform))
        {
            item.position = {
                transform.position.x, transform.position.y,
                transform.position.z};
        }
        item.range = row.kind() == GAME_INTERACTION_OBJECT ?
            GAME_INTERACTION_OBJECT_RANGE :
            fstageCharacterInteractionRadius(
                static_cast<uint16_t>(row.selector()));
    }
    bindNpcPositionRow(persistent.player);
    bindNpcPositionRow(enemy);
    for(int i = 0; i < activeOpponentCount - 1; ++i)
        bindNpcPositionRow(additionalEnemies[i]);
    if(allyActive)
        bindNpcPositionRow(ally);
}

StoryProgressionPhase
Game::storyProgressionPhase(void) const
{
    return persistent.story.progression.phase;
}

void
Game::setStoryProgressionPhase(StoryProgressionPhase phase)
{
    if(phase >= STORY_PROGRESSION_PHASE_BOOT &&
       phase < STORY_PROGRESSION_PHASE_COUNT)
        persistent.story.progression.phase = phase;
}

uint16_t Game::storyPromptPhase(void) const
{
    return static_cast<uint16_t>(
        persistent.story.progression.promptPhase);
}

void
Game::setStoryPromptPhase(uint16_t phase)
{
    persistent.story.progression.promptPhase =
        static_cast<int16_t>(phase);
}

void
Game::requestFstageMarkerTransition(uint16_t destinationStageId,
                                    int16_t arrivalSelector)
{

    queueStoryStageTransition(
        persistent.story, destinationStageId,
        eventVm.eventId(), arrivalSelector);
}

bool
Game::storyStageLoadBoundaryPending(void) const
{
    return persistent.story.stageTransitionPending != 0 &&
           persistent.story.stageTransition.lifecycle ==
               STORY_TRANSITION_STAGE_LOAD;
}

uint8_t
Game::storyStageLoadState(void) const
{
    return persistent.story.stageTransition.loadState;
}

void
Game::initializeLoadedStoryField(void)
{
    battleResources = {};
    resetBehaviourPresentation();
    if(savedFieldResumePending)
        savedFieldResumePending = false;
    else if(stageNavigation.valid)
    {

        persistent.player.position = {
            stageNavigation.spawnX,
            stageNavigation.spawnY,
            stageNavigation.spawnZ
        };
        persistent.player.headingDegrees =
            stageNavigation.spawnHeadingDegrees;
    }
    else if(pendingBattleSpawnValid)
    {

        persistent.player.position = pendingBattleSpawn;
        persistent.player.headingDegrees = pendingBattleSpawnHeading;
    }
    pendingBattleSpawnValid = false;
    persistent.story.battleActive = 0;
    if(persistent.story.stageId == GAME_FIRST_PLAYABLE_STAGE_ID &&
       persistent.story.progression.phase == STORY_PROGRESSION_PHASE_PROLOGUE)
    {

        persistent.story.pickpocketMode = 1;
        setStorySaveFlag(persistent.story, 0x0df0, true);
    }
    persistent.story.resultDisplayActive = 0;
    currentBattleOutcome = BATTLE_OUTCOME_IN_PROGRESS;
    battleTimeScale = BATTLE_TIME_SCALE_NORMAL;
    battleElapsedSeconds = 0.0f;
    for(int i = 0; i < activeOpponentCount; ++i)
    {
        const GameSlotRecord& slot =
            i == 0 ? enemy : additionalEnemies[i - 1];
        StoryActorState* actor = findStoryActor(
            persistent.story, static_cast<uint16_t>(slot.characterId), false);
        if(actor != NULL)
            actor->active = 0;
    }
    activeOpponentCount = 0;
    targetOpponentIndex = 0;
    allyActive = false;
    resetBattleResultController();
    memset(&enemy, 0, sizeof(enemy));
    memset(additionalEnemies, 0, sizeof(additionalEnemies));
    persistent.player.velocity.x = 0.0f;
    persistent.player.velocity.z = 0.0f;
    startAction(persistent.player, COMBAT_ACTION_IDLE);
    bindNpcPositionRow(persistent.player);
    synchronizeBattleStoryState();
    currentScreen = GAME_SCREEN_GAMEPLAY;
    switchGameFlowTaskHandler(updateGameplaySessionFlow, 1);
}

bool
Game::pollStoryStageLoadBoundary(bool resourceReady)
{
    StoryState& story = persistent.story;
    StoryStageTransition& transition = story.stageTransition;
    if(!storyStageLoadBoundaryPending())
        return false;

    switch(transition.loadState)
    {
    case STORY_STAGE_LOAD_OL_WEAPON:
        transition.loadState = STORY_STAGE_LOAD_QUEUE_ST4;
        return false;
    case STORY_STAGE_LOAD_QUEUE_ST4:
    {
        uint16_t destination = transition.destinationStageId;
        if(destination == 0 || destination >= 0x39)
            destination = 4;
        persistent.stageId = destination;
        story.stageId = destination;
        pendingBattleSpawn.x = transition.arrival[0];
        pendingBattleSpawn.y = transition.arrival[1];
        pendingBattleSpawn.z = transition.arrival[2];
        pendingBattleSpawnHeading = transition.headingDegrees;
        pendingBattleSpawnValid = true;
        transition.loadState = STORY_STAGE_LOAD_WAIT_ST4;
        return false;
    }
    case STORY_STAGE_LOAD_WAIT_ST4:
        transition.loadState = STORY_STAGE_LOAD_INITIALIZE_ST4;
        return false;
    case STORY_STAGE_LOAD_INITIALIZE_ST4:
        if(!resourceReady)
            return false;
        transition.loadState = STORY_STAGE_LOAD_DISPLAY;
        return false;
    case STORY_STAGE_LOAD_DISPLAY:
    case STORY_STAGE_LOAD_BEGIN_EFFECTS:
    case STORY_STAGE_LOAD_WAIT_EFFECTS:
    case STORY_STAGE_LOAD_BEGIN_SCENE_SOUND:
    case STORY_STAGE_LOAD_WAIT_SCENE_SOUND:
    case STORY_STAGE_LOAD_BEGIN_AUDIO:
    case STORY_STAGE_LOAD_WAIT_AUDIO:
    case STORY_STAGE_LOAD_FINALIZE_AUDIO:
        transition.loadState =
            static_cast<uint8_t>(transition.loadState + 1);
        return false;
    case STORY_STAGE_LOAD_BIND_EFFECTS:
    {
        transition.lifecycle = STORY_TRANSITION_FIELD_RESUME;
        initializeLoadedStoryField();
        story.stageTransitionPending = 0;
        const uint16_t completedEventId = transition.queuedEventId;
        memset(&transition, 0, sizeof(transition));
        transition.queuedEventId = completedEventId;
        transition.actorProfileId = 0xffff;
        transition.arrivalSelector = -1;
        return true;
    }
    default:
        return false;
    }
}
