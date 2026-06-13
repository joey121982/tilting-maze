#include "drivers/tle94112.hpp"
#include "drivers/spi1.hpp"

enum {
    R_ACT1 = 0, R_ACT2, R_ACT3, /* stare HB1..4 / HB5..8 / HB9..12 */
    R_MODE1, R_MODE2, R_MODE3, /* mapare PWM pentru aceleasi grupuri */
    R_PWMFREQ, /* frecventa canalelor PWM1/2/3 */
    R_PWM1DC, R_PWM2DC, R_PWM3DC, /* duty cycle pe fiecare canal */
    R_FWOL, R_FWCTRL, /* open-load / active freewheeling */
    R_COUNT
};

static const uint8_t REG_ADDR[R_COUNT] = {
    0x03, 0x43, 0x23, /* HB_ACT_1/2/3 */
    0x63, 0x13, 0x53, /* HB_MODE_1/2/3 */
    0x33, /* PWM_CH_FREQ */
    0x73, 0x0B, 0x4B, /* PWM1/2/3_DC */
    0x2B, 0x6B /* FW_OL, FW_CTRL */
};

static const uint8_t ADDR_SYS_DIAG = 0x1B;

static const uint8_t HB_ACT_RI[13]  = { 0, R_ACT1,R_ACT1,R_ACT1,R_ACT1, R_ACT2,R_ACT2,R_ACT2,R_ACT2, R_ACT3,R_ACT3,R_ACT3,R_ACT3 };
static const uint8_t HB_MODE_RI[13] = { 0, R_MODE1,R_MODE1,R_MODE1,R_MODE1, R_MODE2,R_MODE2,R_MODE2,R_MODE2, R_MODE3,R_MODE3,R_MODE3,R_MODE3 };
static const uint8_t HB_SHIFT[13]   = { 0, 0,2,4,6, 0,2,4,6, 0,2,4,6 }; /* 2 biti / semi-punte */
static const uint8_t PWM_FREQ_SHIFT[4] = { 0, 0, 2, 4 };
static const uint8_t PWM_DC_RI[4]      = { 0, R_PWM1DC, R_PWM2DC, R_PWM3DC };


Tle94112::Tle94112(const Hw &hw) : mHw(hw)
{
    for (uint8_t i = 0; i < 12; i++) mShadow[i] = 0;
}

void Tle94112::csLow()  { HAL_GPIO_WritePin(mHw.csPort, mHw.csPin, GPIO_PIN_RESET); }
void Tle94112::csHigh() { HAL_GPIO_WritePin(mHw.csPort, mHw.csPin, GPIO_PIN_SET);   }

void Tle94112::begin()
{
    Spi1::init();

    HAL_GPIO_WritePin(mHw.enPort, mHw.enPin, GPIO_PIN_SET); /* EN sus = cip activ */
    csHigh(); /* CS inactiv */
    HAL_Delay(1);

    for (uint8_t i = 0; i < 12; i++) mShadow[i] = 0;

    setPwmFreqRaw(0x00);
    writeReg(R_FWOL,   0xFF, 0, 0x70);
    writeReg(R_FWCTRL, 0xFF, 0, 0x46);
    setPwmDuty(TLE_PWM1, 0x80); /* duty 50% */
    setPwmDuty(TLE_PWM2, 0x80);
    setPwmDuty(TLE_PWM3, 0x80);

    clearErrors(); /* sterge flag-ul de power-on-reset etc. */
}

void Tle94112::end()
{
    setPwmFreqRaw(0x00);
    writeReg(R_ACT1, 0xFF, 0, 0x00);
    writeReg(R_ACT2, 0xFF, 0, 0x00);
    writeReg(R_ACT3, 0xFF, 0, 0x00);
    HAL_GPIO_WritePin(mHw.enPort, mHw.enPin, GPIO_PIN_RESET);
}

void Tle94112::configHB(HalfBridge hb, HBState state, PWMChannel pwm)
{
    if (hb == TLE_NOHB) return;
    uint8_t sh = HB_SHIFT[hb];
    writeReg(HB_ACT_RI[hb],  (uint8_t)(0x03u << sh), sh, (uint8_t)state); /* HIGH/LOW/FLOAT */
    writeReg(HB_MODE_RI[hb], (uint8_t)(0x03u << sh), sh, (uint8_t)pwm); /* ce canal PWM */
}

void Tle94112::setPwmDuty(PWMChannel pwm, uint8_t duty)
{
    if (pwm == TLE_NOPWM) return;
    writeReg(PWM_DC_RI[pwm], 0xFF, 0, duty);
}

void Tle94112::setPwmFreq(PWMChannel pwm, PWMFreq freq)
{
    if (pwm == TLE_NOPWM) return;
    uint8_t sh = PWM_FREQ_SHIFT[pwm];
    writeReg(R_PWMFREQ, (uint8_t)(0x03u << sh), sh, (uint8_t)freq);
}

void Tle94112::setPwmFreqRaw(uint8_t freqWord)
{
    writeReg(R_PWMFREQ, 0xFF, 0, freqWord);
}

uint8_t Tle94112::getSysDiagnosis()
{
    return readReg(ADDR_SYS_DIAG);
}

void Tle94112::clearErrors()
{
    static const uint8_t errAddr[7] = { 0x1B, 0x5B, 0x3B, 0x7B, 0x07, 0x47, 0x27 };
    for (uint8_t i = 0; i < 7; i++) {
        csLow();
        Spi1::transfer((uint8_t)(errAddr[i] | 0x80u));
        Spi1::transfer(0x00u);
        csHigh();
    }
}

/* nivel jos */
void Tle94112::writeReg(uint8_t regIdx, uint8_t mask, uint8_t shift, uint8_t data)
{
    /* read-modify-write pe copia din RAM (registrele chip-ului sunt write-only) */
    uint8_t toWrite = (uint8_t)((mShadow[regIdx] & ~mask) | ((data << shift) & mask));
    mShadow[regIdx] = toWrite;

    csLow();
    Spi1::transfer((uint8_t)(REG_ADDR[regIdx] | 0x80u)); /* octet 0: adresa + bit7=WRITE */
    Spi1::transfer(toWrite); /* octet 1: datele */
    csHigh();
}

uint8_t Tle94112::readReg(uint8_t address)
{
    csLow();
    Spi1::transfer((uint8_t)(address & 0x7Fu));
    uint8_t v = Spi1::transfer(0xFFu);
    csHigh();
    return v;
}
