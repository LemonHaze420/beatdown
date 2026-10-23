#ifndef STAGE_OBJECTS_H
#define STAGE_OBJECTS_H
#include <stdint.h>

const int STAGE_OBJECT_STAGE_COUNT = 55;
const int STAGE_OBJECT_PHASE_COUNT = 11;
const int STAGE_OBJECT_EVENT_FLAG_FIRST = 6000;
const int STAGE_OBJECT_EVENT_FLAG_LAST = 7500;
struct StageObjectRecord
{
    int8_t kind;
    int8_t sub;
    int16_t eventFlag;
    int16_t position[3];
    int16_t headingTenths;
    int8_t phaseGates[STAGE_OBJECT_PHASE_COUNT];
};
struct StageObjectTable
{
    int8_t pools[4];
    uint16_t firstRecord;
    uint16_t recordCount;
};
extern const StageObjectRecord STAGE_OBJECT_RECORDS[];
extern const StageObjectTable STAGE_OBJECT_TABLE[STAGE_OBJECT_STAGE_COUNT];

int stageObjectsForStage(uint16_t stageId, uint16_t storyPhase,
                         const StageObjectRecord** out, int capacity);
#endif
