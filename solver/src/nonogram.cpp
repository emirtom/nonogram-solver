#include "nonogram/nonogram.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

namespace nonogram {

namespace {
using json = nlohmann::json;
}

Nonogram::Nonogram(int width, int height,
                   std::vector<std::vector<int>> row_clues,
                   std::vector<std::vector<int>> col_clues)
    : width_(width), height_(height) {
    if (static_cast<int>(row_clues.size()) != height)
        throw std::runtime_error("row_clues count != height");
    if (static_cast<int>(col_clues.size()) != width)
        throw std::runtime_error("col_clues count != width");

    rows_.resize(height);
    for (int r = 0; r < height; ++r) {
        Line& ln = rows_[r];
        ln.cells.assign(width, CellState::Unknown);
        for (int lb : row_clues[r])
            ln.runs.push_back(Run{lb, {}});
    }
    cols_.resize(width);
    for (int c = 0; c < width; ++c) {
        Line& ln = cols_[c];
        ln.cells.assign(height, CellState::Unknown);
        for (int lb : col_clues[c])
            ln.runs.push_back(Run{lb, {}});
    }
    init_run_ranges();
}

Nonogram Nonogram::from_json_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("cannot open " + path);
    json j;
    in >> j;

    int w = j.at("width").get<int>();
    int h = j.at("height").get<int>();
    std::vector<std::vector<int>> row_clues = j.at("rows").get<std::vector<std::vector<int>>>();
    std::vector<std::vector<int>> col_clues = j.at("cols").get<std::vector<std::vector<int>>>();
    return Nonogram(w, h, std::move(row_clues), std::move(col_clues));
}

void Nonogram::init_line_run_ranges(Line& line) {
    const int n = line.size();
    const int k = line.num_runs();
    if (k == 0) return;
    // Eq. (1) from the paper:
    //   r1_s = 0
    //   rj_s = sum_{i<j}(LBi + 1)
    //   rj_e = (n-1) - sum_{i>j}(LBi + 1)
    //   rk_e = n-1
    int prefix = 0;  // sum of (LBi + 1) for i < j
    for (int j = 0; j < k; ++j) {
        line.runs[j].range.start = prefix;
        // suffix sum of (LBi + 1) for i > j
        int suffix = 0;
        for (int i = j + 1; i < k; ++i) suffix += line.runs[i].lb + 1;
        line.runs[j].range.end = (n - 1) - suffix;
        prefix += line.runs[j].lb + 1;
    }
}

void Nonogram::init_run_ranges() {
    for (Line& r : rows_) init_line_run_ranges(r);
    for (Line& c : cols_) init_line_run_ranges(c);
}

void Nonogram::sync_row_to_cols(int r) {
    for (int c = 0; c < width_; ++c)
        cols_[c].cells[r] = rows_[r].cells[c];
}

void Nonogram::sync_col_to_rows(int c) {
    for (int r = 0; r < height_; ++r)
        rows_[r].cells[c] = cols_[c].cells[r];
}

void Nonogram::reset() {
    for (Line& ln : rows_)
        std::fill(ln.cells.begin(), ln.cells.end(), CellState::Unknown);
    for (Line& ln : cols_)
        std::fill(ln.cells.begin(), ln.cells.end(), CellState::Unknown);
    init_run_ranges();
}

bool Nonogram::is_complete() const {
    for (const Line& ln : rows_)
        for (CellState s : ln.cells)
            if (s == CellState::Unknown) return false;
    return true;
}

std::string Nonogram::to_ascii() const {
    std::string out;
    out.reserve((height_ + 1) * (width_ + 1));
    for (int r = 0; r < height_; ++r) {
        for (int c = 0; c < width_; ++c) {
            switch (rows_[r].cells[c]) {
                case CellState::Black:   out += '#'; break;
                case CellState::White:   out += '.'; break;
                case CellState::Unknown: out += '?'; break;
            }
        }
        out += '\n';
    }
    return out;
}

}  // namespace nonogram
