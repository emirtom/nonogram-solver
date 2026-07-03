#include "nonogram/nonogram.hpp"
#include "nonogram/logical_rules.hpp"
#include "nonogram/solver.hpp"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

// Usage:
//   ./solver_cli puzzle.json           solve via LR fixpoint, print grid
//   ./solver_cli puzzle.json --step    print after each LR pass
//   ./solver_cli puzzle.json --lr-only stop after LR fixpoint (don't run CB)
//   ./solver_cli puzzle.json --stats   print CB node count
//
// ASCII: '#' black, '.' white, '?' unknown.

namespace {

void print_board(const nonogram::Nonogram& b, const std::string& label) {
    if (!label.empty()) std::printf("%s:\n", label.c_str());
    std::fputs(b.to_ascii().c_str(), stdout);
    std::fputc('\n', stdout);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s puzzle.json [--step] [--lr-only] [--stats]\n", argv[0]);
        return 2;
    }

    const std::string path = argv[1];
    bool step_mode = false, lr_only = false, stats = false;
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if      (a == "--step")    step_mode = true;
        else if (a == "--lr-only") lr_only = true;
        else if (a == "--stats")   stats   = true;
        else { std::fprintf(stderr, "unknown flag: %s\n", a.c_str()); return 2; }
    }

    nonogram::Nonogram board;
    try {
        board = nonogram::Nonogram::from_json_file(path);
    } catch (const std::exception& e) {
        std::fprintf(stderr, "load error: %s\n", e.what());
        return 1;
    }

    std::printf("Loaded %dx%d puzzle: %s\n", board.width(), board.height(), path.c_str());
    print_board(board, "initial");

    nonogram::Solver solver(std::move(board));

    if (step_mode) {
        int pass = 0;
        for (;;) {
            nonogram::SolveOutcome o = solver.step();
            ++pass;
            std::printf("--- after LR pass %d (changed=%d) ---\n", pass, o.lr_changed);
            print_board(solver.board(), "");
            if (o.status == nonogram::SolveStatus::Solved) {
                std::printf("Solved by LR alone after %d passes\n", pass);
                break;
            }
            if (o.status == nonogram::SolveStatus::NoSolution) {
                std::printf("No solution found after %d passes\n", pass);
                break;
            }
            if (!o.lr_changed) {
                std::printf("LR fixpoint reached after %d passes\n", pass);
                break;
            }
        }
    } else {
        nonogram::SolveOptions opts;
        opts.lr_only = lr_only;
        nonogram::SolveOutcome o = solver.solve(opts);
        print_board(solver.board(), lr_only ? "after LR fixpoint" : "after solve (LR + CB)");
        const char* status_str = "in progress";
        switch (o.status) {
            case nonogram::SolveStatus::Solved:            status_str = "solved"; break;
            case nonogram::SolveStatus::NoSolution:        status_str = "no solution"; break;
            case nonogram::SolveStatus::MultipleSolutions: status_str = "multiple solutions"; break;
            case nonogram::SolveStatus::InProgress:        status_str = "in progress (unknowns remain)"; break;
        }
        std::printf("status: %s\n", status_str);
        if (stats) std::printf("CB nodes explored: %lld\n", o.nodes);
    }
    return 0;
}
