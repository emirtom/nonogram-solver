#include "nonogram/solver.hpp"
#include "nonogram/backtracking.hpp"
#include "nonogram/logical_rules.hpp"

namespace nonogram {

SolveOutcome Solver::solve(SolveOptions opts) {
    SolveOutcome out;
    bool ok = run_to_fixpoint(board_);
    if (ok && board_.is_complete() && board_.is_valid_solution()) {
        out.status = SolveStatus::Solved;
        return out;
    }
    if (ok && board_.is_complete()) {
        out.status = SolveStatus::NoSolution;
        return out;
    }
    if (opts.lr_only) {
        out.status = ok ? SolveStatus::InProgress : SolveStatus::NoSolution;
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

    if (board_.is_complete() && board_.is_valid_solution()) {
        out.status = SolveStatus::Solved;
        return out;
    }
    if (board_.is_complete()) {
        out.status = SolveStatus::NoSolution;
        return out;
    }

    // If LR is at fixpoint and the board isn't complete, do one CB branch:
    // pick an MRV cell, try Black + LR, if that contradicts try White + LR.
    if (!any_change) {
        auto [r, c] = pick_mrv_cell(board_);
        if (r < 0) return out;  // no unknowns but not complete — shouldn't happen

        Nonogram try_black = board_;
        try_black.row(r).cells[c] = CellState::Black;
        try_black.sync_row_to_cols(r);
        if (run_to_fixpoint(try_black) && !try_black.is_complete()) {
            board_ = std::move(try_black);
            out.lr_changed = true;
            if (board_.is_complete() && board_.is_valid_solution()) out.status = SolveStatus::Solved;
            return out;
        }
        if (try_black.is_complete() && try_black.is_valid_solution()) {
            board_ = std::move(try_black);
            out.status = SolveStatus::Solved;
            out.lr_changed = true;
            return out;
        }

        // Black led to contradiction — try White.
        Nonogram try_white = board_;
        try_white.row(r).cells[c] = CellState::White;
        try_white.sync_row_to_cols(r);
        if (run_to_fixpoint(try_white) && !try_white.is_complete()) {
            board_ = std::move(try_white);
            out.lr_changed = true;
            if (board_.is_complete() && board_.is_valid_solution()) out.status = SolveStatus::Solved;
            return out;
        }
        if (try_white.is_complete() && try_white.is_valid_solution()) {
            board_ = std::move(try_white);
            out.status = SolveStatus::Solved;
            out.lr_changed = true;
            return out;
        }

        // Both branches contradicted — no solution from this state.
        out.status = SolveStatus::NoSolution;
        return out;
    }

    return out;
}

}  // namespace nonogram
