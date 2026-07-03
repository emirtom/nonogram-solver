# nonogram-solver

C++17 implementation of the nonogram solving algorithm from Yu, Lee & Chen (2009),
"An efficient algorithm for solving nonograms" (*Applied Intelligence* 35:18–31).

## Algorithm

Two phases, exactly as described in the paper:

1. **Logical Rules (LR)** — 11 deterministic rules that progressively color cells
   and refine run ranges. Divided into three parts:
   - Part I (1.1–1.5): determine which cells to color or leave empty.
   - Part II (2.1–2.3): refine the placement range of each black run.
   - Part III (3.1–3.3): combined cell/range deductions.
   Applied iteratively to all rows and columns until a fixpoint is reached.

2. **Chronological Backtracking (CB)** — for any cells left unknown after the LR
   fixpoint. Uses MRV (most-constrained line) variable ordering and re-runs all
   LRs at each node as a look-ahead filter. Detects solved / no-solution /
   multiple-solution outcomes.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Run

```bash
# Solve a puzzle and print the grid as ASCII (# black, . white, ? unknown)
./build/solver_cli/solver_cli puzzles/5x5_heart.json --stats

# Step through one LR pass at a time
./build/solver_cli/solver_cli puzzles/5x5_heart.json --step

# Stop after the LR fixpoint (skip CB)
./build/solver_cli/solver_cli puzzles/5x5_heart.json --lr-only

# Launch the GUI app
./build/app/nonogram_app
```

The GUI app supports:
- **Puzzle picker** — select any JSON preset from `puzzles/`
- **Mouse painting** — left-click = fill (Black), right-click = cross (White)
- **Solve / Step / Reset** buttons — run the solver in-process
- **Status line** — shows solved / no solution / in progress

## Status

LR phase (all 11 rules) and CB with MRV are implemented and tested (47 doctest
cases). 36 puzzle presets (5×5 through 30×30, all with unique solutions) are
included. The GUI app (GLFW + OpenGL + ImGui) is functional with puzzle
selection, mouse interaction, and solver integration. A CUDA GPU line-solver is
planned — see `PLAN.md`.

## Deviation from the paper

**Rule 3.1 — `u < 0` is not a contradiction.**

In the paper, Rule 3.1 gathers scattered black cells in a run's non-overlapping
zone `[lo, hi]` (cells that cannot belong to neighbouring runs). If the span
from the first to the last black cell exceeds the run's length (`u = LBj -
(cn - cm + 1) < 0`), the paper treats this as a contradiction and aborts the
fixpoint.

In practice we found this produces **false contradictions** on larger puzzles.
The non-overlapping zone boundaries are computed from the current range
endpoints of neighbouring runs. When those ranges overlap, `lo` and `hi` can be
imprecise, and `cm`/`cn` may pick up cells that actually belong to *different*
overlapping runs — making the span falsely appear too long. The paper's
assumption that all cells in `[lo, hi]` belong exclusively to one run does not
always hold during intermediate fixpoint passes.

Our implementation skips the run instead (no contradiction, no fill, no range
tightening). The fixpoint loop continues, and the overlapping ranges typically
resolve after more passes. If the span truly is impossible it will eventually
manifest as a different contradiction (e.g. a run range collapsing), at which
point the solver backtracks correctly.

This is the only intentional deviation. All 10 other rules, the fixpoint
driver, and the CB search follow the paper as described.
