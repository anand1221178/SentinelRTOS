#include <stdint.h>
#include "os_kernel.h"

/* Physical storage for every Task Control Block */
TCB_t os_tasks[MAX_TASKS];
static uint8_t task_count = 0;

/* Task the CPU is running. PendSV reads/writes ->stackPtr through this pointer. */
TCB_t *current_tcb = NULL;

static volatile uint32_t os_ticks = 0;
static bool kernel_running = false;

/* --- Stack setup --- */

/*
 * Builds the exception frame a task needs to be "resumed" for the first time:
 * the hardware frame (R0-R3, R12, LR, PC, xPSR) that an exception return pops,
 * plus the software frame (R4-R11) that PendSV pops by hand.
 */
static uint32_t* os_task_init_stack(void (*taskptr)(void), uint32_t *stackTop)
{
    uint32_t *stk = stackTop;

    *(--stk) = 0x01000000;        /* xPSR: Thumb bit set */
    *(--stk) = (uint32_t)(uintptr_t)taskptr; /* PC */
    *(--stk) = 0xFFFFFFFF;        /* LR: tasks must not return */
    *(--stk) = 0x12121212;        /* R12 */
    *(--stk) = 0x03030303;        /* R3 */
    *(--stk) = 0x02020202;        /* R2 */
    *(--stk) = 0x01010101;        /* R1 */
    *(--stk) = 0x00000000;        /* R0 */

    for (int i = 0; i < 8; i++)
    {
        *(--stk) = 0x00000000;    /* R4 - R11 */
    }

    return stk;
}

bool os_task_create(void (*taskptr)(void), uint32_t *stackBase, uint32_t stackWords, uint8_t priority)
{
    if (task_count >= MAX_TASKS || taskptr == NULL || stackWords < 64) return false;

    TCB_t *new_tcb = &os_tasks[task_count];

    new_tcb->base_priority    = priority;
    new_tcb->current_priority = priority;
    new_tcb->state            = READY;
    new_tcb->stackBase        = stackBase;
    new_tcb->stackSize        = stackWords;
    new_tcb->sleep_time       = 0;
    new_tcb->wait_queue       = NULL;
    new_tcb->wait_count       = NULL;
    new_tcb->wait_result      = OS_SUCCESS;

    stackBase[0] = STACK_CANARY;  /* Overflow tripwire, checked on every switch */

    /* AAPCS wants an 8-byte aligned stack pointer. */
    uint32_t *stackTop = (uint32_t *)((uintptr_t)(stackBase + stackWords) & ~(uintptr_t)0x7U);
    new_tcb->stackPtr = os_task_init_stack(taskptr, stackTop);

    if (current_tcb == NULL) current_tcb = new_tcb;

    task_count++;
    return true;
}

__attribute__((weak)) void os_stack_overflow_hook(TCB_t *task)
{
    (void)task;
    for (;;) { }   /* Halt: the TCB in the debugger names the offending task */
}

/* --- Scheduler --- */

/*
 * Highest current_priority wins. Among equal priorities the scan starts one slot
 * past the running task, so equal-priority tasks rotate on every tick instead of
 * the lowest-index one starving the rest.
 */
void os_scheduler(void)
{
    if (task_count == 0) return;

    if (current_tcb != NULL && current_tcb->stackBase[0] != STACK_CANARY)
    {
        os_stack_overflow_hook(current_tcb);
    }

    uint8_t start = (current_tcb != NULL) ? (uint8_t)((current_tcb - os_tasks) + 1) : 0;
    TCB_t *best = NULL;

    for (uint8_t k = 0; k < task_count; k++)
    {
        TCB_t *t = &os_tasks[(start + k) % task_count];

        if (t->state != READY && t->state != RUNNING) continue;
        if (best == NULL || t->current_priority > best->current_priority) best = t;
    }

    if (best == NULL) return;  /* Everything blocked: stay put (idle never blocks) */

    if (current_tcb != NULL && current_tcb != best && current_tcb->state == RUNNING)
    {
        current_tcb->state = READY;
    }

    best->state = RUNNING;
    current_tcb = best;
}

uint32_t os_get_ticks(void)
{
    return os_ticks;
}

/* Pulls a task out of whichever wait queue it is parked in. */
static void wait_queue_remove(TCB_t *task)
{
    if (task->wait_queue == NULL || task->wait_count == NULL) return;

    for (uint8_t i = 0; i < *task->wait_count; i++)
    {
        if (task->wait_queue[i] == task)
        {
            for (uint8_t j = i; j + 1 < *task->wait_count; j++)
            {
                task->wait_queue[j] = task->wait_queue[j + 1];
            }
            (*task->wait_count)--;
            break;
        }
    }

    task->wait_queue = NULL;
    task->wait_count = NULL;
}

void SysTick_Handler(void)
{
    os_ticks++;

    for (uint8_t i = 0; i < task_count; i++)
    {
        TCB_t *t = &os_tasks[i];

        if (t->sleep_time == 0) continue;

        if (--t->sleep_time == 0)
        {
            if (t->wait_queue != NULL)
            {
                /* Timed out waiting on a mutex/semaphore rather than sleeping. */
                wait_queue_remove(t);
                t->wait_result = OS_TIMEOUT;
            }
            t->state = READY;
        }
    }

    /* Unconditional: this is what makes the kernel preemptive. The scheduler
       re-runs every tick so a woken higher-priority task takes the CPU
       immediately, and equal-priority tasks time-slice. */
    os_yield();
}

/*
 * Parks the running task on a wait queue. Returns false if the queue is full.
 * Caller must already be inside a critical section and must yield afterwards.
 */
static bool block_current(TCB_t **queue, uint8_t *count, uint8_t max, uint32_t timeout)
{
    if (*count >= max) return false;

    queue[*count] = current_tcb;
    (*count)++;

    current_tcb->wait_queue  = queue;
    current_tcb->wait_count  = count;
    current_tcb->wait_result = OS_SUCCESS;
    current_tcb->sleep_time  = (timeout == WAIT_FOREVER) ? 0 : timeout;
    current_tcb->state       = BLOCKED;

    return true;
}

/* Hands a blocked task the resource it was waiting for. */
static void wake_task(TCB_t *task)
{
    wait_queue_remove(task);
    task->sleep_time  = 0;
    task->wait_result = OS_SUCCESS;
    task->state       = READY;
}

/* Highest-priority waiter, FIFO among equals. */
static TCB_t* pick_waiter(TCB_t **queue, uint8_t count)
{
    TCB_t *best = NULL;

    for (uint8_t i = 0; i < count; i++)
    {
        if (best == NULL || queue[i]->current_priority > best->current_priority) best = queue[i];
    }
    return best;
}

void os_delay(uint32_t ms)
{
    if (!kernel_running || ms == 0) return;

    uint32_t primask = os_enter_critical();
    current_tcb->sleep_time = ms;
    current_tcb->wait_queue = NULL;
    current_tcb->wait_count = NULL;
    current_tcb->state      = BLOCKED;
    os_exit_critical(primask);

    os_yield();
}

void os_kernel_init(void)
{
    __disable_irq();

    /* Keep the core clocked in Sleep/Stop/Standby so the debugger stays attached */
    DBGMCU->CR |= (DBGMCU_CR_DBG_SLEEP | DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY);

    task_count     = 0;
    current_tcb    = NULL;
    os_ticks       = 0;
    kernel_running = false;
}

void os_kernel_launch(void)
{
    SysTick->LOAD = (SYS_CLK_HZ / SYSTICK_FREQ) - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

    /* PendSV must be the lowest priority in the system: it may only run once
       every other handler has finished, otherwise it would switch context out
       from under a half-finished ISR. */
    NVIC_SetPriority(PendSV_IRQn, 0xFF);
    NVIC_SetPriority(SysTick_IRQn, 0xFE);

    os_scheduler();              /* Start with the highest-priority ready task */
    kernel_running = true;
    os_start_first_task();
}

/* --- Mutex (with priority inheritance) --- */

void os_mutex_init(os_mutex_t* mutex)
{
    mutex->lock       = 0;
    mutex->owner      = NULL;
    mutex->wait_count = 0;
}

uint8_t os_mutex_acquire(os_mutex_t* mutex, uint32_t timeout)
{
    uint32_t primask = os_enter_critical();

    if (mutex->lock == 0)
    {
        mutex->lock  = 1;
        mutex->owner = current_tcb;
        os_exit_critical(primask);
        return OS_SUCCESS;
    }

    if (mutex->owner == current_tcb)   /* Not recursive: report it instead of deadlocking */
    {
        os_exit_critical(primask);
        return OS_FAIL;
    }

    if (timeout == 0 || !kernel_running)
    {
        os_exit_critical(primask);
        return OS_TIMEOUT;
    }

    /* Priority inheritance: lend the owner our priority so a medium-priority
       task cannot preempt it while it holds the lock we need. */
    if (current_tcb->current_priority > mutex->owner->current_priority)
    {
        mutex->owner->current_priority = current_tcb->current_priority;
    }

    if (!block_current(mutex->wait_queue, &mutex->wait_count, MAX_WAITING_TASKS, timeout))
    {
        os_exit_critical(primask);
        return OS_FAIL;
    }

    os_exit_critical(primask);
    os_yield();

    /* Resumes here once released to us, or once the timeout fires. */
    return current_tcb->wait_result;
}

void os_mutex_release(os_mutex_t* mutex)
{
    uint32_t primask = os_enter_critical();

    if (mutex->owner != NULL)
    {
        /* Drop any donated priority. ponytail: a task holding two contested
           mutexes drops back to base one release early; per-mutex donation
           tracking if that ever matters. */
        mutex->owner->current_priority = mutex->owner->base_priority;
    }

    TCB_t *next = pick_waiter(mutex->wait_queue, mutex->wait_count);

    if (next != NULL)
    {
        /* Direct handoff: ownership passes without unlocking, so a third task
           cannot barge in between the release and the waiter running. */
        mutex->owner = next;
        wake_task(next);
        os_exit_critical(primask);
        os_yield();
        return;
    }

    mutex->lock  = 0;
    mutex->owner = NULL;
    os_exit_critical(primask);
}

/* --- Semaphore --- */

void os_semaphore_init(os_semaphore_t* sem, uint32_t initial, uint32_t max_count)
{
    sem->count      = initial;
    sem->max_count  = max_count;
    sem->wait_count = 0;
}

uint8_t os_semaphore_acquire(os_semaphore_t* sem, uint32_t timeout)
{
    uint32_t primask = os_enter_critical();

    if (sem->count > 0)
    {
        sem->count--;
        os_exit_critical(primask);
        return OS_SUCCESS;
    }

    if (timeout == 0 || !kernel_running)
    {
        os_exit_critical(primask);
        return OS_TIMEOUT;
    }

    if (!block_current(sem->wait_queue, &sem->wait_count, MAX_TASKS, timeout))
    {
        os_exit_critical(primask);
        return OS_FAIL;
    }

    os_exit_critical(primask);
    os_yield();

    return current_tcb->wait_result;
}

void os_semaphore_release(os_semaphore_t* sem)
{
    uint32_t primask = os_enter_critical();

    TCB_t *next = pick_waiter(sem->wait_queue, sem->wait_count);

    if (next != NULL)
    {
        /* Token passes straight to the waiter; count stays where it is. */
        wake_task(next);
        os_exit_critical(primask);
        os_yield();
        return;
    }

    if (sem->count < sem->max_count) sem->count++;
    os_exit_critical(primask);
}
