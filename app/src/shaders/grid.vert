#version 330 core
// Instanced quad renderer for the nonogram grid.
// Attributes:
//   a_pos    : 2D position in a unit quad (0..1), static VBO (6 verts).
//   a_offset : per-instance cell offset (cell column, row) in grid coords.
//   a_state  : per-instance cell state (0=White, 1=Black, 2=Unknown).
//   a_gridOrigin : uniform — pixel origin of grid top-left in NDC.
//   a_cellSize   : uniform — size of one cell in NDC.
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_offset;   // (col, row)
layout(location = 2) in float a_state;   // 0=White, 1=Black, 2=Unknown

uniform vec2  u_gridOrigin;  // top-left of grid in NDC
uniform vec2  u_cellSize;    // one cell in NDC
uniform float u_gridlineW;   // gridline inset fraction (e.g., 0.04)

flat out int  v_state;
out vec2      v_uv;          // 0..1 within the cell, for inset

void main() {
    vec2 cell_ndc = u_gridOrigin + a_offset * u_cellSize + a_pos * u_cellSize;
    gl_Position = vec4(cell_ndc, 0.0, 1.0);
    v_state = int(a_state);
    v_uv = a_pos;
}
