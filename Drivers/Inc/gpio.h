#ifndef __GPIO_H__
#define __GPIO_H__

#include <stdbool.h>
#include "stm32f4xx.h"

typedef enum {
    GPIO_INPUT  = 0x0,
    GPIO_OUTPUT = 0x1,
    GPIO_AF     = 0x2,
    GPIO_ANALOG = 0x3
} gpio_mode_t;

typedef enum {
    GPIO_NOPULL = 0x0,
    GPIO_PULLUP = 0x1,
    GPIO_PULLDOWN = 0x2
} gpio_pull_t;

/** Enables the port clock and sets pin mode/pull. af is ignored unless mode is GPIO_AF. */
void gpio_config(GPIO_TypeDef *port, uint8_t pin, gpio_mode_t mode, uint8_t af, gpio_pull_t pull);

/** Open-drain + high speed, for the likes of I2C. */
void gpio_set_open_drain(GPIO_TypeDef *port, uint8_t pin);

void    gpio_write(GPIO_TypeDef *port, uint8_t pin, bool value);
void    gpio_toggle(GPIO_TypeDef *port, uint8_t pin);
bool    gpio_read(GPIO_TypeDef *port, uint8_t pin);

#endif
