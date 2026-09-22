#ifndef __ADC_H__
#define __ADC_H__

#include "stm32f4xx.h"

/** ADC1, 12-bit, software triggered single conversions. */
void adc_init(void);

/** Configures the pin for the given ADC1 channel as analog input (port A channels 0-7). */
void adc_config_channel(uint8_t channel);

/** Blocking single conversion. Returns a 12-bit raw count (0-4095). */
uint16_t adc_read(uint8_t channel);

/** Converts a raw count to millivolts against the given reference. */
uint16_t adc_to_mv(uint16_t raw, uint16_t vref_mv);

#endif
