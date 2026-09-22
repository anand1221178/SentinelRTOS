#ifndef __SPI_H__
#define __SPI_H__

#include "stm32f4xx.h"

/**
 * @brief SPI1 master on PB3 (SCK), PB4 (MISO), PB5 (MOSI), software chip select on PB6.
 * @param baud_div SPI_CR1 BR field (0 = fPCLK/2 ... 7 = fPCLK/256).
 * @param mode     SPI mode 0-3 (CPOL/CPHA pair).
 */
void spi_init(uint8_t baud_div, uint8_t mode);

/** Full-duplex byte exchange: writes tx, returns whatever the slave clocked back. */
uint8_t spi_transfer(uint8_t tx);

void spi_write(const uint8_t *data, uint32_t len);
void spi_read(uint8_t *data, uint32_t len);

/** Software chip select on PB6 (active low). */
void spi_cs_select(void);
void spi_cs_deselect(void);

#endif
