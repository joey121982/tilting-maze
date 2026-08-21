#ifndef TLE94112_MANUAL_H
#define TLE94112_MANUAL_H

#include "stm32f1xx_hal.h"

extern SPI_HandleTypeDef hspi1;

struct STM32_Pin {
    GPIO_TypeDef* port;
    uint16_t pin;
};

// Global Enable pin for the TLE94112 drivers
extern STM32_Pin DRIVER_EN;

// Register Addresses
#define HB_ACT_1_CTRL_ADDR      0b10000011
#define HB_ACT_2_CTRL_ADDR      0b11000011
#define HB_ACT_3_CTRL_ADDR      0b10100011
#define HB_MODE_1_CTRL_ADDR     0b11100011
#define HB_MODE_2_CTRL_ADDR     0b10010011
#define HB_MODE_3_CTRL_ADDR     0b11010011
#define PWM_CH_FREQ_CTRL_ADDR   0b10110011
#define PWM1_DC_CTRL_ADDR       0b11110011
#define PWM2_DC_CTRL_ADDR       0b10001011
#define PWM3_DC_CTRL_ADDR       0b11001011
#define FW_OL_CTRL_ADDR         0b10101011
#define FW_CTRL_ADDR            0b11101011
#define CONFIG_CTRL_ADDR        0b01100111

// PWM SETTINGS
#define PWM_CH_FREQ_CTRL_DATA   0b00111111  // PWM 1,2,3 at 200Hz
#define PWM1_DC_CTRL_DATA       0b01000000 
#define PWM2_DC_CTRL_DATA       0b01000000
#define PWM3_DC_CTRL_DATA       0b01000000
#define FW_OL_CTRL_DATA         0b01001011  // Active freewheeling on LS (HB1,2,4,7)
#define FW_CTRL_DATA            0b00001010  // Active freewheeling on LS (HB10,12)
#define CLEAR_REG_DATA          0b00000000

enum movement { DOWN, UP, KEEP };

class MOTOR {
public:
    movement motor_drive;
    uint8_t current_phase;

    MOTOR() : motor_drive(KEEP) {}

    void set_direction(movement mov) {
        motor_drive = mov;
    }
};

class TLE94112 {
private:
    const STM32_Pin *CS_PIN;

public:
    MOTOR M1, M2, M3;

    TLE94112(const STM32_Pin *CS) { CS_PIN = CS; }

    ~TLE94112() {}

    void spi_transmit_16(uint16_t data) {
        uint8_t tx_data[2] = { (uint8_t)(data >> 8), (uint8_t)(data & 0xFF) };
        HAL_SPI_Transmit(&hspi1, tx_data, 2, HAL_MAX_DELAY);
    }

    void spi_transmit_16_cs(uint16_t data) {
        HAL_GPIO_WritePin(CS_PIN->port, CS_PIN->pin, GPIO_PIN_RESET);
        spi_transmit_16(data);
        HAL_GPIO_WritePin(CS_PIN->port, CS_PIN->pin, GPIO_PIN_SET);
        delay_us(1);
    }

    void delay_us(uint32_t us) {
        uint32_t count = us * (SystemCoreClock / 1000000) / 4;
        while(count--) { __NOP(); }
    }

    void init() {
        // 1. PWM FREQ SET TO 0Hz (Safe Startup)
        spi_transmit_16_cs((PWM_CH_FREQ_CTRL_ADDR << 8) | CLEAR_REG_DATA);

        // 2. FREEWHEELING ENABLED
        spi_transmit_16_cs((FW_OL_CTRL_ADDR << 8) | FW_OL_CTRL_DATA);
        spi_transmit_16_cs((FW_CTRL_ADDR << 8) | FW_CTRL_DATA);

        // 3. PWM CONFIG
        spi_transmit_16_cs((PWM1_DC_CTRL_ADDR << 8) | PWM1_DC_CTRL_DATA);
        spi_transmit_16_cs((PWM2_DC_CTRL_ADDR << 8) | PWM2_DC_CTRL_DATA);
        spi_transmit_16_cs((PWM3_DC_CTRL_ADDR << 8) | PWM3_DC_CTRL_DATA);

        // Ensure all bridges are forced OFF at startup
        stop();
    }

    void start() { 
        uint8_t PHASE_I_PWM_DATA = 0b00000011;
        uint8_t PHASE_II_PWM_DATA = 0b00001111;
        uint8_t PHASE_III_PWM_DATA = 0b00111111;
        
        spi_transmit_16_cs((PWM_CH_FREQ_CTRL_ADDR << 8) | PHASE_I_PWM_DATA);
        delay_us(2500);
        spi_transmit_16_cs((PWM_CH_FREQ_CTRL_ADDR << 8) | PHASE_II_PWM_DATA);
        delay_us(2500);
        spi_transmit_16_cs((PWM_CH_FREQ_CTRL_ADDR << 8) | PHASE_III_PWM_DATA);
    }

    void stop() { 
        // Kill PWM frequency
        spi_transmit_16_cs((PWM_CH_FREQ_CTRL_ADDR << 8) | CLEAR_REG_DATA);
        
        // Disconnect all half-bridges to prevent shorting coils
        spi_transmit_16_cs((HB_MODE_1_CTRL_ADDR << 8) | CLEAR_REG_DATA);
        spi_transmit_16_cs((HB_MODE_2_CTRL_ADDR << 8) | CLEAR_REG_DATA);
        spi_transmit_16_cs((HB_MODE_3_CTRL_ADDR << 8) | CLEAR_REG_DATA);
        delay_us(1);
    }

    void step_motors(uint32_t num_steps, uint32_t speed_delay_ms) {
        start();

        for (uint32_t step_count = 0; step_count < num_steps; step_count++) {
            uint8_t act_1_data = 0;
            uint8_t act_2_data = 0;
            uint8_t act_3_data = 0;

            uint8_t mode_1_data = 0;
            uint8_t mode_2_data = 0;
            uint8_t mode_3_data = 0;

            if (M2.motor_drive != KEEP) {
                
                // Enable PWM1 (01) for all 4 half-bridges attached to Motor 2
                mode_1_data |= 0b00000101; 
                mode_2_data |= 0b01000001;

                // Seamlessly advance the magnetic phase based on direction
                if (M2.motor_drive == UP) {
                    M2.current_phase = (M2.current_phase + 1) % 4;
                } else { // DOWN
                    M2.current_phase = (M2.current_phase == 0) ? 3 : (M2.current_phase - 1);
                }

                switch (M2.current_phase) {
                    case 0: // Step 1: Coil A (+), Coil B (+)
                        act_1_data |= 0b00000101; 
                        act_2_data |= 0b10000010; 
                        break;
                    case 1: // Step 2: Coil A (-), Coil B (+)
                        act_1_data |= 0b00001001; 
                        act_2_data |= 0b01000010; 
                        break;
                    case 2: // Step 3: Coil A (-), Coil B (-)
                        act_1_data |= 0b00001010; 
                        act_2_data |= 0b01000001; 
                        break;
                    case 3: // Step 4: Coil A (+), Coil B (-)
                        act_1_data |= 0b00000110; 
                        act_2_data |= 0b10000001; 
                        break;
                }
            }
            
            // TODO: add motor1 and motor3 control here

            // 1. Transmit the Polarity Configurations (Reverses the current direction)
            spi_transmit_16_cs((HB_ACT_1_CTRL_ADDR << 8) | act_1_data);
            spi_transmit_16_cs((HB_ACT_2_CTRL_ADDR << 8) | act_2_data);
            spi_transmit_16_cs((HB_ACT_3_CTRL_ADDR << 8) | act_3_data);

            // 2. Transmit the Enables (Turns the selected H-Bridges ON using PWM)
            spi_transmit_16_cs((HB_MODE_1_CTRL_ADDR << 8) | mode_1_data);
            spi_transmit_16_cs((HB_MODE_2_CTRL_ADDR << 8) | mode_2_data);
            spi_transmit_16_cs((HB_MODE_3_CTRL_ADDR << 8) | mode_3_data);

            // 3. Wait for the motor shaft to physically complete the step
            HAL_Delay(speed_delay_ms); 
        }
        
        // Ensure motor powers down safely when movement finishes
        stop();
        
        // Safety reset
        M1.set_direction(KEEP);
        M2.set_direction(KEEP);
        M3.set_direction(KEEP);
    }
};

#endif // TLE94112_MANUAL_H