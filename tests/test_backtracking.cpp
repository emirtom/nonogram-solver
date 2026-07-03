#include "nonogram/nonogram.hpp"
#include "nonogram/backtracking.hpp"
#include "nonogram/logical_rules.hpp"
#include "nonogram/solver.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <random>
#include <vector>

using namespace nonogram;

// ---- Paper Fig. 1: a puzzle with a unique solution (the heart) --------------

TEST_CASE("CB: heart 5x5 has a unique solution") {
    Nonogram b(5, 5,
               {{1, 1}, {5}, {5}, {3}, {1}},
               {{2}, {4}, {4}, {4}, {2}});
    bool ok = run_to_fixpoint(b);
    REQUIRE(ok);
    REQUIRE_FALSE(b.is_complete());  // LR alone doesn't solve it
    SolveResult r = chronological_backtracking(b);
    CHECK(r.status == SolveStatus::Solved);
    CHECK(r.nodes_explored > 0);
    REQUIRE(b.is_complete());
    // Expected unique solution:
    //   .#.#.
    //   #####
    //   #####
    //   .###.
    //   ..#..
    const char* expected[5] = {".#.#.", "#####", "#####", ".###.", "..#.."};
    for (int row = 0; row < 5; ++row)
        for (int c = 0; c < 5; ++c) {
            char ch = expected[row][c];
            CellState want = (ch == '#') ? CellState::Black : CellState::White;
            CHECK(b.row(row).cells[c] == want);
        }
}

// ---- Paper Fig. 2: a puzzle with multiple solutions ------------------------

TEST_CASE("CB: 2x2 diagonal puzzle has multiple solutions") {
    // rows [1],[1] and cols [1],[1]: two solutions (the two diagonals).
    Nonogram b(2, 2, {{1}, {1}}, {{1}, {1}});
    SolveResult r = chronological_backtracking(b);
    CHECK(r.status == SolveStatus::MultipleSolutions);
}

// ---- Paper Fig. 3: a puzzle with no solution --------------------------------

TEST_CASE("CB: 2x2 over-constrained puzzle has no solution") {
    // rows [2],[2] force all cells black; cols [1],[1] want one black per
    // column -> contradiction.
    Nonogram b(2, 2, {{2}, {2}}, {{1}, {1}});
    SolveResult r = chronological_backtracking(b);
    CHECK(r.status == SolveStatus::NoSolution);
}

TEST_CASE("CB: LR detects contradiction at root -> NoSolution without CB") {
    // Same no-solution puzzle via the Solver façade: LR fixpoint should
    // already detect the contradiction (Rule 1.1 forces row cells black,
    // then column check conflicts).
    Nonogram b(2, 2, {{2}, {2}}, {{1}, {1}});
    Solver solver(std::move(b));
    SolveOutcome o = solver.solve();
    CHECK(o.status == SolveStatus::NoSolution);
}

// ---- Solver façade end-to-end ----------------------------------------------

TEST_CASE("Solver façade: heart via solve() returns Solved") {
    Nonogram b(5, 5,
               {{1, 1}, {5}, {5}, {3}, {1}},
               {{2}, {4}, {4}, {4}, {2}});
    Solver solver(std::move(b));
    SolveOutcome o = solver.solve();
    CHECK(o.status == SolveStatus::Solved);
    CHECK(solver.board().is_complete());
}

// ---- Random puzzle batch ----------------------------------------------------

namespace {

// Derive row/column clues from a fully-determined grid of Black/White.
std::vector<std::vector<int>> derive_clues(const std::vector<std::vector<CellState>>& grid,
                                           bool by_rows) {
    int outer = static_cast<int>(grid.size());
    int inner = static_cast<int>(grid[0].size());
    int lines = by_rows ? outer : inner;
    int len   = by_rows ? inner : outer;
    std::vector<std::vector<int>> clues(lines);
    for (int l = 0; l < lines; ++l) {
        int run = 0;
        for (int i = 0; i < len; ++i) {
            CellState s = by_rows ? grid[l][i] : grid[i][l];
            if (s == CellState::Black) {
                ++run;
            } else if (run > 0) {
                clues[l].push_back(run);
                run = 0;
            }
        }
        if (run > 0) clues[l].push_back(run);
    }
    return clues;
}

// Generate a random valid puzzle: fill a grid with the given black density,
// derive clues, and build a Nonogram from those clues (cells start Unknown).
Nonogram random_puzzle(int w, int h, double density, std::uint32_t seed) {
    std::mt19937 rng(seed);
    std::bernoulli_distribution bit(density);
    std::vector<std::vector<CellState>> grid(h, std::vector<CellState>(w));
    for (int r = 0; r < h; ++r)
        for (int c = 0; c < w; ++c)
            grid[r][c] = bit(rng) ? CellState::Black : CellState::White;
    auto row_clues = derive_clues(grid, true);
    auto col_clues = derive_clues(grid, false);
    return Nonogram(w, h, std::move(row_clues), std::move(col_clues));
}

// Verify a complete board satisfies all its clues.
bool board_matches_clues(const Nonogram& b) {
    auto check = [](const Line& ln) {
        std::vector<int> segs;
        int run = 0;
        for (CellState s : ln.cells) {
            if (s == CellState::Black) ++run;
            else if (run > 0) { segs.push_back(run); run = 0; }
        }
        if (run > 0) segs.push_back(run);
        if (segs.size() != ln.runs.size()) return false;
        for (size_t i = 0; i < segs.size(); ++i)
            if (segs[i] != ln.runs[i].lb) return false;
        return true;
    };
    for (int r = 0; r < b.height(); ++r) if (!check(b.row(r))) return false;
    for (int c = 0; c < b.width(); ++c)  if (!check(b.col(c))) return false;
    return true;
}

}  // namespace

TEST_CASE("Random puzzle batch: solver terminates and solutions are valid") {
    const int N = 20;
    int solved = 0, multiple = 0, nosolution = 0;
    for (int i = 0; i < N; ++i) {
        Nonogram b = random_puzzle(5, 5, 0.5, 0x5EED + i);
        Solver solver(std::move(b));
        SolveOutcome o = solver.solve();
        if (o.status == SolveStatus::Solved) {
            ++solved;
            CHECK(solver.board().is_complete());
            CHECK(board_matches_clues(solver.board()));
        } else if (o.status == SolveStatus::MultipleSolutions) {
            ++multiple;
        } else if (o.status == SolveStatus::NoSolution) {
            ++nosolution;
        }
        // Generated from a valid board -> at least one solution must exist,
        // so NoSolution must never happen.
        CHECK(o.status != SolveStatus::NoSolution);
    }
    CHECK(solved + multiple == N);
}
