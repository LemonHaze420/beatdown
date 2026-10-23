#ifndef MOVE_STREAM_H
#define MOVE_STREAM_H
#include "move_tables.h"

enum MoveStreamCallMode
{

    MOVE_STREAM_CALL_PARTNER_THROW = -3,

    MOVE_STREAM_CALL_SELF_THROW = -2,

    MOVE_STREAM_CALL_QUEUED = -1,
    MOVE_STREAM_CALL_NEUTRAL = 1
};

const int MOVE_THROW_ACTION_AIR_COMBO = 0x84;
const int MOVE_THROW_ACTION_AIR = 0x85;
const int MOVE_THROW_ACTION_GROUND_COMBO = 0x86;
const int MOVE_THROW_ACTION_GROUND = 0x87;

int moveThrowBaseAction(bool grounded, bool comboBit);

typedef void MoveStreamMotionCallback(void* userData, int motionId,
                                      int blendFrames, int maxDurationFrames,
                                      int transitionFrames);
typedef int MoveStreamCallCallback(void* userData, int argument);
typedef void MoveStreamActionCallback(void* userData, int moveId);
typedef void MoveStreamEffectCallback(void* userData, float scale, int first,
                                      int second, int third);
typedef bool MoveStreamComboGateCallback(void* userData, int argument);
typedef void MoveStreamComboArmCallback(void* userData);
typedef uint32_t MoveStreamRandomCallback(void* userData);
struct MoveStreamCallbacks
{
    MoveStreamMotionCallback* playMotion;
    MoveStreamCallCallback* call;
    MoveStreamActionCallback* setAction;
    MoveStreamEffectCallback* effect;
    MoveStreamComboGateCallback* comboGate;
    MoveStreamComboArmCallback* comboArm;
    MoveStreamComboArmCallback* clearBehaviourWait;
    MoveStreamRandomCallback* random;
    void (*timeScale)(void* userData, float rate);
    void* userData;
    MoveStreamCallbacks();
};
class MoveStreamRunner
{
public:
    MoveStreamRunner();
    void setCallbacks(const MoveStreamCallbacks& callbacks);

    void start(int moveId);
    void stop();

    void update(float deltaSeconds);
    bool running() const;

    bool ended() const;
    int moveId() const;
    int cursor() const;
    int loopCounter() const;
    float animationTimeScale() const;
    int instructionsExecuted() const;

    bool progress() const;

    void suppressNextMotionFlags();
private:
    int16_t word(int offset) const;
    MoveStreamCallbacks callbacks_;
    int moveId_;
    int streamFirst_;
    int streamCount_;
    int cursor_;
    int loopReturn_;
    int loopCounter_;
    float animationTimeScale_;
    float deltaSeconds_;
    bool running_;
    bool ended_;
    bool restarted_;
    bool progress_;
    bool suppressMotionFlags_;
    int instructionsExecuted_;
};
#endif
