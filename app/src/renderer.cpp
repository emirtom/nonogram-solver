#include "renderer.hpp"

#include <glad/gl.h>

#include <cassert>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

namespace nonogram {

namespace {

// Color palette (linear-ish; fine for our purposes).
constexpr float COL_WHITE[3]    = {0.95f, 0.95f, 0.95f};
constexpr float COL_BLACK[3]    = {0.10f, 0.12f, 0.16f};
constexpr float COL_UNKNOWN[3]  = {0.55f, 0.58f, 0.62f};
constexpr float COL_GRIDLINE[3] = {0.25f, 0.27f, 0.31f};

// Unit quad as two triangles. a_pos in 0..1.
const float QUAD_VERTS[] = {
    0.0f, 0.0f,
    1.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 0.0f,
    1.0f, 1.0f,
    0.0f, 1.0f,
};

bool check_shader(unsigned int shader, const char* what) {
    int ok = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
        std::fprintf(stderr, "shader compile error (%s):\n%s\n", what, log);
        return false;
    }
    return true;
}

bool check_program(unsigned int prog) {
    int ok = 0;
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetProgramInfoLog(prog, sizeof(log), nullptr, log);
        std::fprintf(stderr, "program link error:\n%s\n", log);
        return false;
    }
    return true;
}

}  // namespace

std::string GridRenderer::load_file(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        std::fprintf(stderr, "renderer: cannot open shader: %s\n", path.c_str());
        return {};
    }
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

GridRenderer::GridRenderer() {
    // No GL calls here — the OpenGL context may not be current yet.
    // Call init_after_context() once GLAD is loaded.
}

void GridRenderer::init_after_context() {
    build_shaders();
    build_quad_vbo();
}

GridRenderer::~GridRenderer() {
    if (program_)  glDeleteProgram(program_);
    if (quad_vao_) glDeleteVertexArrays(1, &quad_vao_);
    if (quad_vbo_) glDeleteBuffers(1, &quad_vbo_);
    if (inst_vao_) glDeleteVertexArrays(1, &inst_vao_);
    if (inst_vbo_) glDeleteBuffers(1, &inst_vbo_);
}

bool GridRenderer::build_shaders() {
    // Resolve shader paths relative to the executable's source tree. We rely
    // on the app being run from the repo root (per README); this keeps the
    // code simple and avoids embedding shaders as headers.
    const std::string vert_src = load_file("app/src/shaders/grid.vert");
    const std::string frag_src = load_file("app/src/shaders/grid.frag");
    if (vert_src.empty() || frag_src.empty()) return false;

    const char* vsrc = vert_src.c_str();
    const char* fsrc = frag_src.c_str();
    unsigned int vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsrc, nullptr);
    glCompileShader(vs);
    if (!check_shader(vs, "vertex")) return false;

    unsigned int fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsrc, nullptr);
    glCompileShader(fs);
    if (!check_shader(fs, "fragment")) return false;

    program_ = glCreateProgram();
    glAttachShader(program_, vs);
    glAttachShader(program_, fs);
    glLinkProgram(program_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!check_program(program_)) return false;

    loc_gridOrigin_   = glGetUniformLocation(program_, "u_gridOrigin");
    loc_cellSize_     = glGetUniformLocation(program_, "u_cellSize");
    loc_colorWhite_   = glGetUniformLocation(program_, "u_colorWhite");
    loc_colorBlack_   = glGetUniformLocation(program_, "u_colorBlack");
    loc_colorUnknown_ = glGetUniformLocation(program_, "u_colorUnknown");
    loc_colorGridline_= glGetUniformLocation(program_, "u_colorGridline");
    return true;
}

bool GridRenderer::build_quad_vbo() {
    glGenVertexArrays(1, &quad_vao_);
    glBindVertexArray(quad_vao_);
    glGenBuffers(1, &quad_vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(QUAD_VERTS), QUAD_VERTS, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    // Per-instance VAO: a_offset (vec2) at location 1, a_state (int) at 2.
    glGenVertexArrays(1, &inst_vao_);
    glBindVertexArray(inst_vao_);
    // Re-bind quad VBO so the instance VAO also sees location 0 (a_pos).
    glBindBuffer(GL_ARRAY_BUFFER, quad_vbo_);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);

    glGenBuffers(1, &inst_vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, inst_vbo_);
    // stride = 2 floats (offset) + 1 int (state) = 2*4 + 4 = 12 bytes
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glVertexAttribDivisor(1, 1);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)(2 * sizeof(float)));
    glVertexAttribDivisor(2, 1);
    glBindVertexArray(0);
    return true;
}

void GridRenderer::resize(int width, int height) {
    width_  = width;
    height_ = height;
    // (Re)allocate the instance buffer for width*height cells. The actual
    // data is uploaded in update_states() each frame.
    glBindBuffer(GL_ARRAY_BUFFER, inst_vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(width * height) * 3 * sizeof(float),
                 nullptr, GL_DYNAMIC_DRAW);
}

void GridRenderer::update_states(const PuzzleState& state) {
    if (state.width() != width_ || state.height() != height_)
        resize(state.width(), state.height());

    // Pack per-instance data: (col, row, state) per cell.
    std::vector<float> data(static_cast<size_t>(width_ * height_) * 3);
    for (int r = 0; r < height_; ++r) {
        for (int c = 0; c < width_; ++c) {
            size_t idx = (static_cast<size_t>(r) * width_ + c) * 3;
            data[idx + 0] = static_cast<float>(c);
            data[idx + 1] = static_cast<float>(r);
            data[idx + 2] = static_cast<float>(static_cast<int>(state.cell(c, r)));
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, inst_vbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(data.size() * sizeof(float)),
                    data.data());
}

void GridRenderer::draw(int fbW, int fbH, GridLayout& out_layout) {
    out_layout = GridLayout::compute(fbW, fbH, width_, height_);
    if (width_ == 0 || height_ == 0 || program_ == 0) return;

    const float cell_px   = out_layout.cellPx;
    const float grid_w_px = out_layout.gridPxW;
    const float grid_h_px = out_layout.gridPxH;

    // NDC: x in [-1,1] right-positive, y in [-1,1] up-positive.
    // Grid origin (top-left pixel) -> NDC.
    const float origin_x_ndc = (2.0f * out_layout.originX / fbW) - 1.0f;
    const float origin_y_ndc = 1.0f - (2.0f * out_layout.originY / fbH);
    const float cell_w_ndc   = (2.0f * cell_px) / fbW;
    const float cell_h_ndc   = -(2.0f * cell_px) / fbH;  // negative: y-down

    glUseProgram(program_);
    glUniform2f(loc_gridOrigin_, origin_x_ndc, origin_y_ndc);
    glUniform2f(loc_cellSize_,   cell_w_ndc,   cell_h_ndc);
    glUniform3fv(loc_colorWhite_,    1, COL_WHITE);
    glUniform3fv(loc_colorBlack_,    1, COL_BLACK);
    glUniform3fv(loc_colorUnknown_,  1, COL_UNKNOWN);
    glUniform3fv(loc_colorGridline_, 1, COL_GRIDLINE);

    glBindVertexArray(inst_vao_);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 6, width_ * height_);
    glBindVertexArray(0);

    (void)grid_w_px; (void)grid_h_px;
}

}  // namespace nonogram
