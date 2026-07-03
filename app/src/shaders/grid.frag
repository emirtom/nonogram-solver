#version 330 core
// Fragment shader: pick fill color from cell state, with a small inset so
// gridlines show through as background.
flat in  int v_state;
in  vec2 v_uv;
out vec4 fragColor;

const int STATE_WHITE   = 0;
const int STATE_BLACK   = 1;
const int STATE_UNKNOWN = 2;

uniform vec3 u_colorWhite;    // empty cell fill
uniform vec3 u_colorBlack;    // filled cell fill
uniform vec3 u_colorUnknown;  // undetermined cell fill
uniform vec3 u_colorGridline; // gridline color (background)

void main() {
    // inset: discard a thin border so gridlines appear as the cleared color
    float inset = 0.06;
    if (v_uv.x < inset || v_uv.x > 1.0 - inset ||
        v_uv.y < inset || v_uv.y > 1.0 - inset) {
        fragColor = vec4(u_colorGridline, 1.0);
        return;
    }
    vec3 c;
    if      (v_state == STATE_BLACK)   c = u_colorBlack;
    else if (v_state == STATE_WHITE)   c = u_colorWhite;
    else                               c = u_colorUnknown;
    fragColor = vec4(c, 1.0);
}
