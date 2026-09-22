#include "servo.h"
#include "pwm.h"

/*
 * An SG90 has no idea what a degree is: it only measures how long its control
 * line stays high in each 20ms frame. 1.0ms parks it at one end, 1.5ms centres
 * it, 2.0ms is the other end, so setting an angle is just picking a pulse width.
 */
void servo_init(void)
{
    pwm_init(20000);                                  /* 50Hz frame */
    pwm_set_pulse_us((SERVO_MIN_US + SERVO_MAX_US) / 2);  /* Centre */
}

void servo_set_angle(uint16_t degrees)
{
    if (degrees > 180) degrees = 180;

    uint32_t pulse_us = SERVO_MIN_US +
                        (((uint32_t)degrees * (SERVO_MAX_US - SERVO_MIN_US)) / 180U);

    pwm_set_pulse_us(pulse_us);
}
