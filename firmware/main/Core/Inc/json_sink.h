#pragma once

#include <stdint.h>

#include "uart_transmit.h"

/**
 * @file json_sink.h
 * @brief Adapts the character-at-a-time sink used by MazeCore::writeJson() onto the
 *        existing UART driver.
 *
 * ---------------------------------------------------------------------------------
 * FOR REVIEW - why this exists as an adapter instead of a change to the UART driver
 * ---------------------------------------------------------------------------------
 * UART::UART_DEVICE currently exposes only print(const char*), which needs a
 * NUL-terminated string; there is no binary-safe write yet (the TODO for it is still
 * open at the bottom of uart_transmit.h). MazeCore::writeJson() produces one character
 * at a time and never buffers, so the two do not meet directly.
 *
 * This file bridges them from the outside, using nothing but the driver's public API,
 * so the driver itself stays untouched and stays yours to change. The cost is one
 * SINK_BUF_LEN-byte buffer and a flush call at the end of each message.
 *
 * If UART_DEVICE later gains write(const void* buf, size_t nbyte), this whole header
 * collapses into a passthrough and writeJson() does not change at all - the character
 * sink signature is what keeps the serializer independent of the transport.
 * ---------------------------------------------------------------------------------
 */

namespace JsonSink {

/** @brief Size of the staging buffer.
 *
 *  One byte is reserved for the NUL that print() requires, so at most SINK_BUF_LEN - 1
 *  payload characters are held before a flush. 128 is a compromise: large enough that a
 *  ~1KB maze message costs single-digit print() calls rather than one per character,
 *  small enough to be irrelevant against the 20KB of RAM on the part.
 */
constexpr uint8_t SINK_BUF_LEN = 128;

/** @brief Staging buffer plus the device it drains into.
 *
 *  Declare one of these alongside the UART device and pass its address as the ctx
 *  argument of MazeCore::writeJson().
 */
struct UartSink {
    UART::UART_DEVICE* dev;
    char               buf[SINK_BUF_LEN];
    uint8_t            len;

    explicit UartSink(UART::UART_DEVICE* device) : dev(device), len(0) {}
};

/**
 * @brief Pushes the buffered characters to the UART and empties the buffer.
 *
 * @param ctx Pointer to a UartSink
 *
 * Called automatically by put() when the buffer fills, and once by the caller at the end
 * of a message. Doing nothing on an empty buffer makes it safe to call twice.
 */
inline void flush(void* ctx)
{
    UartSink* sink = static_cast<UartSink*>(ctx);
    if (sink->len == 0) return;

    sink->buf[sink->len] = '\0';
    sink->dev->print(sink->buf);
    sink->len = 0;
}

/**
 * @brief Character sink matching the signature MazeCore::writeJson() expects.
 *
 * @param c Character to emit
 * @param ctx Pointer to a UartSink
 *
 * @note The buffer is flushed when it is one byte from full, keeping the last slot free
 *       for the NUL that print() needs.
 */
inline void put(char c, void* ctx)
{
    UartSink* sink = static_cast<UartSink*>(ctx);

    sink->buf[sink->len++] = c;
    if (sink->len >= SINK_BUF_LEN - 1) flush(ctx);
}

}   // namespace JsonSink
