#include "pwm.h"
#include "gpio.h"

#define PWM_TIMER_CLK_HZ 16000000U   /* TIM3 on APB1, 16MHz HSI, no prescaler */

static uint32_t pwm_period_us = 20000;

void pwm_init(uint32_t period_us)
{
    pwm_period_us = period_us;

    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
    gpio_config(GPIOA, 6, GPIO_AF, 2, GPIO_NOPULL);   /* PA6 -> AF2 = TIM3_CH1 */

    /* 16MHz / (15+1) = 1MHz, so one tick is exactly 1us. */
    TIM3->PSC = (PWM_TIMER_CLK_HZ / 1000000U) - 1;
    TIM3->ARR = period_us - 1;

    /* PWM mode 1 (OC1M = 110) with preload, so duty updates take effect at the
       next update event instead of glitching mid-pulse. */
    TIM3->CCMR1 &= ~TIM_CCMR1_OC1M;
    TIM3->CCMR1 |= (TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1PE);
    TIM3->CR1   |= TIM_CR1_ARPE;
    TIM3->CCER  |= TIM_CCER_CC1E;

    TIM3->EGR |= TIM_EGR_UG;      /* Latch PSC/ARR */
    TIM3->CR1 |= TIM_CR1_CEN;
}

void pwm_set_pulse_us(uint32_t pulse_us)
{
    if (pulse_us > pwm_period_us) pulse_us = pwm_period_us;
    TIM3->CCR1 = pulse_us;
}
