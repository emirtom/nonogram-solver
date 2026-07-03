#pragma once

#include "grid_layout.hpp"
#include "puzzle_state.hpp"

#include <string>

namespace nonogram {

// Instanced grid renderer: one static unit-quad VBO + one per-instance VBO
// (offset + state) that is re-uploaded whenever the board changes.
class GridRenderer {
  public:
    GridRenderer();
    ~GridRenderer();

    GridRenderer(const GridRenderer&) = delete;
    GridRenderer& operator=(const GridRenderer&) = delete;

    void init_after_context();
    void resize(int width, int height);
    void update_states(const PuzzleState& state);

    // Draw the grid. The layout (cell size, origin) is computed via
    // GridLayout and returned in `out_layout` so the caller can place
    // clue labels consistently.
    void draw(int fbW, int fbH, GridLayout& out_layout);

  private:
    bool build_shaders();
    bool build_quad_vbo();
    int  width_  = 0;
    int  height_ = 0;

    unsigned int program_   = 0;
    unsigned int quad_vao_  = 0;
    unsigned int quad_vbo_  = 0;
    unsigned int inst_vao_  = 0;
    unsigned int inst_vbo_  = 0;

    // uniform locations
    int loc_gridOrigin_  = -1;
    int loc_cellSize_    = -1;
    int loc_colorWhite_  = -1;
    int loc_colorBlack_  = -1;
    int loc_colorUnknown_ = -1;
    int loc_colorGridline_ = -1;

    // raw shader sources, loaded from disk at build time
    static std::string load_file(const std::string& path);
};

}  // namespace nonogram
