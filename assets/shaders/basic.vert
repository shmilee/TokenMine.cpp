#version 330 core

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in float a_skyLight;
layout(location = 4) in float a_blockLight;
layout(location = 5) in vec2 a_uvLocal;
layout(location = 6) in float a_tiled;

uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

out vec2 v_uv;
out vec2 v_uvLocal;
out float v_tiled;
out float v_skyLight;
out float v_blockLight;
out float v_faceShade;

void main() {
    // Model is rigid (translation + rotation only) in Phase 1, so the
    // model rotation matrix is the correct normal transform.
    vec3 n = mat3(u_model) * a_normal;
    v_uv = a_uv;
    v_uvLocal = a_uvLocal;
    v_tiled = a_tiled;
    v_skyLight = a_skyLight;
    v_blockLight = a_blockLight;
    // Classic per-face shading (the Minecraft lightmap look): a lightmap
    // value alone renders flat, so each face orientation gets a fixed
    // multiplier. Chunk models are pure translations, so the world normal
    // equals the local normal.
    if (n.y > 0.5) {
        v_faceShade = 1.0;
    } else if (n.y < -0.5) {
        v_faceShade = 0.5;
    } else if (abs(n.x) > 0.5) {
        v_faceShade = 0.8;
    } else {
        v_faceShade = 0.6;
    }
    gl_Position = u_proj * u_view * u_model * vec4(a_position, 1.0);
}
