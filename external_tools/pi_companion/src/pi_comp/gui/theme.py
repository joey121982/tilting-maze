"""Shared colours and fonts.

Dark by default: the companion is expected to sit next to the machine on a bench, often
in a lab where a full-brightness white window is unpleasant to look at for long.

One distinction is load-bearing rather than decorative. ``wall`` and ``frame`` are
deliberately different colours: a wall is a motorised cell that the board raised and could
lower again, whereas the frame is fixed casing that is not part of the maze data at all.
Drawing them identically would suggest the border is something the firmware controls.
"""

COLORS = {
    "bg": "#0f172a",          # slate 900, window and canvas background
    "panel": "#1e293b",       # slate 800, raised surfaces
    "border": "#334155",      # slate 700, hairlines between regions
    "free": "#1e293b",        # a cell the ball can occupy
    "wall": "#e2e8f0",        # a motorised wall, raised
    "frame": "#475569",       # the physical casing - not maze data
    "start": "#10b981",       # emerald
    "goal": "#ef4444",        # rose
    "path": "#22d3ee",        # cyan, the animated solution
    "accent": "#06b6d4",      # interactive elements
    "accent_active": "#0891b2",
    "on_accent": "#ffffff",
    "text": "#f8fafc",
    "muted": "#94a3b8",       # labels and secondary values
    "ok": "#10b981",
    "warn": "#f59e0b",
    "error": "#ef4444",
}

FONT_UI = ("Segoe UI", 10)
FONT_UI_BOLD = ("Segoe UI", 10, "bold")
FONT_LABEL = ("Segoe UI", 9)
FONT_MONO = ("Consolas", 9)
FONT_TITLE = ("Segoe UI", 11, "bold")
