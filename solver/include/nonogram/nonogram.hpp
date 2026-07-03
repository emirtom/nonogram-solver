#pragma once

#include "nonogram/types.hpp"

#include <string>
#include <vector>

namespace nonogram {

// A nonogram puzzle: clue sets for every row and column, plus the working
// board (rows and columns are stored twice for fast LR application).
//
// rows_[r] and cols_[c] are *views* of the same board cells kept in sync
// by the solver. The LR rules operate on a single Line at a time.
class Nonogram {
  public:
    Nonogram() = default;
    explicit Nonogram(int width, int height,
                      std::vector<std::vector<int>> row_clues,
                      std::vector<std::vector<int>> col_clues);

    // Load a puzzle preset from a JSON file of the form:
    // { "width": 5, "height": 5,
    //   "rows": [[1,1],[3],...], "cols": [[1],[2,2],...] }
    // Throws std::runtime_error on malformed input.
    static Nonogram from_json_file(const std::string& path);

    int width()  const { return width_; }
    int height() const { return height_; }

    // Mutable line access for the solver. The caller must keep rows/cols
    // consistent via sync_*() after mutating cells.
    Line&       row(int r)       { return rows_[r]; }
    Line&       col(int c)       { return cols_[c]; }
    const Line& row(int r) const { return rows_[r]; }
    const Line& col(int c) const { return cols_[c]; }

    // Copy a row's cells back into the column views (and vice versa) so the
    // two views stay consistent after a rule mutates one of them.
    void sync_row_to_cols(int r);
    void sync_col_to_rows(int c);

    // Reset every cell to Unknown and re-initialize run ranges from clues.
    void reset();

    // True iff no cell is Unknown.
    bool is_complete() const;

    // True iff the board is complete AND the black runs in every row/column
    // match the original clues. Call after is_complete() returns true.
    bool is_valid_solution() const;

    // ASCII render: '#' black, '.' white, '?' unknown. For CLI/debug.
    std::string to_ascii() const;

    // Initialize a line's run ranges from its clues (paper Eq. 1).
    // Public so the logical_rules module can reuse it for single-line tests.
    static void init_line_run_ranges(Line& line);

  private:
    void init_run_ranges();

    int  width_  = 0;
    int  height_ = 0;
    std::vector<Line> rows_;  // height_ lines
    std::vector<Line> cols_;  // width_  lines
};

}  // namespace nonogram
