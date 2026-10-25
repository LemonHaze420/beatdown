#include "stage_interactions.h"

#include "game.h"
#include "story_state.h"
#include "xaf_runtime.h"

#include <vector>

namespace
{

struct StageNpcSelectionQueryContext
{
    Game* game;
    StoryState* story;
    bool suppressPostBattleOverlays;
};

bool stageNpcBaseSelectorAvailable(int16_t selector, void* userData)
{
    const StageNpcSelectionQueryContext* const context =
        static_cast<const StageNpcSelectionQueryContext*>(userData);
    const StoryState* const story =
        context != NULL ? context->story : NULL;

    return story != NULL &&
        storyNpcBaseSelectorAvailable(*story, selector);
}

bool stageEventFlagQuery(uint16_t eventFlag, void* userData)
{
    const StageNpcSelectionQueryContext* const context =
        static_cast<const StageNpcSelectionQueryContext*>(userData);
    const StoryState* const story =
        context != NULL ? context->story : NULL;
    return story != NULL && getStorySaveFlag(*story, eventFlag);
}

PersistentGameRecord* stageNpcPersistentRecord(int16_t characterId,
                                                void* userData)
{
    Game* const game = static_cast<Game*>(userData);
    return game != NULL && characterId >= 0 ?
        game->characterRecord(static_cast<uint16_t>(characterId)) : NULL;
}

FstageNpcOverlayDecision stageNpcOverlayQuery(
    const FstageNpcPositionRow& row, uint32_t, bool,
    int16_t* resolvedSelector, void* userData)
{
    StageNpcSelectionQueryContext* const context =
        static_cast<StageNpcSelectionQueryContext*>(userData);
    if(context == NULL || context->story == NULL || context->game == NULL)
        return FSTAGE_NPC_OVERLAY_KEEP;

    int16_t activeRecordIds[3];
    for(size_t i = 0; i < 3; ++i)
        activeRecordIds[i] = context->game->activeRecordId(i);

    const StoryNpcOverlayDecision decision = storyNpcOverlayDecision(
        *context->story, row.selector(),
        context->game->indexedPersistentValue(6) == 1,
        context->suppressPostBattleOverlays, activeRecordIds,
        stageNpcPersistentRecord, context->game, resolvedSelector);
    switch(decision)
    {
    case STORY_NPC_OVERLAY_COPY:
        return FSTAGE_NPC_OVERLAY_COPY;
    case STORY_NPC_OVERLAY_CLEAR:
        return FSTAGE_NPC_OVERLAY_CLEAR;
    case STORY_NPC_OVERLAY_KEEP:
    default:
        return FSTAGE_NPC_OVERLAY_KEEP;
    }
}

int16_t stageRecruitmentStateQuery(uint16_t phase, uint16_t characterId,
                                   void* userData)
{
    StageNpcSelectionQueryContext* const context =
        static_cast<StageNpcSelectionQueryContext*>(userData);
    StoryState* const story = context != NULL ? context->story : NULL;
    Game* const game = context != NULL ? context->game : NULL;
    if(story == NULL || game == NULL)
        return 0;

    story->recruitmentPlayerResourceProfile =
        static_cast<int16_t>(game->runtimeState().player.resourceProfileId);
    if(characterId < STORY_CHARACTER_COUNT)
    {

        StoryCharacterState& character = story->characters[characterId];
        character.characterFlags &= ~STORY_CHARACTER_RECRUITED;
        const PersistentGameRecord* const record =
            game->characterRecord(characterId);
        if(record != NULL && (record->characterFlags & 1u) != 0)
            character.characterFlags |= STORY_CHARACTER_RECRUITED;
    }

    if(canRecruitStoryCharacter(*story, phase, characterId))
        story->scratch005f24bc = 1;
    return storyRecruitmentState(*story, phase, characterId);
}

uint32_t installedStoryRandom(void* userData)
{
    Game* const game = static_cast<Game*>(userData);
    return game != NULL ? game->nextRandom() : 0;
}

}

bool installStageInteractions(
    const char* archivePath, Game& game, FstageNpcPositionData& positions,
    uint16_t stageId, FstageScene* scene,
    const FstageObjectSpawnState* objectState,
    FstageObjectInstallResult* objectResult,
    size_t* selectedRowCount)
{
    if(selectedRowCount != NULL)
        *selectedRowCount = 0;
    if(positions.bytes().empty())
    {
        std::vector<uint8_t> data;
        const bool loaded =
            archivePath != NULL &&
            readXafEntry(archivePath, "NPOS_DATA.DAT", data) &&
            positions.parse(data.data(), data.size());
        if(!loaded &&
           (archivePath == NULL || !positions.load(archivePath)))
        {
            game.clearStageInteractions();
            return false;
        }
    }

    GameRuntimeState& persistent = game.runtimeState();
    FstageNpcSelectionState selection;
    selection.stageId = stageId;
    selection.storyPhase =
        static_cast<uint16_t>(game.storyProgressionPhase());
    StoryState& story = persistent.story;

    selection.gameStateBit1 = (story.gameStateFlags & 0x2u) != 0;
    selection.storyTransitionMode =
        static_cast<int16_t>(storyTransitionModeValue(story));
    selection.alternateOverlayMode =
        static_cast<int16_t>(storyAlternateOverlayMode(story));

    selection.partyRoots[0] = storyNpcBeforeRoot(story, stageId);
    selection.partyRoots[1] = -1;

    selection.suppressPartyOverlay = stageId == 0;
    selection.trackedOpponent = (story.playerConditionFlags & 0x1u) != 0;
    selection.actorResourceProfile =
        static_cast<int16_t>(persistent.player.resourceProfileId);
    selection.recruitmentMode = story.recruitmentMode;
    selection.resetRecruitmentRoot = story.resetRecruitmentRoot != 0;
    selection.encounterSubstate = story.encounterSubstate != 0;
    selection.phase10Stage31Suppress = story.phase10Stage31Suppress != 0;
    selection.phase11ModeFlags = story.phase11ModeFlags;

    selection.phase11Stage49Override =
        game.indexedPersistentValue(0x0d) != 0;

    bool activePartyBattleFlag = false;
    for(int partyIndex = 1; partyIndex < 3; ++partyIndex)
    {
        const PersistentGameRecord* const record =
            game.characterRecord(story.party[partyIndex]);
        if(record != NULL && (record->flags_0x388 & 0x8u) != 0)
        {
            activePartyBattleFlag = true;
            break;
        }
    }

    storyResolveNpcBattleRoot(
        story, stageId, game.currentTick(), activePartyBattleFlag,
        game.indexedPersistentValue(6) == 1, installedStoryRandom, &game,
        &selection.battleRoot, &selection.suppressPostBattleOverlays);
    StageNpcSelectionQueryContext selectionContext = {
        &game, &story, selection.suppressPostBattleOverlays};
    selection.userData = &selectionContext;
    selection.baseSelectorAvailable = stageNpcBaseSelectorAvailable;
    selection.eventFlag = stageEventFlagQuery;
    selection.recruitmentState = stageRecruitmentStateQuery;
    selection.overlayDecision = stageNpcOverlayQuery;

    persistent.story.resetRecruitmentRoot = 0;

    std::vector<FstageNpcPositionRow> rows;
    if(!selectFstageNpcPositionRecords(positions, selection, rows))
    {
        game.clearStageInteractions();
        return false;
    }
    game.setFstageNpcPositionRows(rows, stageId);
    if(selectedRowCount != NULL)
        *selectedRowCount = rows.size();

    if(scene != NULL && objectState != NULL)
    {
        FstageObjectSpawnState objects = *objectState;
        objects.storyPhase = selection.storyPhase;
        objects.gameStateBit1 = selection.gameStateBit1;
        if(!installFstageStageObjects(scene, stageId, objects, objectResult))
            return false;
    }
    return true;
}
