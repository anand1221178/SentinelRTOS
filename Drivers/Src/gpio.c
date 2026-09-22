#include <stdbool.h>
#include "gpio.h"

/* Port clocks live in consecutive AHB1ENR bits, one per port, starting at GPIOA. */
static void gpio_clock_enable(GPIO_TypeDef *port)
{
    uint32_t index = ((uint32_t)port - GPIOA_BASE) / 0x400U;
    RCC->AHB1ENR |= (1U << index);
}

void gpio_config(GPIO_TypeDef *port, uint8_t pin, gpio_mode_t mode, uint8_t af, gpio_pull_t pull)
{
    gpio_clock_enable(port);

    port->MODER &= ~(0x3U << (pin * 2));
    port->MODER |=  ((uint32_t)mode << (pin * 2));

    port->PUPDR &= ~(0x3U << (pin * 2));
    port->PUPDR |=  ((uint32_t)pull << (pin * 2));

    if (mode == GPIO_AF)
    {
        /* AFR[0] covers pins 0-7, AFR[1] pins 8-15, 4 bits each. */
        uint8_t reg = pin >> 3;
        uint8_t shift = (pin & 0x7U) * 4;
        port->AFR[reg] &= ~(0xFU << shift);
        port->AFR[reg] |=  ((uint32_t)af << shift);
    }
}

void gpio_set_open_drain(GPIO_TypeDef *port, uint8_t pin)
{
    port->OTYPER  |= (1U << pin);
    port->OSPEEDR |= (0x3U << (pin * 2));
}

void gpio_write(GPIO_TypeDef *port, uint8_t pin, bool value)
{
    /* BSRR is atomic: no read-modify-write race against an ISR touching ODR. */
    port->BSRR = value ? (1U << pin) : (1U << (pin + 16));
}

void gpio_toggle(GPIO_TypeDef *port, uint8_t pin)
{
    port->ODR ^= (1U << pin);
}

bool gpio_read(GPIO_TypeDef *port, uint8_t pin)
{
    return (port->IDR & (1U << pin)) != 0;
}
