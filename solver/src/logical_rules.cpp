#include "nonogram/logical_rules.hpp"
#include "nonogram/nonogram.hpp"

#include <algorithm>

namespace nonogram {

void init_run_ranges(Line& line) {
    Nonogram::init_line_run_ranges(line);  // delegate (friend access via header)
}

namespace {

// A maximal contiguous run of Black cells [start, end] inclusive.
struct BlackSeg { int start; int end; };

// Find all maximal Black segments in a line.
std::vector<BlackSeg> find_black_segs(const Line& line) {
    std::vector<BlackSeg> segs;
    const int n = line.size();
    int i = 0;
    while (i < n) {
        if (line.cells[i] == CellState::Black) {
            int j = i;
            while (j + 1 < n && line.cells[j + 1] == CellState::Black) ++j;
            segs.push_back({i, j});
            i = j + 1;
        } else {
            ++i;
        }
    }
    return segs;
}

// Indices of all runs whose range covers cell `i`.
std::vector<int> covering_runs(const Line& line, int i) {
    std::vector<int> idx;
    for (int j = 0; j < line.num_runs(); ++j)
        if (i >= line.runs[j].range.start && i <= line.runs[j].range.end)
            idx.push_back(j);
    return idx;
}

// Set a cell; report contradiction if it was the opposite state.
bool set_white(Line& line, int i, LineUpdate& out) {
    if (i < 0 || i >= line.size()) return true;
    CellState& s = line.cells[i];
    if (s == CellState::Black) { out.contradiction = true; return false; }
    if (s == CellState::Unknown) { s = CellState::White; out.cells_changed = true; }
    return true;
}

bool set_black(Line& line, int i, LineUpdate& out) {
    if (i < 0 || i >= line.size()) return true;
    CellState& s = line.cells[i];
    if (s == CellState::White) { out.contradiction = true; return false; }
    if (s == CellState::Unknown) { s = CellState::Black; out.cells_changed = true; }
    return true;
}

}  // namespace

// ---- Part I ----------------------------------------------------------------

LineUpdate rule_1_1(Line& line) {
    // Intersection of the left-most and right-most placements of each run.
    // Paper: color ci when rj_s + u <= i <= rj_e - u, u = range_len - LBj.
    LineUpdate out;
    const int n = line.size();
    for (Run& run : line.runs) {
        const int range_len = run.range.length();
        if (range_len < run.lb) { out.contradiction = true; return out; }
        const int u  = range_len - run.lb;
        const int lo = run.range.start + u;  // = rj_e - LBj + 1
        const int hi = run.range.end   - u;  // = rj_s + LBj - 1
        if (lo > hi) continue;  // no overlap between the two placements
        for (int i = lo; i <= hi; ++i) {
            if (i < 0 || i >= n) continue;
            if (!set_black(line, i, out)) return out;
        }
    }
    return out;
}

LineUpdate rule_1_2(Line& line) {
    // Cells not covered by any run's range are empty.
    LineUpdate out;
    const int n = line.size();
    for (int i = 0; i < n; ++i) {
        bool covered = false;
        for (const Run& run : line.runs) {
            if (i >= run.range.start && i <= run.range.end) { covered = true; break; }
        }
        if (covered) continue;
        if (!set_white(line, i, out)) return out;
    }
    return out;
}

LineUpdate rule_1_3(Line& line) {
    // Boundary cell of a run range colored, and all *other* covering runs
    // have length 1 -> the cell just outside the range is empty.
    LineUpdate out;
    const int n = line.size();
    for (int j = 0; j < line.num_runs(); ++j) {
        Run& run = line.runs[j];
        const int s = run.range.start;
        const int e = run.range.end;
        if (s < n && line.cells[s] == CellState::Black) {
            auto covers = covering_runs(line, s);
            bool all_len1 = true;
            for (int idx : covers) if (idx != j && line.runs[idx].lb != 1) { all_len1 = false; break; }
            if (all_len1 && !set_white(line, s - 1, out)) return out;
        }
        if (e >= 0 && line.cells[e] == CellState::Black) {
            auto covers = covering_runs(line, e);
            bool all_len1 = true;
            for (int idx : covers) if (idx != j && line.runs[idx].lb != 1) { all_len1 = false; break; }
            if (all_len1 && !set_white(line, e + 1, out)) return out;
        }
    }
    return out;
}

LineUpdate rule_1_4(Line& line) {
    // Two black segments with one unknown between them; if coloring that
    // unknown would create a segment longer than maxL (the max length of
    // all runs covering the three cells), the unknown is empty.
    LineUpdate out;
    const int n = line.size();
    auto segs = find_black_segs(line);
    for (size_t si = 0; si + 1 < segs.size(); ++si) {
        const BlackSeg& a = segs[si];
        const BlackSeg& b = segs[si + 1];
        if (b.start - a.end != 2) continue;            // need exactly one gap
        const int gap = a.end + 1;
        if (line.cells[gap] != CellState::Unknown) continue;
        const int merged_len = b.end - a.start + 1;    // length if gap colored
        // maxL over all runs covering the three cells a.end, gap, b.start
        int maxL = 0;
        for (int cell : {a.end, gap, b.start}) {
            for (int idx : covering_runs(line, cell))
                maxL = std::max(maxL, line.runs[idx].lb);
        }
        if (maxL == 0) continue;
        if (merged_len > maxL) {
            if (!set_white(line, gap, out)) return out;
        }
    }
    (void)n;
    return out;
}

LineUpdate rule_1_5(Line& line) {
    // Two sub-cases:
    //  (A) Wall obstruction: a black cell ci with an empty/unknown ci-1.
    //      minL = min length of runs covering ci. If an empty cm exists in
    //      [i-minL+1, i-1], color [i+1, m+minL]; symmetric on the right.
    //  (B) Equal-length bracketing: a black segment covered by runs all of
    //      the same length L, with overlapping ranges, segment length == L
    //      -> empty cells just past both ends of the segment.
    LineUpdate out;
    const int n = line.size();
    auto segs = find_black_segs(line);

    // (A) Wall obstruction — operate per black cell at a segment boundary.
    for (const BlackSeg& seg : segs) {
        // left boundary cell = seg.start; check wall on its left.
        const int i = seg.start;
        if (i > 0 && line.cells[i - 1] != CellState::Black) {
            auto covers = covering_runs(line, i);
            if (!covers.empty()) {
                int minL = INT32_MAX;
                for (int idx : covers) minL = std::min(minL, line.runs[idx].lb);
                // nearest empty cm in [i-minL+1, i-1]
                int m = -1;
                for (int p = i - 1; p >= std::max(0, i - minL + 1); --p)
                    if (line.cells[p] == CellState::White) { m = p; break; }
                if (m >= 0) {
                    for (int p = i + 1; p <= m + minL && p < n; ++p)
                        if (!set_black(line, p, out)) return out;
                }
            }
        }
        // right boundary cell = seg.end; check wall on its right.
        const int j = seg.end;
        if (j + 1 < n && line.cells[j + 1] != CellState::Black) {
            auto covers = covering_runs(line, j);
            if (!covers.empty()) {
                int minL = INT32_MAX;
                for (int idx : covers) minL = std::min(minL, line.runs[idx].lb);
                int m = -1;
                for (int p = j + 1; p <= std::min(n - 1, j + minL - 1); ++p)
                    if (line.cells[p] == CellState::White) { m = p; break; }
                if (m >= 0) {
                    for (int p = j - 1; p >= m - minL && p >= 0; --p)
                        if (!set_black(line, p, out)) return out;
                }
            }
        }
    }

    // (B) Equal-length bracketing.
    for (const BlackSeg& seg : segs) {
        const int seg_len = seg.end - seg.start + 1;
        // runs covering *both* end cells of the segment
        auto covers_s = covering_runs(line, seg.start);
        auto covers_e = covering_runs(line, seg.end);
        // intersection
        std::vector<int> cov;
        for (int a : covers_s) {
            if (std::find(covers_e.begin(), covers_e.end(), a) != covers_e.end())
                cov.push_back(a);
        }
        if (cov.empty()) continue;
        int L = line.runs[cov[0]].lb;
        bool same = true;
        for (int idx : cov) if (line.runs[idx].lb != L) { same = false; break; }
        if (!same || L != seg_len) continue;
        // ranges must overlap (paper: "overlapping ranges")
        bool overlap = false;
        for (size_t a = 0; a < cov.size() && !overlap; ++a)
            for (size_t b = a + 1; b < cov.size(); ++b)
                if (line.runs[cov[a]].range.start <= line.runs[cov[b]].range.end &&
                    line.runs[cov[b]].range.start <= line.runs[cov[a]].range.end)
                    { overlap = true; break; }
        if (!overlap) continue;
        if (!set_white(line, seg.start - 1, out)) return out;
        if (!set_white(line, seg.end   + 1, out)) return out;
    }
    return out;
}

// ---- Part II ---------------------------------------------------------------

LineUpdate rule_2_1(Line& line) {
    // Keep consecutive run ranges ordered: rj_s must be > r(j-1)_s,
    // rj_e must be < r(j+1)_e. If violated, push forward / pull back.
    LineUpdate out;
    const int k = line.num_runs();
    for (int j = 0; j < k; ++j) {
        Run& run = line.runs[j];
        if (j > 0) {
            const Run& prev = line.runs[j - 1];
            if (run.range.start <= prev.range.start) {
                int new_s = prev.range.start + prev.lb + 1;
                if (new_s > run.range.start) {
                    run.range.start = new_s;
                    out.ranges_changed = true;
                    if (run.range.end < run.range.start) { out.contradiction = true; return out; }
                }
            }
        }
        if (j + 1 < k) {
            const Run& next = line.runs[j + 1];
            if (run.range.end >= next.range.end) {
                int new_e = next.range.end - next.lb - 1;
                if (new_e < run.range.end) {
                    run.range.end = new_e;
                    out.ranges_changed = true;
                    if (run.range.end < run.range.start) { out.contradiction = true; return out; }
                }
            }
        }
    }
    return out;
}

LineUpdate rule_2_2(Line& line) {
    // A colored cell just outside the range end means the run can't reach
    // that far (needs a separator). Shrink the range inward by one.
    LineUpdate out;
    const int n = line.size();
    for (Run& run : line.runs) {
        const int s = run.range.start;
        const int e = run.range.end;
        if (s - 1 >= 0 && line.cells[s - 1] == CellState::Black) {
            run.range.start = s + 1;
            out.ranges_changed = true;
            if (run.range.end < run.range.start) { out.contradiction = true; return out; }
        }
        if (e + 1 < n && line.cells[e + 1] == CellState::Black) {
            run.range.end = e - 1;
            out.ranges_changed = true;
            if (run.range.end < run.range.start) { out.contradiction = true; return out; }
        }
    }
    return out;
}

LineUpdate rule_2_3(Line& line) {
    // A black segment inside run j's range with length > LBj can't belong
    // to run j. If it's covered only by former runs -> run j starts after
    // it (rj_s = seg.end + 2). If only by later runs -> run j ends before
    // it (rj_e = seg.start - 2).
    //
    // We snapshot the ranges before modifying any, because shrinking one
    // run's range would change the covering analysis for the next run.
    LineUpdate out;
    const int k = line.num_runs();
    std::vector<RunRange> snap(k);
    for (int j = 0; j < k; ++j) snap[j] = line.runs[j].range;

    auto segs = find_black_segs(line);
    for (int j = 0; j < k; ++j) {
        Run& run = line.runs[j];
        const int s = snap[j].start;
        const int e = snap[j].end;
        for (const BlackSeg& seg : segs) {
            if (seg.start < s || seg.end > e) continue;
            const int seg_len = seg.end - seg.start + 1;
            if (seg_len <= run.lb) continue;
            std::vector<int> covers;
            for (int idx = 0; idx < k; ++idx) {
                if (idx == j) continue;
                if (seg.start >= snap[idx].start && seg.end <= snap[idx].end)
                    covers.push_back(idx);
            }
            if (covers.empty()) continue;
            bool all_former = true, all_later = true;
            for (int idx : covers) {
                if (idx > j) all_former = false;
                if (idx < j) all_later  = false;
            }
            if (all_former) {
                int new_s = seg.end + 2;
                if (new_s > run.range.start) {
                    run.range.start = new_s;
                    out.ranges_changed = true;
                    if (run.range.end < run.range.start) { out.contradiction = true; return out; }
                }
            } else if (all_later) {
                int new_e = seg.start - 2;
                if (new_e < run.range.end) {
                    run.range.end = new_e;
                    out.ranges_changed = true;
                    if (run.range.end < run.range.start) { out.contradiction = true; return out; }
                }
            }
        }
    }
    return out;
}

// ---- Part III --------------------------------------------------------------

LineUpdate rule_3_1(Line& line) {
    // Scattered colored cells belonging to the same run -> fill between them
    // and tighten the range. For each run j:
    //   cm = first Black cell in range after prev run's range (or range start)
    //   cn = last  Black cell in range before next run's range (or range end)
    //   color all cells in [cm, cn]; u = LBj - (cn - cm + 1);
    //   rj_s = cm - u; rj_e = cn + u.
    LineUpdate out;
    const int k = line.num_runs();
    for (int j = 0; j < k; ++j) {
        Run& run = line.runs[j];
        const int s = run.range.start;
        const int e = run.range.end;
        const int lo = (j > 0) ? std::max(s, line.runs[j - 1].range.end + 1) : s;
        const int hi = (j + 1 < k) ? std::min(e, line.runs[j + 1].range.start - 1) : e;
        int cm = -1, cn = -1;
        for (int i = lo; i <= hi; ++i)
            if (line.cells[i] == CellState::Black) { cm = i; break; }
        for (int i = hi; i >= lo; --i)
            if (line.cells[i] == CellState::Black) { cn = i; break; }
        if (cm < 0) continue;  // no colored cells for this run
        const int u = run.lb - (cn - cm + 1);
        if (u < 0) continue;  // span too wide — cm/cn likely from different overlapping runs
        // fill between cm and cn
        for (int i = cm; i <= cn; ++i)
            if (!set_black(line, i, out)) return out;
        const int new_s = cm - u;
        const int new_e = cn + u;
        if (new_s > run.range.start) { run.range.start = new_s; out.ranges_changed = true; }
        if (new_e < run.range.end)   { run.range.end   = new_e; out.ranges_changed = true; }
        if (run.range.end < run.range.start) { out.contradiction = true; return out; }
    }
    return out;
}

LineUpdate rule_3_2(Line& line) {
    // Segments bounded by empty cells within (rj_s, rj_e). Skip short ones
    // from the left/right to tighten the range, then empty short segments
    // that can't belong to other runs.
    LineUpdate out;
    const int k = line.num_runs();
    // Snapshot ranges so shrinkage of one run doesn't affect another's analysis.
    std::vector<RunRange> snap(k);
    for (int j = 0; j < k; ++j) snap[j] = line.runs[j].range;

    for (int j = 0; j < k; ++j) {
        Run& run = line.runs[j];
        const int s = snap[j].start;
        const int e = snap[j].end;
        // Build segments of non-White cells within [s, e].
        struct Seg { int start, end; };
        std::vector<Seg> segs;
        int i = s;
        while (i <= e) {
            if (line.cells[i] != CellState::White) {
                int st = i;
                while (i <= e && line.cells[i] != CellState::White) ++i;
                segs.push_back({st, i - 1});
            } else {
                ++i;
            }
        }
        if (segs.empty()) continue;
        const int b = static_cast<int>(segs.size());
        // Step 1-2: from left, first segment with length >= LBj -> rj_s = its start.
        for (int si = 0; si < b; ++si) {
            if (segs[si].end - segs[si].start + 1 >= run.lb) {
                if (segs[si].start > run.range.start) {
                    run.range.start = segs[si].start;
                    out.ranges_changed = true;
                }
                break;
            }
        }
        // Step 3-4: from right, last segment with length >= LBj -> rj_e = its end.
        for (int si = b - 1; si >= 0; --si) {
            if (segs[si].end - segs[si].start + 1 >= run.lb) {
                if (segs[si].end < run.range.end) {
                    run.range.end = segs[si].end;
                    out.ranges_changed = true;
                }
                break;
            }
        }
        if (run.range.end < run.range.start) { out.contradiction = true; return out; }
        // Step 5: remaining short segments not belonging to other runs -> empty.
        for (const Seg& seg : segs) {
            const int seg_len = seg.end - seg.start + 1;
            if (seg_len >= run.lb) continue;
            // does it belong to another run?
            bool belongs_other = false;
            for (int idx = 0; idx < k; ++idx) {
                if (idx == j) continue;
                if (seg.start >= snap[idx].start && seg.end <= snap[idx].end &&
                    seg_len <= line.runs[idx].lb) {
                    belongs_other = true;
                    break;
                }
            }
            if (!belongs_other) {
                for (int p = seg.start; p <= seg.end; ++p)
                    if (!set_white(line, p, out)) return out;
            }
        }
    }
    return out;
}

LineUpdate rule_3_3(Line& line) {
    // When run j's range doesn't overlap j-1 (or j+1), three cases resolve it.
    // Case 1: c_{rj_s} black -> finish the run.
    // Case 2: empty cell after black cell in range -> rj_e = w-1.
    // Case 3: multiple black segments; merging first+second > LBj -> rj_e = t-2.
    // Symmetric cases apply when j doesn't overlap j+1 (using rj_e).
    LineUpdate out;
    const int k = line.num_runs();
    const int n = line.size();
    auto overlaps = [](const RunRange& a, const RunRange& b) {
        return a.start <= b.end && b.start <= a.end;
    };

    for (int j = 0; j < k; ++j) {
        Run& run = line.runs[j];
        const int s = run.range.start;
        const int e = run.range.end;

        // --- Doesn't overlap j-1: cases examine the LEFT side (rj_s). ---
        // For j=0 (no previous run), vacuously non-overlapping -> applies.
        bool no_prev = (j == 0) || !overlaps(run.range, line.runs[j - 1].range);
        if (no_prev) {
            // Case 1: c_{rj_s} is Black.
            if (s >= 0 && s < n && line.cells[s] == CellState::Black) {
                for (int i = s + 1; i <= s + run.lb - 1; ++i)
                    if (!set_black(line, i, out)) return out;
                if (!set_white(line, s - 1, out)) return out;
                if (!set_white(line, s + run.lb, out)) return out;
                int new_e = s + run.lb - 1;
                if (new_e < run.range.end) {
                    run.range.end = new_e;
                    out.ranges_changed = true;
                }
                // update j+1 if it overlaps
                if (j + 1 < k && overlaps(run.range, line.runs[j + 1].range)) {
                    int new_s = run.range.end + 2;
                    if (new_s > line.runs[j + 1].range.start) {
                        line.runs[j + 1].range.start = new_s;
                        out.ranges_changed = true;
                    }
                }
                // update j-1 end if adjacent
                if (j > 0 && line.runs[j - 1].range.end == s - 1) {
                    line.runs[j - 1].range.end = s - 2;
                    out.ranges_changed = true;
                }
            }
            // Case 2: empty cell cw after black cell cb in range -> rj_e = w-1.
            int first_black = -1;
            for (int i = s; i <= e; ++i)
                if (line.cells[i] == CellState::Black) { first_black = i; break; }
            if (first_black >= 0) {
                for (int w = first_black + 1; w <= e; ++w) {
                    if (line.cells[w] == CellState::White) {
                        if (w - 1 < run.range.end) {
                            run.range.end = w - 1;
                            out.ranges_changed = true;
                            if (run.range.end < run.range.start) { out.contradiction = true; return out; }
                        }
                        break;
                    }
                }
            }
            // Case 3: multiple black segments; merge first + next; if > LBj, rj_e = t-2.
            {
                auto bsegs = find_black_segs(line);
                // only segments within range
                std::vector<BlackSeg> in_range;
                for (const auto& bs : bsegs)
                    if (bs.start >= s && bs.end <= e) in_range.push_back(bs);
                if (in_range.size() >= 2) {
                    const auto& first = in_range[0];
                    for (size_t si = 1; si < in_range.size(); ++si) {
                        const auto& sec = in_range[si];
                        int merged_len = sec.end - first.start + 1;
                        if (merged_len > run.lb) {
                            int new_e = sec.start - 2;
                            if (new_e < run.range.end) {
                                run.range.end = new_e;
                                out.ranges_changed = true;
                                if (run.range.end < run.range.start) { out.contradiction = true; return out; }
                            }
                            break;
                        }
                    }
                }
            }
        }

        // --- Doesn't overlap j+1: symmetric cases examine the RIGHT side (rj_e). ---
        // Re-read range since no_prev may have shrunk it.
        // For j=k-1 (no next run), vacuously non-overlapping -> applies.
        const int s2 = run.range.start;
        const int e2 = run.range.end;
        bool no_next = (j + 1 == k) || !overlaps(run.range, line.runs[j + 1].range);
        if (no_next) {
            // Case 1 (mirrored): c_{rj_e} is Black.
            if (e2 >= 0 && e2 < n && line.cells[e2] == CellState::Black) {
                for (int i = e2 - 1; i >= e2 - run.lb + 1; --i)
                    if (!set_black(line, i, out)) return out;
                if (!set_white(line, e2 + 1, out)) return out;
                if (!set_white(line, e2 - run.lb, out)) return out;
                int new_s = e2 - run.lb + 1;
                if (new_s > run.range.start) {
                    run.range.start = new_s;
                    out.ranges_changed = true;
                }
                if (j > 0 && overlaps(run.range, line.runs[j - 1].range)) {
                    int new_e = run.range.start - 2;
                    if (new_e < line.runs[j - 1].range.end) {
                        line.runs[j - 1].range.end = new_e;
                        out.ranges_changed = true;
                    }
                }
                if (j + 1 < k && line.runs[j + 1].range.start == e2 + 1) {
                    line.runs[j + 1].range.start = e2 + 2;
                    out.ranges_changed = true;
                }
            }
            // Case 2 (mirrored): empty cell cw before a black cell cb -> rj_s = w+1.
            int last_black = -1;
            for (int i = e2; i >= s2; --i)
                if (line.cells[i] == CellState::Black) { last_black = i; break; }
            if (last_black >= 0) {
                for (int w = last_black - 1; w >= s2; --w) {
                    if (line.cells[w] == CellState::White) {
                        if (w + 1 > run.range.start) {
                            run.range.start = w + 1;
                            out.ranges_changed = true;
                            if (run.range.end < run.range.start) { out.contradiction = true; return out; }
                        }
                        break;
                    }
                }
            }
            // Case 3 (mirrored): merge last + previous; if > LBj, rj_s = t+2.
            {
                auto bsegs = find_black_segs(line);
                std::vector<BlackSeg> in_range;
                for (const auto& bs : bsegs)
                    if (bs.start >= s2 && bs.end <= e2) in_range.push_back(bs);
                if (in_range.size() >= 2) {
                    const auto& last = in_range.back();
                    for (int si = static_cast<int>(in_range.size()) - 2; si >= 0; --si) {
                        const auto& prev = in_range[si];
                        int merged_len = last.end - prev.start + 1;
                        if (merged_len > run.lb) {
                            int new_s = prev.end + 2;
                            if (new_s > run.range.start) {
                                run.range.start = new_s;
                                out.ranges_changed = true;
                                if (run.range.end < run.range.start) { out.contradiction = true; return out; }
                            }
                            break;
                        }
                    }
                }
            }
        }
    }
    return out;
}

// ---- Driver ----------------------------------------------------------------

LineUpdate apply_single_rule(int index, Line& line) {
    switch (index) {
        case  0: return rule_1_1(line);
        case  1: return rule_1_2(line);
        case  2: return rule_1_3(line);
        case  3: return rule_1_4(line);
        case  4: return rule_1_5(line);
        case  5: return rule_2_1(line);
        case  6: return rule_2_2(line);
        case  7: return rule_2_3(line);
        case  8: return rule_3_1(line);
        case  9: return rule_3_2(line);
        case 10: return rule_3_3(line);
        default: return {};
    }
}

const char* rule_name(int index) {
    switch (index) {
        case  0: return "1.1 intersection";
        case  1: return "1.2 cells outside";
        case  2: return "1.3 boundary len-1";
        case  3: return "1.4 merge exceed";
        case  4: return "1.5 wall / eq-len";
        case  5: return "2.1 ranges ordered";
        case  6: return "2.2 neighbor shrink";
        case  7: return "2.3 oversized seg";
        case  8: return "3.1 scattered fill";
        case  9: return "3.2 skip segments";
        case 10: return "3.3 non-overlap";
        default: return "?";
    }
}

LineUpdate apply_all_rules(Line& line) {
    LineUpdate total;
    auto acc = [&](LineUpdate u) {
        total.cells_changed  |= u.cells_changed;
        total.ranges_changed |= u.ranges_changed;
        total.contradiction  |= u.contradiction;
    };
    acc(rule_1_1(line));
    acc(rule_1_2(line));
    acc(rule_1_3(line));
    acc(rule_1_4(line));
    acc(rule_1_5(line));
    acc(rule_2_1(line));
    acc(rule_2_2(line));
    acc(rule_2_3(line));
    acc(rule_3_1(line));
    acc(rule_3_2(line));
    acc(rule_3_3(line));
    return total;
}

bool run_to_fixpoint(Nonogram& board) {
    for (;;) {
        bool any_change = false;
        for (int r = 0; r < board.height(); ++r) {
            LineUpdate u = apply_all_rules(board.row(r));
            if (u.contradiction) return false;
            if (u.cells_changed)  board.sync_row_to_cols(r);
            any_change |= u.cells_changed || u.ranges_changed;
        }
        for (int c = 0; c < board.width(); ++c) {
            LineUpdate u = apply_all_rules(board.col(c));
            if (u.contradiction) return false;
            if (u.cells_changed)  board.sync_col_to_rows(c);
            any_change |= u.cells_changed || u.ranges_changed;
        }
        if (!any_change) return true;
    }
}

}  // namespace nonogram
