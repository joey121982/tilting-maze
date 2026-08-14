/*
 * Pure Manual Motor Control for STM32F103C8T6 (HAL)
 * Custom OUTx Pinout Mapping
 */
#ifndef TLE94112_MANUAL_H
#define TLE94112_MANUAL_H

#include "stm32f1xx_hal.h"

/* Ensure this matches your CubeMX generated SPI handle */
extern SPI_HandleTypeDef hspi1;

struct STM32_Pin {
    GPIO_TypeDef* port;
    uint16_t pin;
};

// Global Enable pin for the TLE94112 drivers
extern STM32_Pin DRIVER_EN;

// Register Addresses
#define HB_ACT_1_CTRL_ADDR    0b10000011
#define HB_ACT_2_CTRL_ADDR    0b11000011
#define HB_ACT_3_CTRL_ADDR    0b10100011
#define HB_MODE_1_CTRL_ADDR   0b11100011
#define HB_MODE_2_CTRL_ADDR   0b10010011
#define HB_MODE_3_CTRL_ADDR   0b11010011
#define PWM_CH_FREQ_CTRL_ADDR 0b10110011
#define PWM1_DC_CTRL_ADDR     0b11110011
#define PWM2_DC_CTRL_ADDR     0b10001011
#define PWM3_DC_CTRL_ADDR     0b11001011
#define FW_OL_CTRL_ADDR       0b10101011
#define FW_CTRL_ADDR          0b11101011
#define CONFIG_CTRL_ADDR      0b11100111

/* 
 * STATIC POLARITY (HB_ACT_x)
 * Defines which pins act as High-Side (10) and Low-Side (01).
 * HB_ACT_1 maps HB1..4, HB_ACT_2 maps HB5..8, HB_ACT_3 maps HB9..12
 */
#define HB_ACT_1_CTRL_DATA    0b01100101 // HB4(LS), HB3(HS), HB2(LS), HB1(LS)
#define HB_ACT_2_CTRL_DATA    0b10011010 // HB8(HS), HB7(LS), HB6(HS), HB5(HS)
#define HB_ACT_3_CTRL_DATA    0b01100110 // HB12(LS), HB11(HS), HB10(LS), HB9(HS)

/* PWM SETTINGS */
#define PWM_CH_FREQ_CTRL_DATA 0b00111111 // PWM 1,2,3 at 200Hz
#define PWM1_DC_CTRL_DATA     0b10000000 // Duty Cycle 50%
#define PWM2_DC_CTRL_DATA     0b10000000
#define PWM3_DC_CTRL_DATA     0b10000000
#define FW_OL_CTRL_DATA       0b01001011 // Active freewheeling on LS (HB1,2,4,7)
#define FW_CTRL_DATA          0b00001010 // Active freewheeling on LS (HB10,12)
#define CLEAR_REG_DATA        0b00000000

/* 
 * DIRECTIONAL MASKS FOR COIL B
 * UP assigns PWM1 (01), DOWN assigns PWM3 (11)
 */
// Motor 1 (Coil B = HB6 & HB7 in MODE_2)
#define MASK_M1_DATA_2_UP     0b00010100
#define MASK_M1_DATA_2_DOWN   0b00111100

// Motor 2 (Coil B = HB1 in MODE_1, HB5 in MODE_2)
#define MASK_M2_DATA_1_UP     0b00000001
#define MASK_M2_DATA_1_DOWN   0b00000011
#define MASK_M2_DATA_2_UP     0b00000001
#define MASK_M2_DATA_2_DOWN   0b00000011

// Motor 3 (Coil B = HB11 & HB12 in MODE_3)
#define MASK_M3_DATA_3_UP     0b01010000
#define MASK_M3_DATA_3_DOWN   0b11110000

enum movement { DOWN, UP, KEEP };

class MOTOR {
public:
    movement motor_drive;

    MOTOR() : motor_drive(KEEP) {}

    void set_direction(movement mov) {
        motor_drive = mov;
    }
};

class TLE94112 {
public:
    const STM32_Pin *CS_PIN;
    MOTOR M1, M2, M3;

    TLE94112() {}

    ~TLE94112() {
    }

    void spi_transmit_16(uint16_t data) {
        uint8_t tx_data[2] = { (uint8_t)(data >> 8), (uint8_t)(data & 0xFF) };
        HAL_SPI_Transmit(&hspi1, tx_data, 2, HAL_MAX_DELAY);
    }

    void delay_us(uint32_t us) {
        uint32_t count = us * (SystemCoreClock / 1000000) / 4;
        while(count--) { __NOP(); }
    }

    void init(const STM32_Pin *CS) {
        CS_PIN = CS;

        // 1. PWM FREQ SET TO 0Hz
        uint16_t cmd_PWM_1 = (PWM_CH_FREQ_CTRL_ADDR << 8) + CLEAR_REG_DATA;
        HAL_GPIO_WritePin(CS_PIN->port, CS_PIN->pin, GPIO_PIN_RESET);
        spi_transmit_16(cmd_PWM_1);
        HAL_GPIO_WritePin(CS_PIN->port, CS_PIN->pin, GPIO_PIN_SET);

        // 2. FREEWHEELING ENABLED
        uint16_t cmd_FW_1 = (FW_OL_CTRL_ADDR << 8) + FW_OL_CTRL_DATA;
        uint16_t cmd_FW_2 = (FW_CTRL_ADDR << 8) + FW_CTRL_DATA;
        HAL_GPIO_WritePin(CS_PIN->port, CS_PIN->pin, GPIO_PIN_RESET);
        spi_transmit_16(cmd_FW_1);
        spi_transmit_16(cmd_FW_2);
        HAL_GPIO_WritePin(CS_PIN->port, CS_PIN->pin, GPIO_PIN_SET);

        // 3. PWM CONFIG
        uint16_t cmd_PWM_DC_1 = (PWM1_DC_CTRL_ADDR << 8) + PWM1_DC_CTRL_DATA;
        uint16_t cmd_PWM_DC_2 = (PWM2_DC_CTRL_ADDR << 8) + PWM2_DC_CTRL_DATA;
        uint16_t cmd_PWM_DC_3 = (PWM3_DC_CTRL_ADDR << 8) + PWM3_DC_CTRL_DATA;
        HAL_GPIO_WritePin(CS_PIN->port, CS_PIN->pin, GPIO_PIN_RESET);
        spi_transmit_16(cmd_PWM_DC_1);
        spi_transmit_16(cmd_PWM_DC_2);
        spi_transmit_16(cmd_PWM_DC_3);
        HAL_GPIO_WritePin(CS_PIN->port, CS_PIN->pin, GPIO_PIN_SET);

        // 4. HB SETUP
        set_hb();
    }

    void set_hb() { 
        uint16_t cmd_HB4_1 = (HB_ACT_1_CTRL_ADDR << 8) + HB_ACT_1_CTRL_DATA;
        uint16_t cmd_HB8_5 = (HB_ACT_2_CTRL_ADDR << 8) + HB_ACT_2_CTRL_DATA;
        uint16_t cmd_HB12_9 = (HB_ACT_3_CTRL_ADDR << 8) + HB_ACT_3_CTRL_DATA;
        
        HAL_GPIO_WritePin(CS_PIN->port, CS_PIN->pin, GPIO_PIN_RESET);
        spi_transmit_16(cmd_HB4_1);
        spi_transmit_16(cmd_HB8_5);
        spi_transmit_16(cmd_HB12_9);
        HAL_GPIO_WritePin(CS_PIN->port, CS_PIN->pin, GPIO_PIN_SET);
    }

    void apply_directions() { 
        // Base states: Coil A bridges default to PWM2 (10), Coil B defaults to OFF (00)
        uint8_t HB_MODE_1_DATA = 0b10101000; // HB4(10), HB3(10), HB2(10), HB1(00)
        uint8_t HB_MODE_2_DATA = 0b10000000; // HB8(10), HB7(00), HB6(00), HB5(00)
        uint8_t HB_MODE_3_DATA = 0b00001010; // HB12(00), HB11(00), HB10(10), HB9(10)

        // Apply Motor 1 Directions
        if (M1.motor_drive == UP) {
            HB_MODE_2_DATA |= MASK_M1_DATA_2_UP;
        } else if (M1.motor_drive == DOWN) {
            HB_MODE_2_DATA |= MASK_M1_DATA_2_DOWN;
        }

        // Apply Motor 2 Directions
        if (M2.motor_drive == UP) {
            HB_MODE_1_DATA |= MASK_M2_DATA_1_UP;
            HB_MODE_2_DATA |= MASK_M2_DATA_2_UP;
        } else if (M2.motor_drive == DOWN) {
            HB_MODE_1_DATA |= MASK_M2_DATA_1_DOWN;
            HB_MODE_2_DATA |= MASK_M2_DATA_2_DOWN;
        }

        // Apply Motor 3 Directions
        if (M3.motor_drive == UP) {
            HB_MODE_3_DATA |= MASK_M3_DATA_3_UP;
        } else if (M3.motor_drive == DOWN) {
            HB_MODE_3_DATA |= MASK_M3_DATA_3_DOWN;
        }

        uint16_t cmd_HB_MODE_1 = (HB_MODE_1_CTRL_ADDR << 8) + HB_MODE_1_DATA;
        uint16_t cmd_HB_MODE_2 = (HB_MODE_2_CTRL_ADDR << 8) + HB_MODE_2_DATA;
        uint16_t cmd_HB_MODE_3 = (HB_MODE_3_CTRL_ADDR << 8) + HB_MODE_3_DATA;

        HAL_GPIO_WritePin(CS_PIN->port, CS_PIN->pin, GPIO_PIN_RESET);
        spi_transmit_16(cmd_HB_MODE_1);
        spi_transmit_16(cmd_HB_MODE_2);
        spi_transmit_16(cmd_HB_MODE_3);
        HAL_GPIO_WritePin(CS_PIN->port, CS_PIN->pin, GPIO_PIN_SET);
    }

    void start() { 
        uint8_t PHASE_I_PWM_DATA = 0b00000011;
        uint8_t PHASE_II_PWM_DATA = 0b00001111;
        uint8_t PHASE_III_PWM_DATA = 0b00111111;
        
        uint16_t cmd_PWM_FRQ_1 = (PWM_CH_FREQ_CTRL_ADDR << 8) + PHASE_I_PWM_DATA;
        uint16_t cmd_PWM_FRQ_2 = (PWM_CH_FREQ_CTRL_ADDR << 8) + PHASE_II_PWM_DATA;
        uint16_t cmd_PWM_FRQ_3 = (PWM_CH_FREQ_CTRL_ADDR << 8) + PHASE_III_PWM_DATA;

        HAL_GPIO_WritePin(CS_PIN->port, CS_PIN->pin, GPIO_PIN_SET);
        spi_transmit_16(cmd_PWM_FRQ_1);
        delay_us(2500);
        spi_transmit_16(cmd_PWM_FRQ_2);
        delay_us(2500);
        spi_transmit_16(cmd_PWM_FRQ_3);
        HAL_GPIO_WritePin(CS_PIN->port, CS_PIN->pin, GPIO_PIN_RESET);
    }

    void stop() { 
        uint16_t cmd_PWM_FRQ = (PWM_CH_FREQ_CTRL_ADDR << 8) + CLEAR_REG_DATA;
        HAL_GPIO_WritePin(CS_PIN->port, CS_PIN->pin, GPIO_PIN_SET);
        spi_transmit_16(cmd_PWM_FRQ);
        HAL_GPIO_WritePin(CS_PIN->port, CS_PIN->pin, GPIO_PIN_RESET);
    }

    /* 
     * Convenience method to execute a manual movement for all 3 motors 
     * for a specific number of milliseconds, then safely stop them.
     */
    void step_motors(uint32_t duration_ms) {
        apply_directions();
        start();
        HAL_Delay(duration_ms);
        stop();
        
        // Safety reset
        M1.set_direction(KEEP);
        M2.set_direction(KEEP);
        M3.set_direction(KEEP);
    }
};

#endif // TLE94112_MANUAL_H