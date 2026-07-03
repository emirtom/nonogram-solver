#pragma once

#include "nonogram/nonogram.hpp"

#include <utility>

namespace nonogram {

enum class SolveStatus {
    InProgress,
    Solved,
    NoSolution,
    MultipleSolutions,
};

struct SolveResult {
    SolveStatus status = SolveStatus::InProgress;
    long long nodes_explored = 0;  // CB nodes visited
};

// If the board has Unknown cells, returns the (row, col) of one in the
// most-constrained line (fewest unknowns). Returns {-1,-1} if complete.
// Exposed so the Solver's step() can do one CB branch when LR is at fixpoint.
std::pair<int, int> pick_mrv_cell(const Nonogram& board);

// Chronological Backtracking with LR filter (paper Section 2.2).
// Assumes run_to_fixpoint() has already been applied to `board` and
// returned true (no contradiction at the root). Picks an Unknown cell
// via MRV (most-constrained line first), branches Black then White, and
// re-runs all LRs at each node as a look-ahead filter.
//
// Stops at the first complete solution (status=Solved) or on proving
// unsatisfiability (status=NoSolution). If a second solution is found
// before the search is exhausted, returns MultipleSolutions.
SolveResult chronological_backtracking(Nonogram& board);

}  // namespace nonogram
