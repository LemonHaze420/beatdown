#ifndef TASK_H
#define TASK_H

#include <stdint.h>

struct UpdateTask;
struct RenderTask;

typedef void UpdateTaskCallback(UpdateTask* self);
typedef void RenderTaskCallback(RenderTask* self);

struct UpdateTask
{
    int32_t             priority;
    uint32_t            suspendMask;
    uint32_t            reserved08;
    UpdateTaskCallback* updateCallback;
    UpdateTask*         prev;
    UpdateTask*         next;
    uint8_t             work[32];
    void*               userData;
    uint32_t            reserved3C;
    uint8_t             state;
    uint8_t             inUse;
    uint16_t            reserved42;
};

struct RenderTask
{
    int32_t             priority;
    int32_t             phase;
    RenderTaskCallback* renderCallback;
    RenderTask*         prev;
    RenderTask*         next;
    void*               userData;
    uint8_t             work[24];
};

void initializeTaskSystem(void);

UpdateTask* createUpdateTask(UpdateTaskCallback* cb, void* userData,
                             uint32_t suspendMask, int32_t priority);
int         runUpdateTasks(void);
void        destroyUpdateTask(UpdateTask* task);
void        setUpdateTaskSuspendMask(uint32_t mask);
uint32_t    getUpdateTaskSuspendMask(void);

RenderTask* createRenderTask(RenderTaskCallback* cb, void* userData,
                             int32_t phase, int32_t priority);
int         runRenderTasks(int32_t phase);

#endif
