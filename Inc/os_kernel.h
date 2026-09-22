#ifndef OS_KERNEL_H
#define OS_KERNEL_H

#include <stdlib.h>
#include <stdbool.h>
#include "os_task.h"
#include "stm32f4xx.h"

/* --- Configuration --- */
#define MAX_TASKS    6          /* Maximum tasks the kernel can manage */
#define STACK_SIZE   256        /* Default stack size in 32-bit words (1024 bytes) */
#define SYSTICK_FREQ 1000       /* 1kHz tick (1ms) */
#define SYS_CLK_HZ   16000000U  /* HSI, no PLL */

#define STACK_CANARY 0xC0FFEEU  /* Written at the bottom of every task stack */

#define OS_SUCCESS 0
#define OS_TIMEOUT 1
#define OS_FAIL    2
#define WAIT_FOREVER 0xFFFFFFFF

/* --- Synchronization Primitives --- */

#define MAX_WAITING_TASKS 5

typedef struct
{
    uint32_t lock;                         /* 0 = unlocked, 1 = locked */
    TCB_t* owner;                          /* Task currently holding the lock */
    TCB_t* wait_queue[MAX_WAITING_TASKS];  /* FIFO of tasks blocked on this mutex */
    uint8_t wait_count;
} os_mutex_t;

typedef struct
{
    uint32_t count;                        /* Available tokens */
    uint32_t max_count;                    /* 1 for binary, >1 for counting */
    TCB_t* wait_queue[MAX_TASKS];
    uint8_t wait_count;
} os_semaphore_t;

/* --- Critical sections ---
 * Save/restore PRIMASK instead of blindly re-enabling: these nest (the queue
 * takes a mutex inside a semaphore path) and they run from ISRs too, where an
 * unconditional __enable_irq() would drop interrupt masking on the floor. */
static inline uint32_t os_enter_critical(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    return primask;
}

static inline void os_exit_critical(uint32_t primask)
{
    __set_PRIMASK(primask);
}

/* Request a context switch. PendSV runs at the lowest priority, so this takes
 * effect as soon as the current ISR (if any) returns. */
static inline void os_yield(void)
{
    SCB->ICSR = SCB_ICSR_PENDSVSET_Msk;
    __DSB();
    __ISB();
}

/* --- Public Kernel API --- */

/** Initializes kernel bookkeeping. Call before creating tasks. */
void os_kernel_init(void);

/** Starts SysTick and launches the highest-priority task. Never returns. */
void os_kernel_launch(void);

/** Assembly entry that switches to PSP and runs the first task (os_kernel_asm.s). */
void os_start_first_task(void);

/** Context switch handler (os_kernel_asm.s). */
void PendSV_Handler(void);

/**
 * @brief Registers a task.
 * @param taskptr    Task entry point.
 * @param stackBase  START (lowest address) of the task's stack array.
 * @param stackWords Size of that array in 32-bit words.
 * @param priority   Higher number = higher priority. Idle should be 0.
 */
bool os_task_create(void (*taskptr)(void), uint32_t *stackBase, uint32_t stackWords, uint8_t priority);

/** Picks the next task to run. Called from PendSV. */
void os_scheduler(void);

/** Milliseconds since os_kernel_launch(). */
uint32_t os_get_ticks(void);

/** Called when a task stack canary is clobbered. Override to report it. */
void os_stack_overflow_hook(TCB_t *task);

/* Mutex API. timeout is in ticks; 0 polls, WAIT_FOREVER blocks indefinitely. */
void    os_mutex_init(os_mutex_t* mutex);
uint8_t os_mutex_acquire(os_mutex_t* mutex, uint32_t timeout);
void    os_mutex_release(os_mutex_t* mutex);

/* Semaphore API */
void    os_semaphore_init(os_semaphore_t* sem, uint32_t initial, uint32_t max_count);
uint8_t os_semaphore_acquire(os_semaphore_t* sem, uint32_t timeout);
void    os_semaphore_release(os_semaphore_t* sem);

void os_delay(uint32_t ms);

/** Power-on self-test of the kernel primitives. Call after uart_init(). */
void os_run_post(void);

#endif /* OS_KERNEL_H */
