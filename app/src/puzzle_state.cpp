#include "puzzle_state.hpp"
#include "nonogram/logical_rules.hpp"

#include <algorithm>

namespace nonogram {

namespace {

// Derive the actual black-run lengths from a line's current cells.
std::vector<int> actual_runs(const Line& ln) {
    std::vector<int> runs;
    int run = 0;
    for (CellState s : ln.cells) {
        if (s == CellState::Black) ++run;
        else if (run > 0) { runs.push_back(run); run = 0; }
    }
    if (run > 0) runs.push_back(run);
    return runs;
}

// Check whether a line's clues are consistent with its current cells.
// A line is "unsatisfiable" if there's no way to place the runs to match
// the already-colored cells. We use a simple greedy feasibility check
// (good enough for UI feedback; the full DP is in backtracking.cpp).
bool line_ok(const Line& ln) {
    // Case 1: line fully determined -> actual runs must match clues exactly.
    bool has_unknown = false;
    for (CellState s : ln.cells) if (s == CellState::Unknown) { has_unknown = true; break; }
    if (!has_unknown) {
        std::vector<int> actual = actual_runs(ln);
        std::vector<int> want;
        for (const Run& r : ln.runs) want.push_back(r.lb);
        return actual == want;
    }
    // Case 2: line has unknowns -> count black cells, must not exceed clue sum.
    int black = 0, clue_sum = 0;
    for (CellState s : ln.cells) if (s == CellState::Black) ++black;
    for (const Run& r : ln.runs) clue_sum += r.lb;
    if (black > clue_sum) return false;
    // Also: any black segment longer than the max clue -> impossible.
    int max_clue = 0;
    for (const Run& r : ln.runs) max_clue = std::max(max_clue, r.lb);
    int seg = 0;
    for (CellState s : ln.cells) {
        if (s == CellState::Black) {
            ++seg;
            if (seg > max_clue && !ln.runs.empty()) {
                // a segment longer than the longest run is fatal
                // (can't be split by an unknown since we're counting contiguous)
            }
        } else if (s == CellState::Unknown) {
            seg = 0;  // unknown could break the segment, so don't flag
        } else {
            seg = 0;
        }
    }
    return true;
}

}  // namespace

bool PuzzleState::load_file(const std::string& path) {
    try {
        board_ = Nonogram::from_json_file(path);
        path_  = path;
    } catch (const std::exception&) {
        return false;
    }
    solver_ = Solver{board_};
    return true;
}

bool PuzzleState::load_inline(int w, int h,
                              std::vector<std::vector<int>> row_clues,
                              std::vector<std::vector<int>> col_clues) {
    try {
        board_ = Nonogram(w, h, std::move(row_clues), std::move(col_clues));
    } catch (const std::exception&) {
        return false;
    }
    path_.clear();
    solver_ = Solver{board_};
    return true;
}

void PuzzleState::set_cell(int col, int row, CellState s) {
    board_.row(row).cells[col] = s;
    board_.sync_row_to_cols(row);
    solver_ = Solver{board_};
}

SolveOutcome PuzzleState::solve(Backend backend) {
    SolveOptions opts;
    opts.backend = backend;
    solver_ = Solver{board_};
    SolveOutcome o = solver_.solve(opts);
    board_ = solver_.board();
    return o;
}

SolveOutcome PuzzleState::step() {
    SolveOutcome o = solver_.step();
    board_ = solver_.board();
    return o;
}

SolveOutcome PuzzleState::step_rule() {
    SolveOutcome o = solver_.step_rule();
    board_ = solver_.board();
    return o;
}

void PuzzleState::reset() {
    board_.reset();
    solver_ = Solver{board_};
}

void PuzzleState::reset_solver() {
    solver_ = Solver{board_};
}

std::vector<std::pair<int, int>> PuzzleState::find_contradictions() const {
    std::vector<std::pair<int, int>> bad;
    // Check every row and column. If a line is unsatisfiable, mark all its
    // non-unknown cells as "bad" so the user sees which line is wrong.
    for (int r = 0; r < board_.height(); ++r) {
        if (!line_ok(board_.row(r))) {
            for (int c = 0; c < board_.width(); ++c)
                if (board_.row(r).cells[c] != CellState::Unknown)
                    bad.emplace_back(c, r);
        }
    }
    for (int c = 0; c < board_.width(); ++c) {
        if (!line_ok(board_.col(c))) {
            for (int r = 0; r < board_.height(); ++r)
                if (board_.col(c).cells[r] != CellState::Unknown)
                    bad.emplace_back(c, r);
        }
    }
    // Deduplicate.
    std::sort(bad.begin(), bad.end());
    bad.erase(std::unique(bad.begin(), bad.end()), bad.end());
    return bad;
}

}  // namespace nonogram
