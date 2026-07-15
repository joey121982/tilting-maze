#include "main.h"
#include "uart_transmit.h"

extern "C" int main() {
    sys_init();

    UART::UART_DEVICE pc_uart = UART::UART_DEVICE(GPIOA, GPIO_PIN_9, GPIOA, GPIO_PIN_10, 115200); 

    if (!pc_uart.init()) {
        // TODO: handle uart init error here
    }

    while (1)
    {
        pc_uart.print("Hello!");
    }
}
