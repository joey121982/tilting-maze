#include "drivers/spi1.hpp"
#include "stm32f1xx_hal.h"

void Spi1::init()
{
    RCC->APB2ENR |= RCC_APB2ENR_SPI1EN;

    /* configuram registrul de control cat timp periferia e dezactivata */
    SPI1->CR1 = 0u;
    SPI1->CR2 = 0u;

    SPI1->CR1 = SPI_CR1_MSTR /* master */
              | SPI_CR1_CPHA /* CPHA=1; CPOL ramane 0 -> SPI Mode 1 (cerut de TLE) */
              | SPI_CR1_LSBFIRST /* LSB first */
              | SPI_CR1_SSM /* software slave management */
              | SPI_CR1_SSI; /* tine NSS-ul intern sus */
    /* BR=000 -> fPCLK2/2 = 4 MHz ; DFF=0 -> cadru 8 biti. */

    SPI1->CR1 |= SPI_CR1_SPE;  /* abia acum activam periferia */
}

uint8_t Spi1::transfer(uint8_t out)
{
    while ((SPI1->SR & SPI_SR_TXE) == 0u) { }
    *(volatile uint8_t *)&SPI1->DR = out;

    while ((SPI1->SR & SPI_SR_RXNE) == 0u) { }
    return *(volatile uint8_t *)&SPI1->DR;
}
