#version 330 core

// UX-9a: the fragment-shader cloud layer (the mesh -> shader port). The
// host draws ONE kCloudQuadSize x kCloudQuadSize world-space quad at
// y = kCloudHeight (local corner coordinates 0..kCloudQuadSize,
// translated per draw to the camera-anchored 4-block sub-lattice origin,
// so the pattern itself never moves with the camera). The cloud PATTERN
// is computed per pixel from the pixel's WORLD x/z in cloud.frag — an
// exact port of the CPU pure field (world/cloud: cloudMask ∧
// cloudSubMask), so there is no geometry upload after init (the iGPU's
// ~140 ms per MB buffer-update wall that the old per-16-block window
// mesh rebuild hit) and no window-edge pop-in.
//
// Only a_position (location 0) is consumed: the MeshVertex VAO enables
// the remaining attributes too, but unused inputs are harmless (the
// ui.vert / sky.vert unlit-pass convention).

layout(location = 0) in vec3 a_position;

// These three names are required — Renderer::draw sets u_model / u_view
// / u_proj by name (src/renderer/renderer.cpp).
uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_proj;

out vec2 v_worldXZ;

void main() {
    vec4 wp = u_model * vec4(a_position, 1.0);
    v_worldXZ = wp.xz;
    gl_Position = u_proj * u_view * wp;
}
