#ifndef DRIVERS_WALL_HPP
#define DRIVERS_WALL_HPP

#include "drivers/stepper_motor.hpp"

/*
 * Wall e doar un rename semantic, for scaling purposes, daca schimbam atctuatorul cu altceva.
 */
class Wall {
public:
    explicit Wall(StepperMotor *motor) : mMotor(motor) {}

    /* continuu */
    void raise() {
        mMotor->driveUp();
    }
    void lower() {
        mMotor->driveDown();
    }
    void hold() {
        mMotor->halt();
    }

    /* discret */
    void moveUp() {
        mMotor->updatePosition(StepperMotor::POS_UP);
        mMotor->step();
    }

    void moveDown() {
        mMotor->updatePosition(StepperMotor::POS_DOWN);
        mMotor->step();
    }

    void home() {
        mMotor->home();
    }

    bool isUp() const {
        return mMotor->position() == StepperMotor::POS_UP;
    }

private:
    StepperMotor *mMotor;
};

#endif
