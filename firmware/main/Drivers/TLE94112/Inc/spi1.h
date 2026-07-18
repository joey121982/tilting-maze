#ifndef DRIVERS_SPI1_HPP
#define DRIVERS_SPI1_HPP

#include <stdint.h>

/*
 * Spi1 - driver minimal, la nivel de registru (CMSIS), pentru magistrala SPI a cipului TLE94112.
 * configuratie:
 *   - master
 *   - SPI Mode 1: CPOL=0, CPHA=1
 *   - LSB first, cadru de 8 biti
 *   - NSS software (CS-ul il controlam manual pe GPIO)
 *   - viteza = fPCLK2 / 2 = 8 MHz / 2 = 4 MHz (sub max de 5 MHz)
 */
namespace Spi1 {

    /** porneste ceasul spi, call once */
    void init();

    /**
     * schimb full-duplex de un octet (blocant).
     * @param out octetul trimis pe MOSI
     * @return octetul primit pe MISO in acelasi timp
     */
    uint8_t transfer(uint8_t out);

}

#endif
