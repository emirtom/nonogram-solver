#include "app.hpp"
#include "nonogram/logical_rules.hpp"

#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <algorithm>
#include <cstdio>
#include <dirent.h>
#include <sys/stat.h>
#include <sstream>

namespace nonogram {

namespace {

void list_json_files(const std::string& dir, std::vector<std::string>& out) {
    DIR* d = opendir(dir.c_str());
    if (!d) return;
    while (struct dirent* ent = readdir(d)) {
        std::string name = ent->d_name;
        if (name.size() < 6) continue;
        if (name.compare(name.size() - 5, 5, ".json") == 0)
            out.push_back(dir + "/" + name);
    }
    closedir(d);
    std::sort(out.begin(), out.end());
}

const char* status_str(SolveStatus s) {
    switch (s) {
        case SolveStatus::Solved:            return "solved";
        case SolveStatus::NoSolution:        return "no solution";
        case SolveStatus::MultipleSolutions: return "multiple solutions";
        case SolveStatus::InProgress:        return "in progress";
    }
    return "?";
}

ImU32 clue_color()    { return IM_COL32(220, 225, 230, 255); }
ImU32 clue_bg_color() { return IM_COL32(30, 32, 36, 200); }

}  // namespace

App::App() {
    refresh_puzzle_list();
}

App::~App() {
    shutdown_imgui();
    shutdown_window();
}

void App::framebuffer_size_cb(GLFWwindow* /*w*/, int width, int height) {
    glViewport(0, 0, width, height);
}

bool App::init_window() {
    if (!glfwInit()) return false;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window_ = glfwCreateWindow(fbW_, fbH_, "nonogram-solver", nullptr, nullptr);
    if (!window_) { glfwTerminate(); return false; }
    glfwMakeContextCurrent(window_);
    glfwSetFramebufferSizeCallback(window_, framebuffer_size_cb);
    glfwSetWindowUserPointer(window_, this);
    if (!gladLoadGL(glfwGetProcAddress)) {
        std::fprintf(stderr, "failed to load GL\n");
        return false;
    }
    return true;
}

void App::shutdown_window() {
    if (window_) { glfwDestroyWindow(window_); window_ = nullptr; }
    glfwTerminate();
}

void App::init_imgui() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");
    ImGui::StyleColorsDark();
}

void App::shutdown_imgui() {
    if (window_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
}

void App::refresh_puzzle_list() {
    puzzles_.clear();
    list_json_files("puzzles", puzzles_);
    if (!puzzles_.empty()) {
        selected_puzzle_ = 0;
        state_.load_file(puzzles_[0]);
    }
}

void App::draw_puzzle_picker() {
    ImGui::Text("Puzzles");
    for (int i = 0; i < static_cast<int>(puzzles_.size()); ++i) {
        // Strip the "puzzles/" prefix for display.
        const std::string& p = puzzles_[i];
        std::string label = p;
        if (label.rfind("puzzles/", 0) == 0) label = label.substr(8);
        bool selected = (selected_puzzle_ == i);
        if (ImGui::Selectable(label.c_str(), selected)) {
            selected_puzzle_ = i;
            stop_animation();
            state_.load_file(p);
            ever_solved_ = false;
            last_status_ = SolveStatus::InProgress;
        }
    }
}

void App::draw_solver_controls() {
    ImGui::Separator();
    ImGui::Text("Solver");

    if (animating_) {
        if (ImGui::Button("Stop")) {
            stop_animation();
        }
    } else {
        if (ImGui::Button("Solve")) {
            bad_cells_ = state_.find_contradictions();
            if (!bad_cells_.empty()) {
                show_error_ = true;
                error_time_ = glfwGetTime();
                last_status_ = SolveStatus::NoSolution;
            } else {
                show_error_ = false;
                state_.reset_solver();
                animating_ = true;
                step_count_ = 0;
                step_accumulator_ = 0.0;
                last_rule_index_ = -1;
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Step")) {
        stop_animation();
        bad_cells_ = state_.find_contradictions();
        if (!bad_cells_.empty()) {
            show_error_ = true;
            error_time_ = glfwGetTime();
            last_status_ = SolveStatus::NoSolution;
        } else {
            show_error_ = false;
            SolveOutcome o = state_.step();
            last_status_     = o.status;
            last_rule_index_ = o.rule_index;
            if (o.status == SolveStatus::Solved) ever_solved_ = true;
            if (o.status == SolveStatus::NoSolution) {
                bad_cells_ = state_.find_contradictions();
                if (!bad_cells_.empty()) { show_error_ = true; error_time_ = glfwGetTime(); }
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) {
        stop_animation();
        state_.reset();
        ever_solved_ = false;
        last_status_ = SolveStatus::InProgress;
        bad_cells_.clear();
        show_error_ = false;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        stop_animation();
        if (!state_.empty()) {
            int w = state_.width(), h = state_.height();
            std::vector<std::vector<int>> empty_rows(h), empty_cols(w);
            state_.load_inline(w, h, empty_rows, empty_cols);
            ever_solved_ = false;
            last_status_ = SolveStatus::InProgress;
            bad_cells_.clear();
            show_error_ = false;
        }
    }

    ImGui::Separator();
    ImGui::Text("Animation speed");
    ImGui::PushItemWidth(120);
    ImGui::SliderFloat("##speed", &step_interval_, 0.02f, 0.5f, "%.2f s", ImGuiSliderFlags_Logarithmic);
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (animating_)
        ImGui::TextDisabled("step %d – %s", step_count_,
            last_rule_index_ >= 0 ? rule_name(last_rule_index_) : "?");

    ImGui::Separator();
    ImGui::Text("Backend");
    ImGui::RadioButton("CPU",  reinterpret_cast<int*>(&backend_), static_cast<int>(Backend::CPU));
    ImGui::SameLine();
    ImGui::RadioButton("CUDA", reinterpret_cast<int*>(&backend_), static_cast<int>(Backend::CUDA));
    if (backend_ == Backend::CUDA) {
        ImGui::SameLine();
        ImGui::TextDisabled("(not built)");
    }
}

void App::draw_paint_mode() {
    ImGui::Separator();
    ImGui::Text("Paint mode (left-click to paint)");
    ImGui::RadioButton("Black",   reinterpret_cast<int*>(&paint_mode_), static_cast<int>(CellState::Black));
    ImGui::SameLine();
    ImGui::RadioButton("White",   reinterpret_cast<int*>(&paint_mode_), static_cast<int>(CellState::White));
    ImGui::SameLine();
    ImGui::RadioButton("Unknown", reinterpret_cast<int*>(&paint_mode_), static_cast<int>(CellState::Unknown));
}

void App::draw_clues(const GridLayout& layout) {
    if (state_.empty() || layout.cellPx <= 0) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const float font_size = std::max(10.0f, std::min(layout.cellPx * 0.35f, 22.0f));
    const float line_h = font_size + 2.0f;

    // Column clues: stacked above each column, bottom-aligned to the grid top.
    for (int c = 0; c < state_.width(); ++c) {
        const auto& clues = state_.col_clues(c);
        if (clues.empty()) continue;
        const float cell_x = layout.originX + c * layout.cellPx;
        const float cell_cx = cell_x + layout.cellPx * 0.5f;
        // Draw clues from bottom up so the last clue sits just above the grid.
        for (int k = static_cast<int>(clues.size()) - 1; k >= 0; --k) {
            const float y = layout.originY - (clues.size() - k) * line_h;
            std::ostringstream ss;
            ss << clues[k].lb;
            const char* txt = ss.str().c_str();
            ImVec2 sz = ImGui::CalcTextSize(txt);
            // Use ImGui's default font; scale not applied (keep simple).
            dl->AddText(ImVec2(cell_cx - sz.x * 0.5f, y), clue_color(), txt);
        }
    }

    // Row clues: laid out to the left of each row, right-aligned to the grid.
    for (int r = 0; r < state_.height(); ++r) {
        const auto& clues = state_.row_clues(r);
        if (clues.empty()) continue;
        const float cell_y = layout.originY + r * layout.cellPx;
        const float cell_cy = cell_y + layout.cellPx * 0.5f;
        // Draw clues left-to-right, ending just before the grid.
        const float total_w = clues.size() * line_h;
        for (int k = 0; k < static_cast<int>(clues.size()); ++k) {
            const float x = layout.originX - total_w + k * line_h;
            std::ostringstream ss;
            ss << clues[k].lb;
            const char* txt = ss.str().c_str();
            ImVec2 sz = ImGui::CalcTextSize(txt);
            dl->AddText(ImVec2(x, cell_cy - sz.y * 0.5f), clue_color(), txt);
        }
    }
}

void App::draw_contradictions(const GridLayout& layout) {
    if (!show_error_ || bad_cells_.empty() || layout.cellPx <= 0) return;
    // Auto-dismiss after 4 seconds.
    if (glfwGetTime() - error_time_ > 4.0) {
        show_error_ = false;
        return;
    }
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    const ImU32 red = IM_COL32(235, 60, 60, 220);
    const float inset = layout.cellPx * 0.06f;
    for (const auto& [c, r] : bad_cells_) {
        const float x = layout.originX + c * layout.cellPx + inset;
        const float y = layout.originY + r * layout.cellPx + inset;
        const float w = layout.cellPx - 2 * inset;
        dl->AddRect(ImVec2(x, y), ImVec2(x + w, y + w), red, 4.0f, 0, 3.0f);
    }
    // Red border around the whole grid.
    dl->AddRect(ImVec2(layout.originX - 2, layout.originY - 2),
                ImVec2(layout.originX + layout.gridPxW + 2,
                       layout.originY + layout.gridPxH + 2),
                red, 0, 0, 2.0f);
}

void App::draw_status_line() {
    ImGui::Separator();
    // Error banner — prominent red block, auto-dismisses.
    if (show_error_ && !bad_cells_.empty()) {
        const double elapsed = glfwGetTime() - error_time_;
        if (elapsed > 4.0) {
            show_error_ = false;
        } else {
            int alpha = 255;
            // Fade out in the last second.
            if (elapsed > 3.0) alpha = static_cast<int>(255 * (4.0 - elapsed));
            ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(120, 20, 20, alpha));
            ImGui::BeginChild("err", ImVec2(0, 32), true);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.78f, 0.78f, alpha / 255.0f));
            ImGui::Text("Contradiction! %zu cell(s) conflict with the clues.",
                bad_cells_.size());
            ImGui::PopStyleColor();
            ImGui::SameLine();
            if (ImGui::SmallButton("Dismiss")) show_error_ = false;
            ImGui::EndChild();
            ImGui::PopStyleColor();
        }
    }
    ImGui::Text("Status: %s", status_str(last_status_));
    if (ever_solved_ || last_status_ == SolveStatus::Solved)
        ImGui::Text("CB nodes: %lld", last_nodes_);
    if (!state_.empty())
        ImGui::Text("Grid: %dx%d", state_.width(), state_.height());
}

void App::draw_chrome() {
    ImGui::Begin("Controls");
    draw_puzzle_picker();
    draw_solver_controls();
    draw_paint_mode();
    draw_status_line();
    ImGui::End();
}

bool App::mouse_to_cell(double mouseX, double mouseY, int fbW, int fbH,
                        int& outCol, int& outRow) const {
    if (state_.empty()) return false;
    GridLayout g = GridLayout::compute(fbW, fbH, state_.width(), state_.height());
    const float rel_x = static_cast<float>(mouseX) - g.originX;
    const float rel_y = static_cast<float>(mouseY) - g.originY;
    if (rel_x < 0 || rel_y < 0) return false;
    outCol = static_cast<int>(rel_x / g.cellPx);
    outRow = static_cast<int>(rel_y / g.cellPx);
    return outCol >= 0 && outCol < g.gridW && outRow >= 0 && outRow < g.gridH;
}

void App::handle_mouse(int fbW, int fbH) {
    if (state_.empty() || ImGui::GetIO().WantCaptureMouse) {
        dragging_ = false;
        return;
    }
    double mx, my;
    glfwGetCursorPos(window_, &mx, &my);
    int winW, winH;
    glfwGetWindowSize(window_, &winW, &winH);
    if (winW > 0 && fbW != winW) {
        mx = mx * static_cast<double>(fbW) / winW;
        my = my * static_cast<double>(fbH) / winH;
    }

    int col, row;
    const bool over_cell = mouse_to_cell(mx, my, fbW, fbH, col, row);
    const int left = glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT);

    if (!dragging_) {
        if (over_cell && left == GLFW_PRESS) {
            dragging_ = true;
            drag_button_ = 0;
            stop_animation();
            state_.set_cell(col, row, paint_mode_);
            ever_solved_ = false;
            last_status_ = SolveStatus::InProgress;
            bad_cells_.clear();
            show_error_ = false;
        }
    } else {
        if (left != GLFW_PRESS) {
            dragging_ = false;
            return;
        }
        if (over_cell) {
            state_.set_cell(col, row, paint_mode_);
            bad_cells_.clear();
            show_error_ = false;
        }
    }
}

void App::stop_animation() {
    animating_        = false;
    step_accumulator_ = 0.0;
    step_count_       = 0;
    last_rule_index_  = -1;
}

int App::run() {
    if (!init_window()) return 1;
    init_imgui();
    renderer_.init_after_context();

    // Pre-load a puzzle so the grid is visible immediately.
    if (state_.empty() && !puzzles_.empty())
        state_.load_file(puzzles_[0]);

    last_time_ = glfwGetTime();

    while (!glfwWindowShouldClose(window_)) {
        glfwPollEvents();

        glfwGetFramebufferSize(window_, &fbW_, &fbH_);
        glViewport(0, 0, fbW_, fbH_);
        glClearColor(0.18f, 0.20f, 0.22f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        handle_mouse(fbW_, fbH_);

        // Animated solving: advance one step per interval.
        if (animating_) {
            double now = glfwGetTime();
            step_accumulator_ += now - last_time_;
            last_time_ = now;

            if (step_accumulator_ >= step_interval_) {
                step_accumulator_ -= step_interval_;

                SolveOutcome o = state_.step_rule();
                ++step_count_;
                last_status_     = o.status;
                last_rule_index_ = o.rule_index;

                if (o.status == SolveStatus::Solved) {
                    ever_solved_ = true;
                    last_nodes_  = o.nodes;
                    stop_animation();
                } else if (o.status == SolveStatus::NoSolution) {
                    bad_cells_ = state_.find_contradictions();
                    if (!bad_cells_.empty()) {
                        show_error_ = true;
                        error_time_ = glfwGetTime();
                    }
                    stop_animation();
                }
            }
        } else {
            last_time_ = glfwGetTime();
        }

        // Draw the grid (behind ImGui).
        GridLayout layout;
        if (!state_.empty()) {
            renderer_.update_states(state_);
            renderer_.draw(fbW_, fbH_, layout);
        }

        // Draw ImGui chrome + clue labels on top.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        draw_chrome();
        draw_clues(layout);
        draw_contradictions(layout);
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window_);
    }
    return 0;
}

}  // namespace nonogram
