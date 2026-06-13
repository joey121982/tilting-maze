#include "drivers/button.hpp"

Button::Button(GPIO_TypeDef *port, uint16_t pin, bool activeHigh, uint32_t debounceMs)
    : mPort(port), mPin(pin), mActiveHigh(activeHigh), mDebounceMs(debounceMs),
      mRaw(false), mStable(false), mPrev(false), mLastChange(0)
{
}

void Button::update()
{
    mPrev = mStable;

    /* normalizata la true = apasat */
    bool raw = (HAL_GPIO_ReadPin(mPort, mPin) == GPIO_PIN_SET);
    if (!mActiveHigh) raw = !raw;

    if (raw != mRaw) {
        mRaw = raw;
        mLastChange = HAL_GetTick();
    }
    if ((HAL_GetTick() - mLastChange) >= mDebounceMs) {
        mStable = mRaw;
    }
}
