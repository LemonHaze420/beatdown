#include "app_internal.h"

namespace app_detail
{

static bool stageCombatSurface(void* context, const Vec3& centre, float radius,
                                 GameCombatSurfaceContact* result)
{
    const RwWorldGeometry* const geometry =
        static_cast<const RwWorldGeometry*>(context);
    if(geometry == NULL || result == NULL)
        return false;
    RwWorldVertex position;
    position.x = centre.x;
    position.y = centre.y;
    position.z = centre.z;
    RwWorldSurfaceContact contact;
    if(!rwWorldProjectedSphereContact(*geometry, position, radius, &contact))
        return false;
    result->point = { contact.point.x, contact.point.y, contact.point.z };
    result->normal = { contact.normal.x, contact.normal.y, contact.normal.z };
    result->material = contact.material;
    return true;
}

static bool stageGroundColumn(void* context, float x, float z,
                               float referenceY, float* height,
                               uint16_t* material)
{
    const RwWorldGeometry* const geometry =
        static_cast<const RwWorldGeometry*>(context);
    return geometry != NULL && rwWorldGroundColumn(
        *geometry, x, z, referenceY, height, material);
}

static bool stageGroundContact(void* context, float x, float z,
                                float referenceY, GameCombatSurfaceContact* contact)
{
    const RwWorldGeometry* geometry = static_cast<const RwWorldGeometry*>(context);
    RwWorldVertex normal;
    float height;
    uint16_t material;
    if(geometry == NULL || contact == NULL || !rwWorldGroundColumn(
           *geometry, x, z, referenceY, &height, &material, &normal))
        return false;
    contact->point = { x, height, z };
    contact->normal = { normal.x, normal.y, normal.z };
    contact->material = material;
    return true;
}

bool installedStageEventFlag(uint16_t eventFlag, void* userData)
{
    const StageInstallContext* const context =
        static_cast<const StageInstallContext*>(userData);
    return context != NULL && context->story != NULL &&
        getStorySaveFlag(*context->story, eventFlag);
}

void installedStageEventSet(uint16_t eventFlag, void* userData)
{
    StageInstallContext* const context =
        static_cast<StageInstallContext*>(userData);
    if(context != NULL && context->story != NULL)
        setStorySaveFlag(*context->story, eventFlag, true);
}

void installedStageSound(uint8_t bankSlot, uint8_t cue, uint8_t volume,
                         void*)
{
    playSoundCue(bankSlot, cue, volume, 64);
}

void installedStageEffect(uint8_t recordIndex, const rw::Matrix& world,
                          int16_t tuningA, int16_t tuningB, void*)
{
    EffectMatrix matrix;
    matrix.right = { world.right.x, world.right.y, world.right.z };
    matrix.up = { world.up.x, world.up.y, world.up.z };
    matrix.at = { world.at.x, world.at.y, world.at.z };
    matrix.pos = { world.pos.x, world.pos.y, world.pos.z };
    g_effects.dispatchEst(recordIndex, matrix, tuningA, tuningB);
}

rw::Clump* installedStageModel(int8_t kind, int8_t selector, void* userData)
{
    StageInstallContext* const context =
        static_cast<StageInstallContext*>(userData);
    return context == NULL ? NULL :
        StageObjectAssets::resolveModel(kind, selector, context->assets);
}

float actorPresentationY(float x, float y, float z)
{
    if(g_scene.stage.worldClump != NULL)
    {
        float height = 0.0f;
        const float feet = y - GAME_ACTOR_GROUND_OFFSET;
        if(rwWorldGroundHeight(g_scene.stage.worldGeometry, x, z,
                               feet + GAME_GROUND_STEP_UP, &height))
            return height;
    }
    return y;
}

void updateStoryActorScenes(void)
{
    const StoryState& story = g_game.runtimeState().story;

    uint16_t onScreen[MAX_GAME_OPPONENTS + 2];
    int onScreenCount = 0;
    onScreen[onScreenCount++] = g_game.runtimeState().player.characterId;
    if(g_game.hasAlly())
        onScreen[onScreenCount++] = g_game.allySlot().characterId;
    for(int i = 0; i < g_game.opponentCount(); ++i)
        onScreen[onScreenCount++] = g_game.opponent(i).characterId;

    uint16_t wanted[STORY_ACTOR_SCENE_CAPACITY];
    const int wantedCount = selectRenderableStoryActors(
        story, onScreen, onScreenCount, wanted, STORY_ACTOR_SCENE_CAPACITY);

    for(int slot = 0; slot < STORY_ACTOR_SCENE_CAPACITY; ++slot)
    {
        if(g_scene.storyActorIds[slot] == 0xffffu)
            continue;
        bool stillWanted = false;
        for(int i = 0; i < wantedCount; ++i)
        {
            if(wanted[i] == g_scene.storyActorIds[slot])
            {
                stillWanted = true;
                break;
            }
        }
        if(stillWanted)
            continue;
        freeCharacterScene(&g_scene.storyActors[slot]);
        g_scene.storyActorIds[slot] = 0xffffu;
        g_scene.storyActorResourceIds[slot] = 0xffffu;
    }

    for(int i = 0; i < wantedCount; ++i)
    {
        const uint16_t actorId = wanted[i];
        int slot = -1;
        for(int j = 0; j < STORY_ACTOR_SCENE_CAPACITY; ++j)
        {
            if(g_scene.storyActorIds[j] == actorId)
            {
                slot = j;
                break;
            }
        }
        if(slot < 0)
        {
            for(int j = 0; j < STORY_ACTOR_SCENE_CAPACITY; ++j)
            {
                if(g_scene.storyActorIds[j] == 0xffffu)
                {
                    slot = j;
                    break;
                }
            }
        }
        if(slot < 0)
            continue;

        const StoryActorState* const actor = findStoryActor(story, actorId);
        if(actor == NULL)
            continue;

        uint16_t resource =
            characterResourceId(static_cast<CharacterId>(actorId));
        for(int actorIndex = 0;
            actorIndex < g_game.stageActorCount(); ++actorIndex)
        {
            const StageActorRuntime* runtime =
                g_game.stageActor(actorIndex);
            if(runtime == NULL ||
               runtime->authoredCharacterId != actorId)
                continue;
            const PersistentGameRecord* record =
                g_game.stageActorPersistentRecord(actorIndex);
            if(record != NULL && record->resourceId >= 0)
                resource = static_cast<uint16_t>(record->resourceId);
            break;
        }
        if(g_scene.storyActorResourceIds[slot] != resource)
        {
            if(!loadCharacterScene(g_cdataPath, resource, g_scene.world,
                                   &g_scene.storyActors[slot]))
                continue;
            loadCharacterAnimationBundle(g_stagePath,
                                         g_characterAnimationEntry,
                                         &g_scene.storyActors[slot]);
            loadCharacterBasicAnimation(
                g_stagePath, &g_scene.storyActors[slot]);
            g_scene.storyActorIds[slot] = actorId;
            g_scene.storyActorResourceIds[slot] = resource;
        }
        const CharacterRecord* const character =
            characterRecordForId(actorId);
        size_t animation = 0;
        if(actor->motionId > 0 &&
           loadCharacterScheduledAnimation(
               g_stagePath,
               character != NULL ? character->combatStyleId : -1,
               actor->motionId, &g_scene.storyActors[slot], &animation))
        {
            const CharacterAnimationPlayback playback =
                makeCharacterAnimationPlayback(
                    g_scene.storyActors[slot].animations[animation]->duration,
                    0, -1, 0, 1.0f, true);
            bindCharacterSceneAnimation(
                &g_scene.storyActors[slot], animation,
                static_cast<uint32_t>(actor->motionId), 0.0f, playback);
        }
        setCharacterSceneTransform(
            &g_scene.storyActors[slot],
            actor->position[0],
            actorPresentationY(actor->position[0], actor->position[1],
                               actor->position[2]),
            actor->position[2], actor->headingDegrees, 0.0f, 0.0f);
    }

    int placed = 0;
    for(int slot = 0; slot < STORY_ACTOR_SCENE_CAPACITY; ++slot)
    {
        if(g_scene.storyActorIds[slot] != 0xffffu)
            ++placed;
    }
    static int reported = -1;
    if(placed != reported)
    {
        reported = placed;
        printf("story actors rendered=%d (visible=%d)\n", placed,
               wantedCount);
    }
}

bool titleBackdropScreen(GameScreen screen)
{

    return screen == GAME_SCREEN_TITLE || screen == GAME_SCREEN_MAIN_MENU;
}

bool updateTitleStageScene(void)
{

    const uint16_t titleStageId = 1;
    const uint32_t entryIndex =
        st4EntryIndexForStage(titleStageId, g_titleStageVariant, false, false);
    if(g_titleStageLoaded && g_scene.stageOriginalId == titleStageId &&
       g_scene.stage.clump != NULL)
        return true;

    if(!loadFstageSceneByIndexForStage(
           g_fstagePath, entryIndex, titleStageId,
           g_scene.world, &g_scene.stage))
    {
        printf("title stage entry %u failed to load\n",
               static_cast<unsigned>(entryIndex));
        return false;
    }

    if(!g_effects.loadStageResource(g_stagePath, titleStageId))
    {
        printf("title stage %u effect resource unavailable\n",
               static_cast<unsigned>(titleStageId));
        freeFstageScene(&g_scene.stage);
        return false;
    }
    g_scene.stageId = 0xffffu;
    g_scene.stageOriginalId = titleStageId;
    restoreDefaultLighting();
    g_titleStageLoaded = true;

    g_titleStageVariant = (g_titleStageVariant + 1) % 3;
    printf("title stage entry=%u center=(%.2f,%.2f,%.2f) radius=%.2f\n",
           static_cast<unsigned>(entryIndex),
           g_scene.stage.center.x, g_scene.stage.center.y,
           g_scene.stage.center.z, g_scene.stage.radius);

    if(!g_titleCameraPathStarted)
    {
        cameraPathStart(g_titleCameraPath, TITLE_CAMERA_PATH);
        g_titleCameraPathStarted = true;

        float ex, ey, ez, tx, ty, tz;
        cameraPathSample(g_titleCameraPath, 0.0f, &ex, &ey, &ez, &tx, &ty, &tz);
        g_cameraPresentation.current.eye = presentationVector(ex, ey, ez);
        g_cameraPresentation.current.target = presentationVector(tx, ty, tz);
        g_cameraPresentation.current.fieldOfView = COMBAT_CAMERA_FIELD_OF_VIEW;
        g_cameraPresentation.initialized = true;
    }
    return true;
}

bool updateStageScene(void)
{

    if(g_gameSceneActive ||
       g_sceneRuntime.status() == SceneStatus::Playing)
        return g_scene.stage.clump != NULL;
    if(titleBackdropScreen(g_game.screen()))
        return updateTitleStageScene();

    if(g_titleStageLoaded)
    {
        g_titleStageLoaded = false;
        g_titleCameraPathStarted = false;
        g_scene.stageId = 0xffffu;
    }
    uint16_t stageId = g_game.runtimeState().stageId;
    if(stageId >= 53)
        stageId = 52;
    if(g_scene.stageId == stageId && g_scene.stage.clump != NULL)
        return true;

    if(g_scene.stage.clump != NULL)
        freeFstageScene(&g_scene.stage);

    const uint16_t originalStageId = stageId;
    bool loaded = false;
    if(g_stageOverride)
    {
        loaded = loadFstageScene(g_fstagePath, g_stageEntry,
                                 g_scene.world, &g_scene.stage);
    }
    else
    {
        const uint32_t entryIndex = st4EntryIndexForStage(
            originalStageId, 0,
            g_game.mode() != GAME_MODE_STORY,
            g_game.matchSetupFlag());
        loaded = loadFstageSceneByIndexForStage(
            g_fstagePath, entryIndex, originalStageId,
            g_scene.world, &g_scene.stage);
    }
    if(!loaded)
        return false;

    if(!g_effects.loadStageResource(g_stagePath, originalStageId))
    {
        printf("stage %u effect resource unavailable\n",
               static_cast<unsigned>(originalStageId));
        freeFstageScene(&g_scene.stage);
        return false;
    }
    if(!bindStageSoundBanksForStage(originalStageId))
    {
        freeFstageScene(&g_scene.stage);
        return false;
    }
    g_game.setArenaRadius(g_scene.stage.radius * 0.35f);
    g_scene.stageId = stageId;
    g_scene.stageOriginalId = originalStageId;
    g_cameraPresentation.fieldAzimuthInitialized = false;
    restoreDefaultLighting();
    printf("stage %u (StageId %u) center=(%.2f,%.2f,%.2f) radius=%.2f bounds=(%.2f,%.2f,%.2f)-(%.2f,%.2f,%.2f)\n",
           static_cast<unsigned>(stageId),
           static_cast<unsigned>(originalStageId),
           g_scene.stage.center.x, g_scene.stage.center.y,
           g_scene.stage.center.z, g_scene.stage.radius,
           g_scene.stage.minBounds.x, g_scene.stage.minBounds.y,
           g_scene.stage.minBounds.z, g_scene.stage.maxBounds.x,
           g_scene.stage.maxBounds.y, g_scene.stage.maxBounds.z);
    if(g_scene.stage.worldClump != NULL)
    {
        const RwWorldGeometry& worldGeometry = g_scene.stage.worldGeometry;
        const RwWorldGeometry& collisionGeometry =
            g_scene.stage.collisionGeometry;
        printf("stage world sectors=%d verts=%u tris=%u format=0x%08x\n",
               worldGeometry.sectorCount,
               worldGeometry.vertices.size(),
               worldGeometry.triangles.size(),
               static_cast<unsigned>(worldGeometry.format));

        GameStageNavigation navigation;
        navigation.groundHeight = stageGroundHeight;
        navigation.groundColumn = stageGroundColumn;
        navigation.groundContact = stageGroundContact;
        navigation.combatSurface = stageCombatSurface;
        navigation.combatSurfaceContext = &g_scene.stage.combatSurfaceGeometry;
        navigation.nearestFloor = stageNearestFloor;

        const RwWorldGeometry& groundGeometry =
            g_scene.stage.groundGeometry;
        navigation.context =
            const_cast<RwWorldGeometry*>(&groundGeometry);
        navigation.collisionContext =
            const_cast<RwWorldGeometry*>(&collisionGeometry);
        navigation.minX = worldGeometry.minBounds.x;
        navigation.maxX = worldGeometry.maxBounds.x;
        navigation.minZ = worldGeometry.minBounds.z;
        navigation.maxZ = worldGeometry.maxBounds.z;
        RwWorldVertex anchor;
        navigation.valid = groundGeometry.haveBounds &&
            !groundGeometry.triangles.empty() &&
            !collisionGeometry.triangles.empty() &&
            stageSpawnAnchor(groundGeometry, originalStageId, &anchor,
                             &navigation.spawnHeadingDegrees);
        navigation.spawnX = anchor.x;
        navigation.spawnY = anchor.y;
        navigation.spawnZ = anchor.z;
        printf("stage spawn anchor=(%.2f,%.2f,%.2f) heading=%.1f valid=%s\n",
               anchor.x, anchor.y, anchor.z,
               navigation.spawnHeadingDegrees,
               navigation.valid ? "yes" : "no");
        g_game.setStageNavigation(navigation);
    }
    else
    {

        g_game.clearStageNavigation();
    }
    g_stageObjectAssets.load(g_fstagePath, g_dataPath);
    g_stageInstallContext.story = &g_game.runtimeState().story;
    FstageObjectSpawnState objects;
    objects.userData = &g_stageInstallContext;
    objects.eventFlag = installedStageEventFlag;
    objects.modelResolver = installedStageModel;
    objects.runtime.userData = &g_stageInstallContext;
    objects.runtime.eventSet = installedStageEventSet;
    objects.runtime.sound = installedStageSound;
    objects.runtime.effect = installedStageEffect;
    FstageObjectInstallResult result;
    size_t selectedRowCount = 0;
    const bool installed = installStageInteractions(
        g_cdataPath, g_game, g_npcPositions, originalStageId, &g_scene.stage,
        &objects, &result, &selectedRowCount);
    g_game.setCombatItemScene(installed ? &g_scene.stage : NULL);
    if(installed)
    {
        printf("stage NPOS rows=%u objects=%u/%u missing=%u\n",
               static_cast<unsigned>(selectedRowCount),
               static_cast<unsigned>(result.instantiated),
               static_cast<unsigned>(result.eligible),
               static_cast<unsigned>(result.missingModel));
    }
    g_npcPositionsLoaded = !g_npcPositions.bytes().empty();
    fflush(stdout);
    return true;
}

void updateStoryStageLoadBoundary(void)
{
    if(!g_game.storyStageLoadBoundaryPending())
        return;
    bool resourceReady = true;
    if(g_game.storyStageLoadState() == STORY_STAGE_LOAD_INITIALIZE_ST4)
        resourceReady = updateStageScene();
    g_game.pollStoryStageLoadBoundary(resourceReady);
}

float characterTilt(const GameSlotRecord&)
{

    return 0.0f;
}

float characterHeightOffset(const GameSlotRecord&)
{

    return 0.0f;
}

}
