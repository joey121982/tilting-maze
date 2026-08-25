"""Raspberry Pi companion for the tilting-maze board.

Reads the board's NDJSON telemetry over a serial link and displays it.

Modules:
    protocol   the wire format, and the only place it is defined on this side
    reader     background serial reader and line framing
    simulator  offline sources for working without hardware
    gui        the Tkinter interface
"""

__version__ = "1.0.0"
