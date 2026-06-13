#ifndef DRIVERS_STEPPER_MOTOR_HPP
#define DRIVERS_STEPPER_MOTOR_HPP

#include "drivers/tle94112.hpp"

/*
 * StepperMotor
 * doua moduri de folosire:
 *   1) continuu: driveUp() / driveDown() ... halt()
 *   2) discret: updatePosition() + step()
 */
class StepperMotor {
public:
    enum Position { POS_DOWN, POS_UP, POS_UNKNOWN };
    enum Movement { MOVE_DOWN, MOVE_UP, KEEP };

    /* coilA = {hbA1,hbA2}  (referinta),  coilB = {hbB1,hbB2}  (faza +/-90) */
    StepperMotor(Tle94112 *drv, Tle94112::HalfBridge hbA1, Tle94112::HalfBridge hbA2, Tle94112::HalfBridge hbB1, Tle94112::HalfBridge hbB2);
    void configure();

    /* mod continuu */
    void driveUp();
    void driveDown();
    void halt();

    /* mod discret */
    void updatePosition(Position target); /* calculeaza directia current -> target  */
    void step(uint16_t ms = 20);
    void home(); /* homing: sus, sus, jos -> current = down */

    Position position() const {
        return mCurrent;
    }

private:
    Tle94112 *mDrv;
    Tle94112::HalfBridge mCoilA[2];
    Tle94112::HalfBridge mCoilB[2];
    Position mCurrent;
    Position mNext;
    Movement mDrive;
    void applyDirection(Movement dir); /* leaga bobinele de canalele PWM (= set_PWM_to_HB) */
    void start(); /* soft-start in 3 faze (conform licentei) */
    void stop();
    void deEnergize();
};

#endif
