#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in float a_skyLight;
layout(location = 4) in float a_blockLight;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

out vec2 v_uv;

void main() {
    // Unlit pass. `a_position` is either SCREEN pixels (the 2D UI, drawn
    // with an ortho u_proj, y-down) or world space (the target-block
    // outline, drawn with the camera matrices). The vertex light slots
    // and the normal are unused by design.
    v_uv = a_uv;
    gl_Position = u_proj * u_view * u_model * vec4(a_position, 1.0);
}
