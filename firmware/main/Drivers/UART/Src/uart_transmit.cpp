#include <cstring>

#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_uart.h"

#include "uart_transmit.h"

namespace UART {

    // --------- UART_PORT ---------

    bool UART_PORT::_setupHardwareInstance() {              // followed setup.c structure for pin and GPIO initialization
        __HAL_RCC_GPIOA_CLK_ENABLE();                       // both pins are on pin A, so this line enables both
        __HAL_RCC_USART1_CLK_ENABLE();                      // turn on uart1

        GPIO_InitTypeDef GPIO_InitStruct = {0};

        GPIO_InitStruct.Pin = _tx_pin;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;             // pulls for peripheral device
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

        HAL_GPIO_Init(_tx_port, &GPIO_InitStruct);          // configured the data in silicon, we can reuse the same struct

        GPIO_InitStruct.Pin = _rx_pin;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_NOPULL;                 // pullup resistor to be physically implemented on PCB
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

        HAL_GPIO_Init(_rx_port, &GPIO_InitStruct);

        return true;
    }

    UART_PORT::UART_PORT(
        GPIO_TypeDef* tx_port, uint16_t tx_pin, 
        GPIO_TypeDef* rx_port, uint16_t rx_pin, 
        uint32_t baud
    ) {
        _huart = {0}; // avoid garbage
        _tx_port = tx_port;
        _tx_pin = tx_pin;
        _rx_port = rx_port;
        _rx_pin = rx_pin;
        _baud_rate = baud;
    }

    bool UART_PORT::init() {
        _setupHardwareInstance();

        _huart.Instance = USART1; // USART1 is a pointer to the hardware REGISTERS for the peripheral, so it CAN go in the *Instance field of the struct
        _huart.Init.BaudRate        = _baud_rate;
        _huart.Init.WordLength      = UART_WORDLENGTH_8B;   // or UART_WORDLENGTH_9B if we use parity
        _huart.Init.Parity          = UART_PARITY_NONE;     // from what I understand, the 8 bit mode is pretty reliable, we dont need the checksum
        _huart.Init.StopBits        = UART_STOPBITS_1;      // 1 bit is the standard
        _huart.Init.Mode            = UART_MODE_TX_RX;      // duplex
        _huart.Init.HwFlowCtl       = UART_HWCONTROL_NONE;
        _huart.Init.OverSampling    = UART_OVERSAMPLING_16; // mentioned in stm32f1xx_hal_uart.h
                                                            /*!< Specifies whether the Over sampling 8 is enabled or disabled, to achieve higher speed (up to fPCLK/8).
                                                            // This parameter can be a value of @ref UART_Over_Sampling. This feature is only available
                                                            // on STM32F100xx family, so OverSampling parameter should always be set to 16. */
        return HAL_OK == HAL_UART_Init(&_huart);
    }

    void UART_PORT::setBaudRate(uint32_t baud) {
        _baud_rate = baud;
        _huart.Init.BaudRate = baud;
        HAL_UART_Init(&_huart);                             // it is necessary to reapply the new value to the silicon
    }

    uint32_t UART_PORT::getBaudRate() const {
        return _baud_rate;
    }

    UART_HandleTypeDef* UART_PORT::getHandle() {
        return &_huart;
    }
    
    // --------- UART_DEVICE ---------

    bool UART_DEVICE::print(const char* msg, TRANSMIT_MODE mode) {
        UART_HandleTypeDef *huart = _port.getHandle();
        uint16_t len = strlen(msg);

        int timeout = 1000;         // TODO: make this a parameter, or a class member, or something
        if (mode == BLOCKING) {
            return HAL_OK == HAL_UART_Transmit(huart, (const uint8_t*)msg, len, timeout);   // we transform the char pointer to a uint8_t according to the function definition in stm32f1xx_hal_uart.h
        } else if (mode == INTERRUPT) {
            return HAL_OK == HAL_UART_Transmit_IT(huart, (const uint8_t*)msg, len);         // TODO: implement the interrupt handler, change the doxygen comment 
        } else {
            return false;
        }
    }

    bool UART_DEVICE::println(const char* msg, TRANSMIT_MODE mode) {
        return print(msg, mode) && print("\r\n", mode);
    }
}