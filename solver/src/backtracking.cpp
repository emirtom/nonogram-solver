#include "nonogram/backtracking.hpp"
#include "nonogram/logical_rules.hpp"

#include <climits>
#include <utility>
#include <vector>
namespace nonogram {

namespace {

// DP line-satisfiability check: is there at least one valid placement of the
// runs into the line consistent with the current Black/White/Unknown cells?
// O(n*k) per line. Used as a look-ahead filter at each CB node — catches
// contradictions the local LR rules miss (e.g., too many black segments).
//
// feasible(i, j) = can runs j..k-1 be placed in cells i..n-1?
//   - j == k: yes iff no Black cell in i..n-1
//   - j <  k: either leave cell i non-Black and recurse (i+1, j),
//             or start run j at i (cells i..i+lb-1 non-White, separator at
//             i+lb non-Black) and recurse (i+lb+1, j+1).
bool line_satisfiable(const Line& line) {
    const int n = line.size();
    const int k = line.num_runs();
    std::vector<char> dp((n + 1) * (k + 1), 0);
    auto idx = [&](int i, int j) { return i * (k + 1) + j; };
    dp[idx(n, k)] = 1;
    for (int i = n - 1; i >= 0; --i)
        dp[idx(i, k)] = dp[idx(i + 1, k)] && (line.cells[i] != CellState::Black);
    for (int j = k - 1; j >= 0; --j) {
        const int lb = line.runs[j].lb;
        for (int i = n - 1; i >= 0; --i) {
            bool ok = false;
            // option 1: cell i is not Black -> skip it
            if (line.cells[i] != CellState::Black)
                ok = ok || dp[idx(i + 1, j)];
            // option 2: start run j at i
            if (!ok && i + lb <= n) {
                bool can_place = true;
                for (int p = i; p < i + lb; ++p)
                    if (line.cells[p] == CellState::White) { can_place = false; break; }
                if (can_place) {
                    const int after = i + lb;
                    if (after < n) {
                        if (line.cells[after] != CellState::Black)
                            ok = dp[idx(after + 1, j + 1)];
                    } else {
                        ok = dp[idx(after, j + 1)];
                    }
                }
            }
            dp[idx(i, j)] = ok ? 1 : 0;
        }
    }
    return dp[idx(0, 0)] == 1;
}

bool all_lines_satisfiable(const Nonogram& b) {
    for (int r = 0; r < b.height(); ++r)
        if (!line_satisfiable(b.row(r))) return false;
    for (int c = 0; c < b.width(); ++c)
        if (!line_satisfiable(b.col(c))) return false;
    return true;
}

}  // namespace (anonymous)

// MRV: pick an Unknown cell in the line (row or column) with the fewest
// remaining Unknowns. Returns {-1,-1} if the board is complete.
// (Exposed via the header for Solver::step().)
std::pair<int, int> pick_mrv_cell(const Nonogram& b) {
    int best_unk = INT_MAX;
    std::pair<int, int> best = {-1, -1};
    for (int r = 0; r < b.height(); ++r) {
        int unk = 0, first = -1;
        for (int c = 0; c < b.width(); ++c)
            if (b.row(r).cells[c] == CellState::Unknown) { ++unk; if (first < 0) first = c; }
        if (unk > 0 && unk < best_unk) { best_unk = unk; best = {r, first}; }
    }
    for (int c = 0; c < b.width(); ++c) {
        int unk = 0, first = -1;
        for (int r = 0; r < b.height(); ++r)
            if (b.col(c).cells[r] == CellState::Unknown) { ++unk; if (first < 0) first = r; }
        if (unk > 0 && unk < best_unk) { best_unk = unk; best = {first, c}; }
    }
    return best;
}

namespace {

void set_cell(Nonogram& b, int r, int c, CellState s) {
    b.row(r).cells[c] = s;
    b.sync_row_to_cols(r);
}

// Recursive CB search. Updates `found_count` and writes the first complete
// solution into `first_solution`. Stops early once `found_count >= limit`.
void cb_search(Nonogram& b, int& found_count, Nonogram& first_solution,
               long long& nodes, int limit) {
    if (found_count >= limit) return;
    ++nodes;
    if (!run_to_fixpoint(b)) return;
    if (!all_lines_satisfiable(b)) return;
    if (b.is_complete()) {
        if (b.is_valid_solution()) {
            ++found_count;
            if (found_count == 1) first_solution = b;
        }
        return;
    }
    auto [r, c] = pick_mrv_cell(b);
    if (r < 0) return;  // no unknown cell but not complete (shouldn't happen)

    Nonogram copy = b;
    set_cell(copy, r, c, CellState::Black);
    cb_search(copy, found_count, first_solution, nodes, limit);
    if (found_count >= limit) return;

    copy = b;
    set_cell(copy, r, c, CellState::White);
    cb_search(copy, found_count, first_solution, nodes, limit);
}

}  // namespace

SolveResult chronological_backtracking(Nonogram& board) {
    if (!run_to_fixpoint(board)) return {SolveStatus::NoSolution, 0};
    if (board.is_complete() && board.is_valid_solution())
        return {SolveStatus::Solved, 0};
    if (board.is_complete())
        return {SolveStatus::NoSolution, 0};

    int found = 0;
    Nonogram first_solution = board;
    long long nodes = 0;
    const int limit = 2;  // stop after finding 2 (multiple-solution detection)
    cb_search(board, found, first_solution, nodes, limit);

    if (found == 0) return {SolveStatus::NoSolution, nodes};
    if (found == 1) { board = first_solution; return {SolveStatus::Solved, nodes}; }
    return {SolveStatus::MultipleSolutions, nodes};
}

}  // namespace nonogram
