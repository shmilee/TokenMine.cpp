#version 330 core

// Billboarded sky sprites (sun, moon, stars): small quads centered on
// world-space points, sized in pixels. Point sprites are not used because
// some drivers (e.g. Mesa ANV) clamp shader gl_PointSize to 1.
//   a_position : quad center (the star centers; the sun/moon use the shared
//                origin vertex together with u_offset)
//   a_corner   : quad corner in [-0.5, 0.5]^2 (view-space XY, Z unused) —
//                carried in the vertex "normal" slot by Mesh
layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_corner;

uniform mat4 u_view;
uniform mat4 u_proj;
uniform vec3 u_offset;
uniform vec2 u_sizePixels;
// 2 * tan(fovY / 2) / viewportHeight: the view-space length of one pixel at
// the sprite's depth (identical in X and Y for a symmetric perspective).
uniform float u_pxToWorld;

out vec2 v_corner;

void main() {
    v_corner = a_corner.xy;
    vec4 viewPos = u_view * vec4(a_position + u_offset, 1.0);
    viewPos.xy += a_corner.xy * u_sizePixels * (u_pxToWorld * -viewPos.z);
    gl_Position = u_proj * viewPos;
}
