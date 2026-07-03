#pragma once

#include "nonogram/nonogram.hpp"

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
