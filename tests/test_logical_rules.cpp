#include "nonogram/logical_rules.hpp"
#include "nonogram/types.hpp"

#include <doctest/doctest.h>

using namespace nonogram;

// Build a line of n Unknown cells with the given clue lengths and
// (optionally) pre-set cell states. Run ranges are initialized per Eq. 1,
// then optionally overridden via `ranges`.
static Line make_line(int n, std::vector<int> clues,
                      std::vector<CellState> cells = {},
                      std::vector<std::pair<int,int>> ranges = {}) {
    Line ln;
    ln.cells.assign(n, CellState::Unknown);
    if (!cells.empty()) ln.cells = std::move(cells);
    for (int lb : clues) ln.runs.push_back(Run{lb, RunRange{}});
    init_run_ranges(ln);
    for (size_t j = 0; j < ranges.size() && j < ln.runs.size(); ++j)
        ln.runs[j].range = {ranges[j].first, ranges[j].second};
    return ln;
}

TEST_CASE("Run-range init: paper Fig. 6 clue [1,3,2] on n=10") {
    // Paper Fig. 6: clue (1, 3, 2), expected ranges (0,2),(2,6),(6,9).
    Line ln = make_line(10, {1, 3, 2});
    REQUIRE(ln.num_runs() == 3);
    CHECK(ln.runs[0].range.start == 0);
    CHECK(ln.runs[0].range.end   == 2);
    CHECK(ln.runs[1].range.start == 2);
    CHECK(ln.runs[1].range.end   == 6);
    CHECK(ln.runs[2].range.start == 6);
    CHECK(ln.runs[2].range.end   == 9);
}

TEST_CASE("Rule 1.1: intersection of left/right-most placements") {
    // Paper Fig. 6: clue (1, 3, 2) on n=10.
    //   run 0: LB=1, range [0,2], u=2 -> [2,0] empty
    //   run 1: LB=3, range [2,6], u=2 -> [4,4] colors cell 4
    //   run 2: LB=2, range [6,9], u=2 -> [8,7] empty
    Line ln = make_line(10, {1, 3, 2});
    LineUpdate u = rule_1_1(ln);
    REQUIRE_FALSE(u.contradiction);
    REQUIRE(u.cells_changed);
    for (int i = 0; i < 10; ++i) {
        if (i == 4) CHECK(ln.cells[i] == CellState::Black);
        else        CHECK(ln.cells[i] == CellState::Unknown);
    }
}

TEST_CASE("Rule 1.1: tight range colors the whole run") {
    // Clue [3] on n=3: range [0,2], u=0 -> color [0,2] entirely.
    Line ln = make_line(3, {3});
    LineUpdate u = rule_1_1(ln);
    REQUIRE_FALSE(u.contradiction);
    CHECK(ln.cells[0] == CellState::Black);
    CHECK(ln.cells[1] == CellState::Black);
    CHECK(ln.cells[2] == CellState::Black);
}

TEST_CASE("Rule 1.1: slack range colors nothing") {
    // Clue [1] on n=5: range [0,4], u=4 -> [4,0] empty, no cell colored.
    Line ln = make_line(5, {1});
    LineUpdate u = rule_1_1(ln);
    REQUIRE_FALSE(u.contradiction);
    CHECK_FALSE(u.cells_changed);
    for (CellState s : ln.cells) CHECK(s == CellState::Unknown);
}

TEST_CASE("Rule 1.1: contradiction if forced cell is already White") {
    // Clue [3] on n=3 forces all cells black; pre-mark cell 1 White -> conflict.
    Line ln = make_line(3, {3}, {CellState::Unknown, CellState::White, CellState::Unknown});
    LineUpdate u = rule_1_1(ln);
    CHECK(u.contradiction);
}

TEST_CASE("Rule 1.2: cells outside all run ranges are empty") {
    // Clue [3, 2] on n=8:
    //   run 0: range [0,4], run 1: range [4,7]. Union = [0,7]. Nothing outside.
    Line ln = make_line(8, {3, 2});
    LineUpdate u = rule_1_2(ln);
    REQUIRE_FALSE(u.contradiction);
    CHECK_FALSE(u.cells_changed);  // all cells covered

    // Clue [3] on n=6: range [0,5], all covered -> nothing to empty.
    Line ln2 = make_line(6, {3});
    LineUpdate u2 = rule_1_2(ln2);
    REQUIRE_FALSE(u2.contradiction);
    CHECK_FALSE(u2.cells_changed);

    // Clue [1, 1] on n=6:
    //   run 0: range [0,4], run 1: range [2,5]. Union [0,5]. All covered.
    // Use a clue that leaves a real gap.
    // Clue [2, 2] on n=8:
    //   run 0: range [0,4], run 1: range [3,7]. Union [0,7]. All covered.
    // Clue [2] on n=6: range [0,5] -> all covered.
    // To get an outside cell we need a clue whose left-most + right-most
    // placements don't reach the ends. That needs multiple runs with slack.
    // Clue [1, 1] on n=5: run0 [0,3], run1 [1,4] -> union [0,4], all covered.
    // Hmm — initial ranges always tile the whole line (Eq.1 packs them).
    // So Rule 1.2 only fires *after* ranges are refined by other rules.
    // Simulate a refined range manually:
    Line ln3 = make_line(6, {2});
    ln3.runs[0].range = {1, 3};  // shrunk: cell 0 and 4,5 are outside
    LineUpdate u3 = rule_1_2(ln3);
    REQUIRE_FALSE(u3.contradiction);
    REQUIRE(u3.cells_changed);
    CHECK(ln3.cells[0] == CellState::White);
    CHECK(ln3.cells[4] == CellState::White);
    CHECK(ln3.cells[5] == CellState::White);
    CHECK(ln3.cells[1] == CellState::Unknown);
    CHECK(ln3.cells[2] == CellState::Unknown);
    CHECK(ln3.cells[3] == CellState::Unknown);
}

TEST_CASE("Rule 1.2: gap between two runs becomes empty") {
    // Clue [2, 2] on n=8, ranges manually shrunk to leave a gap:
    //   run0 [0,2], run1 [5,7] -> cells 3,4 are between runs -> empty.
    Line ln = make_line(8, {2, 2});
    ln.runs[0].range = {0, 2};
    ln.runs[1].range = {5, 7};
    LineUpdate u = rule_1_2(ln);
    REQUIRE_FALSE(u.contradiction);
    REQUIRE(u.cells_changed);
    CHECK(ln.cells[3] == CellState::White);
    CHECK(ln.cells[4] == CellState::White);
}

TEST_CASE("Rule 1.2: contradiction if outside cell is already Black") {
    Line ln = make_line(6, {2});
    ln.runs[0].range = {1, 3};  // cell 0 outside
    ln.cells[0] = CellState::Black;
    LineUpdate u = rule_1_2(ln);
    CHECK(u.contradiction);
}

TEST_CASE("Rule 1.3: boundary cell covered only by length-1 runs") {
    // Paper Fig. 8: a row with runs [1, 1, 3] on n=10.
    // Eq.1 ranges: run0 [0,3], run1 [2,5], run2 [4,9].
    // Cell 4 = run2.range.start, colored, covered by run1 (lb=1, the only
    // other covering run) -> cell 3 (s-1) becomes empty.
    Line ln = make_line(10, {1, 1, 3});
    ln.cells[4] = CellState::Black;  // c_{rj_s} of run 2
    LineUpdate u = rule_1_3(ln);
    REQUIRE_FALSE(u.contradiction);
    REQUIRE(u.cells_changed);
    CHECK(ln.cells[3] == CellState::White);
}

TEST_CASE("Rule 1.3: does not fire when a covering run has length > 1") {
    // Runs [2, 3] on n=8: run0 [0,4], run1 [3,7]. Cell 3 = run1.range.start,
    // colored. It is covered by run0 (lb=2). Since run0.lb != 1, no empty.
    Line ln = make_line(8, {2, 3});
    ln.cells[3] = CellState::Black;
    LineUpdate u = rule_1_3(ln);
    REQUIRE_FALSE(u.contradiction);
    CHECK_FALSE(u.cells_changed);
}

TEST_CASE("Rule 1.3: end-boundary cell -> empty cell after range") {
    // Runs [3, 1, 1] on n=10: run0 [0,7], run1 [4,8], run2 [6,9].
    // Cell 6 = run2.range.start, colored, covered by run0(lb=3) and run1(lb=1).
    // run0.lb != 1 -> no fire at start. Try cell 7 = run1.range.end, covered by
    // run0(lb=3) and run2(lb=1). run0.lb != 1 -> no fire either. Construct a
    // minimal firing case at the end instead:
    // Runs [1, 1] on n=5: run0 [0,3], run1 [2,4]. Cell 4 = run1.range.end,
    // colored, covered by run0 (lb=1) -> cell 5 (e+1, out of range) ignored,
    // but cell 5 is past n. Use n=6 with shrunk run1 range [3,4]:
    Line ln = make_line(6, {1, 1}, {}, {});
    ln.runs[1].range = {3, 4};
    ln.cells[4] = CellState::Black;  // c_{rj_e} of run 1, covered by run0 (lb=1)
    LineUpdate u = rule_1_3(ln);
    REQUIRE_FALSE(u.contradiction);
    REQUIRE(u.cells_changed);
    CHECK(ln.cells[5] == CellState::White);
}

TEST_CASE("Rule 1.4: merging two segments exceeds maxL -> empty") {
    // Paper Fig. 9: clue with maxL=3. Two black segments length 2 separated
    // by one unknown; merging gives length 4 > 3 -> unknown empty.
    // Runs [3] on n=6 -> range [0,5]; maxL over covering runs of cells 1,2,3 = 3.
    // Pre-place: cells 0,1 black; cell 2 unknown; cell 3 black (segment len 2+2).
    // Actually we need two segments with exactly one unknown between them.
    // Segments: [0,1] and [3,4], gap at 2. Merged len = 5 > 3 -> cell 2 empty.
    Line ln = make_line(6, {3});
    ln.cells[0] = CellState::Black;
    ln.cells[1] = CellState::Black;
    ln.cells[2] = CellState::Unknown;
    ln.cells[3] = CellState::Black;
    ln.cells[4] = CellState::Black;
    LineUpdate u = rule_1_4(ln);
    REQUIRE_FALSE(u.contradiction);
    REQUIRE(u.cells_changed);
    CHECK(ln.cells[2] == CellState::White);
}

TEST_CASE("Rule 1.4: no fire when merged length <= maxL") {
    // Same setup but maxL=5 (clue [5]); merged len 5 == maxL -> not > maxL.
    Line ln = make_line(6, {5});
    ln.cells[0] = CellState::Black;
    ln.cells[1] = CellState::Black;
    ln.cells[2] = CellState::Unknown;
    ln.cells[3] = CellState::Black;
    ln.cells[4] = CellState::Black;
    LineUpdate u = rule_1_4(ln);
    REQUIRE_FALSE(u.contradiction);
    CHECK_FALSE(u.cells_changed);  // gap stays unknown
}

TEST_CASE("Rule 1.4: no fire when gap is not a single unknown") {
    // Gap of two cells -> not the rule's pattern.
    Line ln = make_line(7, {3});
    ln.cells[0] = CellState::Black;
    ln.cells[1] = CellState::Black;
    ln.cells[2] = CellState::Unknown;
    ln.cells[3] = CellState::Unknown;
    ln.cells[4] = CellState::Black;
    ln.cells[5] = CellState::Black;
    LineUpdate u = rule_1_4(ln);
    REQUIRE_FALSE(u.contradiction);
    CHECK_FALSE(u.cells_changed);
}

TEST_CASE("Rule 1.5(A): wall obstruction colors cells on the open side") {
    // Paper Fig. 10: clue [3, 4] on n=10. Cell i (say index 4) black, with an
    // empty wall at i-2 (index 2). minL over covering runs of cell 4 = 3
    // (run0 lb=3 covers index 4 since run0 range [0,5]). Empty cm at m=2 in
    // [4-3+1, 4-1] = [2,3]. Color [4+1, 2+3] = [5,5]. So cell 5 becomes black.
    Line ln = make_line(10, {3, 4});
    ln.cells[2] = CellState::White;  // wall
    ln.cells[4] = CellState::Black;  // ci
    LineUpdate u = rule_1_5(ln);
    REQUIRE_FALSE(u.contradiction);
    REQUIRE(u.cells_changed);
    CHECK(ln.cells[5] == CellState::Black);
}

TEST_CASE("Rule 1.5(B): equal-length bracketing empties ends") {
    // Paper Fig. 11: a black segment of length 2 covered by runs all of
    // length 2 with overlapping ranges -> cells just past both ends empty.
    // Runs [2, 2] on n=8: run0 [0,4], run1 [3,7] (overlap [3,4]).
    // Segment [3,4] length 2 == L=2; covered by both runs (lb=2 each).
    // -> cell 2 and cell 5 become white.
    Line ln = make_line(8, {2, 2});
    ln.cells[3] = CellState::Black;
    ln.cells[4] = CellState::Black;
    LineUpdate u = rule_1_5(ln);
    REQUIRE_FALSE(u.contradiction);
    REQUIRE(u.cells_changed);
    CHECK(ln.cells[2] == CellState::White);
    CHECK(ln.cells[5] == CellState::White);
}

TEST_CASE("Rule 1.5(B): no fire when segment length != covering run length") {
    // Segment length 3, covering runs all length 2 -> L != seg_len -> no fire.
    Line ln = make_line(8, {2, 2});
    ln.cells[3] = CellState::Black;
    ln.cells[4] = CellState::Black;
    ln.cells[5] = CellState::Black;
    LineUpdate u = rule_1_5(ln);
    REQUIRE_FALSE(u.contradiction);
    // 1.5(A) might fire; we only assert 1.5(B) didn't empty cells 2 and 6.
    CHECK(ln.cells[2] == CellState::Unknown);
    CHECK(ln.cells[6] == CellState::Unknown);
}

// ========================================================================== //
// Part II — range refinement                                                  //
// ========================================================================== //

TEST_CASE("Rule 2.1: push start forward when rj_s <= r(j-1)_s") {
    // Clue [2, 2] on n=8. Eq.1: run0 [0,5], run1 [3,7]. They overlap but
    // run1.start(3) > run0.start(0) — so 2.1 doesn't fire on run1's start.
    // Manually shrink run0 to [0,2] so run1.start(3) > run0.start(0): still fine.
    // Instead, force the violation: set run0 [0,6], run1 [0,7].
    // Then run1.start(0) <= run0.start(0) -> run1.start = 0 + 2 + 1 = 3.
    Line ln = make_line(8, {2, 2}, {}, {{0, 6}, {0, 7}});
    LineUpdate u = rule_2_1(ln);
    REQUIRE_FALSE(u.contradiction);
    REQUIRE(u.ranges_changed);
    CHECK(ln.runs[1].range.start == 3);
}

TEST_CASE("Rule 2.1: pull end back when rj_e >= r(j+1)_e") {
    // Clue [2, 2] on n=8. Force run0 [0,7], run1 [3,7].
    // run0.end(7) >= run1.end(7) -> run0.end = 7 - 2 - 1 = 4.
    Line ln = make_line(8, {2, 2}, {}, {{0, 7}, {3, 7}});
    LineUpdate u = rule_2_1(ln);
    REQUIRE_FALSE(u.contradiction);
    REQUIRE(u.ranges_changed);
    CHECK(ln.runs[0].range.end == 4);
}

TEST_CASE("Rule 2.1: no fire when ranges already ordered") {
    // Clue [2, 2] on n=8. Eq.1: run0 [0,5], run1 [3,7]. Already ordered.
    Line ln = make_line(8, {2, 2});
    LineUpdate u = rule_2_1(ln);
    REQUIRE_FALSE(u.contradiction);
    CHECK_FALSE(u.ranges_changed);
}

TEST_CASE("Rule 2.2: colored cell before range start -> shrink start") {
    // Clue [3] on n=6: range [0,5]. Color cell 0 (just before range start?).
    // Actually we need a cell at s-1 colored. Shrink range to [1,5] so cell 0
    // is at s-1 and colored -> range becomes [2,5].
    Line ln = make_line(6, {3}, {}, {{1, 5}});
    ln.cells[0] = CellState::Black;
    LineUpdate u = rule_2_2(ln);
    REQUIRE_FALSE(u.contradiction);
    REQUIRE(u.ranges_changed);
    CHECK(ln.runs[0].range.start == 2);
}

TEST_CASE("Rule 2.2: colored cell after range end -> shrink end") {
    // Clue [3] on n=6: range [0,4]. Color cell 5 (e+1) -> range becomes [0,3].
    Line ln = make_line(6, {3}, {}, {{0, 4}});
    ln.cells[5] = CellState::Black;
    LineUpdate u = rule_2_2(ln);
    REQUIRE_FALSE(u.contradiction);
    REQUIRE(u.ranges_changed);
    CHECK(ln.runs[0].range.end == 3);
}

TEST_CASE("Rule 2.2: no fire when no colored neighbor outside range") {
    Line ln = make_line(6, {3});  // range [0,5], no colored cells
    LineUpdate u = rule_2_2(ln);
    REQUIRE_FALSE(u.contradiction);
    CHECK_FALSE(u.ranges_changed);
}

TEST_CASE("Rule 2.3: oversized segment from former runs -> push start") {
    // Paper Fig. 12: clue [2, 2] on n=12. Eq.1: run0 [0,8], run1 [3,11].
    // Place a black segment of length 3 at [4,6] inside run1's range [3,11].
    // seg_len=3 > LB1=2. Run0's range [0,8] also covers [4,6]. run0 index=0
    // < j=1 -> all_former -> run1.start = 6 + 2 = 8.
    Line ln = make_line(12, {2, 2});  // run0 [0,8], run1 [3,11]
    ln.cells[4] = CellState::Black;
    ln.cells[5] = CellState::Black;
    ln.cells[6] = CellState::Black;
    LineUpdate u = rule_2_3(ln);
    REQUIRE_FALSE(u.contradiction);
    REQUIRE(u.ranges_changed);
    CHECK(ln.runs[1].range.start == 8);
}

TEST_CASE("Rule 2.3: oversized segment from later runs -> pull end") {
    // Clue [2, 2] on n=12. Eq.1: run0 [0,8], run1 [3,11].
    // Place a black segment of length 3 at [5,7] inside run0's range [0,8].
    // seg_len=3 > LB0=2. Run1's range [3,11] covers [5,7]. run1 index=1
    // > j=0 -> all_later -> run0.end = 5 - 2 = 3.
    Line ln = make_line(12, {2, 2});
    ln.cells[5] = CellState::Black;
    ln.cells[6] = CellState::Black;
    ln.cells[7] = CellState::Black;
    LineUpdate u = rule_2_3(ln);
    REQUIRE_FALSE(u.contradiction);
    REQUIRE(u.ranges_changed);
    CHECK(ln.runs[0].range.end == 3);
}

TEST_CASE("Rule 2.3: no fire when segment length <= LBj") {
    // Clue [3] on n=8: range [0,5]. Segment [2,4] length 3 == LB=3 -> no fire.
    Line ln = make_line(8, {3});
    ln.cells[2] = CellState::Black;
    ln.cells[3] = CellState::Black;
    ln.cells[4] = CellState::Black;
    LineUpdate u = rule_2_3(ln);
    REQUIRE_FALSE(u.contradiction);
    CHECK_FALSE(u.ranges_changed);
}

// ========================================================================== //
// Part III — combined: cells + ranges                                         //
// ========================================================================== //

TEST_CASE("Rule 3.1: scattered colored cells -> fill between + tighten range") {
    // Clue [4] on n=10, range [0,9]. Black cells at 3 and 5.
    // u = 4 - (5-3+1) = 1. rj_s = 3-1 = 2, rj_e = 5+1 = 6. Fill cell 4.
    Line ln = make_line(10, {4});
    ln.cells[3] = CellState::Black;
    ln.cells[5] = CellState::Black;
    LineUpdate u = rule_3_1(ln);
    REQUIRE_FALSE(u.contradiction);
    REQUIRE(u.cells_changed);
    CHECK(ln.cells[4] == CellState::Black);
    REQUIRE(u.ranges_changed);
    CHECK(ln.runs[0].range.start == 2);
    CHECK(ln.runs[0].range.end   == 6);
}

TEST_CASE("Rule 3.1: no colored cells -> no fire") {
    Line ln = make_line(6, {3});
    LineUpdate u = rule_3_1(ln);
    REQUIRE_FALSE(u.contradiction);
    CHECK_FALSE(u.cells_changed);
    CHECK_FALSE(u.ranges_changed);
}

TEST_CASE("Rule 3.1: segment too long -> skip fill and range tightening") {
    // Clue [3] on n=10, range [0,9]. Black at 1 and 6.
    // u = 3 - (6-1+1) = -3 < 0. Skip fill and range update.
    Line ln = make_line(10, {3});
    ln.cells[1] = CellState::Black;
    ln.cells[6] = CellState::Black;
    LineUpdate u = rule_3_1(ln);
    CHECK_FALSE(u.contradiction);
    CHECK_FALSE(u.cells_changed);
    CHECK_FALSE(u.ranges_changed);
}

TEST_CASE("Rule 3.1: colored at exact run -> tighten to segment") {
    // Clue [3] on n=5, range [0,4]. Black at 1,2,3 (full run).
    // u = 3 - 3 = 0. rj_s = 1, rj_e = 3.
    Line ln = make_line(5, {3});
    ln.cells[1] = CellState::Black;
    ln.cells[2] = CellState::Black;
    ln.cells[3] = CellState::Black;
    LineUpdate u = rule_3_1(ln);
    REQUIRE_FALSE(u.contradiction);
    REQUIRE(u.ranges_changed);
    CHECK(ln.runs[0].range.start == 1);
    CHECK(ln.runs[0].range.end   == 3);
}

TEST_CASE("Rule 3.2: skip short segments and tighten range") {
    // Paper Fig. 14: clue [3] on n=12. Place White-bounded segments:
    // [0,0] len 1 (Unknown), [2,3] len 2 (Unknown), [5,7] len 3 (Black),
    // [9,9] len 1 (Unknown). Short segments don't belong to other runs
    // -> their Unknown cells get emptied.
    Line ln = make_line(12, {3});
    ln.cells[1] = CellState::White;
    ln.cells[4] = CellState::White;
    ln.cells[5] = CellState::Black;
    ln.cells[6] = CellState::Black;
    ln.cells[7] = CellState::Black;
    ln.cells[8] = CellState::White;
    ln.cells[10] = CellState::White;
    LineUpdate u = rule_3_2(ln);
    REQUIRE_FALSE(u.contradiction);
    REQUIRE(u.ranges_changed);
    CHECK(ln.runs[0].range.start == 5);
    CHECK(ln.runs[0].range.end   == 7);
    // short segments emptied (were Unknown)
    CHECK(ln.cells[0] == CellState::White);
    CHECK(ln.cells[2] == CellState::White);
    CHECK(ln.cells[3] == CellState::White);
    CHECK(ln.cells[9] == CellState::White);
    // the qualifying segment stays
    CHECK(ln.cells[5] == CellState::Black);
}

TEST_CASE("Rule 3.2: short segment belongs to other run -> kept") {
    // Clue [1, 3] on n=12. run0 [0,10], run1 [2,11].
    // In run1's range [2,11], segment [2,2] len 1 < LB1=3. But run0 (lb=1,
    // range [0,10]) covers it and seg_len 1 <= run0.lb -> belongs to run0.
    Line ln = make_line(12, {1, 3});
    ln.cells[2] = CellState::Black;
    ln.cells[3] = CellState::White;
    LineUpdate u = rule_3_2(ln);
    REQUIRE_FALSE(u.contradiction);
    CHECK(ln.cells[2] == CellState::Black);
}

TEST_CASE("Rule 3.3-1: c_{rj_s} black -> finish the run") {
    // Clue [1, 3] on n=10. Shrink run0 to [0,1] so it doesn't overlap run1 [3,8].
    // Color cell 3 (rj_s of run1) -> finish: color 3..5, empty 2 and 6, rj_e=5.
    Line ln = make_line(10, {1, 3}, {}, {{0, 1}, {3, 8}});
    ln.cells[3] = CellState::Black;
    LineUpdate u = rule_3_3(ln);
    REQUIRE_FALSE(u.contradiction);
    REQUIRE(u.cells_changed);
    CHECK(ln.cells[4] == CellState::Black);
    CHECK(ln.cells[5] == CellState::Black);
    CHECK(ln.cells[2] == CellState::White);
    CHECK(ln.cells[6] == CellState::White);
    REQUIRE(u.ranges_changed);
    CHECK(ln.runs[1].range.end == 5);
}

TEST_CASE("Rule 3.3-2: empty after black -> shrink end") {
    // Clue [3] on n=10, range [0,9]. Color 2, White 5 -> rj_e = 4.
    Line ln = make_line(10, {3});
    ln.cells[2] = CellState::Black;
    ln.cells[5] = CellState::White;
    LineUpdate u = rule_3_3(ln);
    REQUIRE_FALSE(u.contradiction);
    REQUIRE(u.ranges_changed);
    CHECK(ln.runs[0].range.end == 4);
}

TEST_CASE("Rule 3.3-3: merging first+second > LBj -> shrink end") {
    // Paper Fig. 17: clue [4] on n=12. Segments [2,3] and [6,7].
    // Merged len = 7-2+1 = 6 > 4 -> rj_e = 6-2 = 4.
    Line ln = make_line(12, {4});
    ln.cells[2] = CellState::Black; ln.cells[3] = CellState::Black;
    ln.cells[6] = CellState::Black; ln.cells[7] = CellState::Black;
    LineUpdate u = rule_3_3(ln);
    REQUIRE_FALSE(u.contradiction);
    REQUIRE(u.ranges_changed);
    CHECK(ln.runs[0].range.end == 4);
}

TEST_CASE("Rule 3.3-3: merged length == LBj -> no fire") {
    // Clue [5] on n=12. Segments [2,3] and [5,6]. Merged = 5 == LBj -> no fire.
    Line ln = make_line(12, {5});
    ln.cells[2] = CellState::Black; ln.cells[3] = CellState::Black;
    ln.cells[5] = CellState::Black; ln.cells[6] = CellState::Black;
    LineUpdate u = rule_3_3(ln);
    REQUIRE_FALSE(u.contradiction);
    CHECK_FALSE(u.ranges_changed);
}
