// V3-12b: the net protocol's unit tests (the pure codec's round-trip,
// the FrameState's fixed layout, the stream reassembly's split
// tolerance, the FNV's determinism, the asset / config fingerprints'
// stability + sensitivity). The socket never enters (the R16's seam
// is feed/drain — the deterministic byte stream's proof). The state
// hash's field list is the gate arm's dual-process evidence (the exit
// savenorm's final authority — the R15's fast-guard shape).
#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

#include "core/config.hpp"
#include "net/net_proto.hpp"
#include "net/net_session.hpp"

using mc::FrameState;
using mc::NetCtrl;
using mc::NetHello;
using mc::NetType;
using mc::NetWelcome;

namespace {
// The full-field FrameState (every bit exercised — a layout's silent
// drop would trip the round-trip).
FrameState fullFrameState() {
    FrameState fs;
    fs.keysDown = 0x0000AAAA555555FFull;
    fs.keysPressed = 0xFFFF000000000000ull;
    fs.buttonsDown = 0b001;
    fs.buttonsPressed = 0b010;
    fs.buttonsReleased = 0b100;
    fs.mouseDx = 12.5f;
    fs.mouseDy = -3.25f;
    fs.mouseAx = 632u;
    fs.mouseAy = 352u;
    fs.wheel = -2;
    fs.textLen = 0; // the 12b's 0 (the reserved carrier)
    fs.locked = 1;
    return fs;
}

std::uint64_t fnvOf(const std::string& s) {
    mc::Fnv64 h;
    h.feed(s.data(), s.size());
    return h.digest();
}
} // namespace

// ---------------------------------------------------------------------------
// The FrameState's fixed layout (the 40-byte pin — the design §2.3's
// "~40B" shape; a field's addition is the 12c's version bump, not the
// 12b's silent growth).
// ---------------------------------------------------------------------------
TEST(NetProto, FrameStateIsFortyBytes) {
    // The PACKED wire size (the struct's sizeof carries the u64's
    // trailing alignment padding — the 40 is the payload contract).
    EXPECT_EQ(mc::kNetFrameStateBytes, 40u);
    const auto packed = mc::packInput(0u, fullFrameState(), 0u);
    // frame(4) + FrameState(40) + hash(8).
    EXPECT_EQ(packed.size(), 4u + 40u + 8u);
}

TEST(NetProto, InputRoundTrip) {
    const FrameState fs = fullFrameState();
    const auto packed = mc::packInput(1234u, fs, 0xDEADBEEFCAFEBABEull);
    // The payload = frame(4) + FrameState(40) + hash(8) = 52.
    EXPECT_EQ(packed.size(), 52u);
    std::uint32_t frame = 0;
    FrameState out {};
    std::uint64_t hash = 0;
    ASSERT_TRUE(mc::unpackInput(packed.data(), packed.size(), frame, out, hash));
    EXPECT_EQ(frame, 1234u);
    EXPECT_EQ(hash, 0xDEADBEEFCAFEBABEull);
    EXPECT_EQ(out, fs);
}

TEST(NetProto, FrameRoundTrip) {
    FrameState states[2] = {fullFrameState(), FrameState {}};
    states[1].keysDown = 0x1;
    states[1].locked = 0;
    const auto packed =
        mc::packFrame(42u, 0.0167, states, 2, 0x1234567890ABCDEFull);
    std::uint32_t frame = 0;
    double dt = 0.0;
    std::vector<FrameState> out;
    std::uint64_t hash = 0;
    ASSERT_TRUE(mc::unpackFrame(packed.data(), packed.size(), frame, dt, out,
                                hash));
    EXPECT_EQ(frame, 42u);
    // The dt's raw f64 bits (the strictness — a µs round-trip would
    // lose the last ulp).
    EXPECT_EQ(dt, 0.0167);
    EXPECT_EQ(out.size(), 2u);
    EXPECT_EQ(out[0], states[0]);
    EXPECT_EQ(out[1], states[1]);
    EXPECT_EQ(hash, 0x1234567890ABCDEFull);
}

TEST(NetProto, HelloWelcomeRejectCtrlRoundTrip) {
    NetHello h;
    h.version = "3.29.0";
    h.sim = 2;
    h.assetFNV = 0x1122334455667788ull;
    h.simConfigHash = 0x99AABBCCDDEEFF00ull;
    h.windowW = 679;
    h.windowH = 744;
    h.uiScale = 2.0f;
    h.name = "client1";
    const auto hp = mc::packHello(h);
    NetHello h2;
    ASSERT_TRUE(mc::unpackHello(hp.data(), hp.size(), h2));
    EXPECT_EQ(h2, h);

    NetWelcome w;
    w.fresh = true;
    w.seed = 42;
    w.journalPayload = "";
    w.players = 2;
    w.slot = 1;
    w.delay = mc::kNetInputDelay;
    w.hashInterval = mc::kNetHashInterval;
    // V3-12c2: the startFrame's zero (the fresh form + the initial
    // window's save form — the session's frame zero).
    w.startFrame = 0;
    const auto wp = mc::packWelcome(w);
    NetWelcome w2;
    ASSERT_TRUE(mc::unpackWelcome(wp.data(), wp.size(), w2));
    EXPECT_EQ(w2, w);

    NetWelcome ws;
    ws.fresh = false;
    ws.seed = -7;
    ws.journalPayload = "the save document's payload";
    // V3-12c2: the rejoin's mid-session startFrame (the host's
    // m_frameCount at the handshake — the client's pacing's
    // adoption).
    ws.startFrame = 12345;
    const auto wsp = mc::packWelcome(ws);
    NetWelcome ws2;
    ASSERT_TRUE(mc::unpackWelcome(wsp.data(), wsp.size(), ws2));
    EXPECT_EQ(ws2, ws);

    const auto rp = mc::packReject("version mismatch (the host 3.29.0)");
    std::string reason;
    ASSERT_TRUE(mc::unpackReject(rp.data(), rp.size(), reason));
    EXPECT_EQ(reason, "version mismatch (the host 3.29.0)");

    NetCtrl c;
    c.kind = NetCtrl::Kind::Disconnect;
    c.slot = 1;
    const auto cp = mc::packCtrl(c);
    NetCtrl c2;
    ASSERT_TRUE(mc::unpackCtrl(cp.data(), cp.size(), c2));
    EXPECT_EQ(c2, c);

    // V3-12c2: the Renumber (the removal's broadcast — the slot's
    // face only, no name).
    NetCtrl rn;
    rn.kind = NetCtrl::Kind::Renumber;
    rn.slot = 2;
    const auto rnp = mc::packCtrl(rn);
    NetCtrl rn2;
    ASSERT_TRUE(mc::unpackCtrl(rnp.data(), rnp.size(), rn2));
    EXPECT_EQ(rn2, rn);

    // V3-12c2: the Join (the N's growth's broadcast — the slot + the
    // joining player's name; the name's 16-cap).
    NetCtrl jn;
    jn.kind = NetCtrl::Kind::Join;
    jn.slot = 2;
    jn.name = "reconnector";
    const auto jnp = mc::packCtrl(jn);
    NetCtrl jn2;
    ASSERT_TRUE(mc::unpackCtrl(jnp.data(), jnp.size(), jn2));
    EXPECT_EQ(jn2, jn);
    NetCtrl jl;
    jl.kind = NetCtrl::Kind::Join;
    jl.slot = 3;
    jl.name = "a-very-long-name-over-sixteen";
    const auto jlp = mc::packCtrl(jl);
    NetCtrl jl2;
    ASSERT_TRUE(mc::unpackCtrl(jlp.data(), jlp.size(), jl2));
    EXPECT_EQ(jl2.name, "a-very-long-name");
    EXPECT_EQ(jl2.slot, 3u);
}

// The decoders' corrupt-stream tolerance (the R16's bound check — a
// truncated payload is a false, not a crash / a hang).
TEST(NetProto, WelcomeHostNameAndMotd) {
    // V3-17d: the WELCOME's host's display name + the lobby's MOTD
    // (the 12c1's deferred concept's carrier — the mixed versions are
    // the HELLO's TM_VERSION's reject, BEFORE the WELCOME's schema).
    NetWelcome w;
    w.fresh = true;
    w.seed = 1234;
    w.players = 2;
    w.slot = 1;
    w.hostName = "hgate";
    w.motd = "TokenMine.cpp";
    const auto wp = mc::packWelcome(w);
    NetWelcome w2;
    ASSERT_TRUE(mc::unpackWelcome(wp.data(), wp.size(), w2));
    EXPECT_EQ(w2, w);

    // The empty's hostName (the CLI's --listen's legacy shape — the
    // client's "host" read) round-trips too.
    NetWelcome wl;
    wl.players = 3;
    wl.slot = 2;
    const auto wlp = mc::packWelcome(wl);
    NetWelcome wl2;
    ASSERT_TRUE(mc::unpackWelcome(wlp.data(), wlp.size(), wl2));
    EXPECT_EQ(wl2, wl);
    EXPECT_TRUE(wl2.hostName.empty());
    EXPECT_TRUE(wl2.motd.empty());

    // The caps' reject's faces (the unpack's strictness — a
    // hand-crafted over-cap's payload is the named fail).
    NetWelcome wbig;
    wbig.players = 2;
    wbig.slot = 1;
    wbig.hostName = std::string(13, 'a'); // > 12 Latin
    const auto wb = mc::packWelcome(wbig);
    NetWelcome wb2;
    EXPECT_FALSE(mc::unpackWelcome(wb.data(), wb.size(), wb2));
    NetWelcome wlong;
    wlong.players = 2;
    wlong.slot = 1;
    wlong.motd = std::string(mc::kNetMotdMax + 1, 'm'); // > 64
    const auto wl3 = mc::packWelcome(wlong);
    NetWelcome wl4;
    EXPECT_FALSE(mc::unpackWelcome(wl3.data(), wl3.size(), wl4));
    // The exact cap's boundary (the belt's inclusivity).
    NetWelcome wmax;
    wmax.players = 2;
    wmax.slot = 1;
    wmax.hostName = std::string(12, 'a');
    wmax.motd = std::string(mc::kNetMotdMax, 'm');
    const auto wm = mc::packWelcome(wmax);
    NetWelcome wm2;
    ASSERT_TRUE(mc::unpackWelcome(wm.data(), wm.size(), wm2));
    EXPECT_EQ(wm2, wmax);
}

TEST(NetProto, DecodersRejectTruncated) {
    const auto full = mc::packInput(1u, fullFrameState(), 0u);
    for (std::size_t n = 0; n < full.size(); ++n) {
        std::uint32_t frame;
        FrameState fs;
        std::uint64_t hash;
        EXPECT_FALSE(mc::unpackInput(full.data(), n, frame, fs, hash)) << n;
    }
    const FrameState fa = fullFrameState();
    const auto fp = mc::packFrame(1u, 0.1, &fa, 1, 0u);
    for (std::size_t n = 0; n < fp.size(); ++n) {
        std::uint32_t frame;
        double dt;
        std::vector<FrameState> st;
        std::uint64_t hash;
        EXPECT_FALSE(mc::unpackFrame(fp.data(), n, frame, dt, st, hash)) << n;
    }
}

// ---------------------------------------------------------------------------
// The stream reassembly (the feed/drain seam — split tolerance, the
// order preservation, the magic's corrupt face).
// ---------------------------------------------------------------------------
TEST(NetSession, ReassemblySplitsAndCoalesces) {
    // Build a raw wire stream: two INPUT packets + one FRAME, in order.
    const FrameState a = fullFrameState();
    const FrameState b {};
    std::vector<std::uint8_t> wire;
    auto append = [&](mc::NetType t, const std::vector<std::uint8_t>& p) {
        const std::uint32_t magic = mc::kNetMagic;
        for (int i = 0; i < 4; ++i) {
            wire.push_back(static_cast<std::uint8_t>((magic >> (8 * i)) & 0xFF));
        }
        wire.push_back(static_cast<std::uint8_t>(t));
        const std::uint32_t len = static_cast<std::uint32_t>(p.size());
        for (int i = 0; i < 4; ++i) {
            wire.push_back(static_cast<std::uint8_t>((len >> (8 * i)) & 0xFF));
        }
        wire.insert(wire.end(), p.begin(), p.end());
    };
    append(NetType::Input, mc::packInput(10u, a, 0u));
    append(NetType::Input, mc::packInput(11u, b, 0u));
    append(NetType::Frame, mc::packFrame(10u, 0.016, &a, 1, 0u));

    // Feed the whole stream in 1-byte splits (the worst-case reassembly).
    mc::NetSession s;
    std::vector<mc::NetPacket> got;
    for (std::size_t i = 0; i < wire.size(); ++i) {
        s.feed(wire.data() + i, 1);
        auto q = s.drainPackets();
        for (auto& p : q) {
            got.push_back(std::move(p));
        }
    }
    ASSERT_EQ(got.size(), 3u);
    EXPECT_EQ(got[0].type, NetType::Input);
    EXPECT_EQ(got[1].type, NetType::Input);
    EXPECT_EQ(got[2].type, NetType::Frame);
    std::uint32_t f;
    FrameState fs;
    std::uint64_t h;
    ASSERT_TRUE(mc::unpackInput(got[0].payload.data(), got[0].payload.size(), f,
                                fs, h));
    EXPECT_EQ(f, 10u);
    EXPECT_EQ(fs, a);
    ASSERT_TRUE(mc::unpackInput(got[1].payload.data(), got[1].payload.size(), f,
                                fs, h));
    EXPECT_EQ(f, 11u);
    EXPECT_EQ(fs, b);

    // And the whole stream in one feed (the coalesced face).
    mc::NetSession s2;
    s2.feed(wire.data(), wire.size());
    auto q2 = s2.drainPackets();
    EXPECT_EQ(q2.size(), 3u);
}

TEST(NetSession, ReassemblyMagicBroke) {
    // A corrupt leading byte (the magic's broken face) = the stream's
    // trust cleared (no packet, no hang — the next feed re-parses from
    // the clean byte, the desync's named exit's wire twin).
    mc::NetSession s;
    const auto p = mc::packInput(5u, fullFrameState(), 0u);
    std::vector<std::uint8_t> wire;
    wire.push_back(0x00); // the magic's first byte corrupted
    const std::uint32_t magic = mc::kNetMagic;
    for (int i = 1; i < 4; ++i) {
        wire.push_back(static_cast<std::uint8_t>((magic >> (8 * i)) & 0xFF));
    }
    wire.push_back(static_cast<std::uint8_t>(NetType::Input));
    const std::uint32_t len = static_cast<std::uint32_t>(p.size());
    for (int i = 0; i < 4; ++i) {
        wire.push_back(static_cast<std::uint8_t>((len >> (8 * i)) & 0xFF));
    }
    wire.insert(wire.end(), p.begin(), p.end());
    s.feed(wire.data(), wire.size());
    EXPECT_TRUE(s.drainPackets().empty()); // the corrupt stream yields no
                                           // packet (the named exit)
}

// ---------------------------------------------------------------------------
// The FNV's determinism + sensitivity (the hash discipline's pin).
// ---------------------------------------------------------------------------
TEST(NetProto, FnvDeterministicAndSensitive) {
    EXPECT_EQ(fnvOf("abc"), fnvOf("abc"));
    EXPECT_NE(fnvOf("abc"), fnvOf("abd"));
    EXPECT_NE(fnvOf("abc"), fnvOf("ab"));
    EXPECT_NE(fnvOf(""), fnvOf(" "));
    // The FNV-1a 64's known vector (the empty string's offset basis —
    // the digest of zero bytes is the basis itself).
    mc::Fnv64 h;
    EXPECT_EQ(h.digest(), 0xcbf29ce484222325ull);
}

// ---------------------------------------------------------------------------
// The asset fingerprint (the R5's asset parity — the deterministic
// walk: the same tree hashes the same, a content change moves it).
// ---------------------------------------------------------------------------
TEST(NetProto, AssetFNVStable) {
    std::uint64_t a1 = 0, a2 = 0;
    EXPECT_TRUE(mc::assetFNV("assets", a1).empty());
    EXPECT_TRUE(mc::assetFNV("assets", a2).empty());
    EXPECT_EQ(a1, a2);
    EXPECT_NE(a1, 0u);
    // A missing root = the named error (not a zero).
    EXPECT_FALSE(mc::assetFNV("no/such/root", a1).empty());
}

// ---------------------------------------------------------------------------
// The sim-config hash (the parity's stability + the value's
// sensitivity — a sim key's change moves it, the render.*'s change
// does NOT (the dt-broadcast's normalization's exemption).
// ---------------------------------------------------------------------------
TEST(NetProto, SimConfigHashParity) {
    const std::filesystem::path tmp =
        std::filesystem::temp_directory_path() / "tm_net_cfg_test.json";
    const std::string json1 = R"({
        "world": { "seed": 42, "view_distance": 8,
                   "day_length_seconds": 600, "start_time_of_day": 0.25 },
        "player": { "walk_speed": 4.3, "max_health": 20 },
        "mobs": { "enabled": true, "difficulty": "normal" },
        "render": { "vsync": true }
    })";
    const std::string json2 = json1; // identical
    std::string json3 = json1; // the sim's walk_speed moved
    json3.replace(json3.find("4.3"), 3, std::string("4.4"));
    std::string json4 = json1; // the EXEMPT render.vsync moved
    json4.replace(json4.find("\"vsync\": true"), 14,
                  std::string("\"vsync\": false"));
    auto write = [&](const std::string& body) {
        std::ofstream o(tmp);
        o << body;
    };
    write(json1);
    mc::Config c1 = mc::Config::load(tmp).value();
    write(json2);
    mc::Config c2 = mc::Config::load(tmp).value();
    std::uint64_t h1 = 0, h2 = 0, h3 = 0, h4 = 0;
    EXPECT_TRUE(mc::simConfigHash(c1, h1).empty());
    EXPECT_TRUE(mc::simConfigHash(c2, h2).empty());
    EXPECT_EQ(h1, h2);
    write(json3);
    mc::Config c3 = mc::Config::load(tmp).value();
    EXPECT_TRUE(mc::simConfigHash(c3, h3).empty());
    EXPECT_NE(h1, h3); // the sim's walk_speed moved the hash
    write(json4);
    mc::Config c4 = mc::Config::load(tmp).value();
    EXPECT_TRUE(mc::simConfigHash(c4, h4).empty());
    EXPECT_EQ(h1, h4); // render.vsync is EXEMPT (the dt-broadcast)
    std::filesystem::remove(tmp);
}
