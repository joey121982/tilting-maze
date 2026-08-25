# The Tilting Maze Raspberry Pi Companion

A desktop window that listens to the board's NDJSON stream, draws the maze it generated,
animates the solution, and shows board health. It is a viewer first: the board is the
authority on what the maze is, and the companion never computes one itself.

## Running it

On a Pi or any Linux box:

    make run                       # pipx install -e . then launch
    pi-comp --port /dev/ttyUSB0

On Windows using WSL2, a COM port (such as PL2303 on `COM7`) is managed by the Windows host and is not directly accessible inside WSL2. Use `usbipd-win` to attach the physical USB device to WSL:

1. **Install `usbipd-win` on Windows** (Run in PowerShell):

       winget install dorssel.usbipd-win

2. **Identify your USB device BUSID** (Run in PowerShell):

       & "C:\Program Files\usbipd-win\usbipd.exe" list
       # Example output: 1-1  067b:2303  Prolific USB-to-Serial Comm Port (COM7)

3. **Bind the USB device (Run ONCE in PowerShell as Administrator)**:

       & "C:\Program Files\usbipd-win\usbipd.exe" bind --busid 1-1

4. **Attach the USB device to WSL** (Run in PowerShell whenever plugged in, or add `--auto-attach`):

       & "C:\Program Files\usbipd-win\usbipd.exe" attach --wsl --busid 1-1

5. **Grant permission and launch in WSL**:

       sudo chmod 666 /dev/ttyUSB0
       PYTHONPATH=src python3 -m pi_comp.main --port /dev/ttyUSB0

On Windows natively (without WSL):

    set PYTHONPATH=src
    python -m pi_comp.main --port COM7

Other modes, none of which need hardware:

    pi-comp --simulate             # drive the GUI from the host oracle
    pi-comp --serve-pty            # publish a fake board on a pty (Linux only)
    pi-comp --replay stream.ndjson # parse a capture, no GUI, exit status says if it parsed

`--simulate` and `--serve-pty` shell out to `tools/host_json_dump`, which is built by
`make oracle` and is a Linux binary. Both are Linux-only for that reason.

## The wire format

`src/pi_comp/protocol.py` is the single source of truth on this side, and it names the
firmware file each constant came from. Board → companion is one compact JSON object per
line, LF-terminated: `hello`, `maze`, `status`, `fault`, `log`.

## Board control

The companion can send two commands back to the board. They are plain text, one per line:

    new              generate a fresh maze, seeded the usual way
    seed 109214410   generate the maze for exactly this seed

Plain text rather than JSON on purpose. The firmware *writes* JSON with `putStr()` but has
no parser, and putting a JSON parser on an STM32F103 to read two commands is a poor trade.
Splitting a line on its first space is a `strncmp` and a `strtoul`.

### Status: the board cannot receive these yet

The GUI buttons are wired and the bytes really do go out on the wire, but **nothing on the
board reads them**. Two things are missing:

1. **Wiring.** Only PL2303 RX → PA9 is connected today. Commands need PL2303 TX → PA10 as
   well. PA10 is 5 V tolerant on the F103, so a 5 V adapter will not damage it.
2. **Firmware.** `UART_DEVICE` in `firmware/main/Drivers/UART/Inc/uart_transmit.h` exposes
   only `print()` and `println()`. `_setupHardwareInstance()` does configure PA10 as
   input-with-pullup, so the pin is ready, but no code ever reads the data register.

Until both exist, a command sent from the GUI goes into the void — the companion says so
rather than pretending, by watching for a changed seed and reporting when none arrives.

### RX roadmap

The receive path, in the order worth building it:

1. **Enable the receiver.** `RE` in `USART1->CR1`, then `RXNEIE` for the interrupt. The
   NVIC setup is the same `USART1_IRQn` (37) you already need for interrupt-mode TX.
2. **Keep the ISR short.** Read `USART1->DR` (that read is what clears `RXNE`), append the
   byte to a small line buffer, and set a flag when you see `\n`. No parsing in the ISR.
3. **Watch for overrun.** If `DR` is not read before the next byte lands, `ORE` sets and
   `RXNE` stops firing — the receiver looks dead forever. Clearing it is the documented
   read of `SR` followed by `DR`. This is the classic F1 UART RX hang and it is worth
   handling from the start rather than debugging later.
4. **Parse in the main loop.** Compare against `new` and `seed `, then `strtoul` the rest.
5. **Act and re-announce.** Call `MazeCore::generate()` with the new seed and send the
   maze message again, so the companion sees a changed seed and confirms the command.

References: RM0008 §27 (USART registers, `SR`/`DR`/`CR1`, the ORE sequence), PM0056 (NVIC),
UM1850 (the HAL equivalents if you would rather call `HAL_UART_Receive_IT`).

## Tests

    make test        # 63 tests, needs the oracle built

Tk tests want a display; on a headless box run them under `xvfb-run`.
