#ifndef CAMDAT_H
#define CAMDAT_H

#include "camera.h"

#include <stddef.h>
#include <stdint.h>

#include <vector>

const int CAMDAT_DESCRIPTOR_COUNT = 0x77;

struct CamdatRecord
{
    uint32_t firstWord;
    uint32_t wordCount;
};

class CamdatDatabase
{
public:
    bool parse(const uint8_t* data, size_t size);
    void clear(void);
    int recordCount(void) const;
    bool load(const char* path);
    const int16_t* recordWords(int index) const;
    int recordWordCount(int index) const;

private:
    std::vector<int16_t> words_;
    std::vector<CamdatRecord> records_;
};

int selectCamdatRecordIndex(uint16_t descriptorIndex,
                            uint32_t randomValue,
                            uint32_t repeatRandomValue,
                            uint16_t* previousVariant);

typedef uint32_t CamdatRandomProvider(void* userData);
int selectCamdatRecordIndexWithRandom(uint16_t descriptorIndex,
                            CamdatRandomProvider* randomProvider,
                            void* userData,
                            uint16_t* previousVariant);
typedef bool CamdatBasisProvider(void* userData, bool linkedActor,
                                 uint16_t boneIndex,
                                 CamdatFramingBasis* basis);

class CamdatRuntime
{
public:
    CamdatRuntime();

    bool start(const CamdatDatabase* database, int recordIndex,
               const CamdatFraming& currentFraming);
    void stop(void);
    bool update(float scaledDeltaSeconds,
                CamdatBasisProvider* basisProvider, void* userData,
                CamdatFraming* framing);

    bool active(void) const;
    bool finished(void) const;
    int recordIndex(void) const;
    uint8_t cameraOpcode(void) const;
    float fieldOfView(void) const;
    float timeScale(void) const;
    bool slowMotionEnabled(void) const;
    uint32_t stateFlags(void) const;
    bool consumeActorActionFlag(bool* enabled);

private:
    bool execute(float scaledDeltaSeconds);
    bool resolveSubjectBasis(CamdatBasisProvider* provider, void* userData,
                             CamdatFramingBasis* basis) const;
    bool computeDesired(CamdatBasisProvider* provider, void* userData,
                        bool ending, CamdatFraming* framing) const;

    const CamdatDatabase* database_;
    const int16_t* words_;
    int wordCount_;
    int recordIndex_;
    int cursor_;
    uint8_t cameraOpcode_;
    uint8_t subjectMode_;
    uint8_t orientationMode_;
    uint16_t boneIndex_;
    CameraFramingVector offset_;
    CamdatFramingParameters start_;
    int actorActionFlagCommand_;
    CamdatFramingParameters end_;
    float startRoll_;
    float endRoll_;
    float waitRemaining_;
    float waitDuration_;
    float fieldOfView_;
    uint32_t flags_;
    float timeScale_;
    bool slowMotionEnabled_;
    CamdatFraming current_;
    CamdatFraming desired_;
    bool active_;
    bool finished_;
    bool desiredInitialized_;
    bool endingPending_;
    bool dynamicYawPending_;
};

#endif
