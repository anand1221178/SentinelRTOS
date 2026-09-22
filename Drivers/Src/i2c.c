#include "i2c.h"
#include "gpio.h"

#define I2C_PCLK_MHZ   16U
#define I2C_TIMEOUT    100000U   /* Spin budget per flag; a stuck bus must not hang the task forever */

/* Every wait is bounded: an unpowered or held-low slave otherwise wedges the
   caller permanently, which on a preemptive kernel means a dead task. */
static uint8_t wait_flag(volatile uint32_t *reg, uint32_t mask, bool set)
{
    for (uint32_t i = 0; i < I2C_TIMEOUT; i++)
    {
        if ((((*reg) & mask) != 0) == set) return I2C_OK;
    }
    return I2C_ERROR;
}

void i2c_init(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    gpio_config(GPIOB, 8, GPIO_AF, 4, GPIO_PULLUP);   /* SCL */
    gpio_config(GPIOB, 9, GPIO_AF, 4, GPIO_PULLUP);   /* SDA */
    gpio_set_open_drain(GPIOB, 8);
    gpio_set_open_drain(GPIOB, 9);

    I2C1->CR1 = I2C_CR1_SWRST;    /* Clear any BUSY latched from a previous partial transfer */
    I2C1->CR1 = 0;

    I2C1->CR2   = I2C_PCLK_MHZ;                 /* Peripheral clock, in MHz */
    I2C1->CCR   = (I2C_PCLK_MHZ * 1000000U) / (2U * 100000U);  /* 100kHz standard mode */
    I2C1->TRISE = I2C_PCLK_MHZ + 1U;            /* 1000ns max rise time */

    I2C1->CR1 |= I2C_CR1_PE;
}

static uint8_t i2c_start(uint8_t addr, bool reading)
{
    I2C1->CR1 |= I2C_CR1_START;
    if (wait_flag(&I2C1->SR1, I2C_SR1_SB, true) != I2C_OK) return I2C_ERROR;

    I2C1->DR = (uint8_t)((addr << 1) | (reading ? 1U : 0U));
    if (wait_flag(&I2C1->SR1, I2C_SR1_ADDR, true) != I2C_OK)
    {
        I2C1->CR1 |= I2C_CR1_STOP;   /* NACK: release the bus instead of leaving it held */
        return I2C_ERROR;
    }

    (void)I2C1->SR1;                 /* Reading SR1 then SR2 clears ADDR */
    (void)I2C1->SR2;
    return I2C_OK;
}

uint8_t i2c_write(uint8_t addr, const uint8_t *data, uint32_t len)
{
    if (wait_flag(&I2C1->SR2, I2C_SR2_BUSY, false) != I2C_OK) return I2C_ERROR;
    if (i2c_start(addr, false) != I2C_OK) return I2C_ERROR;

    for (uint32_t i = 0; i < len; i++)
    {
        if (wait_flag(&I2C1->SR1, I2C_SR1_TXE, true) != I2C_OK) return I2C_ERROR;
        I2C1->DR = data[i];
    }

    if (wait_flag(&I2C1->SR1, I2C_SR1_BTF, true) != I2C_OK) return I2C_ERROR;
    I2C1->CR1 |= I2C_CR1_STOP;
    return I2C_OK;
}

uint8_t i2c_read(uint8_t addr, uint8_t *data, uint32_t len)
{
    if (len == 0) return I2C_OK;
    if (wait_flag(&I2C1->SR2, I2C_SR2_BUSY, false) != I2C_OK) return I2C_ERROR;

    I2C1->CR1 |= I2C_CR1_ACK;
    if (i2c_start(addr, true) != I2C_OK) return I2C_ERROR;

    for (uint32_t i = 0; i < len; i++)
    {
        if (i + 1 == len)
        {
            /* NACK the last byte and queue the STOP before reading it, or the
               slave clocks out one more byte we never asked for. */
            I2C1->CR1 &= ~I2C_CR1_ACK;
            I2C1->CR1 |= I2C_CR1_STOP;
        }

        if (wait_flag(&I2C1->SR1, I2C_SR1_RXNE, true) != I2C_OK) return I2C_ERROR;
        data[i] = (uint8_t)I2C1->DR;
    }

    return I2C_OK;
}

uint8_t i2c_read_reg(uint8_t addr, uint8_t reg, uint8_t *data, uint32_t len)
{
    if (wait_flag(&I2C1->SR2, I2C_SR2_BUSY, false) != I2C_OK) return I2C_ERROR;
    if (i2c_start(addr, false) != I2C_OK) return I2C_ERROR;

    if (wait_flag(&I2C1->SR1, I2C_SR1_TXE, true) != I2C_OK) return I2C_ERROR;
    I2C1->DR = reg;
    if (wait_flag(&I2C1->SR1, I2C_SR1_BTF, true) != I2C_OK) return I2C_ERROR;

    /* Repeated start: keep the bus so no other master can interleave. */
    return i2c_read(addr, data, len);
}
