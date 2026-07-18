import tkinter as tk
from tkinter import messagebox
import subprocess
import os

class ModernMazeVisualizer(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("STM32 Maze Core - High Tech Visualizer")
        self.geometry("580x750")
        self.configure(bg="#0f172a")  # Slate 900 (Deep modern dark theme)
        
        # Numele executabilului C++
        self.cpp_binary = "./maze_generator"
        if os.name == 'nt':  # Windows compatibility
            self.cpp_binary = "maze_generator.exe"

        # Variabile animație
        self.path_coords = []
        self.path_animation_index = 0
        self.is_animating = False

        self.setup_ui()
        self.generate_and_render()

    def setup_ui(self):
        # Panou de control
        control_frame = tk.Frame(self, bg="#1e293b", bd=0)  # Slate 800
        control_frame.pack(fill=tk.X, padx=15, pady=15)
        
        # Label & Entry pentru Seed
        tk.Label(control_frame, text="SEED:", fg="#94a3b8", bg="#1e293b", font=("Segoe UI", 10, "bold")).grid(row=0, column=0, padx=8, pady=10, sticky=tk.W)
        self.seed_entry = tk.Entry(control_frame, width=12, bg="#0f172a", fg="#f8fafc", bd=1, relief=tk.FLAT, font=("Segoe UI", 10), insertbackground="white")
        self.seed_entry.insert(0, "6064625") # Seed implicit
        self.seed_entry.grid(row=0, column=1, padx=5, pady=10)
        
        # Buton de Generare & Animație
        self.gen_btn = tk.Button(control_frame, text="GENERATE & RUN SOLVER", command=self.generate_and_render, 
                                 bg="#06b6d4", fg="#ffffff", activebackground="#0891b2", activeforeground="white",
                                 font=("Segoe UI", 10, "bold"), relief=tk.FLAT, cursor="hand2", padx=15)
        self.gen_btn.grid(row=0, column=2, padx=25, pady=10)
        
        # Canvas-ul pentru labirint (stil modern cu spațieri)
        self.canvas_size = 500
        self.canvas = tk.Canvas(self, width=self.canvas_size, height=self.canvas_size, bg="#0f172a", highlightthickness=0)
        self.canvas.pack(padx=15, pady=5)
        
        # Panoul de text/status de jos
        self.info_frame = tk.Frame(self, bg="#0f172a")
        self.info_frame.pack(fill=tk.BOTH, expand=True, padx=15, pady=5)
        
        self.status_label = tk.Label(self.info_frame, text="Ready", fg="#38bdf8", bg="#0f172a", font=("Segoe UI", 11, "bold"))
        self.status_label.pack(anchor=tk.W, padx=5, pady=2)
        
        self.steps_label = tk.Label(self.info_frame, text="", fg="#94a3b8", bg="#0f172a", font=("Courier New", 10), wraplength=520, justify=tk.LEFT)
        self.steps_label.pack(anchor=tk.W, padx=5, pady=2, fill=tk.X)

    def run_cpp_backend(self, seed):
        """Rulează binarul C++ și returnează output-ul parsat."""
        if not os.path.exists(self.cpp_binary):
            raise FileNotFoundError(f"Nu s-a găsit binarul '{self.cpp_binary}'. Te rog să-l compilezi în prealabil din C++.")
            
        # Executăm programul C++ trimițându-i seed-ul ca argument
        result = subprocess.run([self.cpp_binary, str(seed)], capture_output=True, text=True, check=True)
        return self.parse_cpp_stdout(result.stdout)

    def parse_cpp_stdout(self, stdout_str):
        """Parsează direct formatul tău de printare din C++."""
        lines = [line.rstrip('\r\n') for line in stdout_str.splitlines() if line.strip()]
        
        grid_rows = []
        steps = []
        solvable = False
        
        for line in lines:
            if line.startswith("Maze seed:"):
                continue
            elif line.startswith("Solution:"):
                sol_part = line.replace("Solution:", "").strip()
                if sol_part != "no solution":
                    solvable = True
                    steps = sol_part.split()
                continue
            # Liniile rămase reprezintă gridul de caractere ('+' pentru perete, ' ' pentru cale liberă)
            grid_rows.append(line)
            
        if not grid_rows:
            return None
            
        # Determinăm automat dimensiunea (ex: 10x10, 12x12 etc.)
        side_len = max(len(row) for row in grid_rows)
        
        # Reconstruim grid-ul completând rândurile eventual trunchiate la rstrip
        grid = []
        for row in grid_rows:
            padded_row = row.ljust(side_len, ' ')
            grid.append([char == '+' for char in padded_row])
            
        return {
            'grid': grid,
            'side_len': side_len,
            'solvable': solvable,
            'steps': steps
        }

    def generate_and_render(self):
        if self.is_animating:
            return  # Prevenim suprapunerea animațiilor
            
        try:
            seed = int(self.seed_entry.get())
        except ValueError:
            messagebox.showerror("Eroare Input", "Te rog să introduci un seed numeric valid.")
            return

        try:
            maze_data = self.run_cpp_backend(seed)
        except Exception as e:
            messagebox.showerror("Eroare Backend C++", str(e))
            return

        if not maze_data:
            messagebox.showerror("Eroare Date", "Output-ul binarului C++ nu a putut fi parsat corect.")
            return

        self.grid = maze_data['grid']
        self.side_len = maze_data['side_len']
        self.solvable = maze_data['solvable']
        self.steps = maze_data['steps']

        # Convertim pașii text ("D", "R", etc.) în coordonate absolute (row, col)
        self.path_coords = [(0, 0)]
        curr_r, curr_c = 0, 0
        for step in self.steps:
            if step == 'D': curr_r += 1
            elif step == 'U': curr_r -= 1
            elif step == 'R': curr_c += 1
            elif step == 'L': curr_c -= 1
            self.path_coords.append((curr_r, curr_c))

        # Resetăm starea de desenare
        self.canvas.delete("all")
        self.draw_grid()
        
        # Pornește animația drumului
        if self.solvable and len(self.path_coords) > 1:
            self.status_label.config(text=f"Solvable | Steps: {len(self.steps)} (Animating...)", fg="#06b6d4")
            self.steps_label.config(text="Solution: " + " ".join(self.steps))
            self.path_animation_index = 0
            self.is_animating = True
            self.animate_path()
        else:
            self.status_label.config(text="Unsolvable (No Path)", fg="#ef4444")
            self.steps_label.config(text="C++ solver found no solution for this seed.")

    def draw_grid(self):
        cell_size = self.canvas_size / self.side_len
        pad = 2  # Adăugăm padding între celule pentru aspectul modern "spaced grid"
        
        for r in range(self.side_len):
            for c in range(self.side_len):
                x1 = c * cell_size + pad
                y1 = r * cell_size + pad
                x2 = (c + 1) * cell_size - pad
                y2 = (r + 1) * cell_size - pad
                
                # Stabilirea paletei de culori
                if (r, c) == (0, 0):
                    color = "#10b981"  # Start (Emerald Green)
                elif (r, c) == (self.side_len - 1, self.side_len - 1):
                    color = "#ef4444"  # Goal (Rose Red)
                elif self.grid[r][c]:
                    color = "#ffffff"  # Perete interior (Deep purple-indigo)
                else:
                    color = "#1e293b"  # Cale goală (Slate 800)
                    
                self.canvas.create_rectangle(x1, y1, x2, y2, fill=color, outline="", width=0)
                
                # Marcaje minimaliste pentru Start și Goal
                if (r, c) == (0, 0):
                    self.canvas.create_text(x1 + cell_size/2 - pad, y1 + cell_size/2 - pad, text="S", fill="#ffffff", font=("Segoe UI", int(cell_size/3.5), "bold"))
                elif (r, c) == (self.side_len - 1, self.side_len - 1):
                    self.canvas.create_text(x1 + cell_size/2 - pad, y1 + cell_size/2 - pad, text="G", fill="#ffffff", font=("Segoe UI", int(cell_size/3.5), "bold"))

    def animate_path(self):
        """Desenează drumul de rezolvare pas cu pas cu o linie neon luminoasă."""
        if self.path_animation_index < len(self.path_coords) - 1:
            p1 = self.path_coords[self.path_animation_index]
            p2 = self.path_coords[self.path_animation_index + 1]
            
            cell_size = self.canvas_size / self.side_len
            
            # Coordonatele centrului celulei
            x1 = p1[1] * cell_size + cell_size / 2
            y1 = p1[0] * cell_size + cell_size / 2
            x2 = p2[1] * cell_size + cell_size / 2
            y2 = p2[0] * cell_size + cell_size / 2
            
            # Linie neon albastră
            self.canvas.create_line(x1, y1, x2, y2, fill="#22d3ee", width=max(4, int(cell_size/5.5)), 
                                     capstyle=tk.ROUND, joinstyle=tk.ROUND, tags="sol_path")
            
            # Punct luminos la vârful traseului animat
            self.canvas.delete("head")
            r_dot = max(3, int(cell_size/5))
            self.canvas.create_oval(x2 - r_dot, y2 - r_dot, x2 + r_dot, y2 + r_dot, 
                                    fill="#ffffff", outline="#22d3ee", width=2, tags="head")
            
            self.path_animation_index += 1
            # Controlul vitezei animației (ajustată dinamic în funcție de numărul total de pași)
            delay = max(20, min(100, 1000 // len(self.steps)))
            self.after(delay, self.animate_path)
        else:
            self.canvas.delete("head")
            self.is_animating = False
            self.status_label.config(text=f"Solvable | Steps: {len(self.steps)} (Finished)", fg="#10b981")


if __name__ == "__main__":
    app = ModernMazeVisualizer()
    app.mainloop()
    
######################
# HOW TO RUN
# In directorul firmware/main:
# tilting-maze/firmware/main$ g++ -std=c++17 -ICore/Inc maze_console_main.cpp Core/Src/maze_core.cpp -o maze_generator
# python3 vis.py