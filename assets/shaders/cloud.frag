#version 330 core

// UX-9a: the per-pixel cloud pattern — an EXACT GLSL port of the CPU
// pure functions in world/cloud.cpp (cloudMask / cloudSubMask, over the
// shared splitmix64 + hash3 of world/noise.hpp). The pixel's world x/z
// (cloud.vert's v_worldXZ) selects the 4-block SUB cell; the predicates
// re-evaluate the CPU field per pixel, so the pattern is WORLD-ANCHORED
// (zero upload after init, zero camera drift, zero window-edge pop-in)
// and the CPU functions in world/cloud stay the ORACLE (tests/
// test_cloud.cpp pins them; the gate's A/B protocol pins the image).
//
// 64-bit port: the project's context is 3.3 core — GLSL 330 has no
// 64-bit integer type. The CPU's uint64_t is emulated as a (lo, hi)
// uvec2 pair; every emulated op is the SAME algebra over the integers
// as the C++, and uint32 arithmetic wraps mod 2^32 = the per-limb wrap
// of the 64-bit result, so the pair ops are bit-exact. int -> uvec2
// sign-extends (GLSL's int->uint wrap = the CPU's asU64
// two's-complement reinterpretation), so negative world coordinates
// hash identically. Constants are copied hex-for-hex from the C++
// source (the 32-bit salts zero-extend to 64 on the CPU — the hi limb
// is 0 here).
//
// Cost (the documented profile): a pure SKY pixel runs 5 cellOn chains
// (region + sub hash each) and discards; an IN-CLOUD pixel exits after
// the first chain; the boundary band adds up to 4 more chains — the
// full-screen worst case is ~12 chains of emulated 64-bit hashes.

in vec2 v_worldXZ;
out vec4 fragColor;

uniform int u_t;
uniform float u_alpha;
uniform vec2 u_camXZ;

// -- constants (hex-for-hex from world/noise.hpp + world/cloud.hpp) ----
// kGoldenStep 0x9E3779B97F4A7C15 (the PITS #4 advance).
const uvec2 K_GOLDEN = uvec2(0x7F4A7C15u, 0x9E3779B9u);
// kSaltCloud 0xC10D05A1 / kSaltCloudSub 0x5B8C92E7 — the two on/off
// levels. kSaltCloudSubHeight 0x9C2E4B71 is the CPU's third (thickness)
// level — NOT evaluated in this pass (the boundary band below is the
// documented constant-width approximation); it is named here so the
// shader's constant set matches the CPU field's for the future
// true-3D (height-aware) port.
const uvec2 K_SALT_CLOUD = uvec2(0xC10D05A1u, 0u);
const uvec2 K_SALT_SUB = uvec2(0x5B8C92E7u, 0u);
const uvec2 K_SALT_H = uvec2(0x9C2E4B71u, 0u);

// -- 64-bit pair ops (lo, hi) ------------------------------------------
// The high 32 bits of a 32x32 product (the 16-bit half decomposition —
// every 16x16 partial fits a uint32 exactly, no wrap).
uint mulHi(uint a, uint b) {
    uint a0 = a & 0xFFFFu;
    uint a1 = a >> 16;
    uint b0 = b & 0xFFFFu;
    uint b1 = b >> 16;
    uint p0 = a0 * b0;
    uint p1 = a0 * b1;
    uint p2 = a1 * b0;
    uint p3 = a1 * b1;
    uint mid = (p0 >> 16) + (p1 & 0xFFFFu) + (p2 & 0xFFFFu);
    return p3 + (p1 >> 16) + (p2 >> 16) + (mid >> 16);
}

// 64-bit add (the carry from the low limb).
uvec2 add64(uvec2 a, uvec2 b) {
    uint lo = a.x + b.x;
    return uvec2(lo, a.y + b.y + (lo < a.x ? 1u : 0u));
}

// 64x64 multiply -> the LOW 64 bits of the product (the CPU's uint64_t
// wrap): lo*blo + (lo*bhi + hi*blo) * 2^32, mod 2^64.
uvec2 mul64(uvec2 a, uvec2 b) {
    return uvec2(a.x * b.x, mulHi(a.x, b.x) + a.x * b.y + a.y * b.x);
}

// 64-bit right shift by n (n in 1..31 — the sm64 amounts 30/27/31 and
// the hash3 amount 1).
uvec2 shr64(uvec2 v, uint n) {
    return uvec2((v.x >> n) | (v.y << (32u - n)), v.y >> n);
}

// The splitmix64 finalizer (world/noise.hpp, op-for-op).
uvec2 sm64(uvec2 x) {
    x = mul64(x ^ shr64(x, 30u), uvec2(0x1CE4E5B9u, 0xBF58476Du));
    x = mul64(x ^ shr64(x, 27u), uvec2(0x133111EBu, 0x94D049BBu));
    return x ^ shr64(x, 31u);
}

// The lattice hash (world/noise.cpp hash3, op-for-op).
uvec2 hash3u(uvec2 a, uvec2 b, uvec2 c, uvec2 seed) {
    uvec2 h = seed;
    h ^= sm64(add64(a, uvec2(0x7F4A7C15u, 0x9E3779B9u)));
    h = add64(h, uvec2(0x1CE4E5B9u, 0xBF58476Du));
    h ^= shr64(sm64(add64(b, uvec2(0x133111EBu, 0x94D049BBu))), 1u);
    h = add64(h, uvec2(0x165667C5u, 0x27D4EB2Fu));
    h ^= shr64(sm64(add64(c, uvec2(0x4F6CDD1Du, 0x2545F491u))), 1u);
    return sm64(h);
}

// The CPU's asU64(int): int -> the sign-extended 64-bit pair (GLSL's
// int->uint wrap gives the low 32 bits of the two's-complement value).
uvec2 i64(int v) {
    return uvec2(uint(v), v < 0 ? 0xFFFFFFFFu : 0u);
}

// world/coordinates.hpp floorDiv (b > 0): C++/GLSL integer division
// truncates toward zero, so a negative remainder steps the quotient
// down.
int floorDivI(int a, int b) {
    int q = a / b;
    int r = a - q * b;
    if (r < 0) {
        --q;
    }
    return q;
}

// -- the presence field (world/cloud.cpp, expression-for-expression) ---
// cloudMask: the 50% region roll — hash3(rx, rz, 0, kSaltCloud) advanced
// by t * kGoldenStep, the splitmix64 finalizer's TOP bit.
bool regionOn(int rx, int rz) {
    uvec2 state = add64(hash3u(i64(rx), i64(rz), uvec2(0u, 0u),
                               K_SALT_CLOUD),
                        mul64(K_GOLDEN, uvec2(uint(u_t), 0u)));
    return (sm64(state).y >> 31) == 1u;
}

// cloudSubMask: the 75% sub roll — hash3(wx, wz, 0, kSaltCloudSub)
// advanced by t * kGoldenStep, the splitmix64 finalizer's mod-4 < 3
// (the mod 4 of a 64-bit value = its low two bits).
bool subOn(int wx, int wz) {
    uvec2 state = add64(hash3u(i64(wx), i64(wz), uvec2(0u, 0u), K_SALT_SUB),
                        mul64(K_GOLDEN, uvec2(uint(u_t), 0u)));
    return (sm64(state).x & 3u) < 3u;
}

// cloudMask ∧ cloudSubMask restated in WORLD 4-block sub coordinates
// (the CPU's cloudSubHeight on/off half — the SAME two-level predicate;
// the sub hash is world-anchored, no region argument needed).
bool cellOn(int wx, int wz) {
    return regionOn(floorDivI(wx, 4), floorDivI(wz, 4)) && subOn(wx, wz);
}

void main() {
    // The pixel's world 4-block sub-grid coordinates (x 0.25 is a power
    // of two — an exact exponent shift; the quad's local range 0..16384
    // and the 4-block-aligned translation keep every corner exact in
    // float32, and the pixel evaluation stays cell-exact for |eye| far
    // below 2^24).
    int wx = int(floor(v_worldXZ.x * 0.25));
    int wz = int(floor(v_worldXZ.y * 0.25));
    if (cellOn(wx, wz)) {
        // Bottom face: always white (the UX-9a sheet look from below —
        // the same brightness as the mesh version's +Y quads).
        fragColor = vec4(1.0, 1.0, 1.0, u_alpha);
        return;
    }
    // THICKNESS = the boundary band (the documented approximation):
    // this pixel is OFF but a NEIGHBOR sub is on, and the camera sits
    // OUTSIDE that neighbor's shared wall (u_camXZ = the exact eye
    // position) -> paint the wall's MC side shade (+/-X 0.8 / +/-Z 0.6)
    // on the cloud plane. The band is always 1 sub cell (4 blocks)
    // wide — a constant stand-in for the true 3D projection of the
    // {1, 2, 3}-tall columns (width h / tan(elevation)), which is
    // narrower at grazing angles; the band sits at y = kCloudHeight
    // rather than at the wall's real height; a TOP face seen from a
    // peak above the layer reuses the bottom pattern.
    float shade = 1.0;
    bool wall = false;
    if (cellOn(wx - 1, wz) && u_camXZ.x > float(wx) * 4.0) {
        shade = min(shade, 0.8); // west neighbor on: its EAST wall (+X)
        wall = true;
    }
    if (cellOn(wx + 1, wz) && u_camXZ.x < float(wx + 1) * 4.0) {
        shade = min(shade, 0.8); // east neighbor on: its WEST wall (-X)
        wall = true;
    }
    if (cellOn(wx, wz - 1) && u_camXZ.y > float(wz) * 4.0) {
        shade = min(shade, 0.6); // south neighbor on: its NORTH wall (+Z)
        wall = true;
    }
    if (cellOn(wx, wz + 1) && u_camXZ.y < float(wz + 1) * 4.0) {
        shade = min(shade, 0.6); // north neighbor on: its SOUTH wall (-Z)
        wall = true;
    }
    if (wall) {
        fragColor = vec4(shade, shade, shade, u_alpha);
    } else {
        discard;
    }
}
