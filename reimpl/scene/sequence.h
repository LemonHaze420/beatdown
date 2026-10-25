#ifndef SCENE_SEQUENCE_H
#define SCENE_SEQUENCE_H

#include <stdint.h>
#include "move_tables.h"
#include "scene/sequence_tables.h"

const int SCENE_SEQUENCE_ACTOR_COUNT = 2;

const int SCENE_SEQUENCE_EVENT_SOUND_CHANNEL_COUNT = 3;

const int SCENE_SEQUENCE_SCRIPT_FLAG_COUNT = 8;

const int SCENE_SEQUENCE_CAPTION_LINE_COUNT = 4;

const int SCENE_SEQUENCE_STAGE_NAME_COUNT = 56;
extern const uint16_t
    SCENE_SEQUENCE_STAGE_NAME_TEXT_IDS[SCENE_SEQUENCE_STAGE_NAME_COUNT];

enum SceneSequencePromptState
{
    SCENE_SEQUENCE_PROMPT_IDLE,
    SCENE_SEQUENCE_PROMPT_OPEN,
    SCENE_SEQUENCE_PROMPT_ADVANCING,
    SCENE_SEQUENCE_PROMPT_CLOSED
};

enum SceneSequenceCaptionPhase
{
    SCENE_SEQUENCE_CAPTION_OFF,
    SCENE_SEQUENCE_CAPTION_FADE_IN,
    SCENE_SEQUENCE_CAPTION_HOLD,
    SCENE_SEQUENCE_CAPTION_FADE_OUT,
    SCENE_SEQUENCE_CAPTION_CLEANUP
};

struct SceneSequenceCaptionLine
{
    float x;
    float y;
    const char* text;
    uint32_t color;

    SceneSequenceCaptionLine();
};

struct SceneSequenceCaptionConfig
{
    float holdSeconds;
    int16_t loop;
    int16_t fadeStep;
    SceneSequenceCaptionLine lines[SCENE_SEQUENCE_CAPTION_LINE_COUNT];

    SceneSequenceCaptionConfig();
};

struct SceneSequenceCaption
{
    SceneSequenceCaptionPhase phase;
    int lineCount;
    int alpha;
    float heldSeconds;
    bool activeTask;
    bool shadow;

    SceneSequenceCaption();
};

struct SceneSequenceActor
{

    int16_t moveId;
    bool moveMapped;

    bool hasMotion;
    MoveMotion motion;

    float motionSeconds;
    int16_t loadStateFlags;
    uint32_t flags;
    uint32_t motionControlFlags;
    bool placed;
    float position[3];
    float headingDegrees;
    bool motionTaskActive;
    int16_t motionTask[5];

    SceneSequenceActor();
};

struct SceneSequenceLight
{
    bool configured;
    int index;
    bool active;
    float rotation[3];
    float color[3];

    SceneSequenceLight();
};

typedef bool SceneSequenceSoundActiveCallback(void* userData, int channelIndex);
typedef bool SceneSequenceTaskFinishedCallback(void* userData, int subOp);
typedef bool SceneSequencePromptFinishedCallback(void* userData);

typedef int SceneSequenceActorGenderCallback(void* userData, int actorSlot);

typedef bool SceneSequenceActorTransformCallback(void* userData, int actorSlot,
                                                 float* position,
                                                 float* headingDegrees);

typedef int SceneSequenceResolveMoveCallback(void* userData, int actorSlot,
                                             int moveId);

struct SceneSequenceCallbacks
{
    SceneSequenceSoundActiveCallback* eventSoundActive;
    SceneSequenceTaskFinishedCallback* sceneTaskFinished;
    SceneSequencePromptFinishedCallback* promptFinished;
    SceneSequenceActorGenderCallback* actorGender;
    SceneSequenceActorTransformCallback* actorTransform;
    SceneSequenceResolveMoveCallback* resolveMove;
    void* userData;

    SceneSequenceCallbacks();
};

const int SCENE_OVERLAY_NONE = -1;
const int SCENE_OVERLAY_LETTERBOX = 0;
const int SCENE_OVERLAY_FADE = 1;

class SceneSequencer
{
public:
    SceneSequencer();

    void setCallbacks(const SceneSequenceCallbacks& callbacks);
    void setCaptionConfig(const SceneSequenceCaptionConfig& config);

    void setCaptionShadowEnabled(bool enabled);

    void setEscapeEnabled(bool enabled);

    void setActorSlots(int first, int second);

    void start(int sequenceId);
    void stop();
    void stopOverlayFade();
    void update(float sceneDeltaSeconds, float scaledCaptionDeltaSeconds);

    bool running() const;
    bool complete() const;
    bool available() const;
    int sequenceId() const;
    int instructionsExecuted() const;
    bool scriptFlag(int index) const;

    const SceneSequenceActor& actor(int index) const;
    int actorSlot(int index) const;
    const SceneSequenceCaption& caption() const;
    const SceneSequenceCaptionConfig& captionConfig() const;
    const SceneSequenceLight& light() const;

    int slowMotion() const;
    int letterboxState() const;

    int overlayKind() const;
    int overlayFadeState() const;
    bool overlayActive() const;
    bool overlayFadeStopping() const;
    int hudMode() const;
    int hudPrideSide() const;
    bool hudVisible() const;
    int characterSelectState() const;
    int eventMode() const;
    int eventValue() const;

    uint32_t controlFlagBits() const;

    bool extraActorFlag() const;
    bool manualCamera() const;
    bool relationshipStatesReset() const;
    bool eventFlowStarted() const;
    SceneSequencePromptState promptState() const;
    int sceneTaskSubOp() const;
    int sceneTaskArchive() const;
    int sceneTaskEntry() const;
    bool sceneTaskActive() const;
    int waitingEventSound() const;
    const int* lastSpatialAudio() const;
    const int* lastSceneEffect() const;
    int clearedSceneEffect() const;

private:
    bool step();
    void updateCaption(float scaledDeltaSeconds);
    void startCaption();
    void queueCaptionCleanup();
    void stopCaption();
    int16_t word(int offset) const;

    SceneSequenceCallbacks callbacks_;
    SceneSequenceCaptionConfig captionConfig_;
    SceneSequenceCaption caption_;
    SceneSequenceActor actors_[SCENE_SEQUENCE_ACTOR_COUNT];
    SceneSequenceLight light_;

    int sequenceId_;
    int programFirst_;
    int programCount_;
    int pc_;
    float waitSeconds_;
    float sceneDeltaSeconds_;
    bool skipWaits_;
    bool complete_;
    bool available_;
    bool started_;
    bool escapeEnabled_;
    int actorSlots_[SCENE_SEQUENCE_ACTOR_COUNT];
    uint8_t scriptFlags_[SCENE_SEQUENCE_SCRIPT_FLAG_COUNT];
    int instructionsExecuted_;

    int slowMotion_;
    int letterboxState_;
    int overlayKind_;
    int overlayFadeState_;
    bool overlayActive_;
    int overlayFadeOutFrames_;
    bool overlayFadeStopping_;
    int hudMode_;
    int hudPrideSide_;
    bool hudVisible_;
    int characterSelectState_;
    int eventMode_;
    int eventValue_;
    bool extraActorFlag_;
    uint32_t controlFlagBits_;
    bool manualCamera_;
    bool relationshipStatesReset_;
    bool eventFlowStarted_;
    SceneSequencePromptState promptState_;
    int sceneTaskSubOp_;
    int sceneTaskArchive_;
    int sceneTaskEntry_;
    bool sceneTaskActive_;
    int waitingEventSoundChannel_;
    int lastSpatialAudio_[3];
    int lastSceneEffect_[3];
    int clearedSceneEffect_;
};

#endif
