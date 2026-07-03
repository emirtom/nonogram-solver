#pragma once

#include "nonogram/backtracking.hpp"
#include "nonogram/nonogram.hpp"

namespace nonogram {

enum class Backend { CPU, CUDA };

// Unified façade: runs LR to fixpoint, then CB if needed.
struct SolveOptions {
    Backend backend   = Backend::CPU;
    bool    lr_only   = false;  // stop after the LR fixpoint (for M5 benchmark)
    bool    step_mode = false;  // one LR pass per step() call (for M3 UI)
};

struct SolveOutcome {
    SolveStatus status       = SolveStatus::InProgress;
    long long   nodes        = 0;
    bool        lr_changed   = false;  // last step() changed cells/ranges
};

class Solver {
  public:
    explicit Solver(Nonogram board) : board_(std::move(board)) {}

    Nonogram&       board()       { return board_; }
    const Nonogram& board() const { return board_; }

    // Full solve: LR fixpoint then (optionally) CB.
    SolveOutcome solve(SolveOptions opts = {});

    // One pass of all rules over all rows + columns. Returns whether
    // anything changed. Used by the Step button and for animation.
    SolveOutcome step();

    void reset() { board_.reset(); }

  private:
    Nonogram board_;
};

}  // namespace nonogram
