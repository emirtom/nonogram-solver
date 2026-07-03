#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nonogram {

// Shared cell-state encoding, used throughout the solver and the renderer.
enum class CellState : uint8_t {
    White   = 0,  // empty / crossed
    Black   = 1,  // filled
    Unknown = 2,  // undetermined
};

// Half-open inclusive index range [start, end] over a line's cells.
// A run j of length LBj may only occupy cells in [rj_s, rj_e].
// Invariant: end - start + 1 >= LBj (else the line is unsatisfiable).
struct RunRange {
    int start = 0;
    int end   = 0;

    int length() const { return end - start + 1; }
    bool valid() const { return end >= start; }
};

// One clue (length of a contiguous black run) plus its current run range.
// Indices are stable: clue j always refers to the j-th run in the line.
struct Run {
    int       lb = 0;       // length of this black run (the clue value)
    RunRange  range;        // current estimated placement range
};

// A single line (row or column) of the board: its clues + current cells.
// cells.size() == line length n; runs.size() == number of clues k.
struct Line {
    std::vector<Run>       runs;
    std::vector<CellState> cells;

    int size() const { return static_cast<int>(cells.size()); }
    int num_runs() const { return static_cast<int>(runs.size()); }
};

}  // namespace nonogram
