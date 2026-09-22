#ifndef __PWM_H__
#define __PWM_H__

#include "stm32f4xx.h"

/**
 * @brief Sets up TIM3_CH1 on PA6 as a 1us-resolution PWM output.
 * @param period_us Frame length, e.g. 20000 for the 50Hz an RC servo expects.
 */
void pwm_init(uint32_t period_us);

/** Sets the high time of the pulse in microseconds. Clamped to the period. */
void pwm_set_pulse_us(uint32_t pulse_us);

#endif
