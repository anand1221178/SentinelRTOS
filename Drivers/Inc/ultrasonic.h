#ifndef __ULTRASONIC_H__
#define __ULTRASONIC_H__

#include "stm32f4xx.h"
#include <stdint.h>

#define TRIG_PIN    0   /* PA0 */
#define ECHO_PIN    1   /* PA1, TIM2_CH2 input capture */

#define ULTRASONIC_NO_ECHO 0xFFFFFFFF

/** TIM2 free-running at 1MHz with both-edge capture on the echo pin. */
void ultrasonic_init(void);

/** Fires the 10us trigger pulse. The echo semaphore is posted from the capture ISR. */
void ultrasonic_trigger(void);

/**
 * @brief Blocking measurement.
 * @param timeout_ms How long to wait for the echo.
 * @return Distance in cm, or ULTRASONIC_NO_ECHO if nothing came back.
 */
uint32_t ultrasonic_read_cm(uint32_t timeout_ms);

#endif
