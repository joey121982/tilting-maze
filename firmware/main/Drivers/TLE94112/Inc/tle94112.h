#ifndef DRIVERS_TLE94112_HPP
#define DRIVERS_TLE94112_HPP

#include <stdint.h>
#include "stm32f1xx_hal.h"

/*
 * Tle94112 driver pentru un singur cip TLE94112ES (12 semi-punti H), pe SPI.
 *
 * stratul asta NU stie de motor sau perete,
 * el doar pune semi-puntile in stari (HIGH / LOW / FLOATING) si le leaga de
 * canale PWM, scriind in registrele de control.
 */
class Tle94112 {
public:

    struct Hw {
        GPIO_TypeDef *csPort; uint16_t csPin; /* chip-select (NSS software), activ JOS */
        GPIO_TypeDef *enPort; uint16_t enPin; /* enable cip, activ SUS */
    };

    /* 12 semi-punti. */
    enum HalfBridge : uint8_t {
        TLE_NOHB = 0,
        TLE_HB1, /** = 1 */
        TLE_HB2, /** = 2 etc */
        TLE_HB3,
        TLE_HB4,
        TLE_HB5,
        TLE_HB6,
        TLE_HB7,
        TLE_HB8,
        TLE_HB9,
        TLE_HB10,
        TLE_HB11,
        TLE_HB12
    };

    /* 3 canale PWM interne (TLE_NOPWM = iesire DC). */
    enum PWMChannel : uint8_t {
        TLE_NOPWM = 0,
        TLE_PWM1, /** = 1 */
        TLE_PWM2,
        TLE_PWM3
    };

    /* starea semi-punte */
    enum HBState : uint8_t {
        TLE_FLOATING = 0b00,
        TLE_LOW = 0b01,
        TLE_HIGH = 0b10
    };

    /* frecv canal PWM (2 biti in registrul PWM_CH_FREQ). */
    enum PWMFreq : uint8_t {
        TLE_FREQ_OFF = 0b00,
        TLE_FREQ_80HZ = 0b01,
        TLE_FREQ_100HZ = 0b10,
        TLE_FREQ_200HZ = 0b11
    };

    /* flag-uri din SYS_DIAG (vezi getSysDiagnosis). */
    enum DiagFlag : uint8_t {
        DIAG_SPI_ERROR = 0x80,
        DIAG_LOAD_ERROR = 0x40,
        DIAG_UNDER_VOLTAGE = 0x20,
        DIAG_OVER_VOLTAGE = 0x10,
        DIAG_POWER_ON_RESET = 0x08,
        DIAG_TEMP_SHUTDOWN = 0x04,
        DIAG_TEMP_WARNING = 0x02
    };

    static const uint8_t STATUS_OK = 0u;

    explicit Tle94112(const Hw &hw);

    void begin(); /* porneste magistrala, sterge erorile */
    void end(); /* dezactiveaza cipul (EN=0) */

    /* semi-punte state set si o leaga de un canal */
    void configHB(HalfBridge hb, HBState state, PWMChannel pwm);
    void setPwmDuty(PWMChannel pwm, uint8_t duty);
    void setPwmFreq(PWMChannel pwm, PWMFreq freq);
    void setPwmFreqRaw(uint8_t freqWord);

    uint8_t getSysDiagnosis(); /* citeste SYS_DIAG; 0 = totul ok */
    void clearErrors(); /* scrie 0 in registrele de stare ca sa stearga flag-urile latched */

private:
    Hw mHw;
    uint8_t mShadow[12]; /* copie a celor 12 registre de control */
    void writeReg(uint8_t regIdx, uint8_t mask, uint8_t shift, uint8_t data);
    uint8_t readReg(uint8_t address);
    void csLow();
    void csHigh();
};

#endif
