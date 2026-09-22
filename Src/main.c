#include "os_kernel.h"
#include "os_queue.h"
#include "tasks.h"
#include "uart.h"
#include "servo.h"
#include "ultrasonic.h"
#include "microbench.h"

/* Task stacks. 256 words = 1KB each; the kernel writes a canary at [0] and
   checks it on every context switch. */
static uint32_t idle_stack[STACK_SIZE];

#ifdef SENTINEL_BENCH
static uint32_t ping_stack[STACK_SIZE];
static uint32_t pong_stack[STACK_SIZE];
#else
static uint32_t sweep_stack[STACK_SIZE];
static uint32_t radar_stack[STACK_SIZE];
#endif

/* Shared resources */
os_message_queue_t sweep_queue;
static uint32_t sweep_queue_buffer[8];
os_semaphore_t echo_ready;
os_mutex_t uart_lock;

int main(void)
{
    os_kernel_init();
    uart_init();
    microbench_init();

    uart_print("--- SENTINEL RTOS BOOTING ---\r\n");

    os_run_post();

    /* Peripherals */
    servo_init();
    ultrasonic_init();

    /* Synchronization primitives. echo_ready is binary and starts empty: the
       capture ISR posts it when an echo comes back. */
    os_queue_init(&sweep_queue, sweep_queue_buffer, 8);
    os_semaphore_init(&echo_ready, 0, 1);
    os_mutex_init(&uart_lock);

    /* Higher number = higher priority. Idle must be strictly lowest. */
#ifdef SENTINEL_BENCH
    os_task_create(bench_ping_task, ping_stack, STACK_SIZE, 2);
    os_task_create(bench_pong_task, pong_stack, STACK_SIZE, 2);
#else
    os_task_create(radar_task, radar_stack, STACK_SIZE, 2);
    os_task_create(sweep_task, sweep_stack, STACK_SIZE, 1);
#endif
    os_task_create(os_idle_task, idle_stack, STACK_SIZE, 0);

    /* After task creation so the scheduler benchmark sees a realistic task set. */
    microbench_run_all();

    os_kernel_launch();

    for (;;) { }   /* Unreachable: os_kernel_launch() never returns */
}
