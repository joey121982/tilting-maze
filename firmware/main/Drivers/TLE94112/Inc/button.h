#ifndef DRIVERS_BUTTON_HPP
#define DRIVERS_BUTTON_HPP

#include <stdint.h>
#include "stm32f1xx_hal.h"

/*
 * buton cu debounce, non-blocant (cronometrat pe HAL_GetTick, in ms).
 * activeHigh = true -> apasat inseamna nivel logic 1 (buton spre VCC + pull-down).
 * activeHigh = false -> apasat inseamna nivel logic 0 (buton spre GND + pull-up).
 */
class Button {
public:
    Button(GPIO_TypeDef *port, uint16_t pin, bool activeHigh = true, uint32_t debounceMs = 15);

    void update();

    bool isPressed() const {
        return mStable;
    }

    bool justPressed() const {
        return mStable && !mPrev;
    }

    bool justReleased() const {
        return !mStable && mPrev;
    }

private:
    GPIO_TypeDef *mPort;
    uint16_t mPin;
    bool mActiveHigh;
    uint32_t mDebounceMs;
    bool mRaw; /* ultima citire bruta (deja normalizata: true = apasat) */
    bool mStable; /* starea stabila, dupa debounce */
    bool mPrev; /* starea stabila de la apelul update() anterior */
    uint32_t mLastChange; /* momentul ultimei schimbari a semnalului brut (tick ms) */
};

#endif
