#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in float a_light;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

out vec3 v_normal;
out vec2 v_uv;
out float v_light;

void main() {
    // Model is rigid (translation + rotation only) in Phase 1, so the
    // model rotation matrix is the correct normal transform.
    v_normal = mat3(u_model) * a_normal;
    v_uv = a_uv;
    v_light = a_light;
    gl_Position = u_proj * u_view * u_model * vec4(a_position, 1.0);
}
