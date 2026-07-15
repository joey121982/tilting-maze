#!/usr/bin/env python3
"""Visualizer pentru labirintele proiectului tilting-maze.

Generarea si rezolvarea se fac in C++ (nucleul comun ../common/maze_core.cpp,
acelasi care ruleaza si pe placa STM32). Acest program:
  - are UI-ul (seed, butoane),
  - la "Generate" ruleaza binarul C++ compilat (maze/maze.exe) ca subproces,
  - deseneaza matricea si drumul-solutie, cu animatie a bilei.

Formate acceptate: matricea e DOAR interiorul 10x10, fara
    perimetru; carcasa fixa e desenata de vizualizator ca rama, in alta
    nuanta decat peretii motorizati.

Poate incarca si un JSON existent (Open JSON...), deci vizualizatorul e util
chiar fara binarul C++ compilat. Foloseste doar biblioteca standard.

"""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import tkinter as tk
from tkinter import filedialog, messagebox, ttk

HERE = os.path.dirname(os.path.abspath(__file__))
CORE = os.path.join(HERE, os.pardir, "common", "maze_core.cpp")
BIN_NAMES = ["maze.exe", "maze"] if os.name == "nt" else ["maze", "maze.exe"]

# Codurile de directie din vectorul solutie -> (drand, dcol).
DIR_DELTA = {0: (1, 0), 1: (-1, 0), 2: (0, 1), 3: (0, -1)}  # jos, sus, dreapta, stanga

WALL_COLOR = "#243447"
FRAME_COLOR = "#3a4a5e"
OPEN_COLOR = "#eef2f6"
START_COLOR = "#2ecc71"
GOAL_COLOR = "#e74c3c"
PATH_COLOR = "#3498db"
BALL_COLOR = "#f1c40f"


def find_binary():
    """Cauta binarul C++ langa acest script."""
    for name in BIN_NAMES:
        p = os.path.join(HERE, name)
        if os.path.isfile(p):
            return p
    return None


def try_build():
    """Incearca sa compileze nucleul + CLI-ul cu un compilator gasit."""
    src = os.path.join(HERE, "maze.cpp")
    if not (os.path.isfile(src) and os.path.isfile(CORE)):
        return None
    out = os.path.join(HERE, "maze.exe" if os.name == "nt" else "maze")
    for cc in ("g++", "clang++"):
        if shutil.which(cc):
            try:
                subprocess.run([cc, "-std=c++14", "-O2", "-o", out, src, CORE],
                               check=True, capture_output=True)
                return out
            except subprocess.CalledProcessError:
                continue
    if os.name == "nt" and shutil.which("cl"):
        try:
            subprocess.run(["cl", "/std:c++14", "/EHsc", "/O2",
                            "/Fe:" + out, src, CORE], check=True, capture_output=True, cwd=HERE)
            return out
        except subprocess.CalledProcessError:
            pass
    return None


class MazeApp:
    def __init__(self, root):
        self.root = root
        root.title("Tilting-maze — visualizer")
        self.maze = None
        self.last_json_text = None
        self._anim_job = None
        # reprezentarea de desen, normalizata din v2/v3 in load_text():
        self.disp_matrix = None
        self.disp_start = (1, 1)
        self.disp_goal = (1, 1)
        self.has_frame = False

        bar = ttk.Frame(root, padding=8)
        bar.pack(side=tk.TOP, fill=tk.X)

        # dimensiunea nu mai e reglabila: 10x10 fixata de hardware (v3)
        ttk.Label(bar, text="Matrice: 10×10 (hardware)").pack(side=tk.LEFT, padx=(0, 12))

        ttk.Label(bar, text="Seed:").pack(side=tk.LEFT)
        self.seed_var = tk.StringVar(value="")
        ttk.Entry(bar, width=10, textvariable=self.seed_var).pack(side=tk.LEFT, padx=(4, 12))

        ttk.Button(bar, text="Generate", command=self.on_generate).pack(side=tk.LEFT, padx=2)
        ttk.Button(bar, text="Open JSON…", command=self.on_open).pack(side=tk.LEFT, padx=2)
        ttk.Button(bar, text="Save JSON…", command=self.on_save).pack(side=tk.LEFT, padx=2)

        self.show_sol = tk.BooleanVar(value=True)
        ttk.Checkbutton(bar, text="Arata solutia", variable=self.show_sol,
                        command=self.redraw).pack(side=tk.LEFT, padx=(12, 2))
        ttk.Button(bar, text="▶ Play", command=self.play).pack(side=tk.LEFT, padx=2)

        self.canvas = tk.Canvas(root, width=640, height=640, background="#0e1621",
                                highlightthickness=0)
        self.canvas.pack(side=tk.TOP, fill=tk.BOTH, expand=True)
        self.canvas.bind("<Configure>", lambda e: self.redraw())

        self.status = tk.StringVar(value="Gata. Apasa Generate (necesita binarul C++) sau Open JSON…")
        ttk.Label(root, textvariable=self.status, relief=tk.SUNKEN,
                  anchor=tk.W, padding=4).pack(side=tk.BOTTOM, fill=tk.X)

    # ---- actiuni ----
    def on_generate(self):
        binary = find_binary()
        if binary is None:
            self.status.set("Compilez maze.cpp + maze_core.cpp…")
            self.root.update_idletasks()
            binary = try_build()
        if binary is None:
            messagebox.showwarning(
                "Binar C++ lipsa",
                "Generarea/rezolvarea sunt in C++ (nucleul comun) si nu am gasit binarul\n"
                "compilat, nici un compilator (g++/clang++/cl) ca sa il construiesc.\n\n"
                "Compileaza-l o data:\n"
                "    g++ -std=c++14 -O2 -o maze maze.cpp ../common/maze_core.cpp\n"
                "(sau ruleaza build.bat), apoi apasa din nou Generate.\n\n"
                "Intre timp poti folosi Open JSON… pe un labirint deja generat.")
            self.status.set("Binar C++ negasit — vezi build.bat / Open JSON…")
            return

        args = [binary]
        seed = self.seed_var.get().strip()
        if seed:
            args += ["--seed", seed]
        tmp = os.path.join(tempfile.gettempdir(), "tilting_maze_tmp.json")
        args += ["--out", tmp]
        try:
            subprocess.run(args, check=True, capture_output=True)
            with open(tmp, "r", encoding="utf-8") as f:
                text = f.read()
        except (subprocess.CalledProcessError, OSError) as exc:
            messagebox.showerror("Eroare la generare", str(exc))
            return
        self.load_text(text)

    def on_open(self):
        path = filedialog.askopenfilename(
            title="Deschide un labirint JSON",
            filetypes=[("JSON", "*.json"), ("Toate", "*.*")])
        if not path:
            return
        try:
            with open(path, "r", encoding="utf-8") as f:
                self.load_text(f.read())
        except OSError as exc:
            messagebox.showerror("Eroare", str(exc))

    def on_save(self):
        if self.last_json_text is None:
            messagebox.showinfo("Nimic de salvat", "Genereaza sau deschide intai un labirint.")
            return
        path = filedialog.asksaveasfilename(
            title="Salveaza labirintul", defaultextension=".json",
            filetypes=[("JSON", "*.json")])
        if not path:
            return
        try:
            with open(path, "w", encoding="utf-8") as f:
                f.write(self.last_json_text)
            self.status.set(f"Salvat in {path}")
        except OSError as exc:
            messagebox.showerror("Eroare", str(exc))

    def load_text(self, text):
        try:
            data = json.loads(text)
            assert isinstance(data.get("matrix"), list) and data["matrix"], "matrix lipsa"
        except (json.JSONDecodeError, AssertionError) as exc:
            messagebox.showerror("JSON invalid", str(exc))
            return

        fmt = data.get("format", "")
        matrix = data["matrix"]
        n = len(matrix)
        if fmt == "tilting-maze-v3" or data.get("perimeter") == "fixed":
            disp = [[1] * (n + 2)]
            for row in matrix:
                disp.append([1] + list(row) + [1])
            disp.append([1] * (n + 2))
            sr, sc = data.get("start", [0, 0])
            gr, gc = data.get("goal", [n - 1, n - 1])
            self.disp_matrix = disp
            self.disp_start = (sr + 1, sc + 1)
            self.disp_goal = (gr + 1, gc + 1)
            self.has_frame = True
            st = data.get("stats", {})
            walls = st.get("walls", "?")
            self.status.set(
                f"v3  {n}×{n}+carcasa  drum={st.get('path_length', '?')} pasi  "
                f"pereti motorizati={walls}  liberi={st.get('free', '?')}  "
                f"cipuri TLE94112={st.get('tle94112_chips_needed', '?')}  "
                f"seed={data.get('seed', '?')}")
        else:
            self.disp_matrix = matrix
            self.disp_start = tuple(data.get("start", [1, 1]))
            self.disp_goal = tuple(data.get("goal", [n - 2, n - 2]))
            self.has_frame = False
            st = data.get("stats", {})
            self.status.set(
                f"v2  N={data.get('size')}  drum={st.get('path_length', '?')} pasi  "
                f"pereti={st.get('total_walls', '?')} (interiori={st.get('interior_walls', '?')})  "
                f"cipuri TLE94112={st.get('tle94112_chips_needed', '?')}  "
                f"seed={data.get('seed', '?')}")

        self.maze = data
        self.last_json_text = text
        self.redraw()

    def _geometry(self):
        m = self.disp_matrix
        n = len(m)
        w = self.canvas.winfo_width() or 640
        h = self.canvas.winfo_height() or 640
        margin = 12
        cell = max(2, int(min(w, h) - 2 * margin) // n)
        ox = (w - cell * n) // 2
        oy = (h - cell * n) // 2
        return n, cell, ox, oy

    def redraw(self):
        self.canvas.delete("all")
        if not self.maze:
            return
        m = self.disp_matrix
        n, cell, ox, oy = self._geometry()
        for r in range(n):
            for c in range(n):
                x0, y0 = ox + c * cell, oy + r * cell
                if m[r][c] == 1:
                    on_frame = self.has_frame and (r in (0, n - 1) or c in (0, n - 1))
                    color = FRAME_COLOR if on_frame else WALL_COLOR
                else:
                    color = OPEN_COLOR
                self.canvas.create_rectangle(x0, y0, x0 + cell, y0 + cell, fill=color, width=0)

        self._fill_cell(*self.disp_start, START_COLOR, cell, ox, oy)
        self._fill_cell(*self.disp_goal, GOAL_COLOR, cell, ox, oy)

        if self.show_sol.get():
            self._draw_solution(cell, ox, oy)

    def _fill_cell(self, r, c, color, cell, ox, oy):
        pad = max(1, cell // 6)
        x0, y0 = ox + c * cell + pad, oy + r * cell + pad
        self.canvas.create_rectangle(x0, y0, x0 + cell - 2 * pad, y0 + cell - 2 * pad,
                                     fill=color, width=0)

    def _path_points(self):
        """Coordonatele (r, c) de desen ale drumului, din start urmarind vectorul."""
        r, c = self.disp_start
        pts = [(r, c)]
        for code in self.maze.get("solution", []):
            dr, dc = DIR_DELTA[code]
            r, c = r + dr, c + dc
            pts.append((r, c))
        return pts

    def _draw_solution(self, cell, ox, oy):
        pts = self._path_points()
        if len(pts) < 2:
            return
        coords = []
        for (r, c) in pts:
            coords += [ox + c * cell + cell / 2, oy + r * cell + cell / 2]
        self.canvas.create_line(*coords, fill=PATH_COLOR, width=max(2, cell // 5), capstyle=tk.ROUND, joinstyle=tk.ROUND)

    def play(self):
        if not self.maze or not self.maze.get("solution"):
            return
        if self._anim_job:
            self.canvas.after_cancel(self._anim_job)
            self._anim_job = None
        self.redraw()
        pts = self._path_points()
        n, cell, ox, oy = self._geometry()
        rad = max(3, cell // 3)

        def step(i):
            self.canvas.delete("ball")
            r, c = pts[i]
            x, y = ox + c * cell + cell / 2, oy + r * cell + cell / 2
            self.canvas.create_oval(x - rad, y - rad, x + rad, y + rad,
                                    fill=BALL_COLOR, outline="", tags="ball")
            if i + 1 < len(pts):
                self._anim_job = self.canvas.after(90, step, i + 1)
            else:
                self._anim_job = None

        step(0)

def main():
    root = tk.Tk()
    app = MazeApp(root)
    # Daca s-a dat un fisier ca argument, incarca-l direct.
    if len(sys.argv) > 1 and os.path.isfile(sys.argv[1]):
        try:
            with open(sys.argv[1], "r", encoding="utf-8") as f:
                app.load_text(f.read())
        except OSError:
            pass
    root.mainloop()


if __name__ == "__main__":
    main()
