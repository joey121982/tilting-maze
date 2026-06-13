#include "drivers/stepper_motor.hpp"
#include "stm32f1xx_hal.h"

StepperMotor::StepperMotor(Tle94112 *drv,
                           Tle94112::HalfBridge hbA1, Tle94112::HalfBridge hbA2,
                           Tle94112::HalfBridge hbB1, Tle94112::HalfBridge hbB2)
    : mDrv(drv), mCurrent(POS_UNKNOWN), mNext(POS_UNKNOWN), mDrive(KEEP)
{
    mCoilA[0] = hbA1; mCoilA[1] = hbA2;
    mCoilB[0] = hbB1; mCoilB[1] = hbB2;
}

void StepperMotor::configure()
{
    deEnergize();
    mDrv->setPwmDuty(Tle94112::TLE_PWM1, 0x80);
    mDrv->setPwmDuty(Tle94112::TLE_PWM2, 0x80);
    mDrv->setPwmDuty(Tle94112::TLE_PWM3, 0x80);
}

void StepperMotor::deEnergize()
{
    mDrv->configHB(mCoilA[0], Tle94112::TLE_FLOATING, Tle94112::TLE_NOPWM);
    mDrv->configHB(mCoilA[1], Tle94112::TLE_FLOATING, Tle94112::TLE_NOPWM);
    mDrv->configHB(mCoilB[0], Tle94112::TLE_FLOATING, Tle94112::TLE_NOPWM);
    mDrv->configHB(mCoilB[1], Tle94112::TLE_FLOATING, Tle94112::TLE_NOPWM);
}

void StepperMotor::applyDirection(Movement dir)
{
    if (dir == KEEP) return;

    Tle94112::PWMChannel bChan = (dir == MOVE_UP) ? Tle94112::TLE_PWM3 : Tle94112::TLE_PWM1;

    mDrv->configHB(mCoilA[0], Tle94112::TLE_HIGH, Tle94112::TLE_PWM2);
    mDrv->configHB(mCoilA[1], Tle94112::TLE_LOW,  Tle94112::TLE_PWM2);
    mDrv->configHB(mCoilB[0], Tle94112::TLE_HIGH, bChan);
    mDrv->configHB(mCoilB[1], Tle94112::TLE_LOW,  bChan);
}

void StepperMotor::start()
{
    mDrv->setPwmFreqRaw(0x03); HAL_Delay(3);
    mDrv->setPwmFreqRaw(0x0F); HAL_Delay(3);
    mDrv->setPwmFreqRaw(0x3F);
}

void StepperMotor::stop()
{
    mDrv->setPwmFreqRaw(0x00);
}

void StepperMotor::driveUp()   { applyDirection(MOVE_UP);   start(); }
void StepperMotor::driveDown() { applyDirection(MOVE_DOWN); start(); }

void StepperMotor::halt()
{
    stop();
    deEnergize();
}

void StepperMotor::updatePosition(Position target)
{
    mNext = target;
    if (mCurrent == POS_UNKNOWN || mCurrent == target) {
        mDrive = KEEP;
    } else if (target == POS_UP) {
        mDrive = MOVE_UP;
    } else {
        mDrive = MOVE_DOWN;
    }
}

void StepperMotor::step(uint16_t ms)
{
    if (mDrive == KEEP) return;
    applyDirection(mDrive);
    start();
    HAL_Delay(ms);
    stop();
    deEnergize();
    mCurrent = mNext;
    mNext = POS_UNKNOWN;
    mDrive = KEEP;
}

void StepperMotor::home()
{
    /* mergi pana in capatul de sus (2 increceri), apoi un pas jos -> referinta "jos". */
    mDrive = MOVE_UP;
    applyDirection(mDrive); start(); HAL_Delay(20); stop();
    applyDirection(mDrive); start(); HAL_Delay(20); stop();
    mDrive = MOVE_DOWN;
    applyDirection(mDrive); start(); HAL_Delay(20); stop();
    deEnergize();
    mCurrent = POS_DOWN;
    mNext = POS_UNKNOWN;
    mDrive = KEEP;
}
