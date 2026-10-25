#include "app/internal.h"

namespace app_detail
{

TitleAttractState g_titleAttract;

float g_titleAttractFrames;

MenuPulse g_menuPulse = { 0, MENU_PULSE_STEP };

float g_menuPulseFrames;

int g_titleAttractMode = TITLE_ATTRACT_INTERACTIVE;

CameraPathPlayer g_titleCameraPath;

bool g_titleCameraPathStarted = false;

uint32_t g_titleStageVariant = 0;

bool g_titleStageLoaded = false;

const char* g_graphPath = "raw/XAF/GRAPH.XAF";

UiAssetState g_ui;

CameraPresentationState g_cameraPresentation;

FieldCameraRegionState g_fieldCameraRegion = {};

CombatCameraState g_combatCameraState = {};

bool g_combatCameraActive = false;

CamdatRuntime g_camdatRuntime;

uint8_t g_camdatActorSlot = 0;

CombatPresentationState g_combatPresentation;

EventAudioPresentationState g_eventAudioPresentation;

std::multimap<std::string, uint32_t> g_sceneSoundHandles;

EffectManager g_effects;

StageObjectAssets g_stageObjectAssets;

float g_titleAttractSeconds;

uint8_t g_titleMovieIndex;

bool g_titleMoviePending;

bool g_titleMovieWasActive;

bool g_titleWasVisible;

SceneRuntime g_sceneRuntime;

float g_sceneTimeScale = 1.0f;

bool g_sceneRumbleActive = false;

float g_sceneRumbleLow = 0.0f;

float g_sceneRumbleHigh = 0.0f;

const char* g_sceneEntry;

bool g_hostDebugOverlayEnabled;

const char* g_activeSceneEntry;

bool g_gameSceneActive;

bool g_sceneSkipInputReleased;

std::string g_sceneCaption;

SceneCaptionPool g_sceneCaptions;

SceneProps g_sceneProps;

ScenePtaskPool g_scenePtasks;

std::vector<SceneActorState> g_pendingSceneBoneProps;

SceneState g_scene;

const char* g_inputScriptPath;

std::vector<InputScriptCommand> g_inputScriptCommands;

size_t g_inputScriptCommandIndex;

bool g_inputScriptCommandStarted;

bool g_inputScriptCompletionReported;

unsigned long long g_inputScriptCommandFrames;

double g_inputScriptCommandSeconds;

std::chrono::steady_clock::time_point g_inputScriptCommandStartTime;

uint32_t g_keyboardButtons;

GameInputState g_keyboardInput;

GameInputState g_controllerInput[2];

uint32_t g_inputFrameCounter;

float g_inputFrameAccumulator;

const char* g_autoAdvanceTarget;

float g_autoAdvancePulseSeconds;

int g_autoAdvanceSettleFrames;

int g_autoAdvanceMovementFrames;

bool g_autoAdvanceReached;

bool g_argumentError;

char g_runArtifactDirectory[512];

HANDLE g_nativeLogRead;

HANDLE g_nativeLogFile;

HANDLE g_nativeLogThread;

HANDLE g_nativeLogOriginalOutput;

HANDLE g_nativeLogOriginalError;

int g_nativeLogSavedStdout = -1;

int g_nativeLogSavedStderr = -1;

bool g_nativeLogActive;

const char* g_savePath = "RWFightTribe.sav";

int g_windowWidth = 640;

int g_windowHeight = 480;

const char* g_fstagePath = "raw/XAF/FSTAGE.XAF";

const char* g_stagePath = "raw/XAF/STAGE.XAF";

const char* g_characterAnimationEntry = "PA_BASE_00.BIN";

const char* g_fbgmPath = "raw/XAF/FBGM.XAF";

uint16_t g_lastSavedStageId;

const char* g_stageEntry = "STG_01_TRAIN.BIN";

bool g_stageOverride;

const char* g_cdataPath = "raw/XAF/CDATA.XAF";

const char* g_archivePath = "raw/XAF/ARCHIVE.XAF";

const char* g_dataPath = "raw/XAF/DATA.XAF";

const char* g_executablePath = "raw/SLES_535.05";

const char* g_palModeRoot = "raw";

PalModePackage g_palMode;

int g_palModeLanguage = -1;

UiTexture g_palModeTextures[PALMODE_IMAGE_COUNT];

int32_t g_currentMusicTrack;

int32_t g_currentAmbienceTrack;

uint8_t g_currentMusicVolume;

VersusBannerMotionState g_versusBannerMotion;

bool g_versusBannerWasActive;

float g_versusBannerFrames;

LiveBoneEffectSource g_liveBoneEffectSources[EFFECT_EMITTER_CAPACITY] = {};

SceneMotionCursor g_sceneMotionCursors[SCENE_ACTOR_CAPACITY];

FstageNpcPositionData g_npcPositions;

bool g_npcPositionsLoaded;

StageInstallContext g_stageInstallContext = {
    NULL, &g_stageObjectAssets};

bool attachPlugins(void)
{
    rw::ps2::registerPDSPlugin(40);
    rw::ps2::registerPluginPDSPipes();
    rw::registerMeshPlugin();
    rw::registerNativeDataPlugin();
    rw::registerAtomicRightsPlugin();
    rw::registerMaterialRightsPlugin();
    rw::xbox::registerVertexFormatPlugin();
    rw::registerSkinPlugin();
    rw::registerUserDataPlugin();
    rw::registerHAnimPlugin();
    rw::registerMatFXPlugin();
    rw::registerUVAnimPlugin();
    rw::ps2::registerADCPlugin();
    registerCharacterMorphPlugin();
    return true;
}

bool initializationError(const char* message)
{
    fprintf(stderr, "initialization: %s\n", message);
#ifdef _WIN32
    MessageBoxA(NULL, message, "RWFightTribe initialization failed",
                MB_OK | MB_ICONERROR);
#endif
    return false;
}

void enforceFixedClientWindow(void)
{
    HWND window = static_cast<HWND>(engineOpenParams.window);
    if(window == NULL)
        return;

    LONG_PTR style = GetWindowLongPtrA(window, GWL_STYLE);
    style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    SetWindowLongPtrA(window, GWL_STYLE, style);

    RECT windowRect = { 0, 0, g_windowWidth, g_windowHeight };
    const DWORD extendedStyle = static_cast<DWORD>(
        GetWindowLongPtrA(window, GWL_EXSTYLE));
    AdjustWindowRectEx(
        &windowRect, static_cast<DWORD>(style), FALSE, extendedStyle);
    SetWindowPos(
        window, NULL, 0, 0,
        windowRect.right - windowRect.left,
        windowRect.bottom - windowRect.top,
        SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
}

void initializeApplication(void)
{

    resetSceneTasks();
    resetSceneControl();
    resetSceneView();
    sk::globals.windowtitle = "RWFightTribe";
    sk::globals.quit = 0;
    g_scene.world = NULL;
    g_scene.camera = NULL;
    g_keyboardButtons = 0;
    g_keyboardInput.reset();
    for(int i = 0; i < 2; ++i)
    {
        g_controllerInput[i].reset();
    }
    g_inputFrameCounter = 0;
    g_inputFrameAccumulator = 0.0f;
    g_scene.ambient = NULL;
    g_scene.directional = NULL;
    for(int i = 0; i < SCENE_LIGHT_CAPACITY; ++i)
        g_scene.sceneDirectionals[i] = NULL;
    g_scene.playerResourceId = 0xffff;
    g_scene.opponentResourceId = 0xffff;
    g_scene.allyResourceId = 0xffff;
    for(int i = 0; i < STORY_ACTOR_SCENE_CAPACITY; i++)
    {
        g_scene.storyActorIds[i] = 0xffff;
        g_scene.storyActorResourceIds[i] = 0xffff;
    }
    for(int i = 0; i < MAX_GAME_OPPONENTS - 1; i++)
        g_scene.additionalOpponentResourceIds[i] = 0xffff;
    g_scene.stageId = 0xffff;
    g_scene.stageOriginalId = 0xffff;
    g_sceneEntry = NULL;
    g_hostDebugOverlayEnabled = false;
    g_activeSceneEntry = NULL;
    g_gameSceneActive = false;
    g_sceneSkipInputReleased = true;
    g_currentMusicTrack = -1;
    g_currentAmbienceTrack = -1;
    g_currentMusicVolume = 0xff;
    g_titleAttractSeconds = 0.0f;
    g_titleMovieIndex = 0;
    g_titleMoviePending = false;
    g_titleMovieWasActive = false;
    g_autoAdvanceTarget = NULL;
    g_autoAdvancePulseSeconds = 0.0f;
    g_autoAdvanceSettleFrames = 0;
    g_autoAdvanceMovementFrames = 0;
    g_autoAdvanceReached = false;
    g_inputScriptPath = NULL;
    g_inputScriptCommands.clear();
    g_inputScriptCommandIndex = 0;
    g_inputScriptCommandStarted = false;
    g_inputScriptCompletionReported = false;
    g_inputScriptCommandFrames = 0;
    g_inputScriptCommandSeconds = 0.0;
    g_argumentError = false;
    g_runArtifactDirectory[0] = '\0';
    parseArguments();
    if(g_windowWidth < 1)
        g_windowWidth = 1;
    if(g_windowHeight < 1)
        g_windowHeight = 1;

    sk::globals.width = g_windowWidth;
    sk::globals.height = g_windowHeight;
    if(g_autoAdvanceTarget == NULL && !beginNativeLog("debug.log"))
    {
        fprintf(stderr, "ERROR: could not create debug.log\n");
        g_argumentError = true;
    }
    if(g_autoAdvanceTarget != NULL && !beginAutomatedRunArtifacts())
    {
        fprintf(stderr, "ERROR: could not create automated run artifacts\n");
        g_argumentError = true;
    }
    if(g_argumentError)
        sk::globals.quit = 1;
    g_game.initialize();
    GameCombatWeaponCallbacks weaponCallbacks;
    weaponCallbacks.frames = combatWeaponFrames;
    g_game.setCombatWeaponCallbacks(weaponCallbacks);
    BattleResourceCallbacks battleResources;
    battleResources.load = loadBattleAnimationResource;
    g_game.setBattleResourceCallbacks(battleResources);
    g_game.loadGameData("raw/XAF");
    if(!loadCharacterArchiveMetadata(g_executablePath))
    {
        initializationError("Character archive metadata loading failed.");
        sk::globals.quit = 1;
    }
    if(g_game.load(g_savePath))
        g_game.setSaveAvailable(true);
    g_lastSavedStageId = g_game.runtimeState().stageId;
    initializeAudio(g_fbgmPath);
    if(!initializeSoundEffects("raw/XAF/SOUND.XAF"))
    {
        initializationError("Sound-effect initialization failed.");
        sk::globals.quit = 1;
    }
}

bool initializeRenderWare(void)
{
    if(!sk::InitRW())
        return initializationError("RenderWare engine initialization failed.");

    g_scene.world = rw::World::create();
    if(g_scene.world == NULL)
        return initializationError("RenderWare world creation failed.");

    g_scene.camera = sk::CameraCreate(sk::globals.width, sk::globals.height, 1);
    if(g_scene.camera == NULL)
        return initializationError("RenderWare camera creation failed.");
    g_scene.world->addCamera(g_scene.camera);

    g_scene.ambient = rw::Light::create(rw::Light::AMBIENT);
    if(g_scene.ambient == NULL)
        return initializationError("Ambient light creation failed.");
    g_scene.ambient->setFlags(rw::Light::LIGHTATOMICS);
    g_scene.world->addLight(g_scene.ambient);

    g_scene.directional = rw::Light::create(rw::Light::DIRECTIONAL);
    rw::Frame* lightFrame = rw::Frame::create();
    if(g_scene.directional == NULL || lightFrame == NULL)
        return initializationError("Directional light creation failed.");
    g_scene.directional->setFrame(lightFrame);
    g_scene.world->addLight(g_scene.directional);
    restoreDefaultLighting();

    if(!updateStageScene())
        return initializationError("Stage scene loading failed.");
    if(!initializeCharacterShadows(g_dataPath, g_scene.world))
        return initializationError("Character-shadow initialization failed.");
    setCameraForStage();
    if(!g_effects.initialize(g_scene.world))
    {
        fprintf(stderr, "effects: failed to initialize\n");
    }
    else if(!g_effects.load(g_fstagePath, "EFF001.EST"))
    {
        fprintf(stderr, "effects: EFF001.EST unresolved active records: %u\n",
                static_cast<unsigned>(
                    g_effects.asset().unknownActiveRecords));
    }
    initializeUiAssets();
    memset(&g_cameraPresentation, 0, sizeof(g_cameraPresentation));
    memset(&g_combatPresentation, 0, sizeof(g_combatPresentation));
    memset(&g_eventAudioPresentation, 0,
           sizeof(g_eventAudioPresentation));
    for(int i = 0; i < 3; i++)
        g_eventAudioPresentation.track[i] = -1;
    g_eventAudioPresentation.soundBank = -1;
    g_eventAudioPresentation.soundCue = -1;

    if(g_sceneEntry != NULL && startConfiguredScene())
        g_game.beginSceneInspection();
    ImGui_ImplRW_Init();
    ImGui::StyleColorsClassic();
    ImGui::GetStyle().WindowBorderSize = 0.0f;
#ifdef _WIN32
    enforceFixedClientWindow();
#endif
    return true;
}

void terminateRenderWare(void)
{

    resetSceneMask();
    g_game.cancelRumble(0);
    g_game.cancelRumble(1);
    updateControllerRumble();
    g_sceneRuntime.stop();
    releaseUiAssets();
    g_effects.shutdown();
    terminateMoviePlayback();
    terminateAudio();
    ImGui_ImplRW_Shutdown();
    terminateCharacterShadows();
    freeCharacterScene(&g_scene.player);
    freeCharacterScene(&g_scene.opponent);
    freeCharacterScene(&g_scene.ally);
    for(int i = 0; i < STORY_ACTOR_SCENE_CAPACITY; i++)
        freeCharacterScene(&g_scene.storyActors[i]);
    for(int i = 0; i < SCENE_ACTOR_CAPACITY; i++)
        freeCharacterScene(&g_scene.sceneActors[i]);
    for(int i = 0; i < MAX_GAME_OPPONENTS - 1; i++)
        freeCharacterScene(&g_scene.additionalOpponents[i]);
    freeFstageScene(&g_scene.stage);
    g_stageObjectAssets.release();
    for(int i = 1; i < SCENE_LIGHT_CAPACITY; ++i)
    {
        rw::Light* light = g_scene.sceneDirectionals[i];
        if(light == NULL)
            continue;
        if(g_scene.world != NULL)
            g_scene.world->removeLight(light);
        rw::Frame* frame = light->getFrame();
        light->setFrame(NULL);
        if(frame != NULL)
            frame->destroy();
        light->destroy();
        g_scene.sceneDirectionals[i] = NULL;
    }
    if(g_scene.directional != NULL)
    {
        if(g_scene.world != NULL)
            g_scene.world->removeLight(g_scene.directional);
        rw::Frame* frame = g_scene.directional->getFrame();
        g_scene.directional->setFrame(NULL);
        if(frame != NULL)
            frame->destroy();
        g_scene.directional->destroy();
        g_scene.directional = NULL;
    }
    g_scene.sceneDirectionals[0] = NULL;
    if(g_scene.ambient != NULL)
    {
        if(g_scene.world != NULL)
            g_scene.world->removeLight(g_scene.ambient);
        g_scene.ambient->destroy();
        g_scene.ambient = NULL;
    }
    if(g_scene.camera != NULL)
    {
        if(g_scene.world != NULL)
            g_scene.world->removeCamera(g_scene.camera);
        sk::CameraDestroy(g_scene.camera);
        g_scene.camera = NULL;
    }
    if(g_scene.world != NULL)
    {
        g_scene.world->destroy();
        g_scene.world = NULL;
    }
    sk::TerminateRW();
}

void idle(float deltaSeconds)
{
    const float inputFrameSeconds =
        1.0f / static_cast<float>(g_game.nominalFrameRateHz());
    if(deltaSeconds > 0.0f)
        g_inputFrameAccumulator += deltaSeconds;
    const uint32_t elapsedInputFrames = static_cast<uint32_t>(
        g_inputFrameAccumulator / inputFrameSeconds);
    if(elapsedInputFrames != 0)
    {
        g_inputFrameAccumulator -=
            static_cast<float>(elapsedInputFrames) * inputFrameSeconds;
        g_inputFrameCounter += elapsedInputFrames;
    }
    g_keyboardInput.update(deltaSeconds, g_keyboardButtons,
                           g_inputFrameCounter, elapsedInputFrames);
    ControllerFrame controllers[2];
    for(int player = 0; player < 2; ++player)
    {
        controllers[player] = pollController(player);
        if(controllers[player].connected)
        {
            g_controllerInput[player].update(
                deltaSeconds, controllers[player].buttons,
                g_inputFrameCounter, elapsedInputFrames);
        }
        else
            g_controllerInput[player].reset();
        g_game.setControllerConnected(player,
                                      controllers[player].connected);
    }

    if(g_scene.camera != NULL && g_scene.camera->getFrame() != NULL)
    {
        const rw::Matrix& cameraMatrix = g_scene.camera->getFrame()->matrix;

        g_game.setMovementCameraPosition(
            cameraMatrix.pos.x, cameraMatrix.pos.y, cameraMatrix.pos.z);
        g_game.setMovementCameraAxes(
            -cameraMatrix.right.x, -cameraMatrix.right.z,
            cameraMatrix.at.x, cameraMatrix.at.z);
    }
    else
    {
        g_game.setMovementCameraPosition(0.0f, 0.0f, 0.0f);
        g_game.setMovementCameraAxes(1.0f, 0.0f, 0.0f, 1.0f);
    }

    const bool gameplay = g_game.screen() == GAME_SCREEN_GAMEPLAY;
    g_game.setPlayerMovementAxes(
        gameplay && controllers[0].connected ? controllers[0].leftX : 0.0f,
        gameplay && controllers[0].connected ? controllers[0].leftY : 0.0f);
    g_game.setPlayerLookAxes(
        gameplay && controllers[0].connected ? controllers[0].rightX : 0.0f,
        gameplay && controllers[0].connected ? controllers[0].rightY : 0.0f);
    g_game.setSecondPlayerMovementAxes(
        gameplay && controllers[1].connected ? controllers[1].leftX : 0.0f,
        gameplay && controllers[1].connected ? controllers[1].leftY : 0.0f);
    g_game.setSecondPlayerLookAxes(
        gameplay && controllers[1].connected ? controllers[1].rightX : 0.0f,
        gameplay && controllers[1].connected ? controllers[1].rightY : 0.0f);

    uint32_t firstHeld =
        mapHostInput(g_keyboardInput.currentButtons) |
        mapHostInput(g_controllerInput[0].currentButtons);
    uint32_t firstPressed =
        pressedInput(g_keyboardInput) |
        pressedInput(g_controllerInput[0]);
    uint32_t secondHeld =
        mapHostInput(g_controllerInput[1].currentButtons);
    uint32_t secondPressed =
        pressedInput(g_controllerInput[1]);
    applyAutoAdvance(deltaSeconds, &firstHeld, &firstPressed);
    applyInputScript(deltaSeconds, &firstHeld, &firstPressed);
    if(moviePlaybackActive())
    {
        const uint32_t moviePressed = firstPressed | secondPressed;
        const bool preTitleMovie =
            g_game.screen() == GAME_SCREEN_ATTRACT_MOVIE;

        const bool skipMovie = preTitleMovie ?
            (moviePlaybackFrameNumber() > 10 &&
             (firstPressed & GAME_INPUT_DODGE) != 0) :
            moviePressed != 0;
        if(skipMovie)
            stopMoviePlayback();
        firstHeld = firstPressed = secondHeld = secondPressed = 0;
    }
    const uint32_t sceneHeld = firstHeld | secondHeld;
    const uint32_t scenePressed = firstPressed | secondPressed;
    const uint32_t sceneSkipButtons =
        GAME_INPUT_CONFIRM | GAME_INPUT_CANCEL | GAME_INPUT_START;

    if(g_gameSceneActive && sceneEventInputLocked())
        firstHeld = firstPressed = secondHeld = secondPressed = 0;

    publishCombatPoses();
    g_game.update(deltaSeconds, firstHeld, firstPressed,
                  secondHeld, secondPressed);
    updateVersusBannerMotion(deltaSeconds);
    battleHudAdvanceRadarAnimation(g_game.runtimeState().hudRadar);
    hudPromptUpdate(deltaSeconds);

    {
        static GameScreen previous = GAME_SCREEN_LOADING;
        static bool started = false;
        const GameScreen current = g_game.screen();
        if(!started || current != previous)
        {
            if(current == GAME_SCREEN_GAMEPLAY &&
               previous != GAME_SCREEN_PAUSE &&
               previous != GAME_SCREEN_INVENTORY &&
               previous != GAME_SCREEN_OPTIONS)
            {
                g_combatCameraActive = false;
                g_camdatRuntime.stop();
                g_game.setCamdatTimeScale(1.0f);
            }
            printf("screen %s\n", Game::screenName(current));
            if(!inputScriptCaptureActive())
                frameCaptureRequest(Game::screenName(current));
            fflush(stdout);
            previous = current;
            started = true;
        }
    }

    updateSceneManualCamera(
        controllers[0].connected ? controllers[0].rightX : 0.0f,
        controllers[0].connected ? controllers[0].rightY : 0.0f,
        deltaSeconds * static_cast<float>(g_game.nominalFrameRateHz()));
    updateSceneTasks();
    updateSceneControl();

    if(sceneTaskPending() &&
       sceneTaskOperation() == SCENE_TASK_OPERATION_RELOAD_MAIN_CHARACTER)
    {
        g_scene.playerResourceId = 0;
        if(updateCharacterScenes())
            completeSceneTask();
    }

    int ambientMask = 0;
    if(takeSceneAmbientStopRequest(&ambientMask) && (ambientMask & 2) != 0)
        stopAudioChannel(AUDIO_CHANNEL_MUSIC_AMBIENT);
    updateTitleMovieState(deltaSeconds);
    const bool gameSceneStarted = startPendingGameScene();
    observeAutoAdvanceTarget();
    if(g_game.screen() != GAME_SCREEN_GAMEPLAY)
    {
        g_game.setPlayerMovementAxes(0.0f, 0.0f);
        g_game.setPlayerLookAxes(0.0f, 0.0f);
        g_game.setSecondPlayerMovementAxes(0.0f, 0.0f);
        g_game.setSecondPlayerLookAxes(0.0f, 0.0f);
    }
    updateControllerRumble();
    updateGameAudio(firstPressed);
    updateCombatPresentation(deltaSeconds);
    updateStoryStageLoadBoundary();
    if(g_game.screen() == GAME_SCREEN_GAMEPLAY &&
       g_lastSavedStageId != g_game.runtimeState().stageId)
    {
        g_game.save(g_savePath);
        g_lastSavedStageId = g_game.runtimeState().stageId;
    }
    const bool renderScene =
        g_sceneRuntime.status() == SceneStatus::Playing ||
        titleBackdropScreen(g_game.screen()) ||
        g_game.screen() == GAME_SCREEN_CHARACTER_SELECT ||
        g_game.screen() == GAME_SCREEN_STAGE_SELECT ||
        g_game.screen() == GAME_SCREEN_BRIEFING ||
        g_game.screen() == GAME_SCREEN_GAMEPLAY ||
        g_game.screen() == GAME_SCREEN_PAUSE ||
        g_game.screen() == GAME_SCREEN_INVENTORY ||
        g_game.screen() == GAME_SCREEN_OPTIONS ||
        g_game.screen() == GAME_SCREEN_RESULTS;
    if(renderScene && (!updateStageScene() || !updateCharacterScenes()))
        sk::globals.quit = 1;
    g_game.refreshCombatWeaponAttachments();
    float sceneActorDeltaSeconds = deltaSeconds;
    bool sceneActorFrameUpdated = false;
    g_pendingSceneBoneProps.clear();
    if(g_sceneRuntime.status() == SceneStatus::Playing)
    {
        beginSceneActorFrame();

        sceneActorDeltaSeconds =
            g_game.scaleFrameDelta(deltaSeconds) * g_sceneTimeScale;
        tickCharacterScene(&g_scene.player, sceneActorDeltaSeconds);
        for(int i = 0; i < SCENE_ACTOR_CAPACITY; ++i)
            tickCharacterScene(&g_scene.sceneActors[i],
                               sceneActorDeltaSeconds);

        if(!g_sceneSkipInputReleased &&
           (sceneHeld & sceneSkipButtons) == 0)
            g_sceneSkipInputReleased = true;
        const bool skipScene =
            !gameSceneStarted && g_sceneSkipInputReleased &&
            (scenePressed & sceneSkipButtons) != 0;

        g_sceneRuntime.update(sceneActorDeltaSeconds, skipScene);
        settleSceneActorPlacements();
        sceneActorFrameUpdated = true;
    }
    if(g_gameSceneActive &&
       g_sceneRuntime.status() != SceneStatus::Playing)
    {
        g_gameSceneActive = false;
        g_game.completePendingScene();
    }
    if(renderScene)
        updatePresentationCamera(deltaSeconds);
    if(renderScene && titleBackdropScreen(g_game.screen()))
    {
        tickFstageScene(&g_scene.stage, deltaSeconds);

        float ex, ey, ez, tx, ty, tz;
        const float frameStep =
            deltaSeconds * static_cast<float>(g_game.nominalFrameRateHz());
        cameraPathSample(g_titleCameraPath, frameStep,
                         &ex, &ey, &ez, &tx, &ty, &tz);
    }
    else if(renderScene && g_game.screen() == GAME_SCREEN_INVENTORY)
    {

        if(!sceneActorFrameUpdated)
            tickCharacterScene(&g_scene.player, deltaSeconds);
    }

    else if(renderScene && g_game.screen() != GAME_SCREEN_PAUSE &&
       g_game.screen() != GAME_SCREEN_INVENTORY &&
       g_game.screen() != GAME_SCREEN_OPTIONS)
    {
        tickFstageScene(&g_scene.stage, deltaSeconds);
        if(!sceneActorFrameUpdated)
            tickCharacterScene(&g_scene.player, deltaSeconds);
        tickCharacterScene(&g_scene.ally, deltaSeconds);
        for(int i = 0; i < STORY_ACTOR_SCENE_CAPACITY; i++)
            tickCharacterScene(&g_scene.storyActors[i], deltaSeconds);

        if(!sceneActorFrameUpdated)
        {
            for(int i = 0; i < SCENE_ACTOR_CAPACITY; i++)
                tickCharacterScene(&g_scene.sceneActors[i], deltaSeconds);
        }
        tickCharacterScene(&g_scene.opponent, deltaSeconds);
        for(int i = 1; i < g_game.opponentCount(); i++)
            tickCharacterScene(&g_scene.additionalOpponents[i - 1],
                               deltaSeconds);
    }

    if(sceneActorFrameUpdated)
        presentSceneActors(sceneActorDeltaSeconds);

    updateSceneCharacterVisibility(
        g_sceneRuntime.status() == SceneStatus::Playing);

    for(size_t i = 0; i < g_pendingSceneBoneProps.size(); ++i)
        applySceneActorState(NULL, g_pendingSceneBoneProps[i]);
    g_pendingSceneBoneProps.clear();

    dispatchPendingEstEvents();
    g_effects.update(deltaSeconds);
    CharacterShadowActor shadowActors[MAX_CHARACTER_SHADOW_ACTORS];
    const size_t shadowActorCount = gatherCharacterShadowActors(
        shadowActors, MAX_CHARACTER_SHADOW_ACTORS);
    captureCharacterShadows(shadowActors, shadowActorCount);

    updatePalModePackage();
    if(g_game.screen() == GAME_SCREEN_EXIT)
        sk::globals.quit = 1;

    const bool blackClear = moviePlaybackActive() ||
                            g_game.screen() == GAME_SCREEN_LOADING ||
                            g_game.screen() == GAME_SCREEN_ATTRACT_MOVIE;
    rw::RGBA clear = blackClear ? rw::makeRGBA(0, 0, 0, 255) :
                                  rw::makeRGBA(30, 34, 42, 255);
    g_scene.camera->clear(&clear, rw::Camera::CLEARIMAGE | rw::Camera::CLEARZ);
    g_scene.camera->beginUpdate();
    if(moviePlaybackActive())
        updateMoviePlayback();
    else
    {
        if(renderScene)
        {

            rw::SetRenderState(rw::ZTESTENABLE, 1);
            rw::SetRenderState(rw::CULLMODE, rw::CULLNONE);
            const bool authoredSceneLighting =
                g_sceneRuntime.status() == SceneStatus::Playing;
            if(!authoredSceneLighting)
            {
                setStageLightingScale(
                    stageWorldLightScale(g_scene.stageOriginalId));
            }
            setStageFog(true);
            renderStageClumps();
            setStageFog(false);
            renderCharacterShadows(
                g_scene.stage.worldGeometry,
                shadowActors, shadowActorCount);
            if(!authoredSceneLighting)
                setStageLightingScale(1.0f);
            renderActorAndPropClumps();
        }
        if(renderScene)
            g_effects.render();
        ImGui_ImplRW_NewFrame(deltaSeconds);
        const GameScreen pulseScreen = g_game.screen();
        if(pulseScreen == GAME_SCREEN_OPTIONS ||
           pulseScreen == GAME_SCREEN_CHARACTER_SELECT ||
           pulseScreen == GAME_SCREEN_STAGE_SELECT)
        {
            menuPulseAdvanceElapsed(
                g_menuPulse, g_menuPulseFrames,
                deltaSeconds * g_game.nominalFrameRateHz());
        }
        else
        {

            g_menuPulseFrames = 0.0f;
        }
        updateMenuSystemPresentation(deltaSeconds);
        drawGame();
        ImGui::EndFrame();
        ImGui::Render();
        ImGui_ImplRW_RenderDrawLists(ImGui::GetDrawData());
        hudPromptFlush();
    }
    frameCaptureService();
    g_scene.camera->endUpdate();
    g_scene.camera->showRaster(0);
}

}

using namespace app_detail;

void uiCenterText(const char* text)
{
    centerText(text);
}

void uiDrawMenuItem(const char* label, bool selected, bool enabled)
{
    drawMenuItem(label, selected, enabled);
}

void uiDrawCommandPrompt(bool backAvailable)
{
    drawCommandPrompt(backAvailable);
}
bool uiCharacterCustomizationReady(uint64_t fingerprint)
{
    return fingerprint != 0 && g_scene.player.assembledFromArchive &&
        g_scene.player.archiveFingerprint == fingerprint &&
        !g_scene.player.clumps.empty();
}

void uiDrawPauseFallback(void)
{
    drawPauseMenuImGui();
}

void uiDrawOptionsFallback(void)
{
    drawOptionsMenu();
}

rw::EngineOpenParams engineOpenParams;

sk::EventStatus
AppEventHandler(sk::Event event, void* parameter)
{
    using namespace sk;

    ImGuiEventHandler(event, parameter);
    switch(event)
    {
    case INITIALIZE:
        initializeApplication();
        return EVENTPROCESSED;
    case RWINITIALIZE:
        return initializeRenderWare() ? EVENTPROCESSED : EVENTERROR;
    case RWTERMINATE:
        terminateRenderWare();
#ifdef _WIN32
        endNativeLog();
#endif
        return EVENTPROCESSED;
    case PLUGINATTACH:
        return attachPlugins() ? EVENTPROCESSED : EVENTERROR;
    case KEYDOWN:
        keyDown(*static_cast<int*>(parameter));
        return EVENTPROCESSED;
    case KEYUP:
        keyUp(*static_cast<int*>(parameter));
        return EVENTPROCESSED;
    case RESIZE:
    {
        const rw::Rect* rect = static_cast<const rw::Rect*>(parameter);
        if(rect == NULL || rect->w == 0 || rect->h == 0)
            return EVENTPROCESSED;
#ifdef _WIN32
        if(rect->w != g_windowWidth || rect->h != g_windowHeight)
        {
            enforceFixedClientWindow();
            return EVENTPROCESSED;
        }
#endif
        sk::globals.width = g_windowWidth;
        sk::globals.height = g_windowHeight;
        return EVENTPROCESSED;
    }
    case IDLE:
        idle(*static_cast<float*>(parameter));
        return EVENTPROCESSED;
    default:
        break;
    }
    return EVENTNOTPROCESSED;
}
