#ifndef OS_TASK_H
#define OS_TASK_H

#include <stdint.h>

/* Task states */
typedef enum
{
    READY,
    RUNNING,
    BLOCKED,
    SUSPENDED
} TaskState_t;

/* Task Control Block (TCB) */
typedef struct TCB
{
    uint32_t *stackPtr;        /* Current stack ptr (MUST stay first: os_kernel_asm.s assumes offset 0) */
    TaskState_t state;         /* Current state of the task */
    uint8_t base_priority;     /* Priority the task was created with (higher number = higher priority) */
    uint8_t current_priority;  /* Effective priority; raised above base by priority inheritance */
    uint32_t *stackBase;       /* Lowest address of the task stack (holds the overflow canary) */
    uint32_t stackSize;        /* Size of the stack in 32-bit words */
    uint32_t sleep_time;       /* Ticks left before the kernel wakes the task (0 = not timing out) */

    /* Set while the task sits in a mutex/semaphore wait queue, so the tick handler
       can pull it back out when its timeout expires. */
    struct TCB **wait_queue;
    uint8_t *wait_count;
    uint8_t wait_result;       /* OS_SUCCESS if handed the resource, OS_TIMEOUT if it timed out */
} TCB_t;

#endif
