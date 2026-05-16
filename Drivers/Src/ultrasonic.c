#include "ultrasonic.h"
#include "os_kernel.h"

extern os_semaphore_t echo_ready;
extern volatile uint32_t time_start;
extern volatile uint32_t time_end;

void ultrasonic_init(void)
{
    /* 1. Enable clocks (GPIOA + TIM2) */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;

    /* 2. Configure PA0 as GPIO output (trigger pin) */
    GPIOA->MODER |=  (1U << 0);    /* Bit 0 = 1 */
    GPIOA->MODER &= ~(1U << 1);    /* Bit 1 = 0  → Mode 01 = General purpose output */

    /* 3. Configure PA1 as alternate function (AF1 = TIM2_CH2) */
    GPIOA->MODER &= ~(1U << 2);    /* Bit 2 = 0 */
    GPIOA->MODER |=  (1U << 3);    /* Bit 3 = 1  → Mode 10 = Alternate function */
    GPIOA->AFR[0] |=  (1U << 4);   /* AFRL1[3:0] = 0001 = AF1 (TIM2) */
    GPIOA->AFR[0] &= ~(1U << 5);
    GPIOA->AFR[0] &= ~(1U << 6);
    GPIOA->AFR[0] &= ~(1U << 7);

    /* 4. Configure TIM2: PSC = 15 (16MHz/16 = 1MHz, 1 tick = 1us), ARR = max (free-run) */
    TIM2->PSC = 15;
    TIM2->ARR = 0xFFFFFFFF;

    /* 5. Configure TIM2 Channel 2 for input capture (CC2S bits 9:8 = 01 → IC2 mapped to TI2) */
    TIM2->CCMR1 |=  (1U << 8);     /* Bit 8 = 1 */
    TIM2->CCMR1 &= ~(1U << 9);     /* Bit 9 = 0 */

    /* 6. Configure edge detection: both edges, enable capture */
    TIM2->CCER |=  (1U << 5);      /* CC2P  = 1 */
    TIM2->CCER |=  (1U << 7);      /* CC2NP = 1  → Both edges */
    TIM2->CCER |=  (1U << 4);      /* CC2E  = 1  → Enable capture on CH2 */

    /* 7. Enable capture interrupt (CC2IE = bit 2) */
    TIM2->DIER |= (1U << 2);

    /* 8. Enable TIM2 in NVIC */
    NVIC_EnableIRQ(TIM2_IRQn);

    /* 9. Start the timer (CEN = bit 0) */
    TIM2->CR1 |= (1U << 0);
}

static volatile uint8_t waiting_for_rising = 1;

void TIM2_IRQHandler(void)
{
    /* 1. Check if it was a capture event (SR: CC2IF) for channel 2 */
    if(!(TIM2->SR & (1U<<2))) { return; }

    /* 2. Read the captured value from CCR2 */
    uint32_t captured = TIM2->CCR2;

    /* 3. Use state tracking instead of reading the pin */
    if (waiting_for_rising)
    {
        /* First edge after trigger → rising edge */
        time_start = captured;
        waiting_for_rising = 0;
    }
    else
    {
        /* Second edge → falling edge */
        time_end = captured;
        waiting_for_rising = 1;
        os_semaphore_release(&echo_ready);
    }

    /* 4. Clear the interrupt flag */
    TIM2->SR &= ~(1U << 2);
}
