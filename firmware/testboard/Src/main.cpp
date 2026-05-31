#include "main.h"
#include "setup.h"

extern "C" int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();

    /*
        PB6 	- Button input
        PB7 	- GPIO output
        PA4..7 	- SPI
    */

    while (1) {
        
    }
}
