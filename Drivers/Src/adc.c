#include "adc.h"
#include "gpio.h"

void adc_init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

    ADC->CCR   = 0;                 /* PCLK2 / 2 */
    ADC1->CR1  = 0;                 /* 12-bit, no scan */
    ADC1->CR2  = 0;                 /* Single conversion, right aligned */
    ADC1->SQR1 = 0;                 /* One conversion in the regular sequence */

    ADC1->CR2 |= ADC_CR2_ADON;
}

void adc_config_channel(uint8_t channel)
{
    if (channel < 8) gpio_config(GPIOA, channel, GPIO_ANALOG, 0, GPIO_NOPULL);

    /* 84-cycle sample time: slow enough for the ~10-50k source impedance of a
       divider or potentiometer to settle onto the sampling capacitor. */
    if (channel < 10)
    {
        ADC1->SMPR2 &= ~(0x7U << (channel * 3));
        ADC1->SMPR2 |=  (0x4U << (channel * 3));
    }
    else
    {
        ADC1->SMPR1 &= ~(0x7U << ((channel - 10) * 3));
        ADC1->SMPR1 |=  (0x4U << ((channel - 10) * 3));
    }
}

uint16_t adc_read(uint8_t channel)
{
    ADC1->SQR3 = channel;           /* First (and only) conversion in the sequence */
    ADC1->SR  &= ~ADC_SR_EOC;
    ADC1->CR2 |= ADC_CR2_SWSTART;

    while (!(ADC1->SR & ADC_SR_EOC)) { }

    return (uint16_t)ADC1->DR;      /* Reading DR clears EOC */
}

uint16_t adc_to_mv(uint16_t raw, uint16_t vref_mv)
{
    return (uint16_t)(((uint32_t)raw * vref_mv) / 4095U);
}
