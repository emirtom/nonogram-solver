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
    out.rule_index = -1;

    if (board_.is_complete() && board_.is_valid_solution()) {
        out.status = SolveStatus::Solved;
        return out;
    }
    if (board_.is_complete()) {
        out.status = SolveStatus::NoSolution;
        return out;
    }

    if (!any_change) {
        auto [r, c] = pick_mrv_cell(board_);
        if (r < 0) return out;

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

        out.status = SolveStatus::NoSolution;
        return out;
    }

    return out;
}

SolveOutcome Solver::step_rule() {
    SolveOutcome out;

    if (step_rule_index_ >= NUM_RULES) {
        step_rule_index_ = 0;
        if (!step_pass_had_change_) {
            if (board_.is_complete() && board_.is_valid_solution()) {
                out.status = SolveStatus::Solved;
                out.rule_index = -1;
                return out;
            }
            if (board_.is_complete()) {
                out.status = SolveStatus::NoSolution;
                out.rule_index = -1;
                return out;
            }

            auto [r, c] = pick_mrv_cell(board_);
            if (r >= 0) {
                Nonogram try_black = board_;
                try_black.row(r).cells[c] = CellState::Black;
                try_black.sync_row_to_cols(r);
                if (run_to_fixpoint(try_black) && !try_black.is_complete()) {
                    board_ = std::move(try_black);
                    out.lr_changed = true;
                    out.rule_index = -1;
                    if (board_.is_complete() && board_.is_valid_solution()) out.status = SolveStatus::Solved;
                    return out;
                }
                if (try_black.is_complete() && try_black.is_valid_solution()) {
                    board_ = std::move(try_black);
                    out.status = SolveStatus::Solved;
                    out.lr_changed = true;
                    out.rule_index = -1;
                    return out;
                }

                Nonogram try_white = board_;
                try_white.row(r).cells[c] = CellState::White;
                try_white.sync_row_to_cols(r);
                if (run_to_fixpoint(try_white) && !try_white.is_complete()) {
                    board_ = std::move(try_white);
                    out.lr_changed = true;
                    out.rule_index = -1;
                    if (board_.is_complete() && board_.is_valid_solution()) out.status = SolveStatus::Solved;
                    return out;
                }
                if (try_white.is_complete() && try_white.is_valid_solution()) {
                    board_ = std::move(try_white);
                    out.status = SolveStatus::Solved;
                    out.lr_changed = true;
                    out.rule_index = -1;
                    return out;
                }

                out.status = SolveStatus::NoSolution;
                out.rule_index = -1;
                return out;
            }
        }
        step_pass_had_change_ = false;
    }

    bool any_change = false;

    for (int r = 0; r < board_.height(); ++r) {
        LineUpdate u = apply_single_rule(step_rule_index_, board_.row(r));
        if (u.contradiction) {
            out.status = SolveStatus::NoSolution;
            out.rule_index = step_rule_index_;
            return out;
        }
        if (u.cells_changed) board_.sync_row_to_cols(r);
        any_change |= u.cells_changed || u.ranges_changed;
    }

    for (int c = 0; c < board_.width(); ++c) {
        LineUpdate u = apply_single_rule(step_rule_index_, board_.col(c));
        if (u.contradiction) {
            out.status = SolveStatus::NoSolution;
            out.rule_index = step_rule_index_;
            return out;
        }
        if (u.cells_changed) board_.sync_col_to_rows(c);
        any_change |= u.cells_changed || u.ranges_changed;
    }

    out.lr_changed = any_change;
    out.rule_index = step_rule_index_;
    step_pass_had_change_ |= any_change;

    ++step_rule_index_;

    if (board_.is_complete() && board_.is_valid_solution()) {
        out.status = SolveStatus::Solved;
        return out;
    }

    return out;
}

}  // namespace nonogram
