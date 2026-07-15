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
        UART_HandleTypeDef _huart;

        GPIO_TypeDef* _tx_port;
        uint16_t _tx_pin;

        GPIO_TypeDef* _rx_port;
        uint16_t _rx_pin;

        uint32_t _baud_rate;

        bool _setupHardwareInstance();

    public:
        // TODO: add missing doxygen comments

        UART_PORT(GPIO_TypeDef* tx_port, uint16_t tx_pin, 
                  GPIO_TypeDef* rx_port, uint16_t rx_pin, 
                  uint32_t baud = 115200);

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
        // TODO: add missing doxygen comments

        UART_DEVICE (
            GPIO_TypeDef* tx_port, uint16_t tx_pin, 
            GPIO_TypeDef* rx_port, uint16_t rx_pin, 
            uint32_t baud = 115200
        ) : _port(tx_port, tx_pin, rx_port, rx_pin, baud) {};

        /**
         * @brief 
         * 
         * @return true 
         * @return false 
         */
        bool init() { return _port.init(); };

        /**
         * @brief 
         * 
         * @param msg 
         * @param mode 
         * 
         * @return true 
         * @return false 
         */
        bool print(const char* msg, TRANSMIT_MODE mode = BLOCKING);

        /**
         * @brief 
         * 
         * @param msg 
         * @param mode 
         * 
         * @return true 
         * @return false 
         */
        bool println(const char* msg, TRANSMIT_MODE mode = BLOCKING);
    };
}