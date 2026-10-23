#include "move_stream.h"
#include <stddef.h>
int
moveThrowBaseAction(bool grounded, bool comboBit)
{
    if(grounded)
        return comboBit ? MOVE_THROW_ACTION_GROUND_COMBO :
                          MOVE_THROW_ACTION_GROUND;
    return comboBit ? MOVE_THROW_ACTION_AIR_COMBO : MOVE_THROW_ACTION_AIR;
}
MoveStreamCallbacks::MoveStreamCallbacks()
    : playMotion(NULL), call(NULL), setAction(NULL), effect(NULL),
      comboGate(NULL), comboArm(NULL), clearBehaviourWait(NULL), random(NULL), timeScale(NULL), userData(NULL)
{
}
MoveStreamRunner::MoveStreamRunner()
    : moveId_(-1), streamFirst_(0), streamCount_(0), cursor_(0),
      loopReturn_(0), loopCounter_(0), animationTimeScale_(1.0f),
      deltaSeconds_(0.0f), running_(false), ended_(false), restarted_(false),
      progress_(false), suppressMotionFlags_(false), instructionsExecuted_(0)
{
}
void
MoveStreamRunner::setCallbacks(const MoveStreamCallbacks& callbacks)
{
    callbacks_ = callbacks;
}
void
MoveStreamRunner::start(int moveId)
{

    moveId_ = moveId;
    loopReturn_ = 0;
    loopCounter_ = 0;
    animationTimeScale_ = 1.0f;
    if(callbacks_.timeScale != NULL)
        callbacks_.timeScale(callbacks_.userData, 1.0f);
    progress_ = false;
    instructionsExecuted_ = 0;
    restarted_ = true;
    ended_ = false;
    if(moveId < 0 || moveId >= MOVE_RECORD_COUNT)
    {
        streamFirst_ = 0;
        streamCount_ = 0;
        cursor_ = 0;
        running_ = false;
        return;
    }
    const MoveStream& stream = MOVE_STREAMS[moveId];
    streamFirst_ = stream.firstWord;
    streamCount_ = stream.wordCount;
    cursor_ = streamFirst_;
    running_ = streamCount_ > 0;
}
void
MoveStreamRunner::stop()
{
    running_ = false;
    ended_ = false;
}
void
MoveStreamRunner::suppressNextMotionFlags()
{
    suppressMotionFlags_ = true;
}
int16_t
MoveStreamRunner::word(int offset) const
{
    const int index = cursor_ + offset;
    if(index < streamFirst_ || index >= streamFirst_ + streamCount_)
        return 0;
    return MOVE_STREAM_WORDS[index];
}
void
MoveStreamRunner::update(float deltaSeconds)
{
    deltaSeconds_ = deltaSeconds;
    if(!running_)
        return;
    for(;;)
    {
        if(cursor_ < streamFirst_ || cursor_ >= streamFirst_ + streamCount_)
        {
            running_ = false;
            return;
        }
        const int available = streamFirst_ + streamCount_ - cursor_;
        const int opcode = word(0);
        int next = cursor_ + moveStreamInstructionWords(
            &MOVE_STREAM_WORDS[cursor_], available);
        bool yielding = false;
        ++instructionsExecuted_;
        switch(opcode)
        {
        case MOVE_STREAM_PLAY_MOTION:
        {
            int flags = word(4);
            if(suppressMotionFlags_)
            {
                suppressMotionFlags_ = false;
                flags = 0;
            }
            if(callbacks_.playMotion != NULL)
                callbacks_.playMotion(callbacks_.userData, word(1), word(2),
                                      word(3), flags);
            yielding = true;
            break;
        }
        case MOVE_STREAM_END:
            running_ = false;
            ended_ = true;
            yielding = true;
            break;
        case MOVE_STREAM_LOOP_BEGIN:
            loopReturn_ = cursor_ + 2;
            loopCounter_ = word(1) - 1;
            break;
        case MOVE_STREAM_LOOP_END:
            if(loopCounter_ != 0)
            {
                if(loopCounter_ > 0)
                    --loopCounter_;
                next = loopReturn_;
            }
            break;
        case MOVE_STREAM_CLEAR_PROGRESS:
            progress_ = false;
            if(callbacks_.clearBehaviourWait != NULL)
                callbacks_.clearBehaviourWait(callbacks_.userData);
            break;
        case MOVE_STREAM_SET_TIME_SCALE:
            animationTimeScale_ = static_cast<float>(word(1)) * 0.001f;
            if(callbacks_.timeScale != NULL)
                callbacks_.timeScale(callbacks_.userData, animationTimeScale_);
            break;
        case MOVE_STREAM_CALL:
        {
            restarted_ = false;
            const int result = callbacks_.call != NULL ?
                callbacks_.call(callbacks_.userData, word(1)) : 0;
            (void)result;
            if(deltaSeconds_ != 0.0f && restarted_)
                next = cursor_;
            break;
        }
        case MOVE_STREAM_SET_ACTION:
            restarted_ = false;
            if(callbacks_.setAction != NULL)
                callbacks_.setAction(callbacks_.userData, word(1));
            if(deltaSeconds_ != 0.0f && restarted_)
                next = cursor_;
            break;
        case MOVE_STREAM_NOP:
        case MOVE_STREAM_SKIP_3:
            break;
        case MOVE_STREAM_EFFECT:
            if(callbacks_.effect != NULL)
                callbacks_.effect(callbacks_.userData,
                                  static_cast<float>(word(4)) * 0.001f,
                                  word(1), word(2), word(3));
            break;
        case MOVE_STREAM_COMBO_ARM:
            if(callbacks_.comboArm != NULL)
                callbacks_.comboArm(callbacks_.userData);
            break;
        case MOVE_STREAM_COMBO_GATE:
            if(callbacks_.comboGate != NULL &&
               callbacks_.comboGate(callbacks_.userData, word(1)))
                yielding = true;
            break;
        case MOVE_STREAM_RANDOM_CALL:
        {
            const int count = word(1);
            restarted_ = false;
            if(count > 0)
            {
                const uint32_t roll = callbacks_.random != NULL ?
                    callbacks_.random(callbacks_.userData) : 0;
                const int choice = word(2 + static_cast<int>(
                    roll % static_cast<uint32_t>(count)));
                const int result = callbacks_.call != NULL ?
                    callbacks_.call(callbacks_.userData, choice) : 0;
                if(result != 0 && deltaSeconds_ != 0.0f && restarted_)
                    next = cursor_;
            }
            break;
        }
        default:
            break;
        }
        cursor_ = next;
        if(yielding)
            return;
    }
}
bool MoveStreamRunner::running() const { return running_; }
bool MoveStreamRunner::ended() const { return ended_; }
int MoveStreamRunner::moveId() const { return moveId_; }
int MoveStreamRunner::cursor() const { return cursor_; }
int MoveStreamRunner::loopCounter() const { return loopCounter_; }
float MoveStreamRunner::animationTimeScale() const
{
    return animationTimeScale_;
}
int MoveStreamRunner::instructionsExecuted() const
{
    return instructionsExecuted_;
}
bool MoveStreamRunner::progress() const { return progress_; }
