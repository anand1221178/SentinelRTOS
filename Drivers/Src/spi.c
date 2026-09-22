#include "spi.h"
#include "gpio.h"

#define SPI_CS_PIN 6   /* PB6 */

void spi_init(uint8_t baud_div, uint8_t mode)
{
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    /* SPI1 remapped onto port B: PA5-PA7 would collide with the servo PWM on PA6. */
    gpio_config(GPIOB, 3, GPIO_AF, 5, GPIO_NOPULL);   /* SCK  */
    gpio_config(GPIOB, 4, GPIO_AF, 5, GPIO_NOPULL);   /* MISO */
    gpio_config(GPIOB, 5, GPIO_AF, 5, GPIO_NOPULL);   /* MOSI */
    gpio_config(GPIOB, SPI_CS_PIN, GPIO_OUTPUT, 0, GPIO_NOPULL);
    spi_cs_deselect();

    SPI1->CR1 = 0;
    SPI1->CR1 |= ((uint32_t)(baud_div & 0x7U) << SPI_CR1_BR_Pos);
    if (mode & 0x1U) SPI1->CR1 |= SPI_CR1_CPHA;
    if (mode & 0x2U) SPI1->CR1 |= SPI_CR1_CPOL;

    /* Software slave management, otherwise a low NSS input would drop the
       peripheral out of master mode the moment it is enabled. */
    SPI1->CR1 |= (SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI);
    SPI1->CR1 |= SPI_CR1_SPE;
}

uint8_t spi_transfer(uint8_t tx)
{
    while (!(SPI1->SR & SPI_SR_TXE)) { }
    *(volatile uint8_t *)&SPI1->DR = tx;      /* 8-bit access: a 16-bit write would send two frames */

    while (!(SPI1->SR & SPI_SR_RXNE)) { }
    return *(volatile uint8_t *)&SPI1->DR;
}

void spi_write(const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) (void)spi_transfer(data[i]);
}

void spi_read(uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) data[i] = spi_transfer(0xFF);
}

void spi_cs_select(void)   { gpio_write(GPIOB, SPI_CS_PIN, false); }
void spi_cs_deselect(void) { gpio_write(GPIOB, SPI_CS_PIN, true);  }
