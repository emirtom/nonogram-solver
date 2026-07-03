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
```

## Status

LR phase (all 11 rules) and CB with MRV are implemented and tested (47 doctest
cases). A GUI app (GLFW + ImGui) and a CUDA GPU line-solver are planned — see
`PLAN.md`.
