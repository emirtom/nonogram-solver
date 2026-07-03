#pragma once

#include <algorithm>

namespace nonogram {

// Shared grid geometry. Both the renderer and the app use this so mouse
// hit-testing and clue-label placement agree on the same layout.
//
// The grid is centered in the framebuffer with a margin fraction on each
// side. Cell size is the smaller of (avail_w / W, avail_h / H). We also
// reserve space above and to the left of the grid for clue numbers.
struct GridLayout {
    int    fbW = 0, fbH = 0;
    int    gridW = 0, gridH = 0;     // grid dimensions in cells
    float  cellPx = 0;              // pixel size of one cell
    float  originX = 0, originY = 0;// top-left pixel of the grid (y-down)
    float  gridPxW = 0, gridPxH = 0;

    // Margin fraction (each side). Clue area is taken from inside this margin.
    static constexpr float MARGIN = 0.12f;

    // Build a layout centered in the framebuffer, leaving room for clues.
    static GridLayout compute(int fbW, int fbH, int gridW, int gridH) {
        GridLayout g;
        g.fbW = fbW; g.fbH = fbH;
        g.gridW = gridW; g.gridH = gridH;
        if (gridW == 0 || gridH == 0) return g;

        // Reserve space for clues: max number of runs in any line, times a
        // per-clue line height. We approximate with a fixed fraction.
        const float clueFracX = 0.08f;  // 8% of fb width for row clues
        const float clueFracY = 0.08f;  // 8% of fb height for col clues
        const float avail_w = fbW * (1.0f - 2.0f * MARGIN) - fbW * clueFracX;
        const float avail_h = fbH * (1.0f - 2.0f * MARGIN) - fbH * clueFracY;
        g.cellPx = std::min(avail_w / gridW, avail_h / gridH);
        g.gridPxW = g.cellPx * gridW;
        g.gridPxH = g.cellPx * gridH;
        // Center the grid in the framebuffer, shifted right/down to leave
        // room for clues on the top and left.
        const float totalW = g.gridPxW + fbW * clueFracX;
        const float totalH = g.gridPxH + fbH * clueFracY;
        g.originX = (fbW - totalW) * 0.5f + fbW * clueFracX;
        g.originY = (fbH - totalH) * 0.5f + fbH * clueFracY;
        return g;
    }
};

}  // namespace nonogram
