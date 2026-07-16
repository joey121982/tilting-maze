#include "uart_transmit.h"

namespace UART {

    // --------- UART_PORT ---------

    bool UART_PORT::_setupHardwareInstance() {
        return false;
    }

    UART_PORT::UART_PORT(
        GPIO_TypeDef* tx_port, uint16_t tx_pin, 
        GPIO_TypeDef* rx_port, uint16_t rx_pin, 
        uint32_t baud = 115200
    ) {
        _tx_port = tx_port;
        _tx_pin = tx_pin;
        _rx_port = rx_port;
        _rx_pin = rx_pin;
        _baud_rate = baud;
    }

    bool UART_PORT::init() {
        return false;
    }

    void UART_PORT::setBaudRate(uint32_t baud) {

    }

    uint32_t UART_PORT::getBaudRate() const {
        return 0;
    }

    UART_HandleTypeDef* UART_PORT::getHandle() {
        return nullptr;
    }
    
    // --------- UART_DEVICE ---------

    bool UART_DEVICE::print(const char* msg, TRANSMIT_MODE mode) {
        return false;
    }

    bool UART_DEVICE::println(const char* msg, TRANSMIT_MODE mode) {
        return false;
    }
}