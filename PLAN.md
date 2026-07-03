# Nonogram Solver — Implementation Plan

Reference: `Nonogram_Paper.pdf` — Yu, Lee, Chen (2009), "An efficient algorithm for
solving nonograms". Two-phase algorithm: **Logical Rules (LR)** for deterministic
deduction, then **Chronological Backtracking (CB)** for the remaining cells.

## Tech stack

- **Language / app**: C++17
- **Windowing / GL**: GLFW + glad, OpenGL 3.3 core profile
- **UI**: Dear ImGui
- **Math**: glm (only if needed)
- **JSON**: nlohmann/json (puzzle presets only)
- **GPU**: CUDA C++ (`nvcc 13.3`, `CMAKE_CUDA_ARCHITECTURES=120` for RTX 5070 Blackwell)
- **Build**: CMake 3.28+, `FetchContent` for all third-party deps
- **Tests**: doctest (lightweight, fast compiles, single header)
- **Python learning track (later)**: PyTorch + CUDA, then Triton — standalone, not
  wired into the app.

## Environment (verified)

- GPU: NVIDIA GeForce RTX 5070 Laptop, 12 GB, driver CUDA 13.2
- Toolchain: `nvcc 13.3`, `cmake 3.28.3`, `g++ 13.3.0`, `clang++`
- CUDA toolkit: `/usr/local/cuda-13.3`
- Target arch: `sm_120` (Blackwell)
- OS: Linux (Ubuntu 24.04)

## Architecture

Single C++ process. The solver is an in-process library linked into the app —
no IPC, no JSON marshalling on the hot path. Python (M5/M6) is a separate,
standalone learning track that reimplements the GPU line-solver and benchmarks
against the C++/CUDA version; it is **not** connected to the app.

```
┌─────────────────────── C++ application (one process) ───────────────────────┐
│  GLFW window + OpenGL 3.3 core + Dear ImGui                                 │
│  Instanced grid renderer  ·  mouse fill/cross/mark  ·  puzzle picker        │
│                                                                              │
│  solver/  (C++ library, linked in)                                           │
│   ├── logical_rules   : 11 LRs, run-range (rj_s, rj_e), iterative fixpoint  │
│   ├── backtracking    : CB + MRV ordering + look-ahead, calls LRs           │
│   └── cuda/ (M4)      : batched line-solver on GPU, LR phase on device,      │
│                         CB stays on CPU (inherently sequential)              │
│                                                                              │
│  In-process: Solve / Step / Reset buttons call solver directly.             │
│  Solver runs on background thread; UI polls shared board buffer.            │
└──────────────────────────────────────────────────────────────────────────────┘

┌── solver_cli (M1d, standalone binary) ──┐
│  ./solver_cli puzzle.json [--step]       │
│  [--stats]  ASCII grid to stdout         │
└──────────────────────────────────────────┘

           (later, separate — not connected to the app)

┌────────── Python learning track (M5/M6) ──────────┐
│  pytorch_line_solver.py  ·  triton_kernels.py     │
│  Reimplements the GPU LR phase; benchmarks vs C++ │
└───────────────────────────────────────────────────┘
```

## Cell state encoding

Shared C++ enum, used throughout the solver and renderer:

```cpp
enum class CellState : uint8_t {
    White   = 0,   // empty / crossed
    Black   = 1,   // filled
    Unknown = 2,   // undetermined
};
```

JSON is only used for **puzzle preset files** (read once by the app at load
time, never on the solve hot path).

## Project layout

```
nonogram-solver/
├── CMakeLists.txt                # top-level: project + options + add_subdirectory
├── third_party/                  # FetchContent: glfw, glad, imgui, nlohmann/json, doctest
├── solver/                       # C++ solver library
│   ├── CMakeLists.txt
│   ├── include/nonogram/
│   │   ├── types.hpp             # CellState enum, Line, RunRange
│   │   ├── nonogram.hpp          # board + clue model
│   │   ├── logical_rules.hpp     # 11 rules, run-range init, fixpoint driver
│   │   ├── backtracking.hpp      # CB + look-ahead + MRV variable ordering
│   │   └── solver.hpp            # unified façade: solve(mode=cpu|cuda), step()
│   └── src/
│       ├── nonogram.cpp
│       ├── logical_rules.cpp     # rules 1.1–3.3
│       ├── backtracking.cpp
│       └── cuda/                 # M4 — guarded by CMake option ENABLE_CUDA
│           ├── line_solver.cu    # host-side launch + memcpy
│           └── kernels.cu        # __global__ placement/intersection kernels
├── solver_cli/                   # M1d — minimal CLI driver for debugging
│   ├── CMakeLists.txt
│   └── main.cpp                  # ./solver_cli puzzle.json [--step] [--stats]
├── app/                          # C++ OpenGL app
│   ├── CMakeLists.txt
│   └── src/
│       ├── main.cpp
│       ├── app.{hpp,cpp}         # main loop, ImGui chrome
│       ├── renderer.{hpp,cpp}    # instanced grid renderer
│       ├── puzzle_state.{hpp,cpp}
│       └── shaders/grid.vert, grid.frag
├── puzzles/
│   ├── 5x5_heart.json
│   ├── 10x10_cat.json
│   └── 15x15_smiley.json
├── tests/
│   ├── CMakeLists.txt
│   ├── test_logical_rules.cpp    # one case per rule
│   ├── test_backtracking.cpp     # unique / multiple / no-solution puzzles
│   └── test_random.cpp           # M1d — random puzzle batch
├── python/                       # M5/M6 — added later
│   ├── pyproject.toml
│   ├── pytorch_line_solver.py
│   ├── triton_kernels.py
│   └── benchmarks/compare.py
└── README.md                     # M7 — build/run + architecture + benchmarks
```

## Milestones

### M1a — Types, run-range estimation, Part I rules, fixpoint driver
*Foundation. Get the data model right and the simplest rules working.*

- `types.hpp`: `CellState`, `Line` (clues + cells + run ranges), `RunRange{start,end}`.
- `nonogram.hpp/cpp`: board + clue model, JSON loading (nlohmann/json).
- Run-range initial estimation per paper Eq. 1:
  - `r1_s = 0`
  - `rj_s = sum_{i<j}(LBi + 1)` for j = 2..k
  - `rj_e = (n-1) - sum_{i>j}(LBi + 1)` for j = 1..k-1
  - `rk_e = n-1`
- Part I rules (determine cells):
  - 1.1 Intersection of left-most and right-most placements → color cells in
    `rj_s + u .. rj_e - u` where `u = range_len - LBj`.
  - 1.2 Cells outside all run ranges → empty.
  - 1.3 Boundary cells of a run range colored and covered only by length-1
    runs → adjacent cell empty.
  - 1.4 Merging two black segments across an unknown yields a segment longer
    than max covering run length → the unknown is empty.
  - 1.5 Wall (empty cell) obstructs segment expansion → cells on the open
    side get colored; equal-length overlapping runs bracket a filled segment
    → ends get emptied.
- Fixpoint driver: apply all rules to all rows, then all columns, repeat until
  no cell change AND no range change.
- Tests with **doctest**:
  - One hand-crafted row per Part I rule (input cells + ranges → expected output).
  - Run-range init test: verify `rj_s`, `rj_e` for a known clue set.
- **Done when**: `ctest` green for all Part I rule tests + run-range init test.
  Fixpoint converges on a trivial puzzle (e.g., fully determined 5×5).

### M1b — Part II rules (refine ranges)
- 2.1 Shrink run range based on already-colored cells inside it.
- 2.2 Shrink run range based on cells known empty at the ends.
- 2.3 Shrink run range based on colored cells belonging to other runs.
- Tests: one hand-crafted row per rule verifying range shrinkage.
- **Done when**: `ctest` green. Fixpoint with Parts I+II solves puzzles that
  Part I alone cannot (e.g., a 5×5 where range refinement is needed).

### M1c — Part III rules (combined: cells + ranges)
- 3.1 A colored segment whose length equals a run's LB and which can only
  belong to that run → fix its range, empty its neighbors.
- 3.2 A colored segment longer than any single run → it must contain a run
  boundary → empty the gap between the two runs.
- 3.3 Refined range forces a unique placement → color it.
- Tests: one hand-crafted row per rule.
- **Done when**: `ctest` green. Full LR phase (Parts I+II+III) solves the
  paper's Fig. 1 example (unique solution) to completion — no backtracking
  needed.

### M1d — Chronological Backtracking (CB) + CLI driver
*Complete the solver. Add a minimal CLI for debugging before the GUI exists.*

- **Chronological Backtracking (CB):**
  - **Variable ordering**: MRV (minimum remaining values) — pick a cell in the
    most constrained line (fewest unknowns relative to clue complexity). This
    dramatically prunes the search tree and is a fundamental CSP technique
    worth learning.
  - Branch: try Black, try White.
  - At each node, re-run all LRs (they act as CSP filters / look-ahead).
  - If a contradiction is detected (a line cannot satisfy its clues), backtrack
    chronologically to the last decision.
  - Detect: **solved** (no unknowns, all lines satisfied), **no_solution**
    (root node contradicts), **multiple_solutions** (CB finds ≥2 complete
    assignments).
- **Minimal CLI driver** (`solver_cli`):
  - `./solver_cli puzzle.json` → prints solved grid to stdout (ASCII: `#` `. ` `?`).
  - Flags: `--step` (print after each LR pass), `--stats` (CB nodes explored).
  - ~50 lines of code; invaluable for debugging the solver before the GUI.
- **Random puzzle generator** (test utility):
  - Generate a random valid board (pick density, fill cells, derive clues).
  - Use in tests to stress the solver beyond hand-crafted presets.
- Tests:
  - Full small puzzles reproducing paper Figs. 1 (unique), 2 (multiple), 3
    (no-solution).
  - Random puzzle batch: generate 20 small puzzles, verify solver terminates
    and solution is valid.
- **Done when**: `ctest` green. CLI solves all 3 paper examples correctly.
  CLI `--stats` reports CB node count.

### M2a — CMake scaffolding + window + grid rendering
*Get a window open and a grid on screen. No interaction yet.*

- CMake: `FetchContent` for glfw, glad, imgui, nlohmann_json; OpenGL 3.3 core
  context.
- Instanced grid renderer:
  - One VBO of cell states (per-cell instance attribute).
  - Vertex shader instanced quad per cell.
  - Fragment shader picks fill/cross/unknown color from instance attribute.
  - 5×5 block gridlines via a separate line shader or geometry.
- App reads JSON preset directly (nlohmann/json) to populate board + clues
  locally, so the UI renders immediately.
- **Done when**: app launches, shows a 5×5 grid with hardcoded cell states
  (all Unknown). JSON preset loads and renders correct grid dimensions.

### M2b — ImGui layer + mouse interaction
*Make the grid interactive. Still no solver calls.*

- ImGui layer:
  - Puzzle selector (lists `puzzles/*.json`).
  - Solve / Step / Reset / Clear buttons (wired to stubs for now).
  - Mode radio: CPU / CUDA.
  - Status text line.
- Mouse:
  - Left click = fill (Black).
  - Right click = cross (White).
  - Drag = paint (mode follows the first cell toggled in the drag).
- Update local cell state only; no solver calls yet.
- **Done when**: can click/drag to paint cells, switch puzzles via the picker,
  and see the grid update in real time.

### M3 — Wire solver into app (in-process)
- Link `solver/` library into the `app` target.
- Solve button → `solver.solve(mode=CPU)` → paint returned cells.
- Step button → `solver.step()` → one full LR pass over all rows+columns
  (or one CB branch if LR is at fixpoint). Animate at ~50ms throttle until `done`.
- Reset/Clear → `solver.reset()`.
- Show solver status string ("solved" / "no solution" / "multiple solutions"
  / "in progress") in ImGui.
- **Async solve**: run the solver on a background `std::thread` so the render
  loop stays responsive. The solver writes to a shared board buffer protected
  by a mutex (or atomic cell states). The UI polls the buffer each frame.
- End-to-end demo on the 5×5 preset.
- **Done when**: Solve button completes the 5×5 and 10×10 presets. Step button
  advances one pass at a time and the grid updates visually. UI remains
  responsive during a solve.

### M4 — CUDA C++ GPU line-solver
- CMake option `ENABLE_CUDA` (default ON), `enable_language(CUDA)`,
  `CMAKE_CUDA_ARCHITECTURES=120`.
- Data layout: pack each line's cells into a device buffer (one byte per cell),
  run ranges into a parallel device buffer.
- Kernel design:
  - One thread block per line (rows + columns batched into one launch).
  - Within a block, enumerate valid placements of the run sequence, compute
    per-cell AND over all valid placements (vectorized Rules 1.1 / 1.2) and
    run-range refinement (Rule 2.x) in a single pass.
  - Write updated cells + ranges back to global memory.
- **Placement enumeration bounding**: the number of valid placements is
  exponential in the worst case (long lines with few constraints). Cap the
  enumeration at a configurable threshold (e.g., 10K placements per line).
  Lines exceeding the threshold fall back to the CPU rule-based approach for
  that iteration. This prevents GPU stalls on pathological inputs.
- Host driver:
  - Launch batched over all rows + all columns.
  - Iterate to fixpoint on device (no per-iteration host sync).
  - Once LR fixpoint reached, copy remaining unknowns back to CPU and hand to
    CPU CB (CB is inherently sequential — stays on CPU).
- Tests: `test_cuda_vs_cpu` asserts GPU LR output == CPU LR output on all
  presets + a batch of random puzzles (using the generator from M1d).
- Benchmark wall-time CPU vs CUDA; log results in README.
- **Done when**: `ctest` green including `test_cuda_vs_cpu`. Benchmark shows
  GPU speedup on 15×15+ puzzles. No hangs on adversarial inputs.

### M5 — Python / PyTorch learning track (standalone)
*Added later. Not wired into the app.*

- Reimplement the GPU LR phase in PyTorch on CUDA:
  - Batched placement enumeration as a tensor.
  - Intersections (Rules 1.1 / 1.2) as tensor reductions.
  - Range refinement (Rule 2.x) as tensor ops.
- `python/benchmarks/compare.py`:
  - Load same JSON presets.
  - Run C++/CUDA solver via the CLI shim (`solver_cli --lr-only`) that prints
    the LR-phase result to stdout.
  - Run PyTorch version.
  - Assert equivalence; benchmark wall-time.
- **Done when**: PyTorch output matches C++/CUDA on all presets. Benchmark
  table (PyTorch vs C++/CUDA) documented in README.

### M6 — Triton kernels (learning goal)
- Replace the PyTorch hot path (placement enumeration + intersection) with
  `@triton.jit` kernels over a tile of lines.
- Keep the PyTorch version as a fallback / comparison baseline.
- Benchmark Triton vs PyTorch vs C++/CUDA; document findings in README. This
  benchmark writeup is the core "learning" deliverable for the Python track.
- **Done when**: Triton output matches PyTorch on all presets. Benchmark
  writeup in README covers all 4 backends with analysis of where each wins/loses.

### M7 — Polish
- More presets, incl. a 25×25.
- Save partial solution to JSON.
- Solver progress bar (CB nodes explored).
- Error toasts in ImGui.
- README with build/run instructions, architecture diagram, and a benchmark
  table (CPU vs CUDA vs PyTorch vs Triton).
- **Undo / redo** (deferred — requires a command stack or snapshot history;
  implement only if time permits after the above):
  - Snapshot-based: save board state before each solver step or mouse action.
  - Ctrl+Z / Ctrl+Shift+Z bound in ImGui.
- **Done when**: 25×25 preset loads and solves. README is complete with
  benchmark table. Save/load round-trips a partial solution.

## Build & run (target)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure

# CLI driver (M1d)
./build/solver_cli/solver_cli puzzles/5x5_heart.json
./build/solver_cli/solver_cli puzzles/10x10_cat.json --stats

# GUI app (M2+)
./build/app/nonogram_app
```

## Decisions log

- **C++ core + CUDA C++** (not Python base, not Triton) — single process,
  removes IPC, lets us learn CUDA natively. Python is a later standalone
  learning track.
- **doctest** over Catch2 — lighter, faster compiles, simpler CMake
  integration for a project this size.
- **`sm_120` pinned** for RTX 5070 Blackwell — fast builds, no fat-binary
  overhead. Revisit if portability matters.
- **CLI driver in M1d** — ~50 lines, gives immediate visual feedback on the
  solver before the GUI exists. Essential for debugging CB.
- **MRV variable ordering** in backtracking — pick the cell in the most
  constrained line. Fundamental CSP technique, dramatically prunes the search
  tree, and is educational to implement.
- **Async solve** — solver runs on a background thread so the ImGui render
  loop stays responsive. Shared board buffer protected by mutex.
- **Placement enumeration cap** in CUDA kernel — prevents exponential blowup
  on pathological inputs. Lines exceeding the threshold fall back to CPU
  rule-based approach.
- **Python track standalone** — reimplements + benchmarks, no socket
  alternative-backend switch in the app. Keeps the C++ architecture clean.
- **JSON only for preset files** — the hot path uses a shared in-process
  `CellState` enum, no marshalling.
- **Undo/redo deferred** — requires snapshot history; implement only if time
  permits after core polish items.
