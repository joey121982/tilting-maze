#pragma once

#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_uart.h"

namespace UART {
    enum TRANSMIT_MODE {
        /** @brief Used for priority transmission, blocks CPU until end of message */
        BLOCKING,
        /** @brief Used for non-priority transmission, does NOT block CPU during transmission */
        INTERRUPT,
        /** @brief Used for offloaded transmission, uses DMA instead of CPU */
        DMA
    };

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

        /**
         * @brief Construct a new uart port object
         * 
         * @param tx_port 
         * @param tx_pin 
         * @param rx_port 
         * @param rx_pin 
         * @param baud 
         */
        UART_PORT(GPIO_TypeDef* tx_port, uint16_t tx_pin, 
                  GPIO_TypeDef* rx_port, uint16_t rx_pin, 
                  uint32_t baud = 115200);

        /**
         * @brief 
         * 
         * @return true 
         * @return false 
         */
        bool init();
        
        /**
         * @brief Set the Baud Rate object
         */
        void setBaudRate(uint32_t baud);

        /**
         * @brief Get the Baud Rate object
         */
        uint32_t getBaudRate() const;

        /**
         * @brief Get the (mutable reference to) Handle object
         */
        UART_HandleTypeDef* getHandle();

        friend bool print(const char* msg, TRANSMIT_MODE mode, UART_PORT& port);
        friend bool println(const char* msg, TRANSMIT_MODE mode, UART_PORT& port);
    };

    /**
     * @brief 
     * 
     * @param msg 
     * @param mode 
     * @param port 
     * 
     * @return true 
     * @return false 
     */
    bool print(const char* msg, TRANSMIT_MODE mode, UART_PORT& port);

    /**
     * @brief 
     * 
     * @param msg 
     * @param mode 
     * @param port 
     * 
     * @return true 
     * @return false 
     */
    bool println(const char* msg, TRANSMIT_MODE mode, UART_PORT& port);
}