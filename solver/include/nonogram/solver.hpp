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
    int         rule_index   = -1;     // which rule (0..10) was applied last (-1 for full step)
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

    // Apply a single rule (0..10) to all rows then all columns. Automatically
    // advances to the next rule; when NUM_RULES is reached a full pass is
    // completed and CB is triggered if the pass had no changes.
    SolveOutcome step_rule();

    // Current rule index (0..10), or -1 if not tracking.
    int rule_index() const { return step_rule_index_; }

    void reset() { board_.reset(); step_rule_index_ = 0; step_pass_had_change_ = false; }

  private:
    Nonogram board_;
    int  step_rule_index_     = 0;
    bool step_pass_had_change_ = false;
};

}  // namespace nonogram
