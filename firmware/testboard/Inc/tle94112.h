#ifndef TLE94112_H
#define TLE94112_H

#include "stm32f1xx_hal.h"

extern SPI_HandleTypeDef hspi1;
#define TLE_CS_PORT GPIOA
#define TLE_CS_PIN  GPIO_PIN_4

typedef enum {
    COIL_OFF     = 0,
    COIL_FORWARD = 1,
    COIL_REVERSE = 2
} TLE_CoilState_t;

/* Flexible mapping struct for any arbitrary wiring */
typedef struct {
    uint8_t out_A_plus;   // TLE OUT number (1-12)
    uint8_t out_A_minus;  // TLE OUT number (1-12)
    uint8_t out_B_plus;   // TLE OUT number (1-12)
    uint8_t out_B_minus;  // TLE OUT number (1-12)
    uint8_t step_index;   // Internal state tracking
} TLE_Actuator_t;

/* Function Prototypes */
void TLE94112_Init(void);
void TLE94112_SetHalfBridge(uint8_t out_num, uint8_t state);
void TLE94112_SetActuatorCoils(TLE_Actuator_t *act, TLE_CoilState_t coilA, TLE_CoilState_t coilB);
void TLE94112_StepActuator(TLE_Actuator_t *act, int8_t direction);

#endif /* TLE94112_H */