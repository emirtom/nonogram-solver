#include "nonogram/nonogram.hpp"
#include "nonogram/logical_rules.hpp"
#include "nonogram/solver.hpp"

#include <doctest/doctest.h>

using namespace nonogram;

// TODO M1d — random puzzle generator + batch tests.
TEST_CASE("Random generator placeholder: trivial solve via LR") {
    // A single fully-determined row/column: clue [n] on an n-wide board.
    Nonogram b(5, 1, {{5}}, {{1}, {1}, {1}, {1}, {1}});
    bool ok = run_to_fixpoint(b);
    CHECK(ok);
    // Once rules are implemented this should fully solve.
}

TEST_CASE("End-to-end: all-black 5x5 solvable by Rule 1.1 alone") {
    Nonogram b(5, 5,
               {{5}, {5}, {5}, {5}, {5}},
               {{5}, {5}, {5}, {5}, {5}});
    bool ok = run_to_fixpoint(b);
    REQUIRE(ok);
    REQUIRE(b.is_complete());
    for (int r = 0; r < 5; ++r)
        for (int c = 0; c < 5; ++c)
            CHECK(b.row(r).cells[c] == CellState::Black);
}

TEST_CASE("End-to-end: plus sign solved by LR (Part III 3.1/3.3)") {
    Nonogram b(5, 5,
               {{1}, {1}, {5}, {1}, {1}},
               {{1}, {1}, {5}, {1}, {1}});
    bool ok = run_to_fixpoint(b);
    REQUIRE(ok);
    REQUIRE(b.is_complete());
    // Plus pattern: only col 2 and row 2 are Black.
    for (int r = 0; r < 5; ++r) {
        for (int c = 0; c < 5; ++c) {
            if (r == 2 || c == 2)
                CHECK(b.row(r).cells[c] == CellState::Black);
            else
                CHECK(b.row(r).cells[c] == CellState::White);
        }
    }
}

TEST_CASE("End-to-end: H-shape solved by LR (Part I 1.1+1.2)") {
    Nonogram b(5, 5,
               {{2, 2}, {2, 2}, {5}, {2, 2}, {2, 2}},
               {{5}, {5}, {1}, {5}, {5}});
    bool ok = run_to_fixpoint(b);
    REQUIRE(ok);
    REQUIRE(b.is_complete());
    // Rows 0,1,3,4: ##.## ; Row 2: #####
    for (int r = 0; r < 5; ++r) {
        for (int c = 0; c < 5; ++c) {
            if (r == 2 || c != 2)
                CHECK(b.row(r).cells[c] == CellState::Black);
            else
                CHECK(b.row(r).cells[c] == CellState::White);
        }
    }
}

TEST_CASE("End-to-end: heart partial — LR fixpoint leaves unknowns") {
    Nonogram b(5, 5,
               {{1, 1}, {5}, {5}, {3}, {1}},
               {{2}, {4}, {4}, {4}, {2}});
    bool ok = run_to_fixpoint(b);
    REQUIRE(ok);
    CHECK_FALSE(b.is_complete());  // needs CB
    // Rows 1,2 fully Black; row 3 = .###.
    for (int c = 0; c < 5; ++c) {
        CHECK(b.row(1).cells[c] == CellState::Black);
        CHECK(b.row(2).cells[c] == CellState::Black);
    }
    CHECK(b.row(3).cells[0] == CellState::White);
    CHECK(b.row(3).cells[1] == CellState::Black);
    CHECK(b.row(3).cells[2] == CellState::Black);
    CHECK(b.row(3).cells[3] == CellState::Black);
    CHECK(b.row(3).cells[4] == CellState::White);
}
