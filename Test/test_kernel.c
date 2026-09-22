/*
 * Host-side kernel tests: gcc Test/test_kernel.c Kernel/os_kernel.c Kernel/os_queue.c
 *
 * The scheduler, the timeout bookkeeping, the priority inheritance rules and
 * the queue are all plain C data-structure logic, so they can be driven on a
 * workstation without an STM32 attached. What cannot be tested here is the
 * assembly context switch and the peripheral registers: those need hardware.
 *
 * Context switching is simulated by calling os_scheduler() directly, which is
 * exactly what PendSV does between saving and restoring registers.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "os_kernel.h"
#include "os_queue.h"

/* Backing store for the stubbed registers */
static SCB_Type     scb_storage;
static SysTick_Type systick_storage;
static DBGMCU_Type  dbgmcu_storage;

SCB_Type     *SCB     = &scb_storage;
SysTick_Type *SysTick = &systick_storage;
DBGMCU_Type  *DBGMCU  = &dbgmcu_storage;
uint32_t      stub_primask = 0;

extern TCB_t os_tasks[MAX_TASKS];
extern TCB_t *current_tcb;

/* The real one is in assembly and never returns; on the host, launching just
   means "the kernel is now running", which is what unblocks blocking calls. */
void os_start_first_task(void) { }

void SysTick_Handler(void);   /* Defined in os_kernel.c; the tests drive it by hand */

void os_stack_overflow_hook(TCB_t *task) { (void)task; assert(0 && "stack overflow"); }

static uint32_t stacks[MAX_TASKS][STACK_SIZE];
static void dummy_task(void) { }

static bool pendsv_pending(void)
{
    bool p = (SCB->ICSR & SCB_ICSR_PENDSVSET_Msk) != 0;
    SCB->ICSR = 0;
    return p;
}

/* Fresh kernel with `n` tasks; priorities[i] applies to task i. */
static void setup(uint8_t n, const uint8_t *priorities)
{
    os_kernel_init();
    memset(os_tasks, 0, sizeof(os_tasks));
    current_tcb = NULL;

    for (uint8_t i = 0; i < n; i++)
    {
        assert(os_task_create(dummy_task, stacks[i], STACK_SIZE, priorities[i]));
    }

    os_kernel_launch();   /* Runs the scheduler, then returns via the stub above */
    SCB->ICSR = 0;
}

static void test_scheduler_picks_highest_priority(void)
{
    const uint8_t prios[] = {0, 3, 1};
    setup(3, prios);

    assert(current_tcb == &os_tasks[1]);
    assert(current_tcb->state == RUNNING);

    /* Blocking the top task must hand the CPU to the next highest, not to idle. */
    os_tasks[1].state = BLOCKED;
    os_scheduler();
    assert(current_tcb == &os_tasks[2]);

    /* With everything else blocked, the priority-0 idle task still runs. */
    os_tasks[2].state = BLOCKED;
    os_scheduler();
    assert(current_tcb == &os_tasks[0]);

    printf("  scheduler picks highest ready priority ... ok\n");
}

static void test_equal_priority_round_robins(void)
{
    const uint8_t prios[] = {2, 2, 2};
    setup(3, prios);

    /* Each scheduler pass must advance to a different task, and three passes
       must visit all three: otherwise equal-priority tasks starve. */
    TCB_t *seen[3];
    for (int i = 0; i < 3; i++)
    {
        seen[i] = current_tcb;
        os_scheduler();
        assert(current_tcb != seen[i]);
    }

    assert(seen[0] != seen[1] && seen[1] != seen[2] && seen[0] != seen[2]);
    printf("  equal priorities round-robin ... ok\n");
}

static void test_tick_preempts_and_wakes_sleepers(void)
{
    const uint8_t prios[] = {0, 1};
    setup(2, prios);
    assert(current_tcb == &os_tasks[1]);

    os_delay(3);
    assert(current_tcb->state == BLOCKED);
    assert(pendsv_pending());          /* os_delay must request a switch */

    os_scheduler();
    assert(current_tcb == &os_tasks[0]);   /* Idle takes over */

    SysTick_Handler();
    SysTick_Handler();
    assert(os_tasks[1].state == BLOCKED);  /* Not yet: 3 ticks were asked for */

    SysTick_Handler();
    assert(os_tasks[1].state == READY);
    assert(pendsv_pending());          /* Every tick pends PendSV: preemptive */

    os_scheduler();
    assert(current_tcb == &os_tasks[1]);   /* Woken high-priority task preempts idle */

    printf("  tick preemption and sleep wakeup ... ok\n");
}

static void test_priority_inheritance(void)
{
    const uint8_t prios[] = {0, 1, 3};   /* idle, low, high */
    setup(3, prios);

    TCB_t *low  = &os_tasks[1];
    TCB_t *high = &os_tasks[2];

    os_mutex_t m;
    os_mutex_init(&m);

    /* Low priority task takes the lock */
    current_tcb = low;
    low->state = RUNNING;
    assert(os_mutex_acquire(&m, WAIT_FOREVER) == OS_SUCCESS);
    assert(m.owner == low);

    /* High priority task blocks on it and donates its priority */
    current_tcb = high;
    high->state = RUNNING;
    os_mutex_acquire(&m, WAIT_FOREVER);

    assert(high->state == BLOCKED);
    assert(m.wait_count == 1);
    assert(low->current_priority == 3);      /* Inherited */
    assert(low->base_priority == 1);         /* Base untouched */

    /* Without inheritance the scheduler would run a medium-priority task here
       instead of the lock holder, which is exactly the inversion this prevents. */
    os_scheduler();
    assert(current_tcb == low);

    /* Release hands the lock straight to the waiter and drops the donation */
    current_tcb = low;
    os_mutex_release(&m);

    assert(low->current_priority == 1);
    assert(m.owner == high);
    assert(m.lock == 1);                     /* Stays locked: direct handoff */
    assert(high->state == READY);
    assert(m.wait_count == 0);
    assert(high->wait_result == OS_SUCCESS);

    os_scheduler();
    assert(current_tcb == high);

    printf("  priority inheritance and handoff ... ok\n");
}

static void test_mutex_timeout(void)
{
    const uint8_t prios[] = {0, 1, 2};
    setup(3, prios);

    TCB_t *low  = &os_tasks[1];
    TCB_t *high = &os_tasks[2];

    os_mutex_t m;
    os_mutex_init(&m);

    current_tcb = low;
    assert(os_mutex_acquire(&m, 0) == OS_SUCCESS);

    /* Polling acquire on a held mutex must report the timeout, not block */
    current_tcb = high;
    assert(os_mutex_acquire(&m, 0) == OS_TIMEOUT);
    assert(m.wait_count == 0);

    /* Blocking acquire parks the task with a deadline */
    os_mutex_acquire(&m, 5);
    assert(high->state == BLOCKED);
    assert(high->sleep_time == 5);
    assert(m.wait_count == 1);

    for (int i = 0; i < 5; i++) SysTick_Handler();

    assert(high->state == READY);
    assert(high->wait_result == OS_TIMEOUT);
    assert(m.wait_count == 0);          /* Pulled out of the wait queue */
    assert(m.owner == low);             /* Ownership unaffected */

    printf("  mutex timeout releases the waiter ... ok\n");
}

static void test_semaphore_handoff(void)
{
    const uint8_t prios[] = {0, 1};
    setup(2, prios);

    TCB_t *t = &os_tasks[1];

    os_semaphore_t s;
    os_semaphore_init(&s, 0, 1);

    current_tcb = t;
    assert(os_semaphore_acquire(&s, 0) == OS_TIMEOUT);

    os_semaphore_acquire(&s, WAIT_FOREVER);
    assert(t->state == BLOCKED);
    assert(s.wait_count == 1);
    assert(t->sleep_time == 0);         /* WAIT_FOREVER: no deadline */

    os_semaphore_release(&s);
    assert(t->state == READY);
    assert(t->wait_result == OS_SUCCESS);
    assert(s.wait_count == 0);
    assert(s.count == 0);               /* Token passed straight to the waiter */

    /* Counting semaphores must saturate at max_count */
    os_semaphore_init(&s, 0, 2);
    os_semaphore_release(&s);
    os_semaphore_release(&s);
    os_semaphore_release(&s);
    assert(s.count == 2);

    printf("  semaphore blocking and handoff ... ok\n");
}

static void test_queue(void)
{
    const uint8_t prios[] = {0, 1};
    setup(2, prios);
    current_tcb = &os_tasks[1];

    os_message_queue_t q;
    uint32_t buffer[4];
    uint32_t out = 0;

    os_queue_init(&q, buffer, 4);

    assert(os_queue_receive(&q, &out, 0) == OS_TIMEOUT);

    for (uint32_t i = 1; i <= 4; i++) assert(os_queue_send(&q, i, 0) == OS_SUCCESS);
    for (uint32_t i = 1; i <= 4; i++)
    {
        assert(os_queue_receive(&q, &out, 0) == OS_SUCCESS);
        assert(out == i);
    }

    /* Full queue overwrites the oldest entry and keeps the ring consistent */
    for (uint32_t i = 1; i <= 6; i++) assert(os_queue_send(&q, i, 0) == OS_SUCCESS);

    for (uint32_t i = 3; i <= 6; i++)
    {
        assert(os_queue_receive(&q, &out, 0) == OS_SUCCESS);
        assert(out == i);
    }
    assert(os_queue_receive(&q, &out, 0) == OS_TIMEOUT);

    printf("  queue FIFO, overwrite and empty behaviour ... ok\n");
}

static void test_critical_sections_nest(void)
{
    const uint8_t prios[] = {0, 1};
    setup(2, prios);
    current_tcb = &os_tasks[1];

    os_message_queue_t q;
    uint32_t buffer[2];
    uint32_t out = 0;
    os_queue_init(&q, buffer, 2);

    /* A queue send takes a semaphore and a mutex, each with its own critical
       section. If those restored PRIMASK unconditionally, interrupts would come
       back on inside the outer one. */
    stub_primask = 1;
    os_queue_send(&q, 42, 0);
    assert(stub_primask == 1);

    os_queue_receive(&q, &out, 0);
    assert(stub_primask == 1);
    assert(out == 42);

    stub_primask = 0;
    os_queue_send(&q, 7, 0);
    assert(stub_primask == 0);

    printf("  critical sections nest without dropping masking ... ok\n");
}

int main(void)
{
    printf("SENTINEL kernel logic tests\n");

    test_scheduler_picks_highest_priority();
    test_equal_priority_round_robins();
    test_tick_preempts_and_wakes_sleepers();
    test_priority_inheritance();
    test_mutex_timeout();
    test_semaphore_handoff();
    test_queue();
    test_critical_sections_nest();

    printf("all tests passed\n");
    return 0;
}
