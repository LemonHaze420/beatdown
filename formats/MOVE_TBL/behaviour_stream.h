#ifndef BEHAVIOUR_STREAM_H
#define BEHAVIOUR_STREAM_H
#include "move_tables.h"

typedef void BehaviourHitBlockCallback(void* userData, int blockIndex);
typedef void BehaviourHitWindowCallback(void* userData, int16_t centreFrame,
                                        int16_t framesBefore,
                                        int16_t framesAfter,
                                        uint32_t channelMask);
typedef void BehaviourApplyHitCallback(void* userData, int finisher);
typedef void BehaviourQueueMoveCallback(void* userData, int moveId);
typedef void BehaviourActionCallback(void* userData, int action);
typedef void BehaviourFlagsCallback(void* userData, uint32_t mask, bool set);
typedef void BehaviourSetFlagsCallback(void* userData, uint32_t value);
typedef void BehaviourMovementCallback(void* userData, int16_t x, int16_t y,
                                       int16_t z, int16_t heading,
                                       uint8_t frames);
typedef void BehaviourContactMovementCallback(void* userData, int16_t offset,
                                               int16_t heading, int16_t frames);
typedef bool BehaviourComboGateCallback(void* userData, int argument);
typedef void BehaviourInputHoldCallback(void* userData, int opcode,
                                        int16_t value);
typedef void BehaviourWeaponCallback(void* userData, int opcode, int16_t value);

typedef bool BehaviourTestFlagsCallback(void* userData, uint32_t mask,
                                        bool expectSet);
typedef void BehaviourEffectCallback(void* userData, int16_t recordIndex,
                                     int16_t boneIndex,
                                     int16_t angleBase,
                                     int16_t tuningA,
                                     int16_t tuningB);
typedef void BehaviourEffectListCallback(void* userData, int first,
                                         int count);
typedef void BehaviourRunActionCallback(void* userData);

typedef void BehaviourSoundCallback(void* userData, int kind,
                                    int first, int second);

typedef void BehaviourCameraSettleCallback(void* userData,
                                           int closeUpDurationFrames);

typedef void BehaviourCameraCallback(void* userData, int primaryDescriptor,
                                     int middleDescriptor,
                                     int lowDescriptor, bool exclusive);
struct BehaviourStreamCallbacks
{
    BehaviourHitBlockCallback* setHitBlock;
    BehaviourHitWindowCallback* armHitWindow;
    BehaviourApplyHitCallback* applyHit;
    BehaviourQueueMoveCallback* queueMove;
    BehaviourActionCallback* setAction;
    BehaviourFlagsCallback* actionFlags;
    BehaviourFlagsCallback* targetFlags;
    BehaviourFlagsCallback* cueFlags;
    BehaviourFlagsCallback* statusFlags;
    BehaviourSetFlagsCallback* behaviourFlags;
    BehaviourSetFlagsCallback* impactState;
    BehaviourSetFlagsCallback* recoveryState;
    BehaviourMovementCallback* movement;
    BehaviourContactMovementCallback* contactMovement;
    BehaviourActionCallback* turnDuration;
    BehaviourActionCallback* spacingCorrection;
    BehaviourInputHoldCallback* inputHold;
    BehaviourComboGateCallback* comboGate;
    BehaviourTestFlagsCallback* testTargetFlags;
    BehaviourEffectCallback* effect;
    BehaviourEffectListCallback* effectList;
    BehaviourRunActionCallback* runAction;
    BehaviourRunActionCallback* fireProjectile;
    BehaviourWeaponCallback* weapon;
    BehaviourSoundCallback* sound;
    BehaviourCameraSettleCallback* cameraSettle;
    BehaviourCameraCallback* camera;
    void (*timeScale)(void* userData, float rate);
    void* userData;
    BehaviourStreamCallbacks();
};
class BehaviourStreamRunner
{
public:
    BehaviourStreamRunner();
    void setCallbacks(const BehaviourStreamCallbacks& callbacks);

    void start(int moveId);
    void stop();

    void clearWait();

    void update(float deltaSeconds, float animationFrame);

    void setWaitInSeconds(bool seconds);

    void setEffectSuppressionMask(uint32_t mask);
    uint32_t effectSuppressionMask() const;
    bool running() const;
    bool parked() const;
    int moveId() const;
    int cursor() const;
    int loopCounter() const;
    float waitRemaining() const;
    float animationTimeScale() const;
    int instructionsExecuted() const;
private:
    const int16_t* wordsAt(int offset) const;
    int available(int offset) const;
    BehaviourStreamCallbacks callbacks_;
    int moveId_;
    int streamFirst_;
    int streamCount_;
    int cursor_;
    int loopReturn_;
    int loopCounter_;
    float wait_;
    float animationTimeScale_;
    uint32_t effectSuppressionMask_;
    bool running_;
    bool parked_;
    bool waitInSeconds_;
    int instructionsExecuted_;
};
#endif
