#pragma once

#include "nonogram/types.hpp"

namespace nonogram {

// Result of applying the rule set to a single line.
struct LineUpdate {
    bool cells_changed  = false;  // any cell flipped Unknown -> Black/White
    bool ranges_changed = false;  // any RunRange tightened
    bool contradiction  = false;  // a run range collapsed (end < start) or
                                  // a cell was forced to two different states
};

// Initialize a line's run ranges from its clues (paper Eq. 1).
// Equivalent to Nonogram::init_line_run_ranges but exposed for testing
// single lines without a full board.
void init_run_ranges(Line& line);

// Part I rules — determine which cells to color / leave empty.
// Each mutates `line` in place and reports what changed.
LineUpdate rule_1_1(Line& line);  // intersection of left/right-most placements
LineUpdate rule_1_2(Line& line);  // cells outside all run ranges -> empty
LineUpdate rule_1_3(Line& line);  // boundary cell covered only by len-1 runs
LineUpdate rule_1_4(Line& line);  // merging two segments exceeds maxL -> empty
LineUpdate rule_1_5(Line& line);  // wall obstruction + equal-len bracketing

// Part II rules — refine run ranges.
LineUpdate rule_2_1(Line& line);  // ranges must stay ordered run-to-run
LineUpdate rule_2_2(Line& line);  // colored neighbor at range edge -> shrink
LineUpdate rule_2_3(Line& line);  // oversized segment belongs to other runs

// Part III rules — combined: cells + ranges.
LineUpdate rule_3_1(Line& line);  // scattered colored cells of one run -> fill
LineUpdate rule_3_2(Line& line);  // skip short segments bounded by empties
LineUpdate rule_3_3(Line& line);  // non-overlapping neighbor -> unique placement

// Apply all 11 rules to a single line once, accumulating updates.
LineUpdate apply_all_rules(Line& line);

// Apply all rules to all rows, then all columns, repeat until a full pass
// produces no cell change AND no range change (paper's fixpoint).
// Returns false if a contradiction is detected on any line.
bool run_to_fixpoint(class Nonogram& board);

}  // namespace nonogram
