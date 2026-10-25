#include "scene_sequence.h"

#include <math.h>
#include <string.h>

const int SCENE_TASK_MAIN_CHARACTER = SCENE_SEQUENCE_WAIT_FRAMES;
const int SCENE_TASK_EVENT_SCRIPT = SCENE_SEQUENCE_SET_ACTOR_ACTION;
const int SCENE_TASK_BATTLE_FLOW = SCENE_SEQUENCE_SET_CAMERA_MODE;

const int16_t SCENE_MOVE_GENDER_SOURCE = 0x249;
const int16_t SCENE_MOVE_GENDER_TARGET = 0x25a;

const int SCENE_PROMPT_FINISHED_STATE = 6;

const int SCENE_CAPTION_ALPHA_MAX = 0xff;

const uint16_t
SCENE_SEQUENCE_STAGE_NAME_TEXT_IDS[SCENE_SEQUENCE_STAGE_NAME_COUNT] =
{
    0, 18001, 18001, 18001, 18052, 18002, 18019, 18004,
    18009, 18005, 18008, 18004, 18010, 18007, 18013, 18011,
    18012, 18014, 18015, 18016, 18020, 18021, 18022, 18023,
    0, 18003, 18024, 0, 0, 18027, 18028, 18033,
    18032, 18034, 18031, 18030, 0, 18029, 18036, 18037,
    18038, 18050, 18039, 18040, 18041, 18042, 18017, 18044,
    18045, 18046, 18047, 18048, 18049, 18050, 18051, 0
};

const uint32_t SCENE_SEQUENCE_CONTROL_FLAG_BASE = 0x10000u;

static uint32_t
sceneSequenceControlFlagBit(int selector)
{
    switch(selector)
    {
    case SCENE_SEQUENCE_WAIT_FRAMES:
        return 0x20000u;
    case SCENE_SEQUENCE_SET_ACTOR_ACTION:
        return 0x40000u;
    case SCENE_SEQUENCE_SET_CAMERA_MODE:
        return 0x80000u;
    case SCENE_SEQUENCE_SET_ACTOR_FIELD_0X16:
        return 0x100000u;
    default:
        return SCENE_SEQUENCE_CONTROL_FLAG_BASE;
    }
}

int
sceneSequenceInstructionWords(const int16_t* words, int available)
{
    if(words == NULL || available <= 0)
        return 0;
    const int opcode = words[0] & 0xffff;
    switch(opcode)
    {
    case SCENE_SEQUENCE_END:
    case SCENE_SEQUENCE_START_CAPTION:
    case SCENE_SEQUENCE_STOP_CAPTION:
    case SCENE_SEQUENCE_STOP_OVERLAY:
    case SCENE_SEQUENCE_RESET_RELATIONSHIP_STATES:
    case SCENE_SEQUENCE_START_EVENT_FLOW:
    case SCENE_SEQUENCE_RESERVED_0C:
    case SCENE_SEQUENCE_START_PROMPT:
    case SCENE_SEQUENCE_ACCEPT_PROMPT:
    case SCENE_SEQUENCE_CANCEL_PROMPT:
    case SCENE_SEQUENCE_WAIT_PROMPT:
    case SCENE_SEQUENCE_PAUSE:
        return 1;
    case SCENE_SEQUENCE_WAIT_FRAMES:
    case SCENE_SEQUENCE_RESERVED_WITH_ARG:
    case SCENE_SEQUENCE_SET_BASE_CONTROL_FLAG:
    case SCENE_SEQUENCE_WAIT_SCENE_TASK:
    case SCENE_SEQUENCE_SET_CONTROL_FLAG_INDEX:
    case SCENE_SEQUENCE_CLEAR_CONTROL_FLAG_INDEX:
    case SCENE_SEQUENCE_WAIT_EVENT_SOUND:
    case SCENE_SEQUENCE_SET_EXTRA_ACTOR_FLAG:
    case SCENE_SEQUENCE_SET_SLOW_MOTION:
    case SCENE_SEQUENCE_CLEAR_SCENE_EFFECT:
    case SCENE_SEQUENCE_SET_SCRIPT_FLAG:
    case SCENE_SEQUENCE_SET_EVENT_VALUE:
    case SCENE_SEQUENCE_SET_CHARACTER_SELECT_STATE:
    case SCENE_SEQUENCE_SET_EVENT_MODE:
        return 2;
    case SCENE_SEQUENCE_SET_ACTOR_ACTION:
    case SCENE_SEQUENCE_SET_CAMERA_MODE:
    case SCENE_SEQUENCE_SET_ACTOR_FIELD_0X16:
    case SCENE_SEQUENCE_SET_OVERLAY_STATE:
    case SCENE_SEQUENCE_SET_BATTLE_HUD_MODE:
        return 3;
    case SCENE_SEQUENCE_SET_SCENE_CONTROL_STATE:
    case SCENE_SEQUENCE_PLAY_SPATIAL_AUDIO:
    case SCENE_SEQUENCE_SET_SCENE_EFFECT:
        return 4;
    case SCENE_SEQUENCE_START_ACTOR_MOTION_TASK:
    case SCENE_SEQUENCE_PLACE_ACTOR:
        return 7;
    case SCENE_SEQUENCE_START_SCENE_TASK:
        if(available < 2)
            return 2;
        return words[1] == SCENE_TASK_EVENT_SCRIPT ? 4 : 2;
    case SCENE_SEQUENCE_CONFIGURE_LIGHT:
        if(available < 3)
            return 3;
        return words[2] == 0 ? 3 : 9;
    default:

        return 1;
    }
}

SceneSequenceCaptionLine::SceneSequenceCaptionLine()
    : x(0.0f), y(0.0f), text(NULL), color(0xffffffffu)
{
}

SceneSequenceCaptionConfig::SceneSequenceCaptionConfig()
    : holdSeconds(0.0f), loop(0), fadeStep(0)
{
}

SceneSequenceCaption::SceneSequenceCaption()
    : phase(SCENE_SEQUENCE_CAPTION_OFF), lineCount(0), alpha(0),
      heldSeconds(0.0f), activeTask(false), shadow(true)
{
}

SceneSequenceActor::SceneSequenceActor()
    : moveId(-1), moveMapped(false), hasMotion(false), motionSeconds(-1.0f),
      loadStateFlags(0), flags(0),
      motionControlFlags(0), placed(false), headingDegrees(0.0f),
      motionTaskActive(false)
{
    position[0] = 0.0f;
    position[1] = 0.0f;
    position[2] = 0.0f;
    memset(motionTask, 0, sizeof(motionTask));
}

SceneSequenceLight::SceneSequenceLight()
    : configured(false), index(0), active(false)
{
    for(int i = 0; i < 3; ++i)
    {
        rotation[i] = 0.0f;
        color[i] = 0.0f;
    }
}

SceneSequenceCallbacks::SceneSequenceCallbacks()
    : eventSoundActive(NULL), sceneTaskFinished(NULL), promptFinished(NULL),
      actorGender(NULL), actorTransform(NULL), resolveMove(NULL),
      userData(NULL)
{
}

SceneSequencer::SceneSequencer()
    : sequenceId_(0), programFirst_(0), programCount_(0), pc_(0),
      waitSeconds_(0.0f), sceneDeltaSeconds_(0.0f), skipWaits_(false),
      complete_(true), available_(false), started_(false),
      escapeEnabled_(false),
      instructionsExecuted_(0), slowMotion_(0), letterboxState_(0),
      overlayKind_(SCENE_OVERLAY_NONE),
      overlayFadeState_(0), overlayActive_(false),
      overlayFadeOutFrames_(0), overlayFadeStopping_(false), hudMode_(-1),
      hudPrideSide_(-1), hudVisible_(false), characterSelectState_(-1),
      eventMode_(-1), eventValue_(-1), extraActorFlag_(false),
      controlFlagBits_(0), manualCamera_(false),
      relationshipStatesReset_(false), eventFlowStarted_(false),
      promptState_(SCENE_SEQUENCE_PROMPT_IDLE), sceneTaskSubOp_(-1),
      sceneTaskArchive_(-1), sceneTaskEntry_(-1), sceneTaskActive_(false),
      waitingEventSoundChannel_(-1),
      clearedSceneEffect_(-1)
{
    actorSlots_[0] = 0;
    actorSlots_[1] = 1;
    memset(scriptFlags_, 0, sizeof(scriptFlags_));
    for(int i = 0; i < 3; ++i)
    {
        lastSpatialAudio_[i] = -1;
        lastSceneEffect_[i] = -1;
    }
}

void
SceneSequencer::setCallbacks(const SceneSequenceCallbacks& callbacks)
{
    callbacks_ = callbacks;
}

void
SceneSequencer::setCaptionConfig(const SceneSequenceCaptionConfig& config)
{
    captionConfig_ = config;
}

void
SceneSequencer::setCaptionShadowEnabled(bool enabled)
{
    caption_.shadow = enabled;
}

void
SceneSequencer::setEscapeEnabled(bool enabled)
{
    escapeEnabled_ = enabled;
}

void
SceneSequencer::setActorSlots(int first, int second)
{
    actorSlots_[0] = first;
    actorSlots_[1] = second;
}

void
SceneSequencer::start(int sequenceId)
{

    memset(scriptFlags_, 0, sizeof(scriptFlags_));
    available_ = true;
    sequenceId_ = sequenceId;
    instructionsExecuted_ = 0;
    waitSeconds_ = 0.0f;
    skipWaits_ = false;
    stopCaption();

    promptState_ = SCENE_SEQUENCE_PROMPT_IDLE;
    waitingEventSoundChannel_ = -1;
    if(sequenceId <= 0 || sequenceId >= SCENE_SEQUENCE_COUNT)
    {
        complete_ = true;
        started_ = false;
        return;
    }
    const SceneSequenceProgram& program = SCENE_SEQUENCE_PROGRAMS[sequenceId];
    programFirst_ = program.firstWord;
    programCount_ = program.wordCount;
    pc_ = programFirst_;
    complete_ = programCount_ <= 0;
    started_ = !complete_;
}

void
SceneSequencer::stop()
{

    queueCaptionCleanup();
    complete_ = true;
    started_ = false;
}

void
SceneSequencer::stopOverlayFade()
{
    if(!overlayActive_ || overlayKind_ != SCENE_OVERLAY_FADE)
        return;

    overlayFadeOutFrames_ = 0;
    overlayFadeStopping_ = true;
}

void
SceneSequencer::update(float sceneDeltaSeconds, float scaledCaptionDeltaSeconds)
{
    sceneDeltaSeconds_ = sceneDeltaSeconds;
    updateCaption(scaledCaptionDeltaSeconds);
    if(overlayFadeStopping_ && ++overlayFadeOutFrames_ >= 32)
    {
        overlayActive_ = false;
        overlayFadeStopping_ = false;
        overlayKind_ = SCENE_OVERLAY_NONE;
    }
    if(!started_ || complete_)
        return;
    while(!step())
        ;
}

int16_t
SceneSequencer::word(int offset) const
{
    const int index = pc_ + offset;
    if(index < programFirst_ || index >= programFirst_ + programCount_)
        return 0;
    return SCENE_SEQUENCE_WORDS[index];
}

bool
SceneSequencer::step()
{
    if(waitSeconds_ != 0.0f)
    {
        waitSeconds_ -= sceneDeltaSeconds_;
        if(waitSeconds_ >= 0.0f)
            return true;
        waitSeconds_ = 0.0f;
        return false;
    }
    if(pc_ < programFirst_ || pc_ >= programFirst_ + programCount_)
    {
        complete_ = true;
        return true;
    }

    const int available = programFirst_ + programCount_ - pc_;
    const int opcode = word(0) & 0xffff;
    const int words = sceneSequenceInstructionWords(
        &SCENE_SEQUENCE_WORDS[pc_], available);
    const int next = pc_ + words;
    ++instructionsExecuted_;

    switch(opcode)
    {
    case SCENE_SEQUENCE_END:

        if(!caption_.activeTask)
        {
            complete_ = true;
            skipWaits_ = false;
        }
        return true;
    case SCENE_SEQUENCE_WAIT_FRAMES:
        if(!skipWaits_)
            waitSeconds_ = static_cast<float>(word(1)) / 60.0f;
        break;
    case SCENE_SEQUENCE_SET_ACTOR_ACTION:
    {
        const int index = word(1);
        int16_t move = word(2);
        if(index >= 0 && index < SCENE_SEQUENCE_ACTOR_COUNT)
        {
            SceneSequenceActor& target = actors_[index];
            if(move == SCENE_MOVE_GENDER_SOURCE && callbacks_.actorGender !=
                   NULL &&
               callbacks_.actorGender(callbacks_.userData,
                                      actorSlots_[index]) == 1)
                move = SCENE_MOVE_GENDER_TARGET;

            target.moveMapped = move < 0;
            if(move < 0 && callbacks_.resolveMove != NULL)
            {
                const int resolved = callbacks_.resolveMove(
                    callbacks_.userData, actorSlots_[index], move);
                if(resolved >= 0)
                    move = static_cast<int16_t>(resolved);
            }
            target.moveId = move;

            target.hasMotion = moveMotions(move, &target.motion, 1) == 1;
            target.motionSeconds =
                target.hasMotion && target.motion.maxDurationFrames != -1 ?
                    static_cast<float>(target.motion.maxDurationFrames) /
                        60.0f :
                    -1.0f;
        }
        break;
    }
    case SCENE_SEQUENCE_SET_CAMERA_MODE:

        break;
    case SCENE_SEQUENCE_SET_ACTOR_FIELD_0X16:
    {
        const int index = word(1);
        if(index >= 0 && index < SCENE_SEQUENCE_ACTOR_COUNT)
            actors_[index].loadStateFlags = word(2);
        break;
    }
    case SCENE_SEQUENCE_START_CAPTION:
        startCaption();
        break;
    case SCENE_SEQUENCE_STOP_CAPTION:
        stopCaption();
        break;
    case SCENE_SEQUENCE_SET_OVERLAY_STATE:

        overlayKind_ = word(1) == 0 ? SCENE_OVERLAY_LETTERBOX :
                                      SCENE_OVERLAY_FADE;
        if(word(1) == 0)
            letterboxState_ = word(2);
        else
            overlayFadeState_ = word(2);
        overlayActive_ = true;
        overlayFadeOutFrames_ = 0;
        overlayFadeStopping_ = false;
        break;
    case SCENE_SEQUENCE_STOP_OVERLAY:
        overlayActive_ = false;
        overlayFadeOutFrames_ = 0;
        overlayFadeStopping_ = false;
        break;
    case SCENE_SEQUENCE_RESERVED_WITH_ARG:
    case SCENE_SEQUENCE_RESERVED_0C:

        break;
    case SCENE_SEQUENCE_RESET_RELATIONSHIP_STATES:
        relationshipStatesReset_ = true;
        break;
    case SCENE_SEQUENCE_START_EVENT_FLOW:
        eventFlowStarted_ = true;
        break;
    case SCENE_SEQUENCE_START_PROMPT:
        if(!escapeEnabled_)
            promptState_ = SCENE_SEQUENCE_PROMPT_OPEN;
        break;
    case SCENE_SEQUENCE_ACCEPT_PROMPT:
        if(!escapeEnabled_)
            promptState_ = SCENE_SEQUENCE_PROMPT_ADVANCING;
        break;
    case SCENE_SEQUENCE_CANCEL_PROMPT:
        if(!escapeEnabled_)
            promptState_ = SCENE_SEQUENCE_PROMPT_CLOSED;
        break;
    case SCENE_SEQUENCE_WAIT_PROMPT:
    {
        if(escapeEnabled_)
            break;
        const bool finished = callbacks_.promptFinished == NULL ||
            callbacks_.promptFinished(callbacks_.userData);
        if(!finished)
            return true;
        promptState_ = SCENE_SEQUENCE_PROMPT_CLOSED;
        break;
    }
    case SCENE_SEQUENCE_SET_BASE_CONTROL_FLAG:

        controlFlagBits_ = word(1) == 0 ?
            controlFlagBits_ & ~SCENE_SEQUENCE_CONTROL_FLAG_BASE :
            controlFlagBits_ | SCENE_SEQUENCE_CONTROL_FLAG_BASE;
        break;
    case SCENE_SEQUENCE_START_SCENE_TASK:
        waitSeconds_ = 0.0f;
        sceneTaskSubOp_ = word(1);
        sceneTaskActive_ = true;
        if(sceneTaskSubOp_ == SCENE_TASK_EVENT_SCRIPT)
        {
            sceneTaskArchive_ = word(2);
            sceneTaskEntry_ = word(3);
        }
        break;
    case SCENE_SEQUENCE_WAIT_SCENE_TASK:
    {
        waitSeconds_ = 0.0f;
        const int subOp = word(1);
        const bool finished = callbacks_.sceneTaskFinished == NULL ||
            callbacks_.sceneTaskFinished(callbacks_.userData, subOp);
        if(!finished)
        {

            waitSeconds_ = 1.0f;
            return false;
        }
        sceneTaskActive_ = false;
        sceneTaskSubOp_ = -1;
        break;
    }
    case SCENE_SEQUENCE_SET_SCENE_CONTROL_STATE:
    {
        const int index = word(1);
        const int selector = word(2);
        const int value = word(3);
        if(selector == SCENE_SEQUENCE_WAIT_FRAMES)
        {
            manualCamera_ = value == 1;
            break;
        }
        if(index < 0 || index >= SCENE_SEQUENCE_ACTOR_COUNT)
            break;
        SceneSequenceActor& target = actors_[index];
        if(selector == SCENE_SEQUENCE_SET_ACTOR_FIELD_0X16)
        {

            target.flags |= value == 1 ? 1u : 0xfffffffeu;
        }
        else if(selector == SCENE_SEQUENCE_SET_CAMERA_MODE)
        {
            target.motionControlFlags = value == 1 ?
                target.motionControlFlags & ~2u :
                (target.motionControlFlags & ~2u) | 2u;
        }
        else if(selector == SCENE_SEQUENCE_SET_ACTOR_ACTION)
        {
            target.motionControlFlags = value == 1 ?
                target.motionControlFlags & ~1u :
                (target.motionControlFlags & ~1u) | 1u;
        }
        break;
    }
    case SCENE_SEQUENCE_START_ACTOR_MOTION_TASK:
    {
        const int index = word(1);
        if(index >= 0 && index < SCENE_SEQUENCE_ACTOR_COUNT)
        {
            SceneSequenceActor& target = actors_[index];
            target.motionTaskActive = true;
            for(int i = 0; i < 5; ++i)
                target.motionTask[i] = word(2 + i);
        }
        break;
    }
    case SCENE_SEQUENCE_PLACE_ACTOR:
    {
        const int index = word(1);
        const int reference = word(2);
        float position[3];
        position[0] = static_cast<float>(word(3)) / 10.0f;
        position[1] = static_cast<float>(word(4)) / 10.0f;
        position[2] = static_cast<float>(word(5)) / 10.0f;
        float heading = static_cast<float>(word(6)) / 10.0f;
        if(reference >= 0 && reference < SCENE_SEQUENCE_ACTOR_COUNT &&
           callbacks_.actorTransform != NULL)
        {
            float origin[3] = { 0.0f, 0.0f, 0.0f };
            float originHeading = 0.0f;
            if(callbacks_.actorTransform(callbacks_.userData,
                                         actorSlots_[reference], origin,
                                         &originHeading))
            {

                const float radians = originHeading * 3.14159265358979f /
                                      180.0f;
                const float sine = sinf(radians);
                const float cosine = cosf(radians);
                const float x = position[0] * cosine + position[2] * sine;
                const float z = position[2] * cosine - position[0] * sine;
                position[0] = origin[0] + x;
                position[1] = origin[1] + position[1];
                position[2] = origin[2] + z;
                heading += originHeading;
                if(heading < -180.0f)
                    heading += 360.0f;
                else if(heading > 180.0f)
                    heading -= 360.0f;
            }
        }
        if(index >= 0 && index < SCENE_SEQUENCE_ACTOR_COUNT)
        {
            SceneSequenceActor& target = actors_[index];
            target.placed = true;
            target.position[0] = position[0];
            target.position[1] = position[1];
            target.position[2] = position[2];
            target.headingDegrees = heading;
        }
        break;
    }
    case SCENE_SEQUENCE_SET_CONTROL_FLAG_INDEX:
    case SCENE_SEQUENCE_CLEAR_CONTROL_FLAG_INDEX:
    {
        const bool set = opcode == SCENE_SEQUENCE_SET_CONTROL_FLAG_INDEX;
        controlFlagBits_ = set ?
            controlFlagBits_ | sceneSequenceControlFlagBit(word(1)) :
            controlFlagBits_ & ~sceneSequenceControlFlagBit(word(1));
        break;
    }
    case SCENE_SEQUENCE_WAIT_EVENT_SOUND:
    {
        const int channelIndex = word(1);

        const bool active = callbacks_.eventSoundActive != NULL &&
            callbacks_.eventSoundActive(callbacks_.userData, channelIndex);
        if(active)
        {
            waitingEventSoundChannel_ = channelIndex;
            return true;
        }
        waitingEventSoundChannel_ = -1;
        break;
    }
    case SCENE_SEQUENCE_SET_EXTRA_ACTOR_FLAG:

        extraActorFlag_ = word(1) != 0;
        break;
    case SCENE_SEQUENCE_SET_SLOW_MOTION:
        slowMotion_ = word(1);
        break;
    case SCENE_SEQUENCE_PLAY_SPATIAL_AUDIO:
        for(int i = 0; i < 3; ++i)
            lastSpatialAudio_[i] = word(1 + i);
        break;
    case SCENE_SEQUENCE_SET_SCENE_EFFECT:
        for(int i = 0; i < 3; ++i)
            lastSceneEffect_[i] = word(1 + i);
        break;
    case SCENE_SEQUENCE_CLEAR_SCENE_EFFECT:
        clearedSceneEffect_ = word(1);
        break;
    case SCENE_SEQUENCE_CONFIGURE_LIGHT:
        light_.configured = true;
        light_.index = word(1);
        if(word(2) == 0)
        {
            light_.active = true;
        }
        else
        {
            light_.active = false;
            for(int i = 0; i < 3; ++i)
            {
                light_.rotation[i] = static_cast<float>(word(3 + i)) * 0.01f;
                light_.color[i] = static_cast<float>(word(6 + i)) * 0.01f;
            }
        }
        break;
    case SCENE_SEQUENCE_PAUSE:

        waitSeconds_ = 1.0f;
        return true;
    case SCENE_SEQUENCE_SET_SCRIPT_FLAG:
    {
        const int index = word(1);
        if(index >= 0 && index < SCENE_SEQUENCE_SCRIPT_FLAG_COUNT)
            scriptFlags_[index] = 1;
        break;
    }
    case SCENE_SEQUENCE_SET_EVENT_VALUE:
        eventValue_ = word(1);
        break;
    case SCENE_SEQUENCE_SET_BATTLE_HUD_MODE:
        if(word(1) == 0)
        {
            hudVisible_ = false;
            hudMode_ = 0;
        }
        else if(word(1) == 1)
        {
            hudVisible_ = true;
            hudMode_ = 1;
            hudPrideSide_ = word(2) == 0 ? 1 : 0;
        }
        break;
    case SCENE_SEQUENCE_SET_CHARACTER_SELECT_STATE:
        characterSelectState_ = word(1);
        break;
    case SCENE_SEQUENCE_SET_EVENT_MODE:
        eventMode_ = word(1);
        break;
    default:
        break;
    }

    pc_ = next;
    return false;
}

void
SceneSequencer::startCaption()
{
    caption_.activeTask = true;
    caption_.lineCount = 0;
    for(int i = 0; i < SCENE_SEQUENCE_CAPTION_LINE_COUNT; ++i)
    {
        if(captionConfig_.lines[i].text == NULL)
            break;
        ++caption_.lineCount;
    }
    if(caption_.lineCount == 0)
    {

        stopCaption();
        return;
    }
    caption_.phase = SCENE_SEQUENCE_CAPTION_FADE_IN;
    caption_.alpha = 0;
    caption_.heldSeconds = 0.0f;
}

void
SceneSequencer::queueCaptionCleanup()
{
    if(caption_.activeTask)
        caption_.phase = SCENE_SEQUENCE_CAPTION_CLEANUP;
}

void
SceneSequencer::stopCaption()
{
    caption_.phase = SCENE_SEQUENCE_CAPTION_OFF;
    caption_.lineCount = 0;
    caption_.alpha = 0;
    caption_.heldSeconds = 0.0f;
    caption_.activeTask = false;
}

void
SceneSequencer::updateCaption(float scaledDeltaSeconds)
{

    switch(caption_.phase)
    {
    case SCENE_SEQUENCE_CAPTION_OFF:
        return;
    case SCENE_SEQUENCE_CAPTION_FADE_IN:
        caption_.alpha += captionConfig_.fadeStep;
        if(caption_.alpha > SCENE_CAPTION_ALPHA_MAX)
        {
            caption_.alpha = SCENE_CAPTION_ALPHA_MAX;
            caption_.phase = SCENE_SEQUENCE_CAPTION_HOLD;
        }
        return;
    case SCENE_SEQUENCE_CAPTION_HOLD:
        caption_.heldSeconds += scaledDeltaSeconds;
        if(caption_.heldSeconds >= captionConfig_.holdSeconds)
            caption_.phase = SCENE_SEQUENCE_CAPTION_FADE_OUT;
        return;
    case SCENE_SEQUENCE_CAPTION_FADE_OUT:
        caption_.alpha -= captionConfig_.fadeStep;
        if(caption_.alpha >= 0)
            return;
        caption_.alpha = 0;
        caption_.heldSeconds = 0.0f;
        if(captionConfig_.loop == 0)
            caption_.phase = SCENE_SEQUENCE_CAPTION_CLEANUP;
        else
            caption_.phase = SCENE_SEQUENCE_CAPTION_FADE_IN;
        return;
    case SCENE_SEQUENCE_CAPTION_CLEANUP:
        stopCaption();
        return;
    }
}

bool SceneSequencer::running() const { return started_ && !complete_; }
bool SceneSequencer::complete() const { return complete_; }
bool SceneSequencer::available() const { return available_; }
int SceneSequencer::sequenceId() const { return sequenceId_; }
int SceneSequencer::instructionsExecuted() const
{
    return instructionsExecuted_;
}

bool
SceneSequencer::scriptFlag(int index) const
{
    return index >= 0 && index < SCENE_SEQUENCE_SCRIPT_FLAG_COUNT &&
           scriptFlags_[index] != 0;
}

const SceneSequenceActor&
SceneSequencer::actor(int index) const
{
    return actors_[index < 0 || index >= SCENE_SEQUENCE_ACTOR_COUNT ? 0 :
                   index];
}

int
SceneSequencer::actorSlot(int index) const
{
    return actorSlots_[index < 0 || index >= SCENE_SEQUENCE_ACTOR_COUNT ? 0 :
                       index];
}

const SceneSequenceCaption& SceneSequencer::caption() const
{
    return caption_;
}
const SceneSequenceCaptionConfig& SceneSequencer::captionConfig() const
{
    return captionConfig_;
}
const SceneSequenceLight& SceneSequencer::light() const { return light_; }

int SceneSequencer::slowMotion() const { return slowMotion_; }
int SceneSequencer::letterboxState() const { return letterboxState_; }
int SceneSequencer::overlayKind() const { return overlayKind_; }
int SceneSequencer::overlayFadeState() const { return overlayFadeState_; }
bool SceneSequencer::overlayActive() const { return overlayActive_; }
bool SceneSequencer::overlayFadeStopping() const
{
    return overlayFadeStopping_;
}
int SceneSequencer::hudMode() const { return hudMode_; }
int SceneSequencer::hudPrideSide() const { return hudPrideSide_; }
bool SceneSequencer::hudVisible() const { return hudVisible_; }
int SceneSequencer::characterSelectState() const
{
    return characterSelectState_;
}
int SceneSequencer::eventMode() const { return eventMode_; }
int SceneSequencer::eventValue() const { return eventValue_; }
bool SceneSequencer::extraActorFlag() const { return extraActorFlag_; }
uint32_t SceneSequencer::controlFlagBits() const { return controlFlagBits_; }
bool SceneSequencer::manualCamera() const { return manualCamera_; }
bool SceneSequencer::relationshipStatesReset() const
{
    return relationshipStatesReset_;
}
bool SceneSequencer::eventFlowStarted() const { return eventFlowStarted_; }
SceneSequencePromptState SceneSequencer::promptState() const
{
    return promptState_;
}
int SceneSequencer::sceneTaskSubOp() const { return sceneTaskSubOp_; }
int SceneSequencer::sceneTaskArchive() const { return sceneTaskArchive_; }
int SceneSequencer::sceneTaskEntry() const { return sceneTaskEntry_; }
bool SceneSequencer::sceneTaskActive() const { return sceneTaskActive_; }
int SceneSequencer::waitingEventSound() const
{
    return waitingEventSoundChannel_;
}
const int* SceneSequencer::lastSpatialAudio() const
{
    return lastSpatialAudio_;
}
const int* SceneSequencer::lastSceneEffect() const { return lastSceneEffect_; }
int SceneSequencer::clearedSceneEffect() const { return clearedSceneEffect_; }
