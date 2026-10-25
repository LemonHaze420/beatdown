#ifndef CHARACTER_H
#define CHARACTER_H

namespace rw
{
struct Clump;
struct Frame;
struct Atomic;
struct Geometry;
struct TexDictionary;
struct Animation;
struct HAnimHierarchy;
struct World;
struct Texture;
struct Image;
}

#include <stddef.h>
#include <stdint.h>
#include <vector>

#include "story_state.h"
#include "palmode.h"

const uint32_t CHARACTER_ARCHIVE_RESOURCE_BASE = 0x3a6;
const uint32_t CHARACTER_ARCHIVE_TEXTURE_BASE = 0x10e7;
const uint32_t CHARACTER_ARCHIVE_METADATA_COUNT = 488;

struct CharacterArchiveMetadata
{
    uint32_t type;
    uint32_t objectTag;
    uint32_t flags;
    uint32_t callbackAddress;
    uint16_t resourceOrdinal;
    uint16_t auxiliaryOrdinal;
    int16_t textureDependencies[CHARACTER_ARCHIVE_TEXTURE_COUNT];
};

struct CharacterAttachment
{
    uint32_t archiveId = 0;
    CharacterArchiveMetadata metadata = {};
    size_t firstClump = 0;
    size_t clumpCount = 0;
    int32_t boneIndex = -1;

    rw::Frame* boneFrame = NULL;
    float scale[3] = { 1.0f, 1.0f, 1.0f };
    bool active = false;
};

struct CharacterBoneTransform
{
    int32_t parentIndex;
    float translation[3];
};

struct CharacterAnimationBlendPose
{
    float rotation[4];
    float translation[3];
};

struct CharacterAnimationPlayback
{
    float startSeconds;
    float endSeconds;
    float transitionSeconds;
    float rate;
    bool loop;
};

struct CharacterScene
{
    CharacterScene();

    std::vector<uint8_t> data;
    std::vector<rw::Clump*> clumps;
    std::vector<rw::Animation*> animations;
    std::vector<uint64_t> animationKeys;

    size_t basicAnimation;
    std::vector<rw::HAnimHierarchy*> hierarchies;
    std::vector<std::vector<CharacterBoneTransform> > hierarchyBoneTransforms;
    size_t activeAnimation;
    uint32_t animationState;
    float animationRate;
    bool animationLoop;
    float animationStartTime;
    float animationEndTime;
    float animationTransitionDuration;
    float animationTransitionRemaining;
    std::vector<std::vector<CharacterAnimationBlendPose> >
        animationBlendSource;
    rw::TexDictionary* textureDictionary;
    rw::World* world;
    std::vector<rw::TexDictionary*> attachmentTextureDictionaries;
    std::vector<CharacterAttachment> attachments;
    uint64_t archiveFingerprint = 0;
    bool assembledFromArchive = false;
    float boundsCenter[3];
    float boundsRadius;
};

const int STORY_ACTOR_SCENE_CAPACITY = 6;

int selectRenderableStoryActors(const StoryState& story,
                                const uint16_t* excluded, int excludedCount,
                                uint16_t* out, int capacity);

bool loadCharacterArchiveMetadata(const char* executablePath);

rw::Image* decodeCharacterImageChunk(const uint8_t* chunk, uint32_t length);
void prepareCharacterRenderClump(rw::Clump* clump);

enum CharacterSceneModelMode
{
    CHARACTER_SCENE_MODEL_DIRECT = 0,
    CHARACTER_SCENE_MODEL_ARCHIVE = 1
};

CharacterSceneModelMode characterSceneModelMode(const char* resourceName);
bool characterArchiveMetadata(uint32_t archiveId,
                              CharacterArchiveMetadata& metadata);
uint64_t characterCustomizationFingerprint(
    const CharacterCustomization& customization);

void normalizeInventoryEquipRoute(int32_t& menuCategory,
                                  int32_t& categorySlot);
bool applyInventoryEquipToCustomization(
    const InventoryItemDefinition& definition, uint8_t variant,
    CharacterCustomization& customization, uint32_t& changedGroupMask);

bool loadCharacterArchiveScene(const char* archivePath,
                               const char* dataPath,
                               const CharacterCustomization& customization,
                               rw::World* world, CharacterScene* scene);
bool applyCharacterCostumeVariant(
    const CharacterCustomization& customization, CharacterScene* scene);

bool loadCharacterShadowScene(const char* archivePath,
                              CharacterScene* scene);
bool copyCharacterPoseToShadowScene(const CharacterScene* source,
                                    CharacterScene* shadow);

bool loadCharacterScene(const char* archivePath, uint16_t resourceId,
                        rw::World* world, CharacterScene* scene);
bool loadCharacterAnimationBundle(const char* archivePath,
                                  const char* entryName,
                                  CharacterScene* scene);

bool loadCharacterAnimationBundleByIndex(const char* archivePath,
                                         uint32_t entryIndex,
                                         CharacterScene* scene);

bool loadCharacterAnimationResourceById(const char* archivePath,
                                        int resourceId, CharacterScene* scene);

bool loadCharacterAnimationClipByIndex(const char* archivePath,
                                       uint32_t entryIndex,
                                       size_t positionInBundle,
                                       CharacterScene* scene,
                                       size_t* animationIndex);

bool loadCharacterBasicAnimation(const char* archivePath,
                                 CharacterScene* scene,
                                 size_t* animationIndex = NULL);

void registerCharacterMorphPlugin(void);

bool applyCharacterSceneMorph(CharacterScene* scene, int attachmentType,
                              int startTarget, int endTarget, float amount);

bool applyCharacterSceneLip(CharacterScene* scene, int lipInterpolator,
                            float lipRate, float elapsedSeconds);

bool loadCharacterScheduledAnimation(const char* archivePath,
                                     int motionResourceSet, int motionId,
                                     CharacterScene* scene,
                                     size_t* animationIndex,
                                     bool* usedBasic = NULL);

bool loadCharacterSceneAnimation(const char* archivePath,
                                 int motionResourceSet, int motionId,
                                 CharacterScene* scene,
                                 size_t* animationIndex);
void setCharacterSceneTransform(CharacterScene* scene,
                                float x, float y, float z,
                                float headingDegrees, float tiltDegrees,
                                float heightOffset);
rw::Frame* characterSceneBoneFrame(const CharacterScene* scene, int boneIndex);

void setCharacterSceneRootY(CharacterScene* scene, float y);

void updateCharacterSceneAttachmentTransforms(CharacterScene* scene);

CharacterAnimationPlayback makeCharacterAnimationPlayback(
    float clipSeconds, int startFrame, int maxDurationFrames,
    int transitionFrames, float rate, bool loop,
    bool suppressTransition = false);

float advanceCharacterAnimationTransition(CharacterScene* scene,
                                          float animationDeltaSeconds);

void setCharacterSceneAnimation(CharacterScene* scene, size_t index);
void bindCharacterSceneAnimation(CharacterScene* scene, size_t index,
                                 uint32_t state, float elapsedSeconds,
                                 float stateDurationSeconds, bool loop);
void bindCharacterSceneAnimation(
    CharacterScene* scene, size_t index, uint32_t state,
    float elapsedSeconds, const CharacterAnimationPlayback& playback);

bool alignCharacterSceneAnimationRoot(CharacterScene* scene,
                                      float worldX, float worldZ);
rw::Texture* loadStxTexture(const char* archivePath, const char* entryName);

rw::Texture* createPalModeTexture(const PalModePackage& package, int imageId);
void tickCharacterScene(CharacterScene* scene, float deltaSeconds);
void freeCharacterScene(CharacterScene* scene);

#endif
