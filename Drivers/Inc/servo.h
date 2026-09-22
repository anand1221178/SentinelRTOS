#ifndef __SERVO_H__
#define __SERVO_H__

#include "stm32f4xx.h"

/* SG90 calibration. Datasheet says 1000-2000us over 0-180 degrees, but real
 * horns rarely hit the ends exactly; trim these two per unit rather than
 * scaling angles in the callers. */
#define SERVO_MIN_US 1000
#define SERVO_MAX_US 2000

void servo_init(void);
void servo_set_angle(uint16_t degrees);

#endif
