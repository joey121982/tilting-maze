"""Reads the board's NDJSON stream off a serial port, on a background thread.

The reader owns the serial port and the framing; it never touches the GUI. Everything it
learns is pushed onto a ``queue.Queue`` as an event dict, and the Tk main thread drains
that queue on a timer. That split is not stylistic - Tk is not thread-safe, and calling
into a widget from this thread is the classic way to make a Tkinter app hang or crash on
a Pi without any useful error.

Events pushed onto the queue
----------------------------
``{"kind": "state",    "state": ..., "detail": ...}``  connection state changed
``{"kind": "message",  "msg": {...}, "raw": "..."}``   a line parsed cleanly
``{"kind": "bad_line", "raw": "...", "error": "..."}`` a line was dropped
"""

from __future__ import annotations

import queue
import threading
from typing import Any

import serial
from serial.tools import list_ports

from . import protocol

#: Connection states reported through the ``state`` event.
STATE_CONNECTING = "connecting"
STATE_CONNECTED = "connected"
STATE_DISCONNECTED = "disconnected"

#: Longest line accepted before the buffer is assumed to be junk and dropped.
#: A maze message is around 400 bytes, so this leaves a wide margin while still
#: bounding memory if the port produces a stream with no newlines in it at all.
MAX_LINE_BYTES = 8192

#: How long to wait before retrying after the port goes away.
RECONNECT_DELAY_S = 2.0

#: Serial read timeout. Short enough that stop() is responsive, long enough that the
#: thread is not spinning when the board is quiet.
READ_TIMEOUT_S = 0.2

DEFAULT_BAUD = 115200


def available_ports() -> list[tuple[str, str]]:
    """List candidate serial ports as (device, human description) pairs.

    On a Pi a USB-TTL adapter shows up as ``/dev/ttyUSB0`` (CH340, FT232) or
    ``/dev/ttyACM0`` (a CDC bridge). ``/dev/ttyAMA0`` is the Pi's own GPIO UART and only
    appears if the board is wired to the header pins instead of over USB.
    """
    return [(p.device, p.description or p.device) for p in list_ports.comports()]


class SerialReader:
    """Background serial reader that feeds an event queue.

    Args:
        port: device path, e.g. ``/dev/ttyUSB0``.
        baud: line rate; the firmware configures 115200 8N1.
        events: queue the GUI drains.
        reconnect: keep retrying when the port disappears. Unplugging the adapter is a
            normal event, not a reason for the app to stop.
    """

    def __init__(
        self,
        port: str,
        baud: int = DEFAULT_BAUD,
        events: queue.Queue | None = None,
        reconnect: bool = True,
    ) -> None:
        self.port = port
        self.baud = baud
        self.events: queue.Queue = events if events is not None else queue.Queue()
        self.reconnect = reconnect

        self._stop = threading.Event()
        self._thread: threading.Thread | None = None

        # The open port lives on the reader thread; send() runs on the Tk thread. The
        # lock guards the handle itself, not the I/O - it stops a write landing on a
        # port that _run() closed a moment ago, which on Windows raises rather than
        # returning an error.
        self._port_lock = threading.Lock()
        self._port: serial.Serial | None = None

    # -- lifecycle ---------------------------------------------------------------

    def start(self) -> None:
        """Begin reading. Safe to call once; use a new instance to reconnect elsewhere."""
        if self._thread is not None:
            raise RuntimeError("reader already started")

        self._stop.clear()
        self._thread = threading.Thread(target=self._run, name="serial-reader", daemon=True)
        self._thread.start()

    def stop(self, timeout: float = 2.0) -> None:
        """Ask the thread to finish and wait briefly for it."""
        self._stop.set()
        if self._thread is not None:
            self._thread.join(timeout=timeout)
            self._thread = None

    @property
    def running(self) -> bool:
        return self._thread is not None and self._thread.is_alive()

    def send(self, data: bytes) -> bool:
        """Write raw bytes to the board. Safe to call from the GUI thread.

        Returns:
            True if the bytes reached the driver. False means there was no open port, or
            the write failed - both of which the caller should surface rather than
            retry, because a command that silently vanished is worse than a visible one
            that failed.
        """
        with self._port_lock:
            port = self._port
            if port is None:
                return False

            try:
                port.write(data)
                port.flush()
            except (serial.SerialException, OSError):
                return False

        return True

    # -- internals ---------------------------------------------------------------

    def _emit(self, **event: Any) -> None:
        self.events.put(event)

    def _run(self) -> None:
        while not self._stop.is_set():
            self._emit(kind="state", state=STATE_CONNECTING, detail=self.port)

            try:
                with serial.Serial(self.port, self.baud, timeout=READ_TIMEOUT_S) as port:
                    self._emit(
                        kind="state",
                        state=STATE_CONNECTED,
                        detail=f"{self.port} @ {self.baud} 8N1",
                    )
                    with self._port_lock:
                        self._port = port
                    try:
                        self._pump(port)
                    finally:
                        with self._port_lock:
                            self._port = None

            except (serial.SerialException, OSError) as exc:
                self._emit(kind="state", state=STATE_DISCONNECTED, detail=str(exc))
            else:
                self._emit(kind="state", state=STATE_DISCONNECTED, detail="closed")

            if not self.reconnect or self._stop.is_set():
                break

            # Interruptible sleep, so stop() does not have to wait out the full delay.
            self._stop.wait(RECONNECT_DELAY_S)

    def _pump(self, port: serial.Serial) -> None:
        """Read bytes and cut them into lines.

        Framing is done here rather than with ``Serial.readline()`` on purpose: readline
        returns whatever it has when its timeout expires, so a message that happens to
        straddle a timeout would be split into two fragments and both would be lost. By
        buffering and splitting on the newline ourselves, a slow or chunked message is
        reassembled correctly and only genuinely broken input is dropped.
        """
        buf = bytearray()

        while not self._stop.is_set():
            chunk = port.read(max(1, port.in_waiting))
            if not chunk:
                continue

            buf.extend(chunk)

            while b"\n" in buf:
                raw, _, rest = buf.partition(b"\n")
                buf = bytearray(rest)
                self._handle(raw)

            # No newline in a very long time means this is not our protocol, or the line
            # rate is wrong and we are reading framing noise. Drop it and resync.
            if len(buf) > MAX_LINE_BYTES:
                self._emit(
                    kind="bad_line",
                    raw=f"<{len(buf)} bytes with no newline>",
                    error="no frame delimiter found; check the baud rate",
                )
                buf.clear()

    def _handle(self, raw: bytes) -> None:
        """Decode, parse, and report one line."""
        # errors="replace" keeps a corrupted byte from killing the thread; the line will
        # simply fail to parse and be counted like any other bad line.
        text = raw.decode("utf-8", errors="replace").strip()
        if not text:
            return

        try:
            msg = protocol.parse_line(text)
        except protocol.ProtocolError as exc:
            self._emit(kind="bad_line", raw=text, error=str(exc))
            return

        if msg is not None:
            self._emit(kind="message", msg=msg, raw=text)
