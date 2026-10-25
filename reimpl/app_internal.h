#ifndef APP_INTERNAL_H
#define APP_INTERNAL_H

#ifdef _WIN32
#include <windows.h>
#include <xinput.h>
#include <fcntl.h>
#include <io.h>
#endif

#include "game.h"
#include "item_world.h"
#include "stage_assets.h"
#include "character.h"
#include "character_shadow.h"
#include "stage.h"
#include "stage_interactions.h"
#include "stage_lighting.h"
#include "stage_objects.h"
#include "character_records.h"
#include "palmode.h"
#include "camera.h"
#include "title_attract.h"
#include "menu_font.h"
#include "menu_system.h"
#include "menu_pause.h"
#include "menu_inventory.h"
#include "scene_overlay.h"
#include "scene_view.h"
#include "scene_task.h"
#include "scene_control.h"
#include "scene_caption_view.h"
#include "scene_mask.h"
#include "menu_layout.h"
#include "options_menu.h"
#include "combat_styles.h"
#include "audio_spatial.h"
#include "audio.h"
#include "effect.h"
#include "xaf_runtime.h"
#include "movie.h"
#include "scene_runtime.h"
#include "scene_captions.h"
#include "scene_props.h"
#include "battle_hud.h"
#include "hud_advice.h"
#include "hud_prompt.h"
#include "frame_capture.h"
#include "sound_cues.h"

#include <skeleton.h>

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <map>
#include <string>

#include <chrono>
#include <cmath>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <vector>

extern rw::EngineOpenParams engineOpenParams;
void hudDrawBattlePromptLine(void);

namespace app_detail
{

inline const int SCENE_ACTOR_CAPACITY = 8;

inline const int SCENE_LIGHT_CAPACITY = 4;

struct SceneActorPresentationState
{
    SceneActorPlacementState placement;
    SceneActorPlacementState authoredPlacement;
    SceneActorPlacementState incomingPlacement;
    bool touched;
    bool placementChanged;
    bool battleSpawn;

    SceneActorPresentationState()
        : placement(), authoredPlacement(), incomingPlacement(),
          touched(false), placementChanged(false), battleSpawn(false)
    {
    }
};

struct SceneState
{
    rw::World* world;
    rw::Camera* camera;
    rw::Light* ambient;
    rw::Light* directional;

    rw::Light* sceneDirectionals[SCENE_LIGHT_CAPACITY];
    FstageScene stage;
    CharacterScene player;
    CharacterScene opponent;
    CharacterScene additionalOpponents[MAX_GAME_OPPONENTS - 1];

    CharacterScene ally;

    CharacterScene storyActors[STORY_ACTOR_SCENE_CAPACITY];
    uint16_t storyActorIds[STORY_ACTOR_SCENE_CAPACITY];
    uint16_t storyActorResourceIds[STORY_ACTOR_SCENE_CAPACITY];

    CharacterScene sceneActors[SCENE_ACTOR_CAPACITY];
    std::string sceneActorTracks[SCENE_ACTOR_CAPACITY];
    uint16_t sceneActorResourceIds[SCENE_ACTOR_CAPACITY];
    bool sceneActorArchiveModes[SCENE_ACTOR_CAPACITY];
    bool sceneActorArchiveFallbacks[SCENE_ACTOR_CAPACITY];
    int16_t sceneActorMotionResourceSets[SCENE_ACTOR_CAPACITY];
    SceneActorPresentationState playerScenePlacement;
    SceneActorPresentationState sceneActorPlacements[SCENE_ACTOR_CAPACITY];
    uint16_t playerResourceId;
    uint64_t playerCustomizationKey;
    uint16_t opponentResourceId;
    uint16_t allyResourceId;
    uint16_t additionalOpponentResourceIds[MAX_GAME_OPPONENTS - 1];
    uint16_t stageId;
    uint16_t stageOriginalId;
};

struct UiTexture
{
    rw::Texture* texture;
    uint16_t width;
    uint16_t height;

    UiTexture() : texture(NULL), width(0), height(0) {}
};

struct EventTextureCacheEntry
{
    int32_t resourceId;
    UiTexture asset;

    EventTextureCacheEntry() : resourceId(-1) {}
};

struct UiAssetState
{

    UiTexture systemFont;
    EventTextureCacheEntry eventTextures[EVENT_VM_TEXTURE_SLOT_COUNT];
    std::vector<XafEntryInfo> graphEntries;
    std::vector<XafEntryInfo> audioEntries;
};

struct CameraPose
{
    rw::V3d eye;
    rw::V3d target;
    float fieldOfView;
    float nearPlane;
    float farPlane;
};

struct CameraPresentationState
{
    CameraPose current;
    GameScreen previousScreen;
    float positionTransitionFrames;
    float targetTransitionFrames;
    float shakeSeconds;
    float shakeMagnitude;
    uint32_t positionSignature;
    uint32_t targetSignature;
    float fieldAzimuthDegrees;
    bool initialized;
    bool followPlayer;
    bool scripted;
    bool fieldAzimuthInitialized;

    GameplayCameraCollisionState collision;
    BattleCameraSubject battleSubjects[2];
    float battleFramingAngleDegrees;
    bool battleSubjectsInitialized;
};

struct SlotPresentationSnapshot
{
    int16_t health;
    CombatAction action;
    CombatReaction reaction;
    uint8_t comboStep;
};

struct CombatPresentationState
{

    SlotPresentationSnapshot slots[MAX_GAME_OPPONENTS + 2];
    float damageFlash[MAX_GAME_OPPONENTS + 2];
    float displayedHealth[MAX_GAME_OPPONENTS + 2];
    GameScreen previousScreen;
    uint8_t targetSlot;
    bool valid;
};

struct EventAudioPresentationState
{
    int32_t track[3];
    int32_t volume[3];
    uint8_t playing[3];
    uint8_t paused[3];
    int32_t soundBank;
    int32_t soundCue;
    uint32_t soundHandle;
    bool valid;
};

inline const float UI_VIRTUAL_WIDTH = 640.0f;

inline const float UI_VIRTUAL_HEIGHT = 448.0f;

inline const float UI_SPACE_1 = 4.0f;

inline const float UI_SPACE_2 = 8.0f;

inline const float UI_SPACE_3 = 12.0f;

inline const float UI_SPACE_4 = 16.0f;

inline const float UI_SPACE_6 = 24.0f;

inline const float UI_SPACE_8 = 32.0f;

inline const float TITLE_ATTRACT_SECONDS = 34.633335f;

extern TitleAttractState g_titleAttract;

extern float g_titleAttractFrames;

extern MenuPulse g_menuPulse;

extern float g_menuPulseFrames;

extern int g_titleAttractMode;

extern CameraPathPlayer g_titleCameraPath;

extern bool g_titleCameraPathStarted;

extern uint32_t g_titleStageVariant;

extern bool g_titleStageLoaded;

inline const float CAMERA_RESPONSE = 8.0f;

inline const float FIELD_CAMERA_SMOOTH_ALPHA = 0.074f;

inline const float FIELD_CAMERA_SMOOTH_DEADBAND = 2.0f;

inline const float COMBAT_CAMERA_FIELD_OF_VIEW = 39.0f;

inline const float COMBAT_CAMERA_FOLLOW_DISTANCE = 45.0f;

inline const float COMBAT_CAMERA_ORBIT_DISTANCE = 36.0f;

inline const float COMBAT_CAMERA_MIN_BOOM = 18.0f;

inline const float STAGE_CAMERA_NEAR_PLANE = 1.0f;

inline const ImU32 UI_COLOR_INK = IM_COL32(236, 232, 218, 255);

inline const ImU32 UI_COLOR_SHADOW = IM_COL32(8, 10, 14, 220);

inline const ImU32 UI_COLOR_SURFACE = IM_COL32(22, 24, 29, 232);

inline const ImU32 UI_COLOR_HEALTH = IM_COL32(218, 187, 62, 255);

inline const ImU32 UI_COLOR_HEALTH_LOW = IM_COL32(194, 48, 42, 255);

inline const ImU32 UI_COLOR_DAMAGE = IM_COL32(238, 225, 173, 150);

inline const ImU32 UI_COLOR_ACCENT = IM_COL32(255, 224, 82, 255);

inline const ImU32 UI_COLOR_DANGER = IM_COL32(166, 22, 28, 150);

extern const char* g_graphPath;

extern UiAssetState g_ui;

extern CameraPresentationState g_cameraPresentation;

extern FieldCameraRegionState g_fieldCameraRegion;

extern CombatCameraState g_combatCameraState;

extern bool g_combatCameraActive;

extern CamdatRuntime g_camdatRuntime;

extern uint8_t g_camdatActorSlot;

extern CombatPresentationState g_combatPresentation;

extern EventAudioPresentationState g_eventAudioPresentation;

extern std::multimap<std::string, uint32_t> g_sceneSoundHandles;

extern EffectManager g_effects;

extern const char* g_fstagePath;

extern StageObjectAssets g_stageObjectAssets;

extern float g_titleAttractSeconds;

extern uint8_t g_titleMovieIndex;

extern bool g_titleMoviePending;

extern bool g_titleMovieWasActive;

extern bool g_titleWasVisible;

extern SceneRuntime g_sceneRuntime;

extern float g_sceneTimeScale;

extern bool g_sceneRumbleActive;

extern float g_sceneRumbleLow;

extern float g_sceneRumbleHigh;

extern const char* g_sceneEntry;

extern bool g_hostDebugOverlayEnabled;

extern const char* g_activeSceneEntry;

extern bool g_gameSceneActive;

extern bool g_sceneSkipInputReleased;

extern std::string g_sceneCaption;

extern SceneCaptionPool g_sceneCaptions;

extern SceneProps g_sceneProps;

extern ScenePtaskPool g_scenePtasks;

extern std::vector<SceneActorState> g_pendingSceneBoneProps;

inline const char* STORY_STAGE_ENTRIES[53] =
{
    "STG_01_TRAIN.BIN", "STG_02_NCAR.BIN", "STG_03_MCAR.BIN",
    "STG_04_NCAR.BIN", "STG_05_MCAR.BIN", "STG_06_MCAR.BIN",
    "STG_07_MCAR.BIN", "STG_08_MCAR.BIN", "STG_09_MCAR.BIN",
    "STG_10_NCAR.BIN", "STG_11_NCAR.BIN", "STG_12_NCAR.BIN",
    "STG_13_NCAR.BIN", "STG_14_NCAR.BIN", "STG_15_NCAR.BIN",
    "STG_16_NCAR.BIN", "STG_17_NCAR.BIN", "STG_18_SCAR.BIN",
    "STG_19_SCAR.BIN", "STG_20_SCAR.BIN", "STG_21_SCAR.BIN",
    "STG_22_SCAR.BIN", "STG_23_SCAR.BIN", "STG_24_SCAR.BIN",
    "STG_25_SCAR.BIN", "STG_26_SCAR.BIN", "STG_27_SCAR.BIN",
    "STG_28_SCAR.BIN", "STG_29_SCAR.BIN", "STG_30_SCAR.BIN",
    "STG_31_SCAR.BIN", "STG_32_SCAR.BIN", "STG_33_SCAR.BIN",
    "STG_34_SCAR.BIN", "STG_35_SCAR.BIN", "STG_36_SCAR.BIN",
    "STG_37_SCAR.BIN", "STG_38_SCAR.BIN", "STG_39_SCAR.BIN",
    "STG_40_SCAR.BIN", "STG_41_SCAR.BIN", "STG_42_SCAR.BIN",
    "STG_43_SCAR.BIN", "STG_44_SCAR.BIN", "STG_45_SCAR.BIN",
    "STG_46_SCAR.BIN", "STG_47_SCAR.BIN", "STG_48_SCAR.BIN",
    "STG_49_SCAR.BIN", "STG_50_SCAR.BIN", "STG_51_SCAR.BIN",
    "STG_52_SCAR.BIN", "STG_53_SCAR.BIN"
};

extern SceneState g_scene;

enum HostInputButton
{
    HOST_INPUT_STICK_UP = 1u << 16,
    HOST_INPUT_STICK_DOWN = 1u << 17,
    HOST_INPUT_STICK_LEFT = 1u << 18,
    HOST_INPUT_STICK_RIGHT = 1u << 19,
    HOST_INPUT_KEY_CONFIRM = 1u << 20,
    HOST_INPUT_KEY_CANCEL = 1u << 21,
    HOST_INPUT_KEY_PAUSE = 1u << 22,
    HOST_INPUT_KEY_QUIT = 1u << 23,
    HOST_INPUT_KEY_ATTACK = 1u << 24,
    HOST_INPUT_KEY_HEAVY = 1u << 25,
    HOST_INPUT_KEY_GRAB = 1u << 26,
    HOST_INPUT_KEY_GUARD = 1u << 27,
    HOST_INPUT_KEY_DODGE = 1u << 28,
    HOST_INPUT_KEY_INTERACT = 1u << 29,
    HOST_INPUT_KEY_USE_FIRST = 1u << 30,
    HOST_INPUT_KEY_USE_SECOND = 1u << 31
};

struct ControllerFrame
{
    uint32_t buttons;
    float leftX;
    float leftY;
    float rightX;
    float rightY;
    bool connected;
};

inline const double INPUT_SCRIPT_DEFAULT_TIMEOUT_SECONDS = 60.0;

enum InputScriptCommandType
{
    INPUT_SCRIPT_WAIT_SCREEN,
    INPUT_SCRIPT_WAIT_FRAMES,
    INPUT_SCRIPT_WAIT_SECONDS,
    INPUT_SCRIPT_PRESS,
    INPUT_SCRIPT_HOLD,
    INPUT_SCRIPT_AXIS,
    INPUT_SCRIPT_CAPTURE,
    INPUT_SCRIPT_EXPECT_SCREEN,
    INPUT_SCRIPT_QUIT
};

enum InputScriptCaptureTiming
{
    INPUT_SCRIPT_CAPTURE_PRE_EDGE,
    INPUT_SCRIPT_CAPTURE_POST_UPDATE
};

struct InputScriptCommand
{
    InputScriptCommandType type;
    int lineNumber;
    std::string text;
    std::vector<std::string> screenNames;
    std::string conditionScreen;
    std::string captureName;
    InputScriptCaptureTiming captureTiming;
    std::string postInputCaptureName;
    uint32_t buttons;
    unsigned long long frameCount;
    double durationSeconds;
    double timeoutSeconds;
    float axisX;
    float axisY;

    InputScriptCommand()
        : type(INPUT_SCRIPT_QUIT), lineNumber(0),
          captureTiming(INPUT_SCRIPT_CAPTURE_PRE_EDGE), buttons(0),
          frameCount(0), durationSeconds(0.0), timeoutSeconds(0.0),
          axisX(0.0f), axisY(0.0f)
    {
    }
};

extern const char* g_inputScriptPath;

extern std::vector<InputScriptCommand> g_inputScriptCommands;

extern size_t g_inputScriptCommandIndex;

extern bool g_inputScriptCommandStarted;

extern bool g_inputScriptCompletionReported;

extern unsigned long long g_inputScriptCommandFrames;

extern double g_inputScriptCommandSeconds;

extern std::chrono::steady_clock::time_point g_inputScriptCommandStartTime;

extern uint32_t g_keyboardButtons;

extern GameInputState g_keyboardInput;

extern GameInputState g_controllerInput[2];

extern uint32_t g_inputFrameCounter;

extern float g_inputFrameAccumulator;

extern const char* g_autoAdvanceTarget;

extern float g_autoAdvancePulseSeconds;

extern int g_autoAdvanceSettleFrames;

extern int g_autoAdvanceMovementFrames;

extern bool g_autoAdvanceReached;

extern bool g_argumentError;

extern char g_runArtifactDirectory[512];

extern HANDLE g_nativeLogRead;

extern HANDLE g_nativeLogFile;

extern HANDLE g_nativeLogThread;

extern HANDLE g_nativeLogOriginalOutput;

extern HANDLE g_nativeLogOriginalError;

extern int g_nativeLogSavedStdout;

extern int g_nativeLogSavedStderr;

extern bool g_nativeLogActive;

extern const char* g_savePath;

extern int g_windowWidth;

extern int g_windowHeight;

extern const char* g_fstagePath;

extern const char* g_stagePath;

extern const char* g_characterAnimationEntry;

extern const char* g_fbgmPath;

extern uint16_t g_lastSavedStageId;

extern const char* g_stageEntry;

extern bool g_stageOverride;

extern const char* g_cdataPath;

extern const char* g_archivePath;

extern const char* g_dataPath;

extern const char* g_executablePath;

extern const char* g_palModeRoot;

extern PalModePackage g_palMode;

extern int g_palModeLanguage;

extern UiTexture g_palModeTextures[PALMODE_IMAGE_COUNT];

extern int32_t g_currentMusicTrack;

extern int32_t g_currentAmbienceTrack;

extern uint8_t g_currentMusicVolume;

extern VersusBannerMotionState g_versusBannerMotion;

extern bool g_versusBannerWasActive;

extern float g_versusBannerFrames;

struct LiveBoneEffectSource
{
    uint32_t emitterHandle;
    uint8_t actorSlot;
    uint8_t boneIndex;
};

extern LiveBoneEffectSource g_liveBoneEffectSources[EFFECT_EMITTER_CAPACITY];

struct SceneMotionCursor : SceneMotionPlaybackState
{
    const CharacterScene* scene;
    int moveId;
    SceneActorRootMotionState rootMotion;
    std::vector<ScenePtaskMotion> ptaskMotions;
    int ptaskLoopStart;
};

extern SceneMotionCursor g_sceneMotionCursors[SCENE_ACTOR_CAPACITY];

inline const size_t MAX_CHARACTER_SHADOW_ACTORS =
    MAX_GAME_OPPONENTS + STORY_ACTOR_SCENE_CAPACITY +
    SCENE_ACTOR_CAPACITY + 2;

extern FstageNpcPositionData g_npcPositions;

extern bool g_npcPositionsLoaded;

inline const float FIRST_PLAYABLE_SPAWN_X = -425.10f;

inline const float FIRST_PLAYABLE_SPAWN_Z = -32.70f;

inline const float FIRST_PLAYABLE_SPAWN_HEADING = 89.3f;

struct StageInstallContext
{
    StoryState* story;
    StageObjectAssets* assets;
};

extern StageInstallContext g_stageInstallContext;

inline const int ALLY_PRESENTATION_INDEX = MAX_GAME_OPPONENTS + 1;

inline const ImVec4 UI_COLOR_SELECTED(0.94f, 0.78f, 0.22f, 1.0f);

inline const ImVec4 UI_COLOR_DISABLED(0.48f, 0.50f, 0.54f, 1.0f);

inline const ImVec4 UI_COLOR_PROMPT(0.74f, 0.76f, 0.80f, 1.0f);

float stageCameraFarPlane(uint16_t stageId);
float stageFieldCameraDistance(uint16_t stageId);

inline const char* APP_UI_LANGUAGES[5] =
        {
            "ENGLISH", "FRANÇAIS", "DEUTSCH", "ESPAÑOL", "ITALIANO"
        };

inline const ImU32 APP_UI_SHADES[5] =
        {
            IM_COL32(0, 0, 0, 255), IM_COL32(48, 48, 48, 255),
            IM_COL32(112, 112, 112, 255), IM_COL32(192, 192, 192, 255),
            IM_COL32(255, 255, 255, 255)
        };

inline const char* const APP_UI_BATTLEFALLBACKS[BATTLE_PAUSE_ROW_COUNT] =
    {
        "RESUME", "MOVE LIST", "RESTART THIS MATCH",
        "RETURN TO MODE SELECT", "RETURN TO CHARACTER SELECT",
        "QUIT GAME"
    };

inline const char* const APP_UI_STORYFALLBACKS[PAUSE_ROW_COUNT] =
    {
        "RESUME", "MOVE LIST", "QUIT GAME"
    };

inline const uint16_t APP_UI_BATTLETEXTIDS[BATTLE_PAUSE_ROW_COUNT] =
    {
        GAME_TEXT_RESUME, 10472, GAME_TEXT_RESTART_MATCH,
        GAME_TEXT_RETURN_MODE_SELECT, GAME_TEXT_RETURN_CHARACTER_SELECT,
        GAME_TEXT_QUIT_GAME
    };

inline const uint16_t APP_UI_STORYTEXTIDS[PAUSE_ROW_COUNT] =
    {
        GAME_TEXT_RESUME, 10472, GAME_TEXT_QUIT_GAME
    };

const char* versusBannerNameForSide(int side);
void updateVersusBannerMotion(float deltaSeconds);
const char* presentationText(uint16_t textId, const char* hostLiteral);
const char* stageSelectionDisplayName(int versusIndex);
uint16_t characterResourceId(CharacterId characterId);
bool hasExtension(const std::string& path, const char* extension);
bool loadUiTexture(const char* archivePath, const char* entryName,
                   UiTexture* asset);
void releaseUiTexture(UiTexture* asset);
void initializeUiAssets(void);
void releaseUiAssets(void);
ImTextureRef uiTextureRef(const UiTexture& asset);
bool
drawBootImage(int imageId, float maxWidth);
bool
drawBootImageStretched(int imageId, float centreX, float centreY,
                       float width, float height);
void updatePalModePackage(void);
void setCameraForStage(void);
void setCameraLookAt(const rw::V3d& eye, const rw::V3d& target);
float clampPresentation(float value, float minimum, float maximum);
float wrapPresentationDegrees(float degrees);
float advancePresentationAngle(float current, float target, float maximumStep);
rw::V3d presentationVector(float x, float y, float z);
rw::V3d mixPresentationVector(const rw::V3d& from, const rw::V3d& to,
                              float amount);
rw::V3d smoothPresentationVector(const rw::V3d& from, const rw::V3d& to,
                                 float amount, float deadband);
uint32_t cameraTransitionSignature(const StoryCameraTransition& transition);
rw::V3d sampleCameraTransition(const StoryCameraTransition& transition,
                               float elapsedFrames);
CameraPose previewCameraPose(void);
CharacterScene* combatCharacterSceneForSlot(uint8_t slotIndex);
CharacterScene* combatCharacterScene(const GameSlotRecord& slot);
bool combatWeaponFrames(void* userData, uint8_t slotIndex, rw::Frame** frames);
bool characterSceneBoneMatrix(const CharacterScene* scene, uint8_t boneIndex,
                              rw::Matrix& matrix);
bool combatBoneMatrix(uint8_t slotIndex, uint8_t boneIndex,
                      EffectMatrix* matrix);
const GameSlotRecord* combatSlotForIndex(uint8_t slotIndex);
bool provideCamdatBasis(void* userData, bool linkedActor,
                        uint16_t boneIndex, CamdatFramingBasis* basis);
bool updateCamdatCamera(float deltaSeconds, CameraPose& pose);
bool combatAnimationFrame(uint8_t slotIndex, float* frame);
void publishCombatantPose(const GameSlotRecord& slot);
bool loadBattleAnimationResource(void* userData, int resourceId);
void publishCombatPoses(void);
bool directionEffectMatrix(const Vec3& direction,
                           const EffectVector3& position,
                           EffectMatrix* matrix);
bool provideLiveBoneEffectMatrix(void* userData, EffectMatrix* matrix);
LiveBoneEffectSource* acquireLiveBoneEffectSource();
bool loadedEstRecord(uint8_t recordIndex);
void dispatchPendingEstEvents();
bool characterSceneCameraSubject(const CharacterScene* scene, Vec3& position);
uint8_t fieldCameraGroundSurface(uint16_t stageId, float playerZ);
CameraPose combatCameraPose(float deltaSeconds);
CameraPose desiredCameraPose(float deltaSeconds);
void applyPresentationCamera(const CameraPose& pose);
void applySceneCamera(void*, const SceneCameraState& state);
const char* selectedSceneActorName(void);
int sceneMotionIdFromPath(const char* path);
SceneMotionCursor* sceneMotionCursorFor(const CharacterScene* scene);
void resetSceneMotionCursors(void);
bool sceneActorRootWorldPosition(const CharacterScene* scene,
                                 SceneVec3* position);
bool sceneActorFramePosition(const CharacterScene* scene,
                             SceneVec3* position);
bool commitSceneActorMotionPosition(CharacterScene* scene,
                                    SceneMotionCursor& cursor);
void resetSceneActorMotionPosition(CharacterScene* scene);
bool moveMotionSequenceLoops(int moveId);
bool bindSceneActorMotion(CharacterScene* scene, int motionResourceSet,
                          const char* mtdPath, float elapsedSeconds);
void retainSceneActorMotionPosition(
    CharacterScene* scene, SceneActorPresentationState* presentation);
bool sceneActorCharacterId(const char* resourceName, uint16_t* characterId,
                           bool* archiveMode);
SceneActorPresentationState* sceneActorPresentationFor(CharacterScene* scene);
void resetSceneActorPlacements(void);
void beginSceneActorPresentationFrame(
    SceneActorPresentationState& presentation);
void beginSceneActorFrame(void);
bool sceneActorPlacementMatches(const SceneActorPlacementState& left,
                                const SceneActorPlacementState& right);
void settleSceneActorPlacement(
    CharacterScene* scene, SceneActorPresentationState& presentation);
void settleSceneActorPlacements(void);
bool sceneActorGroundHeight(void*, float x, float z, float ceiling,
                            float* height);
float sceneActorRootY(const CharacterScene* scene, float fallback);
void presentSceneActor(CharacterScene* scene,
                       SceneActorPresentationState& presentation,
                       float scaledDeltaSeconds);
void presentSceneActors(float scaledDeltaSeconds);
void setCharacterSceneVisible(CharacterScene* scene, bool visible);
void updateSceneCharacterVisibility(bool sceneActive);
bool appendCharacterShadowActor(
    CharacterShadowActor* actors, size_t capacity, size_t& count,
    const CharacterScene* scene);
size_t gatherCharacterShadowActors(
    CharacterShadowActor* actors, size_t capacity);
void renderAttachedClump(rw::Clump* clump);
void renderStageClumps(void);
void renderCharacterClumps(const CharacterScene& scene);
void renderActorAndPropClumps(void);
CharacterScene* sceneActorFor(const char* trackName, const char* resourceName);
CharacterScene* loadedSceneActorFor(const char* trackName);
int sceneActorMotionResourceSet(const CharacterScene* scene);
void applySceneActorState(void*, const SceneActorState& state);
void applySceneActor(void* userData, const SceneActorState& state);
void setSceneLightAngles(rw::Light* light, float pitchDegrees,
                         float yawDegrees);
void setDirectionalLightAngles(float pitchDegrees, float yawDegrees);
void setStageLightingScale(float scale);
void setStageFog(bool enabled);
void restoreDefaultLighting(void);
rw::Light* sceneDirectionalLight(int32_t index);
void applySceneLight(void*, const SceneLightState& state);
void applySceneEffect(void*, const SceneEffectState& state);
void applySceneAudio(void*, const SceneAudioEvent& event);
void applySceneCaption(void*, const SceneCaptionEvent& event);
void restoreScenePresentation(void*, bool, bool);
SceneLanguage selectedSceneLanguage(void);
bool loadSceneStage(uint16_t originalStageId);
bool startSceneEntry(const char* entryName);
bool startConfiguredScene(void);
void updatePresentationCamera(float deltaSeconds);
bool stageGroundHeight(void* context, float x, float z, float ceiling,
                       float* height);
bool stageNearestFloor(void* context, float x, float z, float* outX,
                       float* outY, float* outZ);
bool stageSpawnAnchor(const RwWorldGeometry& geometry, uint16_t stageId,
                      RwWorldVertex* anchor, float* headingDegrees);
bool installedStageEventFlag(uint16_t eventFlag, void* userData);
void installedStageEventSet(uint16_t eventFlag, void* userData);
void installedStageSound(uint8_t bankSlot, uint8_t cue, uint8_t volume,
                         void*);
void installedStageEffect(uint8_t recordIndex, const rw::Matrix& world,
                          int16_t tuningA, int16_t tuningB, void*);
rw::Clump* installedStageModel(int8_t kind, int8_t selector, void* userData);
float actorPresentationY(float x, float y, float z);
void updateStoryActorScenes(void);
bool titleBackdropScreen(GameScreen screen);
bool updateTitleStageScene(void);
bool updateStageScene(void);
void updateStoryStageLoadBoundary(void);
float characterTilt(const GameSlotRecord&);
float characterHeightOffset(const GameSlotRecord&);
size_t combatAnimationIndex(const GameSlotRecord&);
bool loopingCombatAnimation(CombatAction action);
uint32_t combatAnimationState(const GameSlotRecord& slot);
bool bindCombatantAnimation(CharacterScene* scene, const GameSlotRecord& slot,
                            const MoveMotion* scheduledMotion = NULL);
bool bindLiveCombatantAnimation(CharacterScene* scene,
                                const GameSlotRecord& slot);
bool updateCharacterScenes(void);
bool startPendingGameScene(void);
int gatherPresentationIndices(int* indices);
const GameSlotRecord& presentationSlot(int index);
void updateCombatPresentation(float deltaSeconds);
bool startTitleAttractMovie(int movieIndex);
void updateTitleMovieState(float deltaSeconds);
void centerText(const char* text);
void drawMenuItem(const char* label, bool selected, bool enabled);
void drawMenuItem(const char* label, bool selected);
void drawMenuValue(const char* label, const char* value,
                   bool selected, bool enabled);
void drawCommandPrompt(bool backAvailable);
const char* optionsRowValue(const OptionEntry& entry, char* buffer, size_t size);
void drawOptionCommandPrompt(OptionPromptStyle style);
void drawPauseMenuImGui(void);
void drawOptionsMenu(void);
ImU32 eventColor(uint32_t color);
ImU32 eventColorWithAlpha(uint32_t color, float alpha);
UiTexture* eventTextureForSprite(const EventVm& event, int cacheIndex,
                                 int32_t source);
void drawEventOverlay(void);
bool projectWorldPoint(const rw::V3d& world, ImVec2* screen);
void drawGameplayPresentation(void);
bool drawSystemFontText(float x, float y, const char* text,
                        bool alignCentre, const ImVec4& color, float scale);
bool drawMenuLayoutItem(int itemIndex, int colourIndex, float alphaScale);
void drawCenteredAsset(const UiTexture& asset, float width, float y);
float sceneCaptionBattleCameraHeight(char mode);
void drawSceneCaption(void);
void drawSceneConfirmationPrompt(void);
bool markerEventFlagQuery(uint16_t eventFlag, void* userData);
void drawStageInteractionMarker(void);
void drawIntroCardSequence(void);
void drawRecoveredLoadingScreen(void);
void drawHostDebugOverlay(void);
void drawGame(void);
bool attachPlugins(void);
bool inputScriptScreenNameValid(const std::string& name);
std::string trimInputScriptText(const std::string& text);
std::vector<std::string> tokenizeInputScriptLine(const std::string& line);
bool parseInputScriptUnsigned(const std::string& text,
                              unsigned long long* value);
bool parseInputScriptNumber(const std::string& text, double* value);
uint32_t inputScriptButton(const std::string& name);
bool parseInputScriptButtons(const std::string& text, uint32_t* buttons);
bool parseInputScriptScreens(
    const std::string& text, std::vector<std::string>* names);
bool reportInputScriptParseError(int lineNumber, const char* message);
bool parseInputScriptCommand(const std::string& text, int lineNumber);
bool loadInputScript(void);
bool inputScriptAtScreen(const InputScriptCommand& command);
void failInputScript(const InputScriptCommand& command, const char* message);
void completeInputScriptCommand(const InputScriptCommand& command,
                                const char* outcome);
bool inputScriptCommandTimedOut(const InputScriptCommand& command);
void applyInputScript(float deltaSeconds, uint32_t* held, uint32_t* pressed);
bool inputScriptCaptureActive(void);
bool autoAdvanceTargetValid(const char* target);
bool autoAdvanceAtTarget(void);
void observeAutoAdvanceTarget(void);
void applyAutoAdvance(float deltaSeconds,
                      uint32_t* held, uint32_t* pressed);
bool validNativeHandle(HANDLE handle);
HANDLE duplicateNativeHandle(HANDLE source);
DWORD WINAPI nativeLogPump(void*);
bool beginNativeLog(const char* path);
void endNativeLog(void);
bool beginAutomatedRunArtifacts(void);
void parseArguments(void);
const char* audioEntryName(int32_t index);
void updateEventAudioPresentation(void);
AudioSpatialMatrix audioSpatialMatrix(const rw::Matrix& matrix);
AudioListener activeAudioListener(void);
void updateGameAudio(uint32_t);
bool initializationError(const char* message);
void enforceFixedClientWindow(void);
void initializeApplication(void);
bool initializeRenderWare(void);
void terminateRenderWare(void);
uint32_t keyButton(int key);
void keyDown(int key);
void keyUp(int key);
uint32_t mapHostInput(uint32_t buttons);
float normalizedThumb(short value);
void normalizeStick(short rawX, short rawY, float deadZone,
                    float* outputX, float* outputY);
ControllerFrame pollController(int playerIndex);
uint32_t pressedInput(const GameInputState& state);
void updateControllerRumble(void);
void idle(float deltaSeconds);

}

#endif
