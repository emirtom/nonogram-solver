#pragma once

#include "puzzle_state.hpp"
#include "renderer.hpp"

#include <string>
#include <vector>

struct GLFWwindow;

namespace nonogram {

class App {
  public:
    App();
    ~App();

    App(const App&) = delete;
    App& operator=(const App&) = delete;

    // Run the main loop. Returns 0 on clean exit.
    int run();

  private:
    // GLFW callbacks (static, forward to instance via window user pointer)
    static void framebuffer_size_cb(GLFWwindow*, int w, int h);

    bool init_window();
    void shutdown_window();
    void init_imgui();
    void shutdown_imgui();

    // UI chrome
    void draw_chrome();
    void draw_puzzle_picker();
    void draw_solver_controls();
    void draw_paint_mode();
    void draw_status_line();

    // Draw clue numbers around the grid using ImGui's foreground draw list.
    void draw_clues(const GridLayout& layout);
    // Draw red highlights on contradicting cells.
    void draw_contradictions(const GridLayout& layout);

    // Grid interaction
    void handle_mouse(int fbW, int fbH);
    // Convert mouse position to cell index; returns false if outside grid.
    bool mouse_to_cell(double mouseX, double mouseY, int fbW, int fbH,
                       int& outCol, int& outRow) const;

    // Scan puzzles/ directory for available puzzle files.
    void refresh_puzzle_list();

    GLFWwindow* window_     = nullptr;
    int         fbW_        = 1280;
    int         fbH_        = 800;

    PuzzleState   state_;
    GridRenderer  renderer_;

    // Puzzle picker
    std::vector<std::string> puzzles_;
    int                      selected_puzzle_ = -1;

    // Interaction state
    bool   dragging_    = false;
    int    drag_button_ = -1;   // 0=left, 1=right
    CellState paint_mode_ = CellState::Black;  // currently selected paint mode

    // Solver status
    SolveStatus last_status_ = SolveStatus::InProgress;
    long long   last_nodes_  = 0;
    bool        ever_solved_ = false;

    // Contradiction feedback
    std::vector<std::pair<int, int>> bad_cells_;
    double      error_time_   = 0.0;   // glfw time when error was flagged
    bool        show_error_   = false;

    // Backend selection
    Backend backend_ = Backend::CPU;
};

}  // namespace nonogram
