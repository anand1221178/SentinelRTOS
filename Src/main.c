#include <stdlib.h>
#include "os_task.h"
#include "lock.h"
#include "os_kernel.h"
#include "tasks.h"
#include "uart.h"
#include "Bench/microbench.h"
#include "Drivers/Inc/servo.h"
#include "os_queue.h"
#include "ultrasonic.h"

/* Task Stacks */
uint32_t servo_stack[256];
uint32_t radar_stack[256];
uint32_t idle_stack[256];

/* Queue Resources */
os_message_queue_t sweep_queue;
uint32_t sweep_queue_buffer[8];

/* Ultrasonic Resources */
os_semaphore_t echo_ready;
volatile uint32_t time_start = 0;
volatile uint32_t time_end = 0;

int main(void)
{
    /* Initialize Kernel */
    os_kernel_init();

    /* Initialize UART */
    uart_init();
    uart_print("--- SENTINEL RTOS BOOTING ---\r\n");

    /* Run Power-On Self-Tests */
    os_run_post();

    /* Initialize Peripherals */
    servo_init();
    
    /* Initialize Synchronization Primitives */
    os_queue_init(&sweep_queue, sweep_queue_buffer, 8);
    
    /* Binary semaphore for Ultrasonic (starts at 0) */
    echo_ready.count = 0;
    echo_ready.max_count = 1;
    echo_ready.wait_count = 0;

    /* INtialise ultrasonic sensor */
    ultrasonic_init();

    /* Create Tasks */
    os_task_create(sweep_task, servo_stack, 1);
    os_task_create(radar_task, radar_stack, 1);
    os_task_create(os_idle_task, idle_stack, 0);

    /* Launch the kernel */
    os_kernel_launch();

    while(1){}; /* DONT REACH HERE! */
}
