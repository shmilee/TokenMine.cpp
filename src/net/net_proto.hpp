// V3-12b (the 2-player lockstep PoC): the NET PROTOCOL layer — pure
// functions only (the design §2.2's net-agnostic discipline: the
// socket lives in net_session, this file is the codec + the state
// hash, unit-testable without a single fd).
//
// Wire format: little-endian fixed headers, no compression, packets
// <= 512 B. Every packet = {u32 magic, u8 type, u32 payloadLen,
// payload...}. The TCP stream is reassembled per-connection (the
// R16's standard buffer loop — the half-packet / the coalesced-packet
// tolerance lives in net_session, this file's decoders assume a whole
// payload slice).
//
// The frame-ordering law (design §2.3): the simulation of frame N =
// {dt[N] (the host's wall dt, raw f64 bits), FrameState[N-D] x every
// player}; D = kNetInputDelay (2) is the session constant. The INPUT
// packet carries the sender's FrameState for the frame it was captured
// on (its local frame number = the shared sim frame number); the
// host files it by frame and consumes it D frames later.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mc {

// Forward declarations (the hash / config readers need only the
// complete types in net_proto.cpp — the pure-decoder half of this
// header stays dependency-free).
class Game;
class Config;

// The packet's magic (the "TMN1" wire bytes — the HELLO's first read).
inline constexpr std::uint32_t kNetMagic = 0x544D4E31u;
// The packet types (the fixed vocabulary — an unknown type = the
// stream desync's named error, the save version-lie's twin).
enum class NetType : std::uint8_t {
    Hello = 1, // client -> host: the four-fingerprint handshake
    Welcome = 2, // host -> client: the world spec + the session consts
    Reject = 3, // host -> client: the named refusal (six faces, S6)
    Input = 4, // client -> host, every frame: the FrameState + the hash
    Frame = 5, // host -> client, every frame: dt + everyone's states
    Ctrl = 6, // host -> client, frame-boundary: the leave / the disconnect
    Chat = 7, // V3-12c1: the chat relay (client -> host, host rebroadcasts)
};
// The input delay D (design §2.4's 38-42ms read: D x 16.7ms + RTT/2).
inline constexpr std::uint8_t kNetInputDelay = 2;
// The state-hash exchange cadence (every 30 frames; design §2.3).
inline constexpr std::uint8_t kNetHashInterval = 30;
// The FrameState's packed size (the fixed layout below — 40 bytes,
// the "FrameState ~40B" of the design; the text stays 0 in 12b, the
// field is reserved for the 12c chat's carrier).
inline constexpr std::size_t kNetFrameStateBytes = 40;
// The player cap (design D14: the protocol is fixed-size for
// kNetMaxPlayers; 12b ran exactly 2 — the c1 N-generalization lifts
// the host's accept to the cap, the FrameState array's count rides
// the FRAME's u8).
inline constexpr std::uint8_t kNetMaxPlayers = 8;

// The FRAME STATE (design §2.3): one player's input for one frame —
// the level-triggered keys (the OR of the two sources, the V3-8
// bitmask's relay form), the edges (press / release — the wasPressed /
// wasReleased twins), the raw mouse delta (f32 px, ZERO quantization —
// the two machines' shared 0.0025 rad/px sensitivity makes the raw px
// the exact sim input, R4), the absolute mouse position (u32 window px
// — the R8's panel-click cell lands, the window-parity handshake's
// shared premise), the wheel, the pointer-lock flag (R18's local fact
// in the sim), and the text length (12b: always 0 — the creative
// search is disabled in net, D11; the field is the 12c chat's
// reserved carrier).
struct FrameState {
    std::uint64_t keysDown = 0; // 1 bit per GameKey (kKeyCount <= 64)
    std::uint64_t keysPressed = 0; // the frame's key-down edges
    std::uint8_t buttonsDown = 0; // 1 bit per MouseButton (3)
    std::uint8_t buttonsPressed = 0;
    std::uint8_t buttonsReleased = 0;
    float mouseDx = 0.0f; // the frame's accumulated relative motion
    float mouseDy = 0.0f;
    std::uint32_t mouseAx = 0; // the absolute window px (R8)
    std::uint32_t mouseAy = 0;
    std::int8_t wheel = 0;
    std::uint8_t textLen = 0; // 12b: 0 (the reserved carrier)
    std::uint8_t locked = 0; // R18: the pointer-lock sim flag
    // Padding to the fixed 40-byte wire size (the layout's tail — the
    // packed 38 field bytes + 2 reserved = 40, the design's "~40B").
    std::uint8_t reserved[2] = {0, 0};
    bool operator==(const FrameState&) const = default;
};

// The HELLO's payload (client -> host). The four fingerprints
// (design §2.3 / R5): the build version (the TM_VERSION string), the
// sim version (kDemoSimVersion — the replay-semantics lock), the
// asset FNV (assets/data/** — the data-driven content IS the sim
// semantics, the two machines' assets must agree), and the sim-config
// hash (the sim-relevant config keys — render.vsync stays OUT: it
// paces each machine's wall clock only, the host's broadcast dt
// already normalizes the sim).
struct NetHello {
    std::string version; // the TM_VERSION build string
    int sim = 0; // kDemoSimVersion (2 in 12b)
    std::uint64_t assetFNV = 0; // assets/data/**'s FNV-1a 64
    std::uint64_t simConfigHash = 0; // the sim-relevant config keys
    std::uint16_t windowW = 0; // R8: the window-parity handshake
    std::uint16_t windowH = 0;
    float uiScale = 1.0f;
    std::string name; // <= 12 Latin (the 12c nameplate's carrier)
    bool operator==(const NetHello&) const = default;
};

// The WELCOME's payload (host -> client): the world spec (fresh = the
// seed alone, zero bytes — the two machines' worldgen is the same pure
// function of the seed, the design's "world transfer = zero"; save =
// the host's save-document payload, the client applies it over its
// generated world, the applyWorldEdits' load path) + the session
// constants (the player cap, the client's slot, D, the hash interval).
struct NetWelcome {
    bool fresh = true; // worldType: 0 = fresh, 1 = save
    std::int64_t seed = 0;
    std::string journalPayload; // the save-document payload (fresh: "")
    std::uint8_t players = kNetMaxPlayers;
    std::uint8_t slot = 1; // the client's player index
    std::uint8_t delay = kNetInputDelay;
    std::uint8_t hashInterval = kNetHashInterval;
    // V3-12c2: the save-form WELCOME's START FRAME — the host's
    // m_frameCount at the handshake (the client's m_frameCount
    // adoption: the frame-absolute pacing aligns over the document's
    // apply; 0 on the fresh form + the initial window's save form —
    // the session's frame zero).
    std::uint32_t startFrame = 0;
    bool operator==(const NetWelcome&) const = default;
};

// The CTRL's payload (host -> client, applied at the frame boundary):
// type 0 = leave / kick (the slot's player is removed — the 100% drop
// + the dismount, R19), type 1 = disconnect (the slot's transport
// died; the host is the session's owner — a client seeing the host's
// EOF exits, D2). V3-12c2: type 2 = renumber (the slot's removal's
// BROADCAST — the host's compact-erase's face: every slot > removed
// shifts down one; the survivors' bodies / rings / watermarks shift
// WITH their rows (the input history's owner follows the player),
// the local slot's adjustment is the receiver's; the rejoin's
// mid-session removal's desync root's gate-catch fix).
struct NetCtrl {
    enum class Kind : std::uint8_t {
        Leave = 0,
        Disconnect = 1,
        Renumber = 2,
        // V3-12c2: join (the rejoin's mid-session WELCOME's BROADCAST
        // to the EXISTING remotes — the N's growth: their rings /
        // inputs / names push a row, their netAddPlayer(slot) builds
        // the new body (the host's twin — the construction's
        // determinism, no transfer), the slot is ALWAYS the tail
        // (the compact vector's append)).
        Join = 3,
    };
    Kind kind = Kind::Disconnect;
    std::uint8_t slot = 0;
    // The Join's face only: the joining player's name (the nametag's
    // presentation — the latin's 16-cap, the HELLO name's twin).
    std::string name;
    bool operator==(const NetCtrl&) const = default;
};

// -- The codec (pure byte<->struct) -----------------------------------
// Pack / unpack into explicit byte vectors (the little-endian wire
// order, the fixed offsets — the unit tests pin the layout's bytes).
std::vector<std::uint8_t> packHello(const NetHello& h);
bool unpackHello(const std::uint8_t* p, std::size_t n, NetHello& out);
std::vector<std::uint8_t> packWelcome(const NetWelcome& w);
bool unpackWelcome(const std::uint8_t* p, std::size_t n, NetWelcome& out);
std::vector<std::uint8_t> packInput(std::uint32_t frame, const FrameState& fs,
                                    std::uint64_t hash);
// unpackInput's out-params (frame / hash): the payload is
// {u32 frame, FrameState(40), u64 hash}.
bool unpackInput(const std::uint8_t* p, std::size_t n, std::uint32_t& frame,
                 FrameState& out, std::uint64_t& hash);
std::vector<std::uint8_t> packFrame(std::uint32_t frame, double dt,
                                    const FrameState* states,
                                    std::uint8_t playerCount,
                                    std::uint64_t hash);
// out-dt takes the raw f64 bits (the strictness: the two machines'
// bit-identical double — the int64 us round-trip's inexactness is why
// the raw bits).
bool unpackFrame(const std::uint8_t* p, std::size_t n, std::uint32_t& frame,
                 double& dt, std::vector<FrameState>& states,
                 std::uint64_t& hash);
std::vector<std::uint8_t> packCtrl(const NetCtrl& c);
bool unpackCtrl(const std::uint8_t* p, std::size_t n, NetCtrl& out);

// The CHAT's payload (V3-12c1, the D4's carrier): {slot u8, u16 len,
// text bytes} — the text is the T-key's typed Latin (the 12b's
// textLen-0 FrameState reservation's distinct packet shape: chat is
// PRESENTATION, never the sim — the hash excludes it by construction).
// The 256-char cap (the MC 256's read, the kFrameTextMax's double).
std::vector<std::uint8_t> packChat(std::uint8_t slot, const std::string& text);
bool unpackChat(const std::uint8_t* p, std::size_t n, std::uint8_t& slot,
                std::string& text);

// The REJECT's payload (host -> client, the HELLO's named refusal): a
// short reason string (the S6's eight faces each name their own:
// magic / version / sim / assetFNV / configHash / window / port /
// the save-mutex — the version-lie's naming precedent).
std::vector<std::uint8_t> packReject(const std::string& reason);
bool unpackReject(const std::uint8_t* p, std::size_t n, std::string& reason);

// V3-12c1: the chat's Latin-only sanitizer (the c1's font face: the
// chat's render is the Latin subset, the zero-glyph discipline — a
// non-Latin byte drops, the empty result = the send's no-op). The
// inline (the game_ui's chat input + the game_net's relay share it).
inline std::string latinOnly(const std::string& in) {
    std::string out;
    for (unsigned char c : in) {
        if (c >= 0x20 && c <= 0x7E) {
            out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

// -- The FNV-1a 64 (the project's hash discipline — the
// tests/test_font.cpp's twin; the incremental feed form: the state
// carries between fields / files) ------------------------------------
struct Fnv64 {
    std::uint64_t state = 0xcbf29ce484222325ULL; // the offset basis
    void feed(const void* data, std::size_t n) {
        const auto* b = static_cast<const std::uint8_t*>(data);
        for (std::size_t i = 0; i < n; ++i) {
            state ^= b[i];
            state *= 0x100000001b3ULL; // the FNV prime
        }
    }
    template <typename T>
    void feedScalar(T v) {
        feed(&v, sizeof(v));
    }
    std::uint64_t digest() const {
        return state;
    }
};

// The assets/data/** tree's FNV (the R5's asset fingerprint): the
// files' (name, bytes) in the sorted directory walk — the pure read of
// the asset root (the "assets" path, the repo-cwd discipline). Returns
// the empty string's error on a missing root.
std::string assetFNV(const std::string& assetRoot, std::uint64_t& out);

// The sim-relevant config's hash (the R5's config parity): the fixed
// key list (the world / the player's sim params / the mobs' — the
// render.* + the mobs.limb_swing's visual toggle stay OUT, the
// dt-broadcast's normalization). The values feed as their parsed
// types (an absent key feeds the absent sentinel — the two machines'
// same-binary defaults make the absent-both hash agree).
std::string simConfigHash(const class Config& config, std::uint64_t& out);

// The state hash (design §2.3's field list, the R15's desync detector):
// the full sim truth over the 64-bit FNV. The field list is the
// single source (the unit test pins every field's contribution —
// the mutation test: each field's change moves the digest). The
// final authority stays the exit savenorm (the hash is the fast
// guard, not the sole judge).
//
// The players' inventory summary: the 36 slots' (item id, count,
// damage) — the enchantments ride on the item's identity for the
// 12b (the enchant map's FNV is the 12c's refinement; the two
// machines' inventories are the sim's output, the hash's job is the
// divergence tripwire, not the byte proof).
std::uint64_t netStateHash(const Game& g);

} // namespace mc
