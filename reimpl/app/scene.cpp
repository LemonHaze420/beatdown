#include "game/combat/animation.h"
#include "app/internal.h"

namespace app_detail
{

const char* selectedSceneActorName(void)
{
    uint16_t textId = 0;
    switch(g_game.selectedCharacterId())
    {
    case CHARACTER_RAVEN:   textId = GAME_TEXT_CHARACTER_RAVEN_NAME; break;
    case CHARACTER_AARON:   textId = GAME_TEXT_CHARACTER_AARON_NAME; break;
    case CHARACTER_LOLA:    textId = GAME_TEXT_CHARACTER_LOLA_NAME; break;
    case CHARACTER_GINA:    textId = GAME_TEXT_CHARACTER_GINA_NAME; break;
    case CHARACTER_JASON_G: textId = GAME_TEXT_CHARACTER_JASON_G_NAME; break;
    default: return NULL;
    }
    return presentationText(textId, "");
}

int sceneMotionIdFromPath(const char* path)
{
    if(path == NULL)
        return -1;
    const char* name = path;
    for(const char* c = path; *c != 0; ++c)
    {
        if(*c == '\\' || *c == '/')
            name = c + 1;
    }
    if(*name < '0' || *name > '9')
        return -1;
    const long value = strtol(name, NULL, 10);
    return value > 0 && value < 0x8000 ? static_cast<int>(value) : -1;
}

SceneMotionCursor* sceneMotionCursorFor(const CharacterScene* scene)
{
    SceneMotionCursor* free = NULL;
    for(int i = 0; i < SCENE_ACTOR_CAPACITY; ++i)
    {
        if(g_sceneMotionCursors[i].scene == scene)
            return &g_sceneMotionCursors[i];
        if(free == NULL && g_sceneMotionCursors[i].scene == NULL)
            free = &g_sceneMotionCursors[i];
    }
    if(free != NULL)
    {
        free->scene = scene;
        free->moveId = -1;
        free->index = 0;
        free->startSeconds = 0.0f;
        free->clipSeconds = -1.0f;
        free->rootMotion = SceneActorRootMotionState();
        free->ptaskMotions.clear();
        free->ptaskLoopStart = -1;
        free->sequenceGeneration = 0;
    }
    return free;
}

void resetSceneMotionCursors(void)
{
    for(int i = 0; i < SCENE_ACTOR_CAPACITY; ++i)
    {
        g_sceneMotionCursors[i].scene = NULL;
        g_sceneMotionCursors[i].rootMotion = SceneActorRootMotionState();
        g_sceneMotionCursors[i].ptaskMotions.clear();
        g_sceneMotionCursors[i].ptaskLoopStart = -1;
        g_sceneMotionCursors[i].sequenceGeneration = 0;
    }
}

bool sceneActorRootWorldPosition(const CharacterScene* scene,
                                 SceneVec3* position)
{
    if(scene == NULL || position == NULL || scene->hierarchies.empty())
        return false;
    const rw::HAnimHierarchy* hierarchy = scene->hierarchies[0];
    if(hierarchy == NULL || hierarchy->matrices == NULL ||
       hierarchy->numNodes <= 0)
        return false;

    const int rootIndex = hierarchy->numNodes > 1 ? 1 : 0;
    const rw::V3d& root = hierarchy->matrices[rootIndex].pos;
    *position = SceneVec3(root.x, root.y, root.z);
    return true;
}

bool sceneActorFramePosition(const CharacterScene* scene,
                             SceneVec3* position)
{
    if(scene == NULL || position == NULL || scene->clumps.empty())
        return false;
    const rw::Frame* frame = scene->clumps[0]->getFrame();
    if(frame == NULL)
        return false;
    const rw::V3d& world = frame->matrix.pos;
    *position = SceneVec3(world.x, world.y, world.z);
    return true;
}

bool commitSceneActorMotionPosition(CharacterScene* scene,
                                    SceneMotionCursor& cursor)
{
    SceneVec3 root;
    if(!sceneActorRootWorldPosition(scene, &root) ||
       !commitSceneActorRootMotion(cursor.rootMotion, root))
        return false;
    SceneActorPresentationState* presentation =
        sceneActorPresentationFor(scene);
    if(presentation != NULL && presentation->placement.placed)
        presentation->placement.logicalPosition =
            cursor.rootMotion.logicalPosition;
    return true;
}

void resetSceneActorMotionPosition(CharacterScene* scene)
{
    SceneMotionCursor* cursor = sceneMotionCursorFor(scene);
    if(cursor != NULL)
        cursor->rootMotion = SceneActorRootMotionState();
}

bool moveMotionSequenceLoops(int moveId)
{
    if(moveId < 0 || moveId >= MOVE_RECORD_COUNT)
        return false;
    const MoveStream& stream = MOVE_STREAMS[moveId];
    if(stream.firstWord < 0 || stream.wordCount <= 0)
        return false;

    const int16_t* words = &MOVE_STREAM_WORDS[stream.firstWord];
    int depth = 0;
    int lastPlayDepth = -1;
    for(int i = 0; i < stream.wordCount;)
    {
        const int16_t opcode = words[i];
        if(opcode == MOVE_STREAM_LOOP_BEGIN)
            depth++;
        else if(opcode == MOVE_STREAM_LOOP_END && depth > 0)
            depth--;
        else if(opcode == MOVE_STREAM_PLAY_MOTION)
            lastPlayDepth = depth;
        else if(opcode == MOVE_STREAM_END)
            break;
        const int advance =
            moveStreamInstructionWords(&words[i], stream.wordCount - i);
        if(advance <= 0)
            break;
        i += advance;
    }
    return lastPlayDepth > 0;
}

bool bindSceneActorMotion(CharacterScene* scene, int motionResourceSet,
                          const char* mtdPath, float elapsedSeconds)
{
    const int moveId = sceneMotionIdFromPath(mtdPath);
    if(scene == NULL || moveId < 0)
        return false;

    SceneMotionCursor* cursor = sceneMotionCursorFor(scene);
    if(cursor == NULL)
        return false;
    const int previousMoveId = cursor->moveId;
    const int previousIndex = cursor->index;
    const uint8_t previousGeneration = cursor->sequenceGeneration;
    if(cursor->moveId != moveId)
    {
        cursor->moveId = moveId;
        cursor->index = 0;
        cursor->startSeconds = elapsedSeconds;
        cursor->clipSeconds = -1.0f;
        cursor->sequenceGeneration = 0;
        cursor->ptaskMotions.clear();
        cursor->ptaskLoopStart = -1;

        ScenePtaskMotionProgram program;
        if(buildScenePtaskMotionProgram(g_scenePtasks, moveId, program))
        {
            cursor->ptaskMotions.swap(program.motions);
            cursor->ptaskLoopStart = program.loopStart;
        }
    }

    const int count = static_cast<int>(cursor->ptaskMotions.size());
    if(count <= 0)
        return false;

    advanceSceneMotionPlayback(
        *cursor, elapsedSeconds, count, cursor->ptaskLoopStart);
    const bool sequenceChanged =
        previousMoveId != cursor->moveId || previousIndex != cursor->index ||
        previousGeneration != cursor->sequenceGeneration;
    if(sequenceChanged)
        commitSceneActorMotionPosition(scene, *cursor);
    if(cursor->index < 0 || cursor->index >= count)
        return false;

    const ScenePtaskMotion& motion = cursor->ptaskMotions[cursor->index];
    if(motion.motionId <= 0)
        return false;

    size_t animation = 0;
    if(!loadCharacterSceneAnimation(
           g_stagePath, motionResourceSet, motion.motionId, scene, &animation))
    {
        printf("scene motion %d provider could not be decoded\n",
               motion.motionId);
        return false;
    }
    float clipEndSeconds = 0.0f;
    if(animation < scene->animations.size() &&
       scene->animations[animation] != NULL)
    {
        float clipSeconds =
            static_cast<float>(scene->animations[animation]->duration);

        if(motion.maxDurationFrames != -1)
        {
            const float capSeconds =
                static_cast<float>(motion.maxDurationFrames) / 60.0f;
            if(capSeconds <= clipSeconds)
                clipSeconds = capSeconds;
        }
        clipEndSeconds = clipSeconds;
    }

    const CharacterAnimationPlayback playback =
        makeCharacterAnimationPlayback(
            clipEndSeconds, motion.blendFrames,
            motion.maxDurationFrames, motion.transitionFrames,
            1.0f, false, false);
    cursor->clipSeconds = playback.endSeconds - playback.startSeconds;
    if(cursor->clipSeconds < 0.0f)
        cursor->clipSeconds = 0.0f;
    const uint32_t playbackState =
        0x80000000u |
        (static_cast<uint32_t>(moveId & 0x7fff) << 16) |
        (static_cast<uint32_t>(cursor->sequenceGeneration) << 8) |
        static_cast<uint32_t>(cursor->index & 0xff);
    bindCharacterSceneAnimation(
        scene, animation, playbackState,
        elapsedSeconds - cursor->startSeconds, playback);
    if(sequenceChanged || !cursor->rootMotion.active)
    {
        SceneVec3 incomingRoot;
        SceneVec3 logicalPosition;
        if(sceneActorRootWorldPosition(scene, &incomingRoot) &&
           (cursor->rootMotion.active ||
            sceneActorFramePosition(scene, &logicalPosition)))
        {
            if(cursor->rootMotion.active)
                logicalPosition = cursor->rootMotion.logicalPosition;
            if(cursor->rootMotion.active && cursor->rootMotion.committed)
            {
                alignCharacterSceneAnimationRoot(
                    scene, cursor->rootMotion.terminalRootWorldPosition.x,
                    cursor->rootMotion.terminalRootWorldPosition.z);
                sceneActorRootWorldPosition(scene, &incomingRoot);
            }
            beginSceneActorRootMotion(
                cursor->rootMotion, logicalPosition, incomingRoot);
        }
    }
    return true;
}

void retainSceneActorMotionPosition(
    CharacterScene* scene, SceneActorPresentationState* presentation)
{
    SceneMotionCursor* cursor = sceneMotionCursorFor(scene);
    if(cursor == NULL || !cursor->rootMotion.active)
        return;
    commitSceneActorMotionPosition(scene, *cursor);
    if(scene == NULL || scene->clumps.empty())
        return;
    rw::Frame* frame = scene->clumps[0]->getFrame();
    if(frame == NULL)
        return;

    frame->matrix.pos.x = cursor->rootMotion.logicalPosition.x;
    frame->matrix.pos.z = cursor->rootMotion.logicalPosition.z;
    frame->matrix.optimize();
    frame->updateObjects();
    if(presentation != NULL && presentation->placement.placed)
    {
        presentation->placement.logicalPosition.x =
            cursor->rootMotion.logicalPosition.x;
        presentation->placement.logicalPosition.z =
            cursor->rootMotion.logicalPosition.z;
    }
}

bool sceneActorCharacterId(const char* resourceName, uint16_t* characterId,
                           bool* archiveMode)
{
    if(resourceName == NULL || resourceName[0] < '0' || resourceName[0] > '9')
        return false;
    char* end = NULL;
    const unsigned long value = strtoul(resourceName, &end, 10);
    if(value == 0 || value > 0xffffu)
        return false;
    if(characterId != NULL)
        *characterId = static_cast<uint16_t>(value);
    if(archiveMode != NULL)
        *archiveMode =
            characterSceneModelMode(resourceName) ==
            CHARACTER_SCENE_MODEL_ARCHIVE;
    return true;
}

SceneActorPresentationState* sceneActorPresentationFor(CharacterScene* scene)
{
    if(scene == &g_scene.player)
        return &g_scene.playerScenePlacement;
    for(int i = 0; i < SCENE_ACTOR_CAPACITY; ++i)
        if(scene == &g_scene.sceneActors[i])
            return &g_scene.sceneActorPlacements[i];
    return NULL;
}

void resetSceneActorPlacements(void)
{
    g_scene.playerScenePlacement = SceneActorPresentationState();
    for(int i = 0; i < SCENE_ACTOR_CAPACITY; ++i)
        g_scene.sceneActorPlacements[i] = SceneActorPresentationState();
}

void beginSceneActorPresentationFrame(
    SceneActorPresentationState& presentation)
{
    presentation.incomingPlacement = presentation.authoredPlacement;
    presentation.touched = false;
    presentation.placementChanged = false;
    presentation.battleSpawn = false;
}

void beginSceneActorFrame(void)
{
    beginSceneActorPresentationFrame(g_scene.playerScenePlacement);
    for(int i = 0; i < SCENE_ACTOR_CAPACITY; ++i)
        beginSceneActorPresentationFrame(
            g_scene.sceneActorPlacements[i]);
}

bool sceneActorPlacementMatches(const SceneActorPlacementState& left,
                                const SceneActorPlacementState& right)
{
    return left.placed == right.placed &&
           left.logicalPosition.x == right.logicalPosition.x &&
           left.logicalPosition.y == right.logicalPosition.y &&
           left.logicalPosition.z == right.logicalPosition.z &&
           left.heading == right.heading;
}

void settleSceneActorPlacement(
    CharacterScene* scene, SceneActorPresentationState& presentation)
{
    if(!presentation.touched)
        return;
    presentation.placementChanged = !sceneActorPlacementMatches(
        presentation.authoredPlacement, presentation.incomingPlacement);
    if(presentation.placementChanged)
    {
        presentation.authoredPlacement = presentation.incomingPlacement;
        presentation.placement = presentation.incomingPlacement;
        resetSceneActorMotionPosition(scene);
    }
    else
    {
        presentation.placement.noStageCheck =
            presentation.incomingPlacement.noStageCheck;
    }
}

void settleSceneActorPlacements(void)
{
    settleSceneActorPlacement(
        &g_scene.player, g_scene.playerScenePlacement);
    for(int i = 0; i < SCENE_ACTOR_CAPACITY; ++i)
    {
        settleSceneActorPlacement(
            &g_scene.sceneActors[i],
            g_scene.sceneActorPlacements[i]);
    }
}

bool sceneActorGroundHeight(void*, float x, float z, float ceiling,
                            float* height)
{
    return g_scene.stage.worldClump != NULL &&
           rwWorldGroundHeight(g_scene.stage.worldGeometry, x, z, ceiling,
                               height);
}

float sceneActorRootY(const CharacterScene* scene, float fallback)
{
    if(scene == NULL || scene->clumps.empty() ||
       scene->clumps[0]->getFrame() == NULL)
        return fallback;
    return scene->clumps[0]->getFrame()->matrix.pos.y;
}

void presentSceneActor(CharacterScene* scene,
                       SceneActorPresentationState& presentation,
                       float scaledDeltaSeconds)
{
    if(!presentation.touched || !presentation.placement.placed)
        return;

    SceneActorPlacementState& placement = presentation.placement;
    const float currentRootY = presentation.placementChanged ?
        placement.logicalPosition.y :
        sceneActorRootY(scene, placement.renderedRootY);
    float renderedRootY = currentRootY;
    const bool rootChanged = resolveSceneActorRootY(
        placement, currentRootY, scaledDeltaSeconds,
        sceneActorGroundHeight, NULL, &renderedRootY);

    if(presentation.placementChanged)
    {
        setCharacterSceneTransform(
            scene, placement.logicalPosition.x,
            rootChanged ? renderedRootY : placement.logicalPosition.y,
            placement.logicalPosition.z, placement.heading, 0.0f, 0.0f);
        placement.renderedRootY =
            rootChanged ? renderedRootY : placement.logicalPosition.y;
    }
    else if(rootChanged)
    {
        setCharacterSceneRootY(scene, renderedRootY);
    }
    SceneMotionCursor* cursor = sceneMotionCursorFor(scene);
    if(cursor != NULL && cursor->moveId >= 0 &&
       !cursor->rootMotion.active)
    {
        SceneVec3 logicalPosition;
        SceneVec3 rootWorldPosition;
        if(sceneActorFramePosition(scene, &logicalPosition) &&
           sceneActorRootWorldPosition(scene, &rootWorldPosition))
        {
            beginSceneActorRootMotion(
                cursor->rootMotion, logicalPosition, rootWorldPosition);
        }
    }
    if(presentation.battleSpawn)
        g_game.setPendingBattleSpawn(
            placement.logicalPosition.x, placement.logicalPosition.y,
            placement.logicalPosition.z, placement.heading);
}

void presentSceneActors(float scaledDeltaSeconds)
{
    presentSceneActor(&g_scene.player, g_scene.playerScenePlacement,
                      scaledDeltaSeconds);
    for(int i = 0; i < SCENE_ACTOR_CAPACITY; ++i)
        presentSceneActor(&g_scene.sceneActors[i],
                          g_scene.sceneActorPlacements[i],
                          scaledDeltaSeconds);
}

void setCharacterSceneVisible(CharacterScene* scene, bool visible)
{
    if(scene == NULL)
        return;
    for(size_t i = 0; i < scene->clumps.size(); ++i)
    {
        rw::Clump* clump = scene->clumps[i];
        if(clump == NULL)
            continue;
        FORLIST(link, clump->atomics)
        {
            rw::Atomic* atomic = rw::Atomic::fromClump(link);
            if(atomic == NULL)
                continue;
            uint32_t flags = atomic->getFlags();
            if(visible)
                flags |= rw::Atomic::RENDER;
            else
                flags &= ~rw::Atomic::RENDER;
            atomic->setFlags(flags);
        }
    }
}

void updateSceneCharacterVisibility(bool sceneActive)
{
    const bool playerInScene =
        g_scene.playerScenePlacement.touched &&
        g_scene.playerScenePlacement.placement.placed;
    setCharacterSceneVisible(&g_scene.player,
                             !sceneActive || playerInScene);
    setCharacterSceneVisible(&g_scene.ally, !sceneActive);
    setCharacterSceneVisible(&g_scene.opponent, !sceneActive);
    for(int i = 1; i < MAX_GAME_OPPONENTS; ++i)
        setCharacterSceneVisible(&g_scene.additionalOpponents[i - 1],
                                 !sceneActive);
    const bool showStoryActors =
        !sceneActive && g_game.mode() == GAME_MODE_STORY;
    for(int i = 0; i < STORY_ACTOR_SCENE_CAPACITY; ++i)
        setCharacterSceneVisible(&g_scene.storyActors[i], showStoryActors);
    for(int i = 0; i < SCENE_ACTOR_CAPACITY; ++i)
    {
        const bool active =
            sceneActive && g_scene.sceneActorPlacements[i].touched &&
            g_scene.sceneActorPlacements[i].placement.placed;
        setCharacterSceneVisible(&g_scene.sceneActors[i], active);
    }
}

bool appendCharacterShadowActor(
    CharacterShadowActor* actors, size_t capacity, size_t& count,
    const CharacterScene* scene)
{
    if(actors == NULL || count >= capacity || scene == NULL ||
       scene->clumps.empty() || scene->clumps[0] == NULL)
    {
        return false;
    }
    bool visible = false;
    for(size_t i = 0; i < scene->clumps.size() && !visible; ++i)
    {
        rw::Clump* clump = scene->clumps[i];
        if(clump == NULL)
            continue;
        FORLIST(link, clump->atomics)
        {
            rw::Atomic* atomic = rw::Atomic::fromClump(link);
            visible = atomic != NULL &&
                (atomic->getFlags() & rw::Atomic::RENDER) != 0;
            if(visible)
                break;
        }
    }
    rw::Frame* frame = scene->clumps[0]->getFrame();
    if(!visible || frame == NULL)
        return false;
    const rw::V3d& position = frame->getLTM()->pos;
    actors[count++] = { scene, position.x, position.y, position.z };
    return true;
}

size_t gatherCharacterShadowActors(
    CharacterShadowActor* actors, size_t capacity)
{
    const bool sceneActive =
        g_sceneRuntime.status() == SceneStatus::Playing;
    const GameScreen screen = g_game.screen();
    if(!sceneActive &&
       screen != GAME_SCREEN_GAMEPLAY &&
       screen != GAME_SCREEN_PAUSE &&
       screen != GAME_SCREEN_INVENTORY &&
       screen != GAME_SCREEN_OPTIONS &&
       screen != GAME_SCREEN_RESULTS)
    {
        return 0;
    }

    size_t count = 0;
    appendCharacterShadowActor(
        actors, capacity, count, &g_scene.player);
    if(sceneActive)
    {
        for(int i = 0; i < SCENE_ACTOR_CAPACITY; ++i)
        {
            appendCharacterShadowActor(
                actors, capacity, count, &g_scene.sceneActors[i]);
        }
        return count;
    }

    if(g_game.hasAlly())
    {
        appendCharacterShadowActor(
            actors, capacity, count, &g_scene.ally);
    }
    for(int i = 0; i < g_game.opponentCount(); ++i)
    {
        appendCharacterShadowActor(
            actors, capacity, count,
            i == 0 ? &g_scene.opponent :
                &g_scene.additionalOpponents[i - 1]);
    }
    if(g_game.mode() == GAME_MODE_STORY)
    {
        for(int i = 0; i < STORY_ACTOR_SCENE_CAPACITY; ++i)
        {
            appendCharacterShadowActor(
                actors, capacity, count, &g_scene.storyActors[i]);
        }
    }

    return count;
}

void renderAttachedClump(rw::Clump* clump)
{
    if(clump != NULL && clump->world == g_scene.world)
        clump->render();
}

void renderStageClumps(void)
{
    for(size_t i = 0; i < g_scene.stage.clumps.size(); ++i)
        renderAttachedClump(g_scene.stage.clumps[i]);
    for(size_t i = 0; i < g_scene.stage.stageObjects.size(); ++i)
        renderAttachedClump(g_scene.stage.stageObjects[i].clump);

    for(uint16_t i = 0;
        i < g_scene.stage.stageWeapons.managerCount; ++i)
    {
        rw::Clump* clump = g_scene.stage.stageWeapons.records[i].clump;
        bool duplicate = false;
        for(uint16_t j = 0; j < i; ++j)
        {
            if(g_scene.stage.stageWeapons.records[j].clump == clump)
            {
                duplicate = true;
                break;
            }
        }
        if(!duplicate)
            renderAttachedClump(clump);
    }
}

void renderCharacterClumps(const CharacterScene& scene)
{
    size_t hiddenFirst = scene.clumps.size();
    size_t hiddenEnd = hiddenFirst;
    if(scene.assembledFromArchive)
    {
        for(size_t group = 0; group < scene.attachments.size(); ++group)
        {
            const CharacterAttachment& attachment = scene.attachments[group];
            if(!attachment.active || attachment.metadata.type != 21)
                continue;

            hiddenFirst = attachment.firstClump;
            hiddenEnd = attachment.firstClump + attachment.clumpCount;
            break;
        }
    }

    for(size_t i = 0; i < scene.clumps.size(); ++i)
    {
        if(i >= hiddenFirst && i < hiddenEnd)
            continue;
        renderAttachedClump(scene.clumps[i]);
    }
}

void renderActorAndPropClumps(void)
{
    renderCharacterClumps(g_scene.player);
    renderCharacterClumps(g_scene.ally);
    renderCharacterClumps(g_scene.opponent);
    for(int i = 0; i < MAX_GAME_OPPONENTS - 1; ++i)
        renderCharacterClumps(g_scene.additionalOpponents[i]);
    for(int i = 0; i < STORY_ACTOR_SCENE_CAPACITY; ++i)
        renderCharacterClumps(g_scene.storyActors[i]);
    for(int i = 0; i < SCENE_ACTOR_CAPACITY; ++i)
        renderCharacterClumps(g_scene.sceneActors[i]);
    for(size_t i = 0; i < g_sceneProps.props.size(); ++i)
        renderAttachedClump(g_sceneProps.props[i].clump);
}

CharacterScene* sceneActorFor(const char* trackName, const char* resourceName)
{
    if(trackName == NULL)
        return NULL;

    uint16_t characterId = 0;
    bool archiveMode = false;
    const bool haveCharacter =
        sceneActorCharacterId(resourceName, &characterId, &archiveMode);
    const CharacterRecord* character = haveCharacter ?
        characterRecordForId(characterId) : NULL;
    const int mappedResource = character != NULL ? character->resourceId : -1;
    const bool haveResource = mappedResource > 0 && mappedResource <= 0xffff;
    const uint16_t resource = haveResource ?
        static_cast<uint16_t>(mappedResource) : 0;

    int slot = -1;
    int free = -1;
    for(int i = 0; i < SCENE_ACTOR_CAPACITY && slot < 0; ++i)
    {
        if(g_scene.sceneActorTracks[i] == trackName)
            slot = i;
        else if(free < 0 && g_scene.sceneActorTracks[i].empty())
            free = i;
    }
    if(slot < 0 && haveResource)
        slot = free;
    if(slot < 0)
        return NULL;

    const bool archiveRetrySuppressed =
        archiveMode && g_scene.sceneActorArchiveFallbacks[slot];
    if(haveResource &&
       (g_scene.sceneActorResourceIds[slot] != resource ||
        (g_scene.sceneActorArchiveModes[slot] != archiveMode &&
         !archiveRetrySuppressed)))
    {

        bool loaded = false;
        bool archiveFallback = false;

        const bool archiveRouteEnabled = true;
        if(archiveMode && archiveRouteEnabled)
        {
            CharacterCustomization customization = {};
            if(g_game.characterCustomization(characterId, customization))
                loaded = loadCharacterArchiveScene(
                    g_archivePath, g_dataPath, customization, g_scene.world,
                    &g_scene.sceneActors[slot]);
            if(!loaded)
            {

                printf("scene actor %s archive assembly unavailable; "
                       "falling back to direct package\n", trackName);
                archiveFallback = true;
                loaded = loadCharacterScene(
                    g_cdataPath, resource, g_scene.world,
                    &g_scene.sceneActors[slot]);
                archiveMode = false;
            }
        }
        else
        {
            loaded = loadCharacterScene(
                g_cdataPath, resource, g_scene.world,
                &g_scene.sceneActors[slot]);
        }
        if(!loaded)
        {
            printf("scene actor %s (character %u, resource %u) unavailable\n",
                   trackName, static_cast<unsigned>(characterId),
                   static_cast<unsigned>(resource));
            return NULL;
        }
        if(archiveFallback)
            printf("scene actor %s direct fallback recorded\n", trackName);
        if(!loadCharacterBasicAnimation(
               g_stagePath, &g_scene.sceneActors[slot]))
        {
            printf("scene actor %s BASIC.ANM unavailable\n", trackName);
            freeCharacterScene(&g_scene.sceneActors[slot]);
            return NULL;
        }
        g_scene.sceneActorTracks[slot] = trackName;
        g_scene.sceneActorResourceIds[slot] = resource;
        g_scene.sceneActorArchiveModes[slot] = archiveMode;
        g_scene.sceneActorArchiveFallbacks[slot] = archiveFallback;
        g_scene.sceneActorMotionResourceSets[slot] =
            static_cast<int16_t>(character->combatStyleId);
        g_scene.sceneActorPlacements[slot] = SceneActorPresentationState();
        printf("scene actor %s = character %u, resource %u (%s selector)\n",
               trackName, static_cast<unsigned>(characterId),
               static_cast<unsigned>(resource),
               archiveMode ? "archive" : "direct");
    }
    if(g_scene.sceneActorResourceIds[slot] == 0)
        return NULL;
    return &g_scene.sceneActors[slot];
}

CharacterScene* loadedSceneActorFor(const char* trackName)
{
    if(trackName == NULL)
        return NULL;
    const char* selected = selectedSceneActorName();
    if(strcmp(trackName, "PLY1") == 0 ||
       (selected != NULL && strcmp(trackName, selected) == 0))
        return &g_scene.player;
    for(int i = 0; i < SCENE_ACTOR_CAPACITY; ++i)
        if(g_scene.sceneActorResourceIds[i] != 0 &&
           g_scene.sceneActorTracks[i] == trackName)
            return &g_scene.sceneActors[i];
    return NULL;
}

int sceneActorMotionResourceSet(const CharacterScene* scene)
{
    if(scene == &g_scene.player)
    {
        const GameSlotRecord& player = g_game.runtimeState().player;
        const CharacterRecord* character =
            characterRecordForId(static_cast<uint16_t>(player.characterId));
        return character != NULL ?
            static_cast<int>(character->combatStyleId) : -1;
    }

    for(int i = 0; i < SCENE_ACTOR_CAPACITY; ++i)
    {
        if(scene == &g_scene.sceneActors[i])
            return g_scene.sceneActorMotionResourceSets[i];
    }
    return -1;
}

void applySceneActorState(void*, const SceneActorState& state)
{
    if(state.kind == SceneTrackKind::Morph)
    {
        CharacterScene* actor = loadedSceneActorFor(state.trackName);
        if(actor != NULL)
            applyCharacterSceneMorph(
                actor, state.morphAttachment, state.morphStartTarget,
                state.morphEndTarget, state.morphAmount);
        return;
    }
    const bool player = state.kind == SceneTrackKind::Player ||
                        state.kind == SceneTrackKind::PlayerTask;

    CharacterScene* scene = NULL;
    if(player)
    {

        const char* selected = selectedSceneActorName();
        if(state.trackName != NULL &&
           (strcmp(state.trackName, "PLY1") == 0 ||
            (selected != NULL && strcmp(state.trackName, selected) == 0)))
            scene = &g_scene.player;
        else
            scene = sceneActorFor(state.trackName, state.resourceName);
    }
    else if(state.kind == SceneTrackKind::Model)
    {
        SceneProp* prop = g_sceneProps.find(state.resourceName);
        if(prop == NULL)
            return;

        if(state.targetId <= 0)
        {
            setScenePropTransform(*prop, state.visible,
                                  state.position.x, state.position.y,
                                  state.position.z, state.rotation.x,
                                  state.rotation.y, state.rotation.z);
            return;
        }

        const SceneTrack* targetTrack = NULL;
        const std::string* targetActorTrackName = NULL;
        const SceneDefinition* definition = g_sceneRuntime.definition();
        if(definition != NULL && state.targetName != NULL)
        {
            auto findTargetTrack =
                [&](auto&& self, const std::vector<SceneTrack>& tracks,
                    const std::string* inheritedName) -> bool
                {
                    for(size_t i = 0; i < tracks.size(); ++i)
                    {
                        const SceneTrack& track = tracks[i];
                        const std::string* actorTrackName = inheritedName;
                        if((track.kind == SceneTrackKind::Model ||
                            track.kind == SceneTrackKind::Player) &&
                           !track.name.empty())
                            actorTrackName = &track.name;

                        if(track.kind == SceneTrackKind::Player ||
                           track.kind == SceneTrackKind::Model)
                        {
                            for(size_t j = 0; j < track.clips.size(); ++j)
                            {
                                const SceneClip& clip = track.clips[j];
                                const char* cutName = clip.name.empty() ?
                                                      "NONE" : clip.name.c_str();
                                if(strcmp(state.targetName, cutName) == 0)
                                {
                                    targetTrack = &track;
                                    targetActorTrackName = actorTrackName;
                                    return true;
                                }
                            }
                        }

                        for(size_t j = 0; j < track.clips.size(); ++j)
                        {
                            if(self(self, track.clips[j].children,
                                    actorTrackName))
                                return true;
                        }
                        if(self(self, track.children, actorTrackName))
                            return true;
                    }
                    return false;
                };
            findTargetTrack(findTargetTrack, definition->tracks, NULL);
        }

        CharacterScene* actor = NULL;
        if(targetTrack != NULL &&
           targetTrack->kind == SceneTrackKind::Player &&
           targetActorTrackName != NULL)
        {
            for(int i = 0; i < SCENE_ACTOR_CAPACITY; ++i)
            {
                if(g_scene.sceneActorResourceIds[i] != 0 &&
                   g_scene.sceneActorTracks[i] == *targetActorTrackName)
                {
                    actor = &g_scene.sceneActors[i];
                    break;
                }
            }
        }

        const rw::Matrix* boneMatrix = NULL;
        if(actor != NULL && !actor->hierarchies.empty())
        {
            rw::HAnimHierarchy* hierarchy = actor->hierarchies[0];

            if(hierarchy != NULL && hierarchy->matrices != NULL &&
               state.targetId >= 0 && state.targetId < hierarchy->numNodes)
                boneMatrix = &hierarchy->matrices[state.targetId];
        }
        if(boneMatrix == NULL)
        {
            setScenePropTransform(*prop, false,
                                  state.position.x, state.position.y,
                                  state.position.z, state.rotation.x,
                                  state.rotation.y, state.rotation.z);
            return;
        }

        setScenePropBoneTransform(*prop, state.visible, *boneMatrix,
                                  state.position.x, state.position.y,
                                  state.position.z, state.rotation.x,
                                  state.rotation.y, state.rotation.z);
        return;
    }
    if(scene == NULL)
        return;

    SceneActorPresentationState* presentation =
        sceneActorPresentationFor(scene);
    if(presentation == NULL)
        return;

    presentation->touched = true;
    retainSceneActorPlacement(presentation->incomingPlacement, state);

    if(state.kind == SceneTrackKind::PlayerTask)
        bindSceneActorMotion(
            scene, sceneActorMotionResourceSet(scene),
            state.motionName != NULL ? state.motionName : state.resourceName,
            state.sampleTime);
    if(g_gameSceneActive && state.kind == SceneTrackKind::PlayerTask &&
       scene == &g_scene.player)
        presentation->battleSpawn = true;
}

void applySceneActor(void* userData, const SceneActorState& state)
{
    if(state.kind == SceneTrackKind::Model && state.targetId > 0)
    {

        g_pendingSceneBoneProps.push_back(state);
        return;
    }
    applySceneActorState(userData, state);
}

void setSceneLightAngles(rw::Light* light, float pitchDegrees,
                         float yawDegrees)
{
    if(light == NULL || light->getFrame() == NULL)
        return;
    const float pitch = pitchDegrees * 0.01745329251994329577f;
    const float yaw = yawDegrees * 0.01745329251994329577f;
    rw::Matrix& matrix = light->getFrame()->matrix;
    matrix.at.x = sinf(yaw) * cosf(pitch);
    matrix.at.y = -sinf(pitch);
    matrix.at.z = cosf(yaw) * cosf(pitch);
    matrix.right.x = cosf(yaw);
    matrix.right.y = 0.0f;
    matrix.right.z = -sinf(yaw);
    matrix.up.x = matrix.at.y * matrix.right.z;
    matrix.up.y = matrix.at.z * matrix.right.x -
                  matrix.at.x * matrix.right.z;
    matrix.up.z = -matrix.at.y * matrix.right.x;
    matrix.optimize();
    light->getFrame()->updateObjects();
}

void setDirectionalLightAngles(float pitchDegrees, float yawDegrees)
{
    setSceneLightAngles(g_scene.directional, pitchDegrees, yawDegrees);
}

void setStageLightingScale(float scale)
{
    const StageLightingPreset* preset =
        stageLightingPreset(g_scene.stageOriginalId);
    if(preset == NULL)
        return;

    const uint32_t flags = rw::Light::LIGHTATOMICS;
    if(g_scene.ambient != NULL)
    {
        g_scene.ambient->color = rw::makeRGBAf(
            preset->ambientColor[0] * scale,
            preset->ambientColor[1] * scale,
            preset->ambientColor[2] * scale, 1.0f);
        g_scene.ambient->setFlags(flags);
    }

    for(int i = 0; i < 2; ++i)
    {
        rw::Light* light = sceneDirectionalLight(i);
        if(light == NULL)
            continue;
        light->color = rw::makeRGBAf(
            preset->directionalColor[i][0] * scale,
            preset->directionalColor[i][1] * scale,
            preset->directionalColor[i][2] * scale, 1.0f);
        light->setFlags(flags);
        setSceneLightAngles(
            light, preset->directionalRotation[i][0],
            preset->directionalRotation[i][1]);
    }
    for(int i = 2; i < SCENE_LIGHT_CAPACITY; ++i)
        if(g_scene.sceneDirectionals[i] != NULL)
            g_scene.sceneDirectionals[i]->setFlags(0);
}

void setStageFog(bool enabled)
{
    const StageLightingPreset* preset =
        stageLightingPreset(g_scene.stageOriginalId);
    if(!enabled || preset == NULL || !preset->fogEnabled)
    {
        rw::SetRenderState(rw::FOGENABLE, 0);
        return;
    }

    rw::SetRenderState(rw::FOGCOLOR, preset->fogColor);
    if(g_scene.camera != NULL)
    {
        const float depthRange =
            g_scene.camera->farPlane - g_scene.camera->nearPlane;
        g_scene.camera->fogPlane =
            g_scene.camera->nearPlane +
            depthRange * (1.0f - preset->fogDensity);
    }
    rw::SetRenderState(rw::FOGENABLE, 1);
}

void restoreDefaultLighting(void)
{
    if(stageLightingPreset(g_scene.stageOriginalId) != NULL)
    {
        setStageLightingScale(1.0f);
        return;
    }

    const uint32_t flags = rw::Light::LIGHTATOMICS;
    if(g_scene.ambient != NULL)
    {
        g_scene.ambient->color =
            rw::makeRGBAf(0.58f, 0.58f, 0.62f, 1.0f);
        g_scene.ambient->setFlags(flags);
    }
    if(g_scene.directional != NULL)
    {
        g_scene.directional->color =
            rw::makeRGBAf(0.72f, 0.68f, 0.62f, 1.0f);
        g_scene.directional->setFlags(flags);
        setDirectionalLightAngles(60.0f, -45.0f);
    }
    for(int i = 1; i < SCENE_LIGHT_CAPACITY; ++i)
        if(g_scene.sceneDirectionals[i] != NULL)
            g_scene.sceneDirectionals[i]->setFlags(0);
}

rw::Light* sceneDirectionalLight(int32_t index)
{
    if(index < 0 || index >= SCENE_LIGHT_CAPACITY)
        return g_scene.directional;
    if(g_scene.sceneDirectionals[index] != NULL)
        return g_scene.sceneDirectionals[index];
    if(index == 0)
    {
        g_scene.sceneDirectionals[0] = g_scene.directional;
        return g_scene.sceneDirectionals[0];
    }
    rw::Light* light = rw::Light::create(rw::Light::DIRECTIONAL);
    rw::Frame* frame = rw::Frame::create();
    if(light == NULL || frame == NULL)
    {
        if(light != NULL)
            light->destroy();
        if(frame != NULL)
            frame->destroy();
        return g_scene.directional;
    }
    light->setFrame(frame);
    light->setFlags(0);
    if(g_scene.world != NULL)
        g_scene.world->addLight(light);
    g_scene.sceneDirectionals[index] = light;
    return light;
}

void applySceneLight(void*, const SceneLightState& state)
{
    const bool ambient = state.resourceName != NULL &&
                         strcmp(state.resourceName, "AMBIENT") == 0;

    rw::Light* light = ambient ? g_scene.ambient :
                                 sceneDirectionalLight(state.index);
    if(light == NULL)
        return;

    light->color = rw::makeRGBAf(
        clampPresentation(state.color.x, 0.0f, 1.0f),
        clampPresentation(state.color.y, 0.0f, 1.0f),
        clampPresentation(state.color.z, 0.0f, 1.0f), 1.0f);
    light->setFlags(state.visible ? rw::Light::LIGHTATOMICS : 0);
    if(!ambient)
        setSceneLightAngles(light, state.rotationStart, state.rotationEnd);
}

void applySceneEffect(void*, const SceneEffectState& state)
{
    setSceneMaskEffectMode(state.blur, state.active);
    g_sceneTimeScale = state.active && state.timeScale > 0.0f ?
        state.timeScale : 1.0f;
}

void applySceneAudio(void*, const SceneAudioEvent& event)
{
    if(event.kind == SceneAudioKind::Vibration)
    {
        g_sceneRumbleActive = event.active;
        if(event.active)
        {
            const float amplitude = clampPresentation(
                event.vibrationAmplify, 0.0f, 1.0f);

            g_sceneRumbleLow = amplitude;
            g_sceneRumbleHigh = amplitude;
        }
        else
        {
            g_sceneRumbleLow = 0.0f;
            g_sceneRumbleHigh = 0.0f;
        }
        return;
    }
    if(event.volumeControl)
    {
        const bool music = event.kind == SceneAudioKind::Music;
        const float master = music ?
            g_game.runtimeState().options.musicVolume / 15.0f :
            g_game.runtimeState().options.effectsVolume / 15.0f;
        setAudioChannelVolume(music ? AUDIO_CHANNEL_MUSIC_SECONDARY :
                                    AUDIO_CHANNEL_VOICE,
                              master * clampPresentation(event.volume,
                                                         0.0f, 1.0f));
        return;
    }
    if(event.kind == SceneAudioKind::Voice && event.continuous)
    {
        CharacterScene* actor = loadedSceneActorFor(event.trackName);
        if(actor != NULL)
            applyCharacterSceneLip(actor, event.lip, event.lipRate,
                                   event.clipElapsed);
        return;
    }

    if(event.kind == SceneAudioKind::Voice)
    {
        if(event.active)
            playAudioClipByIndex(
                static_cast<uint32_t>(event.values[0]),
                AUDIO_CHANNEL_VOICE, false,
                g_game.runtimeState().options.effectsVolume / 15.0f);
        else
        {
            stopAudioChannel(AUDIO_CHANNEL_VOICE);
            CharacterScene* actor = loadedSceneActorFor(event.trackName);
            if(actor != NULL)
                applyCharacterSceneMorph(actor, 1, 0, 0, 0.0f);
        }
        return;
    }
    if(event.kind == SceneAudioKind::Music)
    {
        if(event.active)
            playAudioClipByIndex(
                static_cast<uint32_t>(event.values[1]),
                AUDIO_CHANNEL_MUSIC_SECONDARY, true,
                g_game.runtimeState().options.musicVolume / 15.0f);
        else
            stopAudioChannel(AUDIO_CHANNEL_MUSIC_SECONDARY);
        return;
    }

    if(event.kind != SceneAudioKind::Sound)
        return;
    const std::string track = event.trackName != NULL ? event.trackName : "";
    if(!event.active)
    {
        std::multimap<std::string, uint32_t>::iterator found =
            g_sceneSoundHandles.find(track);
        if(found != g_sceneSoundHandles.end())
        {
            stopSoundHandle(found->second);
            g_sceneSoundHandles.erase(found);
        }
        return;
    }
    if(event.values[0] < 0 || event.values[0] > 0xff ||
       event.values[2] < 0 || event.values[2] > 0xff)
        return;

    const uint32_t handle = playUnplacedSoundCue(
        static_cast<uint8_t>(event.values[0]),
        static_cast<uint8_t>(event.values[2]),
        g_game.runtimeState().options.effectsVolume);
    if(handle != AUDIO_SOUND_HANDLE_INVALID)
        g_sceneSoundHandles.insert(std::make_pair(track, handle));
}

void applySceneCaption(void*, const SceneCaptionEvent& event)
{

    const char* line = event.active ?
        g_sceneCaptions.text(event.language, event.caption) : NULL;
    g_sceneCaption = line != NULL ? line : "";
}

void restoreScenePresentation(void*, bool, bool)
{
    g_activeSceneEntry = NULL;
    g_sceneCaption.clear();
    setSceneMaskEffectMode(SCENE_MASK_NONE, false);
    setSceneLetterboxActive(false);

    g_sceneTimeScale = 1.0f;
    g_sceneRumbleActive = false;
    g_sceneRumbleLow = 0.0f;
    g_sceneRumbleHigh = 0.0f;
    freeSceneProps(g_sceneProps);
    retainSceneActorMotionPosition(
        &g_scene.player, &g_scene.playerScenePlacement);
    if(g_scene.playerScenePlacement.placement.placed)
    {
        const SceneActorPlacementState& retained =
            g_scene.playerScenePlacement.placement;
        g_game.setPendingBattleSpawn(
            retained.logicalPosition.x, retained.logicalPosition.y,
            retained.logicalPosition.z, retained.heading);
    }

    resetSceneMotionCursors();

    for(int i = 0; i < SCENE_ACTOR_CAPACITY; ++i)
    {
        if(g_scene.sceneActorTracks[i].empty())
            continue;
        freeCharacterScene(&g_scene.sceneActors[i]);
        g_scene.sceneActorTracks[i].clear();
        g_scene.sceneActorResourceIds[i] = 0;
        g_scene.sceneActorArchiveModes[i] = false;
        g_scene.sceneActorArchiveFallbacks[i] = false;
        g_scene.sceneActorMotionResourceSets[i] = -1;
    }
    for(std::multimap<std::string, uint32_t>::iterator sound =
            g_sceneSoundHandles.begin(); sound != g_sceneSoundHandles.end();
        ++sound)
        stopSoundHandle(sound->second);
    g_sceneSoundHandles.clear();
    stopAudioChannel(AUDIO_CHANNEL_MUSIC_SECONDARY);
    stopAudioChannel(AUDIO_CHANNEL_VOICE);
    restoreDefaultLighting();
    g_cameraPresentation.initialized = false;
    g_cameraPresentation.fieldAzimuthInitialized = false;
    resetGameplayCameraCollision(g_cameraPresentation.collision);
}

SceneLanguage selectedSceneLanguage(void)
{
    switch(g_game.languageSelection())
    {
    case 1: return SceneLanguage::French;
    case 2: return SceneLanguage::German;
    case 3: return SceneLanguage::Spanish;
    case 4: return SceneLanguage::Italian;
    default: return SceneLanguage::English;
    }
}

bool loadSceneStage(uint16_t originalStageId)
{
    if(g_scene.stageOriginalId == originalStageId &&
       g_scene.stage.clump != NULL)
        return true;

    if(g_scene.stage.clump != NULL)
        freeFstageScene(&g_scene.stage);
    const uint32_t entryIndex =
        st4EntryIndexForStage(originalStageId, 0, false, false);
    if(!loadFstageSceneByIndexForStage(
           g_fstagePath, entryIndex, originalStageId,
           g_scene.world, &g_scene.stage))
    {
        printf("scene stage %u unavailable\n",
               static_cast<unsigned>(originalStageId));
        return false;
    }

    if(!g_effects.loadStageResource(g_stagePath, originalStageId))
    {
        printf("scene stage %u effect resource unavailable\n",
               static_cast<unsigned>(originalStageId));
        freeFstageScene(&g_scene.stage);
        return false;
    }

    g_titleStageLoaded = false;
    g_titleCameraPathStarted = false;
    g_scene.stageId = 0xffffu;
    g_scene.stageOriginalId = originalStageId;
    restoreDefaultLighting();
    g_game.setArenaRadius(g_scene.stage.radius * 0.35f);
    printf("scene stage %u center=(%.2f,%.2f,%.2f) radius=%.2f\n",
           static_cast<unsigned>(originalStageId), g_scene.stage.center.x,
           g_scene.stage.center.y, g_scene.stage.center.z,
           g_scene.stage.radius);
    return true;
}

bool startSceneEntry(const char* entryName)
{
    if(entryName == NULL)
        return false;
    ScenePresentationBindings bindings;
    bindings.userData = NULL;
    bindings.language = selectedSceneLanguage();
    bindings.applyCamera = applySceneCamera;
    bindings.applyActor = applySceneActor;
    bindings.applyLight = applySceneLight;
    bindings.audio = applySceneAudio;
    bindings.caption = applySceneCaption;
    bindings.effect = applySceneEffect;
    bindings.restore = restoreScenePresentation;
    std::string error;
    if(!g_sceneRuntime.loadFromXaf(
           "raw/XAF/FSCENE.XAF", entryName, bindings, &error))
    {
        fprintf(stderr, "scene: %s\n", error.c_str());
        return false;
    }
    const SceneDefinition* scene = g_sceneRuntime.definition();

    if(scene != NULL && scene->stageId >= 0)
        loadSceneStage(static_cast<uint16_t>(scene->stageId));

    loadSceneCaptions("raw/XAF/FSCENE.XAF", entryName, g_sceneCaptions);

    if(loadSceneProps("raw/XAF/FSCENE.XAF", entryName, g_scene.world,
                      g_sceneProps))
    {
        printf("scene props %u:",
               static_cast<unsigned>(g_sceneProps.props.size()));
        for(size_t i = 0; i < g_sceneProps.props.size(); ++i)
        {
            const std::string& path = g_sceneProps.props[i].path;
            const size_t slash = path.rfind('\\');
            printf(" %s", slash == std::string::npos ?
                   path.c_str() : path.c_str() + slash + 1);
        }
        printf("\n");
    }
    setSceneLetterboxActive(
        scene != NULL && scene->hasStageLight && !scene->stageLight);

    if(scene != NULL && scene->hasStageLight && !scene->stageLight)
    {
        if(g_scene.ambient != NULL)
            g_scene.ambient->setFlags(0);
        if(g_scene.directional != NULL)
            g_scene.directional->setFlags(0);
    }
    if(loadScenePtaskPool(
           "raw/XAF/FSCENE.XAF", entryName, g_scenePtasks))
    {
        printf("scene ptasks %u\n",
               static_cast<unsigned>(g_scenePtasks.records.size()));
    }
    else
        g_scenePtasks.records.clear();
    resetSceneMotionCursors();

    resetSceneActorPlacements();
    const bool started = g_sceneRuntime.start();
    if(started)
    {
        g_activeSceneEntry = entryName;
        g_sceneSkipInputReleased = false;
    }
    else
        g_activeSceneEntry = NULL;
    return started;
}

bool startConfiguredScene(void)
{
    return startSceneEntry(g_sceneEntry);
}

void updatePresentationCamera(float deltaSeconds)
{
    if(g_sceneRuntime.status() == SceneStatus::Playing)
        return;
    const GameScreen screen = g_game.screen();
    if(screen == GAME_SCREEN_INVENTORY &&
       !g_scene.player.clumps.empty() &&
       g_scene.player.clumps[0]->getFrame() != NULL)
    {

        const rw::Matrix* actor =
            g_scene.player.clumps[0]->getFrame()->getLTM();
        rw::V3d target =
        {
            actor->pos.x + g_scene.player.boundsCenter[0],
            actor->pos.y + g_scene.player.boundsCenter[1],
            actor->pos.z + g_scene.player.boundsCenter[2]
        };
        rw::V3d eye =
        {
            target.x + actor->right.x * 5.5f - actor->at.x * 22.0f,
            target.y + 5.5f,
            target.z + actor->right.z * 5.5f - actor->at.z * 22.0f
        };
        setCameraLookAt(eye, target);
        g_scene.camera->setNearPlane(0.2f);
        g_scene.camera->setFarPlane(
            fmaxf(39.0f, g_scene.stage.radius * 4.0f));
        g_cameraPresentation.previousScreen = screen;
        return;
    }

    if(screen == GAME_SCREEN_PAUSE ||
       screen == GAME_SCREEN_OPTIONS ||
       screen == GAME_SCREEN_RESULTS)
    {
        g_cameraPresentation.previousScreen = screen;
        return;
    }

    CameraPose desired = desiredCameraPose(deltaSeconds);

    const bool authored = g_cameraPresentation.scripted;
    if(screen == GAME_SCREEN_GAMEPLAY && !authored &&
       !g_fieldCameraRegion.active &&
       g_scene.stage.sectionClumps[1] != NULL)
    {
        const GameSlotRecord& tracked = g_game.runtimeState().player;
        CameraCollisionVector trackedPosition;
        trackedPosition.x = tracked.position.x;
        trackedPosition.y = tracked.position.y;
        trackedPosition.z = tracked.position.z;
        CameraCollisionVector target;
        target.x = desired.target.x;
        target.y = desired.target.y;
        target.z = desired.target.z;
        CameraCollisionVector eye;
        eye.x = desired.eye.x;
        eye.y = desired.eye.y;
        eye.z = desired.eye.z;
        const RwWorldGeometry*
            cameraGeometry[FSTAGE_CAMERA_COLLISION_SOURCE_CAPACITY];
        const size_t cameraGeometryCount =
            gatherFstageCameraCollisionGeometry(
                g_scene.stage, cameraGeometry,
                FSTAGE_CAMERA_COLLISION_SOURCE_CAPACITY);
        if(resolveGameplayCameraCollision(
               g_cameraPresentation.collision,
               cameraGeometry, cameraGeometryCount, trackedPosition,
               tracked.headingDegrees, target, eye))
        {
            desired.eye.x = eye.x;
            desired.eye.y = eye.y;
            desired.eye.z = eye.z;
        }
    }
    if(!g_cameraPresentation.initialized)
    {
        g_cameraPresentation.current = desired;
        g_cameraPresentation.initialized = true;
    }
    else
    {
        float response = 1.0f -
            expf(-CAMERA_RESPONSE * clampPresentation(
                deltaSeconds, 0.0f, 0.1f));
        const bool fieldFollow =
            screen == GAME_SCREEN_GAMEPLAY &&
            g_cameraPresentation.followPlayer &&
            !g_cameraPresentation.scripted;
        if(fieldFollow)
        {
            const float timeScale =
                clampPresentation(g_game.timeScale(), 0.0f, 3.0f);
            const bool fixedRegion = g_fieldCameraRegion.active;
            const float smoothingAlpha = fixedRegion ?
                g_fieldCameraRegion.smoothingAlpha :
                FIELD_CAMERA_SMOOTH_ALPHA;
            const float targetResponse = clampPresentation(
                smoothingAlpha * timeScale, 0.0f, 1.0f);
            const float eyeResponse = clampPresentation(
                smoothingAlpha * timeScale * timeScale,
                0.0f, 1.0f);
            const float targetDeadband = fixedRegion ?
                g_fieldCameraRegion.targetDeadband :
                FIELD_CAMERA_SMOOTH_DEADBAND;
            const float eyeDeadband = fixedRegion ?
                g_fieldCameraRegion.eyeDeadband :
                FIELD_CAMERA_SMOOTH_DEADBAND;
            g_cameraPresentation.current.eye = smoothPresentationVector(
                g_cameraPresentation.current.eye, desired.eye, eyeResponse,
                eyeDeadband);
            g_cameraPresentation.current.target = smoothPresentationVector(
                g_cameraPresentation.current.target, desired.target,
                targetResponse, targetDeadband);
            response = targetResponse;
        }
        else
        {
            g_cameraPresentation.current.eye =
                mixPresentationVector(g_cameraPresentation.current.eye,
                                      desired.eye, response);
            g_cameraPresentation.current.target =
                mixPresentationVector(g_cameraPresentation.current.target,
                                      desired.target, response);
        }
        g_cameraPresentation.current.fieldOfView +=
            (desired.fieldOfView -
             g_cameraPresentation.current.fieldOfView) * response;
        g_cameraPresentation.current.nearPlane = desired.nearPlane;
        g_cameraPresentation.current.farPlane = desired.farPlane;
    }

    CameraPose rendered = g_cameraPresentation.current;
    if(g_cameraPresentation.shakeSeconds > 0.0f)
    {
        const float phase = g_game.battleTime() * 61.0f;
        const float magnitude = g_cameraPresentation.shakeMagnitude *
            clampPresentation(g_cameraPresentation.shakeSeconds / 0.2f,
                              0.0f, 1.0f);
        rendered.eye.x += sinf(phase) * magnitude;
        rendered.eye.y += cosf(phase * 1.31f) * magnitude * 0.55f;
        rendered.target.x += sinf(phase * 0.73f) * magnitude * 0.35f;
        g_cameraPresentation.shakeSeconds -= deltaSeconds;
        if(g_cameraPresentation.shakeSeconds < 0.0f)
            g_cameraPresentation.shakeSeconds = 0.0f;
    }
    applyPresentationCamera(rendered);
    g_cameraPresentation.previousScreen = screen;
}

bool stageGroundHeight(void* context, float x, float z, float ceiling,
                       float* height)
{
    const RwWorldGeometry* geometry =
        static_cast<const RwWorldGeometry*>(context);
    return rwWorldGroundHeight(*geometry, x, z, ceiling, height);
}

bool stageNearestFloor(void* context, float x, float z, float* outX,
                       float* outY, float* outZ)
{
    const RwWorldGeometry* geometry =
        static_cast<const RwWorldGeometry*>(context);
    RwWorldVertex floorPosition;
    if(!rwWorldNearestFloor(*geometry, x, z, &floorPosition))
        return false;
    *outX = floorPosition.x;
    *outY = floorPosition.y;
    *outZ = floorPosition.z;
    return true;
}

bool stageSpawnAnchor(const RwWorldGeometry& geometry, uint16_t stageId,
                      RwWorldVertex* anchor, float* headingDegrees)
{
    *headingDegrees = 0.0f;
    if(!g_npcPositionsLoaded)
    {
        std::vector<uint8_t> data;
        g_npcPositionsLoaded =
            readXafEntry(g_cdataPath, "NPOS_DATA.DAT", data) &&
            g_npcPositions.parse(data.data(), data.size());
    }

    float x = 0.0f;
    float z = 0.0f;
    float heading = 0.0f;
    bool placed = false;
    if(stageId == GAME_FIRST_PLAYABLE_STAGE_ID)
    {
        x = FIRST_PLAYABLE_SPAWN_X;
        z = FIRST_PLAYABLE_SPAWN_Z;
        heading = FIRST_PLAYABLE_SPAWN_HEADING;
        placed = true;
    }
    else if(g_npcPositionsLoaded)
    {
        FstageSpawnTransform spawn;
        if(getFstagePlayerSpawn(g_npcPositions, stageId, &spawn))
        {
            x = spawn.position.x;
            z = spawn.position.z;
            heading = spawn.headingDegrees;
            placed = true;
        }
    }
    if(!placed)
    {
        const float centreX =
            (geometry.minBounds.x + geometry.maxBounds.x) * 0.5f;
        const float centreZ =
            (geometry.minBounds.z + geometry.maxBounds.z) * 0.5f;
        return rwWorldNearestFloor(geometry, centreX, centreZ, anchor);
    }
    float height = 0.0f;
    if(!rwWorldFloorNearHeight(geometry, x, z, 0.0f, &height))
    {
        RwWorldVertex floorPosition;
        if(!rwWorldNearestFloor(geometry, x, z, &floorPosition))
            return false;
        height = floorPosition.y;
    }
    anchor->x = x;
    anchor->y = height;
    anchor->z = z;
    *headingDegrees = heading;
    return true;
}

size_t combatAnimationIndex(const GameSlotRecord&)
{

    return 6;
}

bool loopingCombatAnimation(CombatAction action)
{
    return action == COMBAT_ACTION_IDLE ||
           action == COMBAT_ACTION_MOVE ||
           action == COMBAT_ACTION_GUARD ||
           action == COMBAT_ACTION_VICTORY;
}

uint32_t combatAnimationState(const GameSlotRecord& slot)
{
    return combatAnimationPresentationState(slot);
}

bool bindCombatantAnimation(CharacterScene* scene, const GameSlotRecord& slot,
                            const MoveMotion* scheduledMotion)
{
    const CombatAnimationRequest request = combatAnimationRequest(slot, scheduledMotion);
    const MoveMotion& motion = request.motion;

    if(request.hasMotion)
    {
        const CharacterRecord* character =
            characterRecordForId(static_cast<uint16_t>(slot.characterId));
        size_t animation = 0;
        int position = -1;
        const int battleResource = g_game.battleMotionResource(motion.motionId, &position);
        if(battleResource >= 0)
        {
            const int entry = MOVE_ANIMATION_BUNDLES[battleResource].xafEntry;
            if(entry < 0 || position < 0 || !loadCharacterAnimationClipByIndex(
                   g_stagePath, static_cast<uint32_t>(entry),
                   static_cast<size_t>(position), scene, &animation))
                return false;
        }
        else if(!loadCharacterScheduledAnimation(
                    g_stagePath, character != NULL ? character->combatStyleId : -1,
                    motion.motionId, scene, &animation))
            return false;

        const uint32_t state = request.state;
        const float clipSeconds =
            static_cast<float>(scene->animations[animation]->duration);
        const CharacterAnimationPlayback playback =
            makeCharacterAnimationPlayback(
                clipSeconds, motion.blendFrames, motion.maxDurationFrames,
                motion.transitionFrames, slot.animationTimeScale,
                request.loop, false);
        bindCharacterSceneAnimation(
            scene, animation, state,
            slot.playingAnimationFrame / 60.0f, playback);
        return true;
    }

    bindCharacterSceneAnimation(
        scene, combatAnimationIndex(slot), combatAnimationState(slot),
        slot.playingAnimationFrame / 60.0f, 0.0f,
        loopingCombatAnimation(slot.action));
    return true;
}

bool bindLiveCombatantAnimation(CharacterScene* scene,
                                const GameSlotRecord& slot)
{
    MoveMotion motion = {};
    const MoveMotion* scheduledMotion =
        g_game.combatMotion(slot.slotIndex, &motion) ? &motion : NULL;
    return bindCombatantAnimation(scene, slot, scheduledMotion);
}

bool updateCharacterScenes(void)
{

    if(titleBackdropScreen(g_game.screen()))
        return true;

    if(g_game.screen() == GAME_SCREEN_CHARACTER_SELECT)
        return true;
    const GameRuntimeState& runtime = g_game.runtimeState();
    const GameSlotRecord& player = runtime.player;
    const uint16_t playerResource =
        characterResourceId(static_cast<CharacterId>(player.characterId));
    CharacterCustomization playerCustomization = {};
    const InventoryEquipmentState& equipment =
        inventoryEquipmentState(g_game);
    const bool equipmentRebuild =
        equipment.phase != INVENTORY_EQUIPMENT_INACTIVE;
    bool useCustomization = equipmentRebuild;
    if(equipmentRebuild)
        playerCustomization = runtime.story.customization;
    else if(player.characterId >= 0)
    {
        useCustomization = g_game.characterCustomization(
            static_cast<uint16_t>(player.characterId),
            playerCustomization);
    }
    const uint64_t playerCustomizationKey = useCustomization ?
        characterCustomizationFingerprint(playerCustomization) : 0;
    const bool reloadPlayer =
        g_scene.playerResourceId != playerResource ||
        g_scene.playerCustomizationKey != playerCustomizationKey ||
        g_scene.player.assembledFromArchive != useCustomization;
    if(reloadPlayer)
    {

        const bool loaded = useCustomization ?
            loadCharacterArchiveScene(
                g_archivePath, g_dataPath, playerCustomization,
                g_scene.world, &g_scene.player) :
            loadCharacterScene(
                g_cdataPath, playerResource, g_scene.world, &g_scene.player);
        if(!loaded)
            return false;
        if(!loadCharacterAnimationBundle(g_stagePath,
                                         g_characterAnimationEntry,
                                         &g_scene.player))
            return false;
        if(!loadCharacterBasicAnimation(g_stagePath, &g_scene.player))
            return false;
        g_scene.playerResourceId = playerResource;
        g_scene.playerCustomizationKey = playerCustomizationKey;
    }

    updateStoryActorScenes();

    if(g_game.hasAlly())
    {
        const GameSlotRecord& partner = g_game.allySlot();
        const uint16_t partnerResource =
            characterResourceId(static_cast<CharacterId>(partner.characterId));
        if(g_scene.allyResourceId != partnerResource)
        {
            if(!loadCharacterScene(g_cdataPath, partnerResource,
                                   g_scene.world, &g_scene.ally))
                return false;
            if(!loadCharacterAnimationBundle(g_stagePath,
                                             g_characterAnimationEntry,
                                             &g_scene.ally))
                return false;
            if(!loadCharacterBasicAnimation(g_stagePath, &g_scene.ally))
                return false;
            g_scene.allyResourceId = partnerResource;
        }
        if(!bindLiveCombatantAnimation(&g_scene.ally, partner))
            return false;
        setCharacterSceneTransform(
            &g_scene.ally, partner.position.x,
            actorPresentationY(partner.position.x, partner.position.y,
                               partner.position.z),
            partner.position.z, partner.headingDegrees,
            characterTilt(partner), characterHeightOffset(partner));
    }
    else if(g_scene.allyResourceId != 0xffff)
    {
        freeCharacterScene(&g_scene.ally);
        g_scene.allyResourceId = 0xffff;
    }

    for(int i = 0; i < MAX_GAME_OPPONENTS; i++)
    {
        CharacterScene& scene = i == 0 ? g_scene.opponent :
            g_scene.additionalOpponents[i - 1];
        uint16_t& resourceId = i == 0 ? g_scene.opponentResourceId :
            g_scene.additionalOpponentResourceIds[i - 1];
        if(i >= g_game.opponentCount())
        {
            if(resourceId != 0xffff)
            {
                freeCharacterScene(&scene);
                resourceId = 0xffff;
            }
            continue;
        }

        const GameSlotRecord& opponent = g_game.opponent(i);
        const uint16_t opponentResource =
            characterResourceId(static_cast<CharacterId>(opponent.characterId));
        if(resourceId != opponentResource)
        {
            if(!loadCharacterScene(g_cdataPath, opponentResource,
                                   g_scene.world, &scene))
                return false;
            if(!loadCharacterAnimationBundle(g_stagePath,
                                             g_characterAnimationEntry,
                                             &scene))
                return false;
            if(!loadCharacterBasicAnimation(g_stagePath, &scene))
                return false;
            resourceId = opponentResource;
        }
        if(!bindLiveCombatantAnimation(&scene, opponent))
            return false;
        setCharacterSceneTransform(
            &scene, opponent.position.x,
            actorPresentationY(opponent.position.x, opponent.position.y,
                               opponent.position.z),
            opponent.position.z, opponent.headingDegrees,
            characterTilt(opponent), characterHeightOffset(opponent));
    }

    if(g_sceneRuntime.status() != SceneStatus::Playing)
    {
        if(g_game.screen() == GAME_SCREEN_INVENTORY)
        {
            const int previewAction =
                runtime.story.customization.genderSelector == 0 ?
                    0x6a6 : 0x6c1;
            const CharacterRecord* character =
                characterRecordForId(
                    static_cast<uint16_t>(player.characterId));
            size_t animation = 0;
            if(!loadCharacterScheduledAnimation(
                   g_stagePath,
                   character != NULL ? character->combatStyleId : -1,
                   previewAction, &g_scene.player, &animation))
                return false;
            const CharacterAnimationPlayback playback =
                makeCharacterAnimationPlayback(
                    g_scene.player.animations[animation]->duration,
                    0, -1, 0, 1.0f, true);
            bindCharacterSceneAnimation(
                &g_scene.player, animation,
                static_cast<uint32_t>(previewAction), 0.0f,
                playback);
            setCharacterSceneTransform(
                &g_scene.player, player.position.x,
                actorPresentationY(player.position.x, player.position.y,
                                   player.position.z),
                player.position.z, player.headingDegrees, 0.0f,
                characterHeightOffset(player));
        }
        else
        {

            if(!bindLiveCombatantAnimation(&g_scene.player, player))
                return false;
            setCharacterSceneTransform(
                &g_scene.player, player.position.x,
                actorPresentationY(player.position.x, player.position.y,
                                   player.position.z),
                player.position.z, player.headingDegrees,
                characterTilt(player), characterHeightOffset(player));
        }
    }

    return true;
}

bool startPendingGameScene(void)
{
    const char* entryName = g_game.pendingSceneEntry();
    if(entryName == NULL || g_gameSceneActive)
        return false;

    g_gameSceneActive = true;
    if(updateStageScene() && updateCharacterScenes() &&
       startSceneEntry(entryName))
        return true;

    fprintf(stderr, "scene: could not start New Game entry %s\n", entryName);
    g_gameSceneActive = false;
    g_game.completePendingScene();
    return false;
}

}
