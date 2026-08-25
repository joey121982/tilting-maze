#include "main.h"
#include "uart_transmit.h"

#include "maze_core.h"
#include "json_sink.h"
#include "seed_source.h"

static constexpr uint32_t STATUS_PERIOD_MS = 100u;
static constexpr uint32_t ANNOUNCE_PERIOD_MS = 5000u;

#ifndef MAZE_TELEMETRY_MOTORS
#define MAZE_TELEMETRY_MOTORS 0
#endif

#if MAZE_TELEMETRY_MOTORS
#include "tle94112.h"
#include "button.h"
#endif

static void emitStr(const char* s, void* ctx)
{
    while (*s) JsonSink::put(*s++, ctx);
}

static void emitUint(uint32_t value, void* ctx)
{
    char    digits[10];
    uint8_t n = 0;

    do {
        digits[n++] = (char)('0' + (value % 10u));
        value /= 10u;
    } while (value);

    while (n) JsonSink::put(digits[--n], ctx);
}

static void emitHello(void* ctx)
{
    emitStr("{\"type\":\"hello\",\"proto\":1,\"board\":\"stm32f103c8\"}\n", ctx);
    JsonSink::flush(ctx);
}

static void announce(void* ctx, const MazeCore::Maze& maze)
{
    emitHello(ctx);
    MazeCore::writeJson(maze, JsonSink::put, ctx);
    JsonSink::flush(ctx);
}

/** @brief Reports uptime, diagnostics, hardware button state, ball position, tilt, and system state. */
static void emitStatus(
    void* ctx,
    uint32_t tick,
    uint8_t diagnostics = 0,
    bool button_pressed = false,
    int ball_r = -1,
    int ball_c = -1,
    const char* tilt = nullptr,
    const char* state = nullptr)
{
    emitStr("{\"type\":\"status\",\"tick\":", ctx);
    emitUint(tick, ctx);
    emitStr(",\"diagnostics\":", ctx);
    emitUint((uint32_t)diagnostics, ctx);
    emitStr(",\"button\":", ctx);
    emitStr(button_pressed ? "true" : "false", ctx);

    if (ball_r >= 0 && ball_c >= 0) {
        emitStr(",\"ball\":[", ctx);
        emitUint((uint32_t)ball_r, ctx);
        emitStr(",", ctx);
        emitUint((uint32_t)ball_c, ctx);
        emitStr("]", ctx);
    }

    if (tilt != nullptr) {
        emitStr(",\"tilt\":\"", ctx);
        emitStr(tilt, ctx);
        emitStr("\"", ctx);
    }

    if (state != nullptr) {
        emitStr(",\"state\":\"", ctx);
        emitStr(state, ctx);
        emitStr("\"", ctx);
    }

    emitStr("}\n", ctx);
    JsonSink::flush(ctx);
}

extern "C" int main() {
    sys_init();

    UART::UART_DEVICE pc_uart = UART::UART_DEVICE(GPIOA, GPIO_PIN_9, GPIOA, GPIO_PIN_10, 115200);

    if (!pc_uart.init()) {
        Error_Handler();
    }

    JsonSink::UartSink sink(&pc_uart);

    static MazeCore::Maze maze;
#ifdef MAZE_FIXED_SEED
    MazeCore::generate(maze, (uint32_t)MAZE_FIXED_SEED);
#else
    MazeCore::generate(maze, SeedSource::next());
#endif

    announce(&sink, maze);

    uint32_t last_status   = HAL_GetTick();
    uint32_t last_announce = last_status;

    while (1)
    {
        uint32_t now = HAL_GetTick();

        if ((now - last_status) >= STATUS_PERIOD_MS) {
            last_status = now;
            emitStatus(&sink, now);
        }

        if ((now - last_announce) >= ANNOUNCE_PERIOD_MS) {
            last_announce = now;
            announce(&sink, maze);
        }
    }
}
