#include "tasks.h"
#include "uart.h"
#include "servo.h"
#include "ultrasonic.h"
#include "os_queue.h"

#define SWEEP_STEP_DEG   5
#define SWEEP_PERIOD_MS  20    /* 50Hz, one servo frame per step */
#define RADAR_PERIOD_MS  100   /* 10Hz; HC-SR04 needs ~60ms between pings */

/* UART is a single shared peripheral: without this, one task's line ends up
 * interleaved mid-word with another's. */
static void log_line(const char *label, uint32_t a, const char *label2, uint32_t b)
{
    os_mutex_acquire(&uart_lock, WAIT_FOREVER);
    uart_print(label);
    uart_print_number(a);
    if (label2 != NULL)
    {
        uart_print(label2);
        uart_print_number(b);
    }
    uart_print("\r\n");
    os_mutex_release(&uart_lock);
}

void os_idle_task(void)
{
    for (;;)
    {
        /* Lowest priority task, runs only when everything else is blocked.
           WFI parks the core until the next interrupt (SysTick at worst), which
           is free power saving; drop to __NOP() if the debugger loses the core. */
        __WFI();
    }
}

void sweep_task(void)
{
    /* Home gradually: slamming an SG90 across its full range on power-up pulls
       enough current to brown out the board. */
    os_mutex_acquire(&uart_lock, WAIT_FOREVER);
    uart_print("[Servo] Gradual homing sequence...\r\n");
    os_mutex_release(&uart_lock);

    for (int16_t a = 90; a >= 0; a -= SWEEP_STEP_DEG)
    {
        servo_set_angle((uint16_t)a);
        os_delay(50);
    }
    os_delay(500);

    int16_t angle = 0;
    int16_t step  = SWEEP_STEP_DEG;

    for (;;)
    {
        servo_set_angle((uint16_t)angle);

        /* Non-blocking: if the radar task is behind, the oldest angle gets
           overwritten rather than stalling the sweep. */
        os_queue_send(&sweep_queue, (uint32_t)angle, 0);

        angle += step;

        if (angle >= 180) { angle = 180; step = -SWEEP_STEP_DEG; }
        else if (angle <= 0) { angle = 0; step = SWEEP_STEP_DEG; }

        os_delay(SWEEP_PERIOD_MS);
    }
}

void radar_task(void)
{
    uint32_t angle = 0;

    for (;;)
    {
        uint32_t distance = ultrasonic_read_cm(60);

        if (distance != ULTRASONIC_NO_ECHO &&
            os_queue_receive(&sweep_queue, &angle, 0) == OS_SUCCESS)
        {
            log_line("[Radar] Angle: ", angle, " | Dist (cm): ", distance);
        }

        os_delay(RADAR_PERIOD_MS);
    }
}
