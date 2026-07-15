#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Portul de referinta Python al nucleului C++ (common/maze_core.cpp).

Logica nucleului e validata aici, printr-o implementare linie-cu-linie echivalenta,
rulata pe sute de seed-uri.
Echivalenta cu C++ se verifica prin citire cap-la-cap (code review), nu prin
rulare comparata.

Reguli de echivalenta (identice cu maze_core.cpp):
  - xorshift32 (Marsaglia 2003), aritmetica fortata pe 32 de biti;
  - Fisher-Yates cu `% (i+1)`, acelasi sens de parcurgere;
  - zidire intr-o singura trecere, cu flood-fill de conectivitate;
  - vecinii mereu in ordinea codurilor: 0=jos, 1=sus, 2=dreapta, 3=stanga;
  - BFS cu aceeasi ordine a vecinilor (acelasi drum, nu doar aceeasi lungime).

Utilizare:
  python maze_reference.py --check           # proprietati pe 500 de seed-uri + statistici
  python maze_reference.py --json 42 out.json  # JSON v3 pentru un seed (ex. pt. vizualizator)
  python maze_reference.py --show 42         # deseneaza in ASCII labirintul unui seed
"""

import sys

N = 10
CELLS = N * N
START = 0
GOAL = CELLS - 1   

# directiile in ordinea codurilor de inclinare: 0=jos, 1=sus, 2=dreapta, 3=stanga
DR = (+1, -1, 0, 0)
DC = (0, 0, +1, -1)
MASK32 = 0xFFFFFFFF


def xorshift32(state):
    """Un pas Marsaglia 2003; intoarce (valoare, stare_noua), pe 32 de biti."""
    s = state
    s ^= (s << 13) & MASK32
    s ^= s >> 17
    s ^= (s << 5) & MASK32
    return s, s


def flood_all_free_connected(walls, expected_free):
    """Echivalentul allFreeConnected() din C++: flood din START peste liber."""
    visited = [False] * CELLS
    queue = [START]
    visited[START] = True
    seen = 1
    head = 0
    while head < len(queue):
        cur = queue[head]
        head += 1
        r, c = divmod(cur, N)
        for d in range(4):
            nr, nc = r + DR[d], c + DC[d]
            if not (0 <= nr < N and 0 <= nc < N):
                continue
            nidx = nr * N + nc
            if walls[nidx] or visited[nidx]:
                continue
            visited[nidx] = True
            queue.append(nidx)
            seen += 1
    return seen == expected_free


def generate(seed):
    """Echivalentul generate(): intoarce (walls ca lista 0/1, seed_efectiv)."""
    if seed == 0:
        seed = 1
    rng = seed

    order = list(range(CELLS))
    for i in range(CELLS - 1, 0, -1):
        val, rng = xorshift32(rng)
        j = val % (i + 1)
        order[i], order[j] = order[j], order[i]

    walls = [0] * CELLS
    free_cells = CELLS
    for idx in order:
        if idx in (START, GOAL):
            continue
        walls[idx] = 1
        if flood_all_free_connected(walls, free_cells - 1):
            free_cells -= 1
        else:
            walls[idx] = 0
    return walls, seed


def solve(walls):
    """Echivalentul solve(): BFS start->final; intoarce lista codurilor sau None."""
    prevdir = [None] * CELLS
    visited = [False] * CELLS
    queue = [START]
    visited[START] = True
    head = 0
    reached = False
    while head < len(queue):
        cur = queue[head]
        head += 1
        if cur == GOAL:
            reached = True
            break
        r, c = divmod(cur, N)
        for d in range(4):
            nr, nc = r + DR[d], c + DC[d]
            if not (0 <= nr < N and 0 <= nc < N):
                continue
            nidx = nr * N + nc
            if walls[nidx] or visited[nidx]:
                continue
            visited[nidx] = True
            prevdir[nidx] = d
            queue.append(nidx)
    if not reached:
        return None

    delta = (+N, -N, +1, -1)
    path = []
    cur = GOAL
    while cur != START:
        d = prevdir[cur]
        path.append(d)
        cur -= delta[d]
    path.reverse()
    return path


def to_json(walls, path, seed):
    """JSON, aceeasi schema ca writeJson() din C++ (nu byte-identic garantat)."""
    walls_n = sum(walls)
    rows = ",\n    ".join(
        "[" + ",".join(str(walls[r * N + c]) for c in range(N)) + "]"
        for r in range(N)
    )
    return (
        '{\n'
        '  "format": "tilting-maze-v3",\n'
        f'  "size": {N},\n'
        f'  "seed": {seed},\n'
        f'  "start": [{START // N}, {START % N}],\n'
        f'  "goal": [{GOAL // N}, {GOAL % N}],\n'
        f'  "solvable": {"true" if path is not None else "false"},\n'
        '  "perimeter": "fixed",\n'
        '  "matrix": [\n'
        f'    {rows}\n'
        '  ],\n'
        f'  "solution": [{",".join(str(d) for d in (path or []))}],\n'
        '  "stats": {'
        f'"walls": {walls_n}, "free": {CELLS - walls_n}, '
        f'"path_length": {len(path or [])}, '
        f'"tle94112_chips_needed": {(walls_n + 2) // 3}}}\n'
        '}\n'
    )


def render(walls, path=None):
    """Desen ASCII cu rama de perimetru (implicita, nu e in matrice)."""
    on_path = set()
    if path:
        cur = START
        on_path.add(cur)
        delta = (+N, -N, +1, -1)
        for d in path:
            cur += delta[d]
            on_path.add(cur)
    lines = ["# " * (N + 2)]
    for r in range(N):
        row = ["# "]
        for c in range(N):
            idx = r * N + c
            if idx == START:
                row.append("S ")
            elif idx == GOAL:
                row.append("F ")
            elif walls[idx]:
                row.append("# ")
            elif idx in on_path:
                row.append(". ")
            else:
                row.append("  ")
        row.append("#")
        lines.append("".join(row))
    lines.append("# " * (N + 2))
    return "\n".join(lines)


def path_is_valid(walls, path):
    """Urmarea pasilor din start: fara ziduri, fara iesire din grila, final atins."""
    cur = START
    seen = {cur}
    delta = (+N, -N, +1, -1)
    for d in path:
        r, c = divmod(cur, N)
        nr, nc = r + DR[d], c + DC[d]
        if not (0 <= nr < N and 0 <= nc < N):
            return False
        cur = nr * N + nc
        if walls[cur]:
            return False
        if cur in seen:
            return False
        seen.add(cur)
    return cur == GOAL


def dead_ends(walls):
    """Fundaturile: celule libere (fara start/final) cu exact un vecin liber."""
    count = 0
    for idx in range(CELLS):
        if walls[idx] or idx in (START, GOAL):
            continue
        r, c = divmod(idx, N)
        nfree = 0
        for d in range(4):
            nr, nc = r + DR[d], c + DC[d]
            if 0 <= nr < N and 0 <= nc < N and not walls[nr * N + nc]:
                nfree += 1
        if nfree == 1:
            count += 1
    return count


def check(n_seeds=500):
    stats_walls, stats_len, stats_dead = [], [], []
    for seed in range(1, n_seeds + 1):
        walls, eff = generate(seed)
        walls2, _ = generate(seed)
        assert walls == walls2, f"seed {seed}: nereproductibil"
        assert eff == seed
        assert walls[START] == 0 and walls[GOAL] == 0, f"seed {seed}: start/final zidite"
        free = CELLS - sum(walls)
        assert flood_all_free_connected(walls, free), f"seed {seed}: spatiu liber rupt"
        path = solve(walls)
        assert path is not None, f"seed {seed}: nerezolvabil"
        assert path_is_valid(walls, path), f"seed {seed}: solutie invalida"
        assert len(path) <= CELLS - 1, f"seed {seed}: drum imposibil de lung"
        stats_walls.append(sum(walls))
        stats_len.append(len(path))
        stats_dead.append(dead_ends(walls))

    w0, _ = generate(0)
    w1, _ = generate(1)
    assert w0 == w1, "seed 0 trebuie sa fie identic cu seed 1 (documentat)"

    def mmm(v):
        return f"min {min(v)}, medie {sum(v)/len(v):.1f}, max {max(v)}"

    print(f"OK: toate proprietatile trecute pe {n_seeds} seed-uri (1..{n_seeds}).")
    print(f"  ziduri (din {CELLS} celule): {mmm(stats_walls)}")
    print(f"  lungimea solutiei:           {mmm(stats_len)}")
    print(f"  fundaturi:                   {mmm(stats_dead)}")
    print(f"  cipuri TLE94112 (ceil(z/3)): {mmm([(w + 2) // 3 for w in stats_walls])}")
    print()
    print("Exemplu, seed 42 (S=start, F=final, .=solutia, #=zid):")
    walls, _ = generate(42)
    path = solve(walls)
    print(render(walls, path))
    print(f"  ziduri={sum(walls)}, pasi={len(path)}, fundaturi={dead_ends(walls)}")


def main(argv):
    if len(argv) >= 2 and argv[1] == "--check":
        check(int(argv[2]) if len(argv) > 2 else 500)
    elif len(argv) >= 3 and argv[1] == "--json":
        seed = int(argv[2])
        walls, eff = generate(seed)
        path = solve(walls)
        text = to_json(walls, path, eff)
        if len(argv) > 3:
            with open(argv[3], "w", encoding="ascii") as f:
                f.write(text)
            print(f"scris: {argv[3]}")
        else:
            sys.stdout.write(text)
    elif len(argv) >= 3 and argv[1] == "--show":
        walls, _ = generate(int(argv[2]))
        print(render(walls, solve(walls)))
    else:
        print(__doc__)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
