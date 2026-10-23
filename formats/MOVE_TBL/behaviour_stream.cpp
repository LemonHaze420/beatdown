#include "behaviour_stream.h"
#include <stddef.h>
BehaviourStreamCallbacks::BehaviourStreamCallbacks()
: setHitBlock(NULL)
, armHitWindow(NULL)
, applyHit(NULL)
, queueMove(NULL)
, setAction(NULL)
, actionFlags(NULL)
, targetFlags(NULL)
, cueFlags(NULL)
, statusFlags(NULL)
, behaviourFlags(NULL)
, impactState(NULL)
, recoveryState(NULL)
, movement(NULL)
, contactMovement(NULL)
, turnDuration(NULL)
, spacingCorrection(NULL)
, inputHold(NULL)
, comboGate(NULL)
, testTargetFlags(NULL)
, effect(NULL)
, effectList(NULL)
, runAction(NULL)
, fireProjectile(NULL)
, weapon(NULL)
, sound(NULL)
, cameraSettle(NULL)
, camera(NULL)
, timeScale(NULL)
, userData(NULL)
{
}
BehaviourStreamRunner::BehaviourStreamRunner()
: moveId_(-1)
, streamFirst_(0)
, streamCount_(0)
, cursor_(0)
, loopReturn_(0)
, loopCounter_(0)
, wait_(0.0f)
, animationTimeScale_(1.0f)
, effectSuppressionMask_(0)
, running_(false)
, parked_(false)
, waitInSeconds_(false)
, instructionsExecuted_(0)
{
}
void
BehaviourStreamRunner::setCallbacks(const BehaviourStreamCallbacks& callbacks)
{
    callbacks_ = callbacks;
}
void
BehaviourStreamRunner::start(int moveId)
{
    moveId_ = moveId;
    streamFirst_ = 0;
    streamCount_ = 0;
    if(moveId > 0 && moveId < MOVE_RECORD_COUNT)
    {
        const MoveStream& stream = MOVE_BEHAVIOURS[moveId];
        streamFirst_ = stream.firstWord;
        streamCount_ = stream.wordCount;
    }
    cursor_ = 0;
    loopReturn_ = 0;
    loopCounter_ = 0;
    wait_ = 0.0f;
    animationTimeScale_ = 1.0f;
    parked_ = false;
    instructionsExecuted_ = 0;
    running_ = streamCount_ > 0;
}
void
BehaviourStreamRunner::stop()
{
    running_ = false;
    parked_ = false;
    wait_ = 0.0f;
}
void
BehaviourStreamRunner::setWaitInSeconds(bool seconds)
{
    waitInSeconds_ = seconds;
}
void
BehaviourStreamRunner::setEffectSuppressionMask(uint32_t mask)
{
    effectSuppressionMask_ = mask;
}
uint32_t
BehaviourStreamRunner::effectSuppressionMask() const
{
    return effectSuppressionMask_;
}
const int16_t*
BehaviourStreamRunner::wordsAt(int offset) const
{
    return &MOVE_BEHAVIOUR_WORDS[streamFirst_ + offset];
}
int
BehaviourStreamRunner::available(int offset) const
{
    return streamCount_ - offset;
}
void
BehaviourStreamRunner::update(float deltaSeconds, float animationFrame)
{
    if(!running_)
        return;

    if(wait_ > 0.0f)
    {
        if(waitInSeconds_)
        {
            wait_ -= deltaSeconds;
            if(wait_ > 0.0f)
                return;
        }
        else if(animationFrame < wait_)
        {
            return;
        }
        wait_ = 0.0f;
    }
    for(;;)
    {
        if(cursor_ < 0 || cursor_ >= streamCount_)
        {
            running_ = false;
            return;
        }
        const int16_t* words = wordsAt(cursor_);
        const int count = moveBehaviourInstructionWords(
            words, available(cursor_));
        if(count <= 0 || cursor_ + count > streamCount_)
        {
            running_ = false;
            return;
        }
        const int opcode = words[0] & 0xffff;
        int next = cursor_ + count;
        bool yield = false;
        ++instructionsExecuted_;
        switch(opcode)
        {
        case MOVE_BEHAVIOUR_PARK:

            next = cursor_;
            parked_ = true;
            yield = true;
            break;
        case MOVE_BEHAVIOUR_LOOP_BEGIN:
            loopReturn_ = cursor_ + count;

            loopCounter_ = words[1] - 1;
            break;
        case MOVE_BEHAVIOUR_LOOP_END:
            if(loopCounter_ != 0)
            {
                if(loopCounter_ > 0)
                    --loopCounter_;
                next = loopReturn_;
            }
            break;
        case MOVE_BEHAVIOUR_SET_TIME_SCALE:
            animationTimeScale_ = words[1] * 0.001f;
            if(callbacks_.timeScale != NULL)
                callbacks_.timeScale(callbacks_.userData, animationTimeScale_);
            break;
        case MOVE_BEHAVIOUR_WAIT_FRAMES:
            wait_ = static_cast<float>(words[1]);
            yield = true;
            break;
        case MOVE_BEHAVIOUR_WAIT_MS:
            wait_ = words[1] * 0.001f;
            yield = true;
            break;
        case MOVE_BEHAVIOUR_WAIT_UNITS:
            if(words[1] == 0)
                waitInSeconds_ = false;
            else if(words[1] == 1)
                waitInSeconds_ = true;
            break;
        case MOVE_BEHAVIOUR_PLAY_SOUND:

            if(callbacks_.sound != NULL)
                callbacks_.sound(callbacks_.userData, words[1],
                                 words[2], words[3]);
            break;
        case MOVE_BEHAVIOUR_EFFECT:
        {

            const uint32_t bit = uint32_t(1) <<
                (static_cast<uint16_t>(words[1]) & 31u);
            if((effectSuppressionMask_ & bit) == 0 &&
               callbacks_.effect != NULL)
            {
                callbacks_.effect(callbacks_.userData, words[1], words[2],
                                  words[3], words[4], words[5]);
            }
            break;
        }
        case MOVE_BEHAVIOUR_ARM_HIT_WINDOW:
        {
            uint32_t channelMask = 0;
            for(int index = 4; index + 1 < count; ++index)
            {
                const int channel = words[index];
                if(channel >= 0 && channel < MOVE_HIT_CHANNEL_COUNT)
                    channelMask |= uint32_t(1) << channel;
            }
            if(callbacks_.armHitWindow != NULL)
            {
                callbacks_.armHitWindow(callbacks_.userData, words[1],
                                        words[2], words[3], channelMask);
            }
            break;
        }
        case MOVE_BEHAVIOUR_SET_HIT_BLOCK:
            if(callbacks_.setHitBlock != NULL)
                callbacks_.setHitBlock(callbacks_.userData, words[1] & 0xff);
            break;
        case MOVE_BEHAVIOUR_APPLY_HIT:
            if(callbacks_.applyHit != NULL)
                callbacks_.applyHit(callbacks_.userData, words[1]);
            break;
        case MOVE_BEHAVIOUR_QUEUE_MOVE:
            if(callbacks_.queueMove != NULL)
                callbacks_.queueMove(callbacks_.userData, words[2]);
            yield = true;
            break;
        case MOVE_BEHAVIOUR_QUEUE_IF_FLAG:
        {

            const uint32_t mask =
                (static_cast<uint32_t>(words[1] & 0xffff) << 16) |
                static_cast<uint32_t>(words[2] & 0xffff);
            const bool expectSet = words[3] != 0;
            bool matched = false;
            if(callbacks_.testTargetFlags != NULL)
                matched = callbacks_.testTargetFlags(callbacks_.userData, mask,
                                                     expectSet);
            if(matched)
            {
                if(callbacks_.targetFlags != NULL)
                    callbacks_.targetFlags(callbacks_.userData, mask, false);
                if(callbacks_.queueMove != NULL)
                    callbacks_.queueMove(callbacks_.userData, words[4]);
                yield = true;
            }
            break;
        }
        case MOVE_BEHAVIOUR_ACTION_FLAGS:
        {
            const uint32_t mask =
                (static_cast<uint32_t>(words[2] & 0xffff) << 16) |
                static_cast<uint32_t>(words[3] & 0xffff);
            if(callbacks_.actionFlags != NULL)
                callbacks_.actionFlags(callbacks_.userData, mask,
                                       words[1] == 1);
            break;
        }
        case MOVE_BEHAVIOUR_TARGET_FLAGS:
        {
            const uint32_t mask =
                (static_cast<uint32_t>(words[2] & 0xffff) << 16) |
                static_cast<uint32_t>(words[3] & 0xffff);
            if(callbacks_.targetFlags != NULL)
                callbacks_.targetFlags(callbacks_.userData, mask,
                                       words[1] == 1);
            break;
        }
        case MOVE_BEHAVIOUR_TARGET_FLAG_20:
            if(callbacks_.targetFlags != NULL)
                callbacks_.targetFlags(callbacks_.userData, 0x20u, words[1] != 0);
            break;
        case MOVE_BEHAVIOUR_CUE_FLAG:
            if(callbacks_.cueFlags != NULL)
                callbacks_.cueFlags(callbacks_.userData,
                                   uint32_t(1) << (words[1] & 31),
                                   (words[2] & 1) != 0);
            break;
        case MOVE_BEHAVIOUR_SET_FLAGS:
        {
            const uint32_t value =
                (static_cast<uint32_t>(words[1] & 0xffff) << 16) |
                static_cast<uint32_t>(words[2] & 0xffff);
            if(callbacks_.behaviourFlags != NULL)
                callbacks_.behaviourFlags(callbacks_.userData, value);
            break;
        }
        case MOVE_BEHAVIOUR_SET_RECOVERY_STATE:
            if(callbacks_.recoveryState != NULL)
                callbacks_.recoveryState(callbacks_.userData,
                    static_cast<uint8_t>(words[1]));
            break;
        case MOVE_BEHAVIOUR_SET_IMPACT_STATE:
            if(callbacks_.impactState != NULL)
                callbacks_.impactState(callbacks_.userData,
                                       static_cast<uint8_t>(words[1]));
            break;
        case MOVE_BEHAVIOUR_MOVEMENT_TARGET:
            if(callbacks_.movement != NULL)
                callbacks_.movement(callbacks_.userData, words[1], words[2],
                                    words[3], words[4],
                                    static_cast<uint8_t>(words[5]));
            break;
        case MOVE_BEHAVIOUR_CONTACT_MOVEMENT:
            if(callbacks_.contactMovement != NULL)
                callbacks_.contactMovement(callbacks_.userData, words[1],
                                            words[2], words[3]);
            break;
        case MOVE_BEHAVIOUR_FIRE_PROJECTILE:
            if(callbacks_.fireProjectile != NULL)
                callbacks_.fireProjectile(callbacks_.userData);
            break;
        case MOVE_BEHAVIOUR_WEAPON_ACTION:
        case MOVE_BEHAVIOUR_WEAPON_HAND:
        case MOVE_BEHAVIOUR_WEAPON_VISIBLE:
            if(callbacks_.weapon != NULL)
                callbacks_.weapon(callbacks_.userData, opcode, words[1]);
            break;
        case MOVE_BEHAVIOUR_WEAPON_SNAPSHOT:
        case MOVE_BEHAVIOUR_WEAPON_ALIGN:
            if(callbacks_.weapon != NULL)
                callbacks_.weapon(callbacks_.userData, opcode, 0);
            break;
        case MOVE_BEHAVIOUR_CONTACT_RESPONSE:
            if(callbacks_.statusFlags != NULL)
                callbacks_.statusFlags(callbacks_.userData, 4u, words[1] != 0);
            break;
        case MOVE_BEHAVIOUR_TURN_DURATION:
            if(callbacks_.turnDuration != NULL)
                callbacks_.turnDuration(callbacks_.userData,
                                        static_cast<uint8_t>(words[1]));
            break;
        case MOVE_BEHAVIOUR_SPACING_CORRECTION:
            if(callbacks_.spacingCorrection != NULL)
                callbacks_.spacingCorrection(callbacks_.userData,
                                             static_cast<uint8_t>(words[1]));
            break;
        case MOVE_BEHAVIOUR_COMBO_GATE:
            if(callbacks_.comboGate != NULL)
                yield = callbacks_.comboGate(callbacks_.userData, words[1]);
            break;
        case MOVE_BEHAVIOUR_ARM_HOLD_COMBO:
            if(callbacks_.inputHold != NULL)
                callbacks_.inputHold(callbacks_.userData, opcode, 0);
            break;
        case MOVE_BEHAVIOUR_HOLD_END_FRAME:
        case MOVE_BEHAVIOUR_HOLD_FOLLOWUP:
        case MOVE_BEHAVIOUR_HOLD_THRESHOLD:
        case MOVE_BEHAVIOUR_HOLD_KIND:
            if(callbacks_.inputHold != NULL)
                callbacks_.inputHold(callbacks_.userData, opcode, words[1]);
            break;
        case MOVE_BEHAVIOUR_SET_ACTION:
            if(callbacks_.setAction != NULL)
                callbacks_.setAction(callbacks_.userData, words[2]);
            break;
        case MOVE_BEHAVIOUR_RUN_ACTION:
            if(callbacks_.runAction != NULL)
                callbacks_.runAction(callbacks_.userData);
            break;
        case MOVE_BEHAVIOUR_EFFECT_LIST:
            if(callbacks_.effectList != NULL)
                callbacks_.effectList(callbacks_.userData, words[1],
                                      count - 2);
            break;
        case MOVE_BEHAVIOUR_CAMERA:
        {
            const int primaryDescriptor = words[1];
            const int middleDescriptor =
                count == 6 ? words[2] : -1;
            const int lowDescriptor =
                count == 6 ? words[3] : -1;
            if(callbacks_.camera != NULL)
            {
                callbacks_.camera(callbacks_.userData, primaryDescriptor,
                                  middleDescriptor, lowDescriptor,
                                  words[count - 1] == 1);
            }
            break;
        }
        case 0x37:

            if(callbacks_.cameraSettle != NULL)
                callbacks_.cameraSettle(callbacks_.userData, words[1]);
            break;
        default:

            break;
        }
        cursor_ = next;
        if(yield)
            return;
    }
}
void
BehaviourStreamRunner::clearWait()
{
    wait_ = 0.0f;
}
bool
BehaviourStreamRunner::running() const
{
    return running_;
}
bool
BehaviourStreamRunner::parked() const
{
    return parked_;
}
int
BehaviourStreamRunner::moveId() const
{
    return moveId_;
}
int
BehaviourStreamRunner::cursor() const
{
    return cursor_;
}
int
BehaviourStreamRunner::loopCounter() const
{
    return loopCounter_;
}
float
BehaviourStreamRunner::waitRemaining() const
{
    return wait_;
}
float
BehaviourStreamRunner::animationTimeScale() const
{
    return animationTimeScale_;
}
int
BehaviourStreamRunner::instructionsExecuted() const
{
    return instructionsExecuted_;
}
