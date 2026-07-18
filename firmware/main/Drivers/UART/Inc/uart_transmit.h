#pragma once

#include <cstdint>

#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_uart.h"

namespace UART {
    enum TRANSMIT_MODE {
        /** @brief Used for priority transmission, blocks CPU until end of message */
        BLOCKING,
        /** @brief Used for non-priority transmission, does NOT block CPU during transmission */
        INTERRUPT
    };

    /** @brief UART port definition class */
    class UART_PORT {
    private:
        UART_HandleTypeDef _huart;  // in stm32f1xx_hal_uart.h, same with its Init struct

        GPIO_TypeDef* _tx_port;
        uint16_t _tx_pin;

        GPIO_TypeDef* _rx_port;
        uint16_t _rx_pin;

        uint32_t _baud_rate;

        /** @brief Enables the peripheral clocks and configures the TX and RX pins */
        bool _setupHardwareInstance();

    public:

        UART_PORT(GPIO_TypeDef* tx_port, uint16_t tx_pin,
                  GPIO_TypeDef* rx_port, uint16_t rx_pin,
                  uint32_t baud = 115200);

        /**
         * @brief Configures the clocks, the pins and the USART1. Uses 8N1 settings.
         *
         * @return true if HAL_UART_Init() returned HAL_OK, false otherwise
         */
        bool init();

        void setBaudRate(uint32_t baud);
        uint32_t getBaudRate() const;
        UART_HandleTypeDef* getHandle();
    };

    /** @brief UART device class */
    class UART_DEVICE {
    private:
        UART_PORT _port;

    public:


        UART_DEVICE (
            GPIO_TypeDef* tx_port, uint16_t tx_pin, 
            GPIO_TypeDef* rx_port, uint16_t rx_pin, 
            uint32_t baud = 115200
        ) : _port(tx_port, tx_pin, rx_port, rx_pin, baud) {};

        /**
         * @brief Initializes the UART device
         * 
         * @return true if port succeeded, false otherwise
         */
        bool init() { return _port.init(); };

        /**
         * @brief Prints a message to the UART device. INTRERUPT mode is not finished
         * 
         * @param msg pointer to a string, the message to be printed
         * @param mode the mode of transmission, either BLOCKING (default, CPU will wait until the message is fully transmitted) or INTERRUPT (CPU will not wait for the message to be fully transmitted)
         * 
         * @return true if HAL transmit functions succeeded, false otherwise
         */
        bool print(const char* msg, TRANSMIT_MODE mode = BLOCKING);

        /**
         * @brief Prints a message to the UART device, followed by a newline character. INTERRUPT mode is not finished
         * 
         * @param msg pointer to a string, the message to be printed
         * @param mode the mode of transmission, either BLOCKING (default, CPU will wait until the message is fully transmitted) or INTERRUPT (CPU will not wait for the message to be fully transmitted)
         * 
         * @return true if HAL transmit functions succeeded, false otherwise
         */
        bool println(const char* msg, TRANSMIT_MODE mode = BLOCKING);

        // TODO: add bool write(const void* buf, size_t nbyte)
    };
}