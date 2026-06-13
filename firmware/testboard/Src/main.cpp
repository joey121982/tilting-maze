#include "main.h"
#include "setup.h"

#include "drivers/tle94112.hpp"
#include "drivers/stepper_motor.hpp"
#include "drivers/wall.hpp"
#include "drivers/button.hpp"

/*
 * buton tinut apasat (PB6) -> peretele se ridica
 * buton eliberat -> se opreste
 */
extern "C" void Error_Handler(void)
{
    __disable_irq();
    while (1) { }
}

extern "C" int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    Tle94112::Hw hw{ GPIOA, GPIO_PIN_4, GPIOB, GPIO_PIN_7 };
    Tle94112 tle(hw);
    tle.begin();

    StepperMotor motor(&tle,Tle94112::TLE_HB1, Tle94112::TLE_HB2, Tle94112::TLE_HB3, Tle94112::TLE_HB4);
    motor.configure();

    Wall wall(&motor);
    Button button(GPIOB, GPIO_PIN_6, true);

    wall.home(); /* referinta de pozitie la pornire */

    const uint32_t MAX_ON_MS = 3000u;

    const uint8_t FAULT_MASK = Tle94112::DIAG_TEMP_SHUTDOWN | Tle94112::DIAG_UNDER_VOLTAGE | Tle94112::DIAG_OVER_VOLTAGE;

    uint32_t pressStart = 0u;
    bool driving  = false;
    bool latchOff = false;

    while (1) {
        button.update();
        if (button.justReleased()) {
            latchOff = false;
            tle.clearErrors();
        }

        bool faulted = (tle.getSysDiagnosis() & FAULT_MASK) != 0u;
        if (driving && (HAL_GetTick() - pressStart) >= MAX_ON_MS) latchOff = true;
        if (faulted) latchOff = true;

        if (button.isPressed() && !latchOff) {
            if (!driving) {
                pressStart = HAL_GetTick();
                wall.raise();
                driving = true;
            }
        } else {
            if (driving) {
                wall.hold();
                driving = false;
            }
        }
    }
}
