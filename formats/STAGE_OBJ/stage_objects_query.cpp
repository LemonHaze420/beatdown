#include "stage_objects.h"
#include <stddef.h>

int
stageObjectsForStage(uint16_t stageId, uint16_t storyPhase,
                     const StageObjectRecord** out, int capacity)
{
    if(out == NULL || capacity <= 0 || stageId >= STAGE_OBJECT_STAGE_COUNT)
        return 0;
    int phase = static_cast<int>(storyPhase) - 1;
    if(phase < 0)
        phase = 0;
    if(phase > STAGE_OBJECT_PHASE_COUNT - 1)
        phase = STAGE_OBJECT_PHASE_COUNT - 1;
    const StageObjectTable& table = STAGE_OBJECT_TABLE[stageId];
    int count = 0;
    for(int i = 0; i < table.recordCount && count < capacity; ++i)
    {
        const StageObjectRecord& record =
            STAGE_OBJECT_RECORDS[table.firstRecord + i];
        if(record.phaseGates[phase] < 0)
            continue;
        out[count++] = &record;
    }
    return count;
}
