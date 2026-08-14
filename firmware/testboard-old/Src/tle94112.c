#include "tle94112.h"

/* Correct 5-bit Addresses (A[6:2]) derived directly from Datasheet Figure 25 */
#define TLE_REG_HB_ACT_1_CTRL  0x00  // Control for HB 1-4
#define TLE_REG_HB_ACT_2_CTRL  0x10  // Control for HB 5-8
#define TLE_REG_HB_ACT_3_CTRL  0x08  // Control for HB 9-12

#define HB_STATE_HIGH_Z 0x00 
#define HB_STATE_LOW    0x01 
#define HB_STATE_HIGH   0x02 

/* Shadow registers to track the state of all 12 half-bridges */
static uint8_t shadow_regs[3] = {0x00, 0x00, 0x00};

/* Bipolar Full-Step Sequence */
static const TLE_CoilState_t step_seq_A[4] = {COIL_FORWARD, COIL_REVERSE, COIL_REVERSE, COIL_FORWARD};
static const TLE_CoilState_t step_seq_B[4] = {COIL_FORWARD, COIL_FORWARD, COIL_REVERSE, COIL_REVERSE};

static void TLE94112_WriteReg(uint8_t regAddr, uint8_t data) {
    uint8_t txData[2];
    
    // 100% Datasheet Accurate Infineon SPI Formatting for LSB-First:
    // Bit 7: OP=1 (Write)
    // Bits 6-2: 5-bit Register Address 
    // Bit 1: LABT=1 (Required for non-daisy chain)
    // Bit 0: LSB=1 
    txData[0] = 0x80 | (regAddr << 2) | 0x03; 
    
    // The actual command data
    txData[1] = data;
    
    // 1. Pull CS Low
    HAL_GPIO_WritePin(TLE_CS_PORT, TLE_CS_PIN, GPIO_PIN_RESET);
    
    // 2. TIMING PAD: t_lead is 250ns minimum. At 72MHz, 50 loops is ~700ns.
    for(volatile int i=0; i<50; i++) { __NOP(); }
    
    // 3. Transmit the 2 bytes (Hardware will send LSB of txData[0] first)
    HAL_SPI_Transmit(&hspi1, txData, 2, HAL_MAX_DELAY);
    
    // 4. TIMING PAD: t_lag is 250ns minimum.
    for(volatile int i=0; i<50; i++) { __NOP(); }
    
    // 5. Pull CS High to execute the command
    HAL_GPIO_WritePin(TLE_CS_PORT, TLE_CS_PIN, GPIO_PIN_SET);
}

void TLE94112_Init(void) {
    // Note: The device wakes up automatically when EN is High.
    // We explicitly clear all Half-Bridges to ensure a clean slate.
    TLE94112_WriteReg(TLE_REG_HB_ACT_1_CTRL, 0x00); 
    TLE94112_WriteReg(TLE_REG_HB_ACT_2_CTRL, 0x00); 
    TLE94112_WriteReg(TLE_REG_HB_ACT_3_CTRL, 0x00); 
    HAL_Delay(10);
}

/* Sets a single specific output (1-12) to High, Low, or High-Z */
void TLE94112_SetHalfBridge(uint8_t out_num, uint8_t state) {
    if (out_num < 1 || out_num > 12) return;
    
    // Calculate which register (0, 1, or 2) and which bits (0, 2, 4, or 6)
    uint8_t reg_index = (out_num - 1) / 4; 
    uint8_t bit_shift = ((out_num - 1) % 4) * 2;
    
    // Clear the two bits for this output, then set the new state
    shadow_regs[reg_index] &= ~(0x03 << bit_shift);
    shadow_regs[reg_index] |= (state << bit_shift);
    
    // Map to the non-sequential hardware addresses
    uint8_t hardware_reg;
    switch(reg_index) {
        case 0: hardware_reg = TLE_REG_HB_ACT_1_CTRL; break;
        case 1: hardware_reg = TLE_REG_HB_ACT_2_CTRL; break;
        case 2: hardware_reg = TLE_REG_HB_ACT_3_CTRL; break;
        default: return;
    }
    
    TLE94112_WriteReg(hardware_reg, shadow_regs[reg_index]);
}

static void SetCoil(uint8_t out_plus, uint8_t out_minus, TLE_CoilState_t state) {
    if (state == COIL_FORWARD) {
        TLE94112_SetHalfBridge(out_plus, HB_STATE_HIGH);
        TLE94112_SetHalfBridge(out_minus, HB_STATE_LOW);
    } else if (state == COIL_REVERSE) {
        TLE94112_SetHalfBridge(out_plus, HB_STATE_LOW);
        TLE94112_SetHalfBridge(out_minus, HB_STATE_HIGH);
    } else {
        TLE94112_SetHalfBridge(out_plus, HB_STATE_HIGH_Z);
        TLE94112_SetHalfBridge(out_minus, HB_STATE_HIGH_Z);
    }
}

void TLE94112_SetActuatorCoils(TLE_Actuator_t *act, TLE_CoilState_t coilA, TLE_CoilState_t coilB) {
    SetCoil(act->out_A_plus, act->out_A_minus, coilA);
    SetCoil(act->out_B_plus, act->out_B_minus, coilB);
}

void TLE94112_StepActuator(TLE_Actuator_t *act, int8_t direction) {
    if (direction > 0) {
        act->step_index = (act->step_index + 1) % 4;
    } else if (direction < 0) {
        act->step_index = (act->step_index == 0) ? 3 : act->step_index - 1;
    } else {
        return; 
    }
    
    TLE_CoilState_t stateA = step_seq_A[act->step_index];
    TLE_CoilState_t stateB = step_seq_B[act->step_index];
    
    TLE94112_SetActuatorCoils(act, stateA, stateB);
}