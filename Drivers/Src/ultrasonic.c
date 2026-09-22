#include "ultrasonic.h"
#include "os_kernel.h"
#include "gpio.h"

/*
 * HC-SR04: a 10us trigger pulse makes the sensor emit 8 cycles at 40kHz and
 * hold ECHO high for as long as the sound is in flight. TIM2 counts at 1MHz and
 * latches CCR2 on both edges, so the pulse width in microseconds falls out of
 * the hardware with no polling and no jitter from task scheduling.
 *
 * Sound covers 343m/s, the pulse makes the round trip, so
 *   cm = us * 0.0343 / 2 = us / 58.
 */
#define US_PER_CM 58U

extern os_semaphore_t echo_ready;
volatile uint32_t time_start = 0;
volatile uint32_t time_end   = 0;

static volatile uint8_t waiting_for_rising = 1;

void ultrasonic_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    gpio_config(GPIOA, TRIG_PIN, GPIO_OUTPUT, 0, GPIO_NOPULL);
    gpio_config(GPIOA, ECHO_PIN, GPIO_AF, 1, GPIO_NOPULL);   /* AF1 = TIM2_CH2 */

    TIM2->PSC = 15;            /* 16MHz / 16 = 1MHz -> 1 tick = 1us */
    TIM2->ARR = 0xFFFFFFFF;    /* Free-running 32-bit counter */

    TIM2->CCMR1 &= ~TIM_CCMR1_CC2S;
    TIM2->CCMR1 |= TIM_CCMR1_CC2S_0;                        /* IC2 mapped to TI2 */

    TIM2->CCER |= (TIM_CCER_CC2P | TIM_CCER_CC2NP);         /* Capture on both edges */
    TIM2->CCER |= TIM_CCER_CC2E;

    TIM2->DIER |= TIM_DIER_CC2IE;
    NVIC_SetPriority(TIM2_IRQn, 5);
    NVIC_EnableIRQ(TIM2_IRQn);

    TIM2->CR1 |= TIM_CR1_CEN;
}

void ultrasonic_trigger(void)
{
    uint32_t primask = os_enter_critical();
    waiting_for_rising = 1;       /* Drop any half-finished capture from a timed-out read */
    os_exit_critical(primask);

    gpio_write(GPIOA, TRIG_PIN, true);

    /* 10us at 16MHz. Too short to be worth a timer, too short to block on. */
    for (volatile uint32_t i = 0; i < 40; i++) { __NOP(); }

    gpio_write(GPIOA, TRIG_PIN, false);
}

uint32_t ultrasonic_read_cm(uint32_t timeout_ms)
{
    ultrasonic_trigger();

    if (os_semaphore_acquire(&echo_ready, timeout_ms) != OS_SUCCESS)
    {
        return ULTRASONIC_NO_ECHO;
    }

    /* Unsigned subtraction handles the 32-bit counter wrapping mid-pulse. */
    return (time_end - time_start) / US_PER_CM;
}

void TIM2_IRQHandler(void)
{
    if (!(TIM2->SR & TIM_SR_CC2IF)) return;

    uint32_t captured = TIM2->CCR2;   /* Reading CCR2 clears CC2IF */

    if (waiting_for_rising)
    {
        time_start = captured;
        waiting_for_rising = 0;
    }
    else
    {
        time_end = captured;
        waiting_for_rising = 1;
        os_semaphore_release(&echo_ready);
    }
}
