#include "microbench.h"
#include "os_queue.h"

/*
 * Every number here is a raw core-cycle count from the DWT counter, which ticks
 * once per CPU clock and costs nothing to read. At 16MHz, 16 cycles = 1us.
 *
 * Measurements are averaged over BENCH_ITERATIONS runs so that a single
 * mispredicted branch or flash wait state does not become the headline number,
 * and the empty-loop overhead is subtracted so what is reported is the kernel
 * operation, not the measurement harness.
 */
#define CYCLES_PER_US (SYS_CLK_HZ / 1000000U)

static uint32_t measure_overhead_cycles(void)
{
    uint32_t start = DWT->CYCCNT;
    for (uint32_t i = 0; i < BENCH_ITERATIONS; i++) { __NOP(); }
    return (DWT->CYCCNT - start) / BENCH_ITERATIONS;
}

static void report(const char *name, uint32_t cycles)
{
    uart_print("[Bench] ");
    uart_print(name);
    uart_print(": ");
    uart_print_number(cycles);
    uart_print(" cycles (");
    uart_print_number(cycles / CYCLES_PER_US);
    uart_print(".");
    uart_print_number(((cycles % CYCLES_PER_US) * 10U) / CYCLES_PER_US);
    uart_print(" us)\r\n");
}

static os_semaphore_t ping_sem;
static os_semaphore_t pong_sem;

void microbench_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

    /* Init here, not in bench_ping_task: pong may be scheduled first. */
    os_semaphore_init(&ping_sem, 0, 1);
    os_semaphore_init(&pong_sem, 0, 1);
}

void bench_mutex_overhead(void)
{
    os_mutex_t m;
    os_mutex_init(&m);

    uint32_t start = DWT->CYCCNT;
    for (uint32_t i = 0; i < BENCH_ITERATIONS; i++)
    {
        os_mutex_acquire(&m, 0);
        os_mutex_release(&m);
    }
    uint32_t cycles = (DWT->CYCCNT - start) / BENCH_ITERATIONS;

    report("Mutex acquire+release (uncontested)", cycles - measure_overhead_cycles());
}

void bench_semaphore_overhead(void)
{
    os_semaphore_t s;
    os_semaphore_init(&s, 1, 1);

    uint32_t start = DWT->CYCCNT;
    for (uint32_t i = 0; i < BENCH_ITERATIONS; i++)
    {
        os_semaphore_acquire(&s, 0);
        os_semaphore_release(&s);
    }
    uint32_t cycles = (DWT->CYCCNT - start) / BENCH_ITERATIONS;

    report("Semaphore take+give (uncontested)", cycles - measure_overhead_cycles());
}

void bench_queue_roundtrip(void)
{
    os_message_queue_t q;
    uint32_t buf[8];
    uint32_t out = 0;

    os_queue_init(&q, buf, 8);

    uint32_t start = DWT->CYCCNT;
    for (uint32_t i = 0; i < BENCH_ITERATIONS; i++)
    {
        os_queue_send(&q, i, 0);
        os_queue_receive(&q, &out, 0);
    }
    uint32_t cycles = (DWT->CYCCNT - start) / BENCH_ITERATIONS;

    report("Queue send+receive (1 word)", cycles - measure_overhead_cycles());
}

void bench_scheduler_decision(void)
{
    /* Cost of picking the next task, i.e. the part of a context switch that is
       O(tasks) and would grow with the task count. */
    uint32_t start = DWT->CYCCNT;
    for (uint32_t i = 0; i < BENCH_ITERATIONS; i++)
    {
        os_scheduler();
    }
    uint32_t cycles = (DWT->CYCCNT - start) / BENCH_ITERATIONS;

    report("Scheduler decision", cycles - measure_overhead_cycles());
}

void microbench_run_all(void)
{
    uart_print("\r\n--- SENTINEL MICROBENCH START ---\r\n");

    bench_mutex_overhead();
    bench_semaphore_overhead();
    bench_queue_roundtrip();
    bench_scheduler_decision();

    uart_print("--- BENCHMARK COMPLETE ---\r\n\r\n");
}

/* --- Context switch ping-pong ---
 * Two equal-priority tasks bounce a pair of binary semaphores. One round trip
 * is: give, block, switch to pong, give back, block, switch back. That is two
 * full PendSV context switches plus four semaphore operations, so the per-switch
 * figure below subtracts the already-measured uncontested semaphore cost.
 * This is the same shape as the standard FreeRTOS xSemaphoreGive/Take ping-pong,
 * which is what makes the two numbers comparable. */

void bench_ping_task(void)
{
    os_delay(100);   /* Let the system settle and pong reach its blocking point */

    uint32_t start = DWT->CYCCNT;
    for (uint32_t i = 0; i < BENCH_ITERATIONS; i++)
    {
        os_semaphore_release(&pong_sem);
        os_semaphore_acquire(&ping_sem, WAIT_FOREVER);
    }
    uint32_t round_trip = (DWT->CYCCNT - start) / BENCH_ITERATIONS;

    report("Ping-pong round trip (2 switches)", round_trip);
    report("Context switch (approx, round trip / 2)", round_trip / 2);

    for (;;) os_delay(1000);
}

void bench_pong_task(void)
{
    for (;;)
    {
        os_semaphore_acquire(&pong_sem, WAIT_FOREVER);
        os_semaphore_release(&ping_sem);
    }
}
