#include "nonogram/solver.hpp"
#include "nonogram/logical_rules.hpp"

namespace nonogram {

SolveOutcome Solver::solve(SolveOptions opts) {
    SolveOutcome out;
    bool ok = run_to_fixpoint(board_);
    if (!ok) {
        out.status = SolveStatus::NoSolution;
        return out;
    }
    if (board_.is_complete()) {
        out.status = SolveStatus::Solved;
        return out;
    }
    if (opts.lr_only) {
        out.status = SolveStatus::InProgress;
        return out;
    }
    SolveResult r = chronological_backtracking(board_);
    out.status = r.status;
    out.nodes  = r.nodes_explored;
    return out;
}

SolveOutcome Solver::step() {
    SolveOutcome out;
    bool any_change = false;
    for (int r = 0; r < board_.height(); ++r) {
        LineUpdate u = apply_all_rules(board_.row(r));
        if (u.cells_changed)  board_.sync_row_to_cols(r);
        any_change |= u.cells_changed || u.ranges_changed;
    }
    for (int c = 0; c < board_.width(); ++c) {
        LineUpdate u = apply_all_rules(board_.col(c));
        if (u.cells_changed)  board_.sync_col_to_rows(c);
        any_change |= u.cells_changed || u.ranges_changed;
    }
    out.lr_changed = any_change;
    if (board_.is_complete()) out.status = SolveStatus::Solved;
    return out;
}

}  // namespace nonogram
