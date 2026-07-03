#pragma once

#include "nonogram/nonogram.hpp"
#include "nonogram/solver.hpp"
#include "nonogram/types.hpp"

#include <string>
#include <vector>

namespace nonogram {

// UI-facing wrapper around Nonogram + Solver. Tracks the currently loaded
// puzzle path and exposes simple cell-mutation + solve APIs for the app.
class PuzzleState {
  public:
    PuzzleState() = default;

    // Load a puzzle preset from a JSON file. Resets the board.
    bool load_file(const std::string& path);
    bool load_inline(int w, int h,
                     std::vector<std::vector<int>> row_clues,
                     std::vector<std::vector<int>> col_clues);

    int  width()  const { return board_.width(); }
    int  height() const { return board_.height(); }

    CellState cell(int col, int row) const { return board_.row(row).cells[col]; }

    // Clue access for rendering. Returns the run lengths for a given line.
    const std::vector<Run>& row_clues(int r) const { return board_.row(r).runs; }
    const std::vector<Run>& col_clues(int c) const { return board_.col(c).runs; }

    // Set a single cell (user click). Keeps rows/cols views in sync.
    void set_cell(int col, int row, CellState s);

    // Solve / step / reset via the in-process solver.
    SolveOutcome solve(Backend backend = Backend::CPU);
    SolveOutcome step();
    void reset();

    // Validate the current board against its clues. Returns the set of
    // cell coordinates (col, row) that participate in a contradiction —
    // either the cell's state conflicts with what the rules force, or its
    // line cannot satisfy the clues. Empty if the board is consistent.
    std::vector<std::pair<int, int>> find_contradictions() const;

    const std::string& current_path() const { return path_; }
    bool empty() const { return width() == 0; }

  private:
    Nonogram board_;
    Solver   solver_{Nonogram{0, 0, {}, {}}};
    std::string path_;
};

}  // namespace nonogram
