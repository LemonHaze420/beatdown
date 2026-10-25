#include "task.h"

#include <string.h>
#include <stdio.h>

enum { TASK_POOL_SIZE = 0x200 };

static UpdateTask  g_UpdateTaskPool[TASK_POOL_SIZE];
static UpdateTask* g_pUpdateTaskFreeQueue[TASK_POOL_SIZE];
static int16_t     g_wUpdateTaskFreeReadIndex;
static int16_t     g_wUpdateTaskFreeWriteIndex;
static UpdateTask* g_pCurrentUpdateTask;
static uint32_t    g_dwUpdateTaskSuspendMask;
static int         g_nUpdateTaskCreateCount;
static int         g_nUpdateTaskDestroyCount;

static RenderTask  g_RenderTaskPool[TASK_POOL_SIZE];
static RenderTask* g_pRenderTaskSlots[TASK_POOL_SIZE];
static int         g_nRenderTaskNextSlot;

static void sentinelUpdate(UpdateTask*) {}
static void sentinelRender(RenderTask*) {}

static void initializeUpdateTaskPool(void)
{
    for(int i = 0; i < TASK_POOL_SIZE; i++){
        UpdateTask* t = &g_UpdateTaskPool[i];
        t->priority = 0;
        t->suspendMask = 0;
        t->reserved08 = 0;
        t->inUse = 0;
        g_pUpdateTaskFreeQueue[i] = t;
    }
    g_UpdateTaskPool[0].priority = 0;
    g_UpdateTaskPool[0].suspendMask = 0;
    g_UpdateTaskPool[0].updateCallback = sentinelUpdate;
    g_UpdateTaskPool[0].prev = nullptr;
    g_UpdateTaskPool[0].next = &g_UpdateTaskPool[1];
    g_UpdateTaskPool[1].priority = 0x7fffffff;
    g_UpdateTaskPool[1].suspendMask = 0;
    g_UpdateTaskPool[1].updateCallback = sentinelUpdate;
    g_UpdateTaskPool[1].prev = &g_UpdateTaskPool[0];
    g_UpdateTaskPool[1].next = nullptr;
    g_wUpdateTaskFreeReadIndex = 2;
    g_wUpdateTaskFreeWriteIndex = 2;
    g_pCurrentUpdateTask = g_UpdateTaskPool;
}

static void initializeRenderTaskPool(void)
{
    for(int i = 0; i < TASK_POOL_SIZE; i++){
        RenderTask* r = &g_RenderTaskPool[i];
        r->priority = 0;
        r->phase = 1;
        g_pRenderTaskSlots[i] = r;
    }
    g_nRenderTaskNextSlot = 2;
    g_RenderTaskPool[0].phase = 999;
    g_RenderTaskPool[0].priority = 0;
    g_RenderTaskPool[0].renderCallback = sentinelRender;
    g_RenderTaskPool[0].prev = nullptr;
    g_RenderTaskPool[0].next = &g_RenderTaskPool[1];
    g_RenderTaskPool[1].phase = 999;
    g_RenderTaskPool[1].priority = 0x7fffffff;
    g_RenderTaskPool[1].renderCallback = sentinelRender;
    g_RenderTaskPool[1].prev = &g_RenderTaskPool[0];
    g_RenderTaskPool[1].next = nullptr;
}

void
initializeTaskSystem(void)
{
    g_dwUpdateTaskSuspendMask = 0;
    g_nUpdateTaskCreateCount = 0;
    g_nUpdateTaskDestroyCount = 0;
    initializeUpdateTaskPool();
    initializeRenderTaskPool();
}

UpdateTask*
createUpdateTask(UpdateTaskCallback* cb, void* userData, uint32_t suspendMask, int32_t priority)
{
    UpdateTask* slot = g_pUpdateTaskFreeQueue[g_wUpdateTaskFreeReadIndex];
    if(slot->inUse != 0){

        int scanned = 0;
        while(slot->inUse != 0){
            g_wUpdateTaskFreeReadIndex++;
            if(g_wUpdateTaskFreeReadIndex > 0x1ff)
                g_wUpdateTaskFreeReadIndex = 2;
            slot = g_pUpdateTaskFreeQueue[g_wUpdateTaskFreeReadIndex];
            if(++scanned >= TASK_POOL_SIZE){
                fprintf(stderr, "error: update task pool exhausted\n");
                return nullptr;
            }
        }
        fprintf(stderr, "error: this task used\n");
    }

    slot->updateCallback = cb;
    slot->priority = priority;
    slot->suspendMask = suspendMask;
    slot->reserved08 = 0;

    UpdateTask* after;
    for(after = g_UpdateTaskPool; (uint32_t)after->priority <= (uint32_t)priority; after = after->next){
    }
    UpdateTask* before = after->prev;
    slot->prev = before;
    slot->next = after;
    before->next = slot;
    after->prev = slot;

    g_wUpdateTaskFreeReadIndex++;
    if(g_wUpdateTaskFreeReadIndex > 0x1ff)
        g_wUpdateTaskFreeReadIndex = 2;

    slot->state = 1;
    slot->userData = userData;
    slot->inUse = 1;
    memset(slot->work, 0, sizeof(slot->work));

    if(slot->next != slot && slot->prev != slot){
        g_nUpdateTaskCreateCount++;
        return slot;
    }
    fprintf(stderr, "OUCH!\n");
    for(;;){}
}

void
destroyUpdateTask(UpdateTask* task)
{
    if(task == nullptr || task->inUse == 0)
        return;
    task->inUse = 0;
    task->suspendMask = 0;
    task->next->prev = task->prev;
    task->prev->next = task->next;
    g_pUpdateTaskFreeQueue[g_wUpdateTaskFreeWriteIndex] = task;
    g_wUpdateTaskFreeWriteIndex++;
    if(g_wUpdateTaskFreeWriteIndex > 0x1ff)
        g_wUpdateTaskFreeWriteIndex = 2;
    g_nUpdateTaskDestroyCount++;
}

int
runUpdateTasks(void)
{
    int count = 0;
    do{
        if((g_dwUpdateTaskSuspendMask & g_pCurrentUpdateTask->suspendMask) == 0)
            g_pCurrentUpdateTask->updateCallback(g_pCurrentUpdateTask);
        count++;
        g_pCurrentUpdateTask = g_pCurrentUpdateTask->next;
    }while(g_pCurrentUpdateTask != nullptr);
    g_pCurrentUpdateTask = g_UpdateTaskPool;
    return count;
}

void
setUpdateTaskSuspendMask(uint32_t mask)
{
    g_dwUpdateTaskSuspendMask = mask;
}

uint32_t
getUpdateTaskSuspendMask(void)
{
    return g_dwUpdateTaskSuspendMask;
}

RenderTask*
createRenderTask(RenderTaskCallback* cb, void* userData, int32_t phase, int32_t priority)
{
    if(priority < 1)
        priority = 1;
    if(g_nRenderTaskNextSlot >= TASK_POOL_SIZE)
        return nullptr;

    RenderTask* slot = g_pRenderTaskSlots[g_nRenderTaskNextSlot];
    slot->renderCallback = cb;
    slot->priority = priority;
    slot->phase = phase;

    RenderTask* after;
    for(after = g_RenderTaskPool; after->priority <= priority; after = after->next){
    }
    RenderTask* before = after->prev;
    slot->prev = before;
    slot->next = after;
    before->next = slot;
    after->prev = slot;

    g_nRenderTaskNextSlot++;
    if(g_nRenderTaskNextSlot > 0x1ff)
        g_nRenderTaskNextSlot = TASK_POOL_SIZE;

    memset(slot->work, 0, sizeof(slot->work));
    slot->userData = userData;
    return slot;
}

int
runRenderTasks(int32_t phase)
{
    int count = 0;
    RenderTask* r = g_RenderTaskPool;
    do{
        if(phase != 0 && r->phase <= phase){
            count++;
            r->renderCallback(r);
            r->next->prev = r->prev;
            r->prev->next = r->next;
        }
        r = r->next;
    }while(r != nullptr);

    if(phase == 2){
        g_nRenderTaskNextSlot = 2;
        g_RenderTaskPool[0].next = &g_RenderTaskPool[1];
        g_RenderTaskPool[1].prev = &g_RenderTaskPool[0];
    }
    return count;
}
