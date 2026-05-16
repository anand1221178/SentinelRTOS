#ifndef __ULTRASONIC_H__
#define __ULTRASONIC_H__

#include "stm32f4xx.h"
#include <stdint.h>

#define TRIG_PIN    0   /* PA0 */
#define ECHO_PIN    1   /* PA1 */

void ultrasonic_init(void);

#endif
