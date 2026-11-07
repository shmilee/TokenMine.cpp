// V3-12b: the Game's NET hooks (the handshake's six-face rejection, the
// frame's poll/capture/preload/send, the hash exchange, the player
// add / remove). The loop's wiring is game_init.cpp's runGameplayLoop
// (the m_netActive gate — the solo loop is the pre-V3-12b sequence,
// the structural-immunity root).
#include "core/game.hpp"

#include <cstdio>

#include "core/demo.hpp"
#include "net/net_proto.hpp"

namespace mc {

namespace {
// The ring's depth (D+1 slots — the frame N's read is the slot
// (N-D) mod (D+1); the unsigned wrap-safe index).
constexpr std::size_t kRingDepth = kNetInputDelay + 1;
std::size_t ringSlot(std::size_t frame, std::size_t back = kNetInputDelay) {
    return (frame + (kRingDepth - (back % kRingDepth))) % kRingDepth;
}

std::string hex(std::uint64_t v) {
    char buf[19];
    std::snprintf(buf, sizeof(buf), "%016llx",
                  static_cast<unsigned long long>(v));
    return buf;
}
} // namespace

Result<void> Game::netClientHandshake() {
    // The four fingerprints (the R5's version lock — the two machines'
    // build / sim / assets / sim-config must agree, the named
    // refusal's faces).
    std::uint64_t asset = 0;
    std::string err = assetFNV("assets", asset);
    if (!err.empty()) {
        return MC_FAIL(err);
    }
    std::uint64_t cfg = 0;
    err = simConfigHash(m_config, cfg);
    if (!err.empty()) {
        return MC_FAIL(err);
    }
    NetHello hello;
    hello.version = TM_VERSION;
    hello.sim = kDemoSimVersion;
    hello.assetFNV = asset;
    hello.simConfigHash = cfg;
    hello.windowW = static_cast<std::uint16_t>(m_window.requestedWidth());
    hello.windowH = static_cast<std::uint16_t>(m_window.requestedHeight());
    // The uiScale's config read (the client's handshake runs before
    // buildScene's m_uiScale set — the pure read is the same value).
    const auto uiScale = uiScaleFromConfig(m_config);
    if (!uiScale.ok()) {
        return MC_FAIL(uiScale.error().describe());
    }
    hello.uiScale = *uiScale;
    hello.name = "client";
    m_net = std::make_unique<NetSession>();
    NetWelcome welcome;
    err = m_net->startClient(m_args.connectHost, m_args.connectPort, hello,
                             welcome);
    if (!err.empty()) {
        m_net.reset();
        return MC_FAIL(err);
    }
    if (welcome.players < 2 || welcome.slot != 1) {
        m_net.reset();
        return MC_FAIL("net: the WELCOME's player shape is off (the 12b "
                       "fixed pair)");
    }
    m_netActive = true;
    m_netIsHost = false;
    m_localSlot = 1;
    m_netSeedOverride = welcome.seed;
    for (auto& r : m_frameRings) {
        r.assign(kRingDepth, FrameState {});
    }
    MC_LOG_INFO("net: joined the host (slot " +
                std::to_string(welcome.slot) + ", the world seed " +
                std::to_string(welcome.seed) + ")");
    return {};
}

Result<void> Game::netHostHandshake() {
    // The host's fingerprints (the comparison's baseline — the client's
    // HELLO rides the same computation).
    std::uint64_t asset = 0;
    std::string err = assetFNV("assets", asset);
    if (!err.empty()) {
        return MC_FAIL(err);
    }
    std::uint64_t cfg = 0;
    err = simConfigHash(m_config, cfg);
    if (!err.empty()) {
        return MC_FAIL(err);
    }
    m_net = std::make_unique<NetSession>();
    NetHello hello;
    err = m_net->startHost(m_args.listenPort, hello);
    if (!err.empty()) {
        m_net.reset();
        return MC_FAIL(err);
    }
    // The six-face rejection (the S6's naming — the version-lie's
    // precedent, each face its own word).
    const std::int64_t seed = m_config.getInt64("world.seed", 0);
    // The uiScale's parity (the config's strict read — the client's
    // HELLO's value's twin).
    const auto uiScaleParity = uiScaleFromConfig(m_config);
    if (!uiScaleParity.ok()) {
        return MC_FAIL(uiScaleParity.error().describe());
    }
    std::string reject;
    if (hello.version != TM_VERSION) {
        reject = "version mismatch (the host " + std::string(TM_VERSION) +
                 " / the client " + hello.version + ")";
    } else if (hello.sim != kDemoSimVersion) {
        reject = "sim version mismatch (the host " +
                 std::to_string(kDemoSimVersion) + " / the client " +
                 std::to_string(hello.sim) + ")";
    } else if (hello.assetFNV != asset) {
        reject = "asset fingerprint mismatch (the data-driven content is "
                 "the sim's semantics — the two machines' assets/data "
                 "diverge)";
    } else if (hello.simConfigHash != cfg) {
        reject = "sim config parity mismatch (a sim-relevant config key "
                 "diverges — the render.* is exempt, the dt-broadcast's "
                 "normalization)";
    } else if (hello.windowW !=
                   static_cast<std::uint16_t>(m_window.requestedWidth()) ||
               hello.windowH !=
                   static_cast<std::uint16_t>(m_window.requestedHeight()) ||
               (uiScaleParity.ok() && hello.uiScale != uiScaleParity.value())) {
        reject = "window parity mismatch (the R8's panel cell lands — "
                 "the two machines' window / ui.scale must agree)";
    } else if (m_player != nullptr && m_player->isCreative()) {
        reject = "creative multi is disabled in the 12b net session "
                 "(the D11's text-relay + panel-parity complexity is "
                 "the 12c's)";
    }
    if (!reject.empty()) {
        (void)m_net->send(NetType::Reject, packReject(reject));
        m_net->close();
        m_net.reset();
        return MC_FAIL("net rejected the client: " + reject);
    }
    // The 12b's world shape: the FRESH one (the client's zero-transfer
    // worldgen). A host with a LOADED save (no --new) = the save-form
    // WELCOME — the 12c's reconnect snapshot's reserved face, the
    // named reject (the client never joins a loaded world in 12b).
    if (m_args.saveSlot.has_value() && !m_args.newWorld) {
        (void)m_net->send(
            NetType::Reject,
            packReject("the host's save-form WELCOME is the 12c's "
                       "reconnect snapshot (the 12b's net world is "
                       "fresh — the host's --new)"));
        m_net->close();
        m_net.reset();
        return MC_FAIL("net: the save-form WELCOME is the 12c's face "
                       "(the host's --save without --new)");
    }
    NetWelcome welcome;
    welcome.fresh = true; // the 12b's fresh-world shape
    welcome.seed = seed;
    welcome.players = kNetMaxPlayers;
    welcome.slot = 1;
    err = m_net->send(NetType::Welcome, packWelcome(welcome));
    if (!err.empty()) {
        m_net.reset();
        return MC_FAIL(err);
    }
    m_netActive = true;
    m_netIsHost = true;
    m_localSlot = 0;
    for (auto& r : m_frameRings) {
        r.assign(kRingDepth, FrameState {});
    }
    netAddPlayer(1);
    MC_LOG_INFO("net: the client joined (slot 1, the world seed " +
                std::to_string(seed) + ")");
    return {};
}

void Game::netAddPlayer(std::size_t slot) {
    // The player's construction (the init's shape — the same params,
    // the per-index spawn, the per-index xp seed's salt, R10's
    // de-collision).
    auto params = Player::Params::fromConfig(m_config);
    if (!params.ok()) {
        MC_LOG_ERROR("net: the player params' re-derivation failed (the "
                     "config's parity already passed — the belt)");
        return;
    }
    const Vec3 spawn = spawnPositionForIndex(slot);
    auto p = std::make_unique<Player>(m_world, m_blocks, spawn, *params);
    const std::int64_t seed = m_config.getInt64("world.seed", 0);
    p->setXpSeed(xpSeedInitFor(static_cast<std::uint64_t>(seed), slot));
    p->setWorldSeed(static_cast<std::uint64_t>(seed));
    p->pitch = -0.25f; // the init's look-slightly-down's twin
    while (m_players.size() < slot + 1) {
        m_players.push_back(nullptr);
        m_sessions.push_back(
            std::make_unique<PlayerSession>(m_items, m_recipes));
    }
    // The fresh-world gift (the host's buildScene's twin — the
    // assetFNV's parity guarantees the same hotbar.json).
    auto starter = fillStarterInventory(*m_sessions[slot]);
    if (!starter.ok()) {
        MC_LOG_ERROR("net: the starter inventory's fill failed (the "
                     "assetFNV's parity already passed — the belt): " +
                     starter.error().describe());
    }
    m_players[slot] = std::move(p);
    m_player = m_players[0].get(); // the alias re-pin (slot 0's belt)
    MC_LOG_INFO("net: player " + std::to_string(slot) + " added at (" +
                std::to_string(spawn.x) + ", " + std::to_string(spawn.y) +
                ", " + std::to_string(spawn.z) + ")");
}

void Game::netRemovePlayer(std::size_t slot, const std::string& why) {
    // The R19's removal (the frame-boundary act — the caller runs it
    // at the frame's edge, before the next update): the 100% drop
    // (the ItemEntity's rest-position spawn, the merge's identity
    // rule), the dismount, the effect clear, the vector's erase.
    MC_LOG_INFO("net: player " + std::to_string(slot) + " removed (" +
                why + ")");
    Player& p = *m_players[slot];
    PlayerSession& s = *m_sessions[slot];
    dismountRideFor(p, s);
    s.effects.clearAll();
    const Vec3 pos = p.position();
    for (std::size_t k = 0; k < s.inventory.size(); ++k) {
        const ItemStack& st = s.inventory.at(k);
        if (st.empty()) {
            continue;
        }
        const Item& it = m_items.at(st.item);
        currentEntities().push_back(std::make_unique<ItemEntity>(
            currentWorld(), m_blocks, it.name, it.maxStack, st.count, pos,
            st.damage, st.enchants));
        MC_LOG_DEBUG("net drop '" + it.name + "' x" +
                     std::to_string(st.count) + " at (" +
                     std::to_string(pos.x) + ", " + std::to_string(pos.y) +
                     ", " + std::to_string(pos.z) + ") f" +
                     std::to_string(m_frameCount));
    }
    m_players.erase(m_players.begin() + static_cast<std::ptrdiff_t>(slot));
    m_sessions.erase(m_sessions.begin() + static_cast<std::ptrdiff_t>(slot));
    m_player = m_players[0].get(); // the alias re-pin (the primary's belt)
    if (slot == static_cast<std::size_t>(m_localSlot)) {
        // The client's own removal (the host's kick — the D2's
        // exit; the 12b's fixed pair's shape: the client exits, the
        // host's EOF does the host-side removal).
        m_netActive = false;
    }
    m_net->close();
    m_net.reset();
    m_netActive = false;
}

FrameState Game::captureLocalFrameState() const {
    // The live pump's frame state (the frame's input — the capture
    // runs after the pump + the script injection, before the update:
    // the frame's own edges + deltas, the frameBegin's reset's twin).
    FrameState fs;
    for (int k = 0; k < kKeyCount; ++k) {
        const GameKey key = static_cast<GameKey>(k);
        if (m_input.isDown(key)) {
            fs.keysDown |= std::uint64_t {1} << k;
        }
        if (m_input.wasPressed(key)) {
            fs.keysPressed |= std::uint64_t {1} << k;
        }
    }
    for (int b = 0; b < kMouseButtonCount; ++b) {
        const MouseButton btn = static_cast<MouseButton>(b);
        if (m_input.isDown(btn)) {
            fs.buttonsDown |= static_cast<std::uint8_t>(1u << b);
        }
        if (m_input.wasPressed(btn)) {
            fs.buttonsPressed |= static_cast<std::uint8_t>(1u << b);
        }
        if (m_input.wasReleased(btn)) {
            fs.buttonsReleased |= static_cast<std::uint8_t>(1u << b);
        }
    }
    const Vec2 d = m_input.mouseDelta();
    fs.mouseDx = d.x;
    fs.mouseDy = d.y;
    const Vec2 pos = m_input.mousePosition();
    fs.mouseAx = static_cast<std::uint32_t>(pos.x < 0 ? 0 : pos.x);
    fs.mouseAy = static_cast<std::uint32_t>(pos.y < 0 ? 0 : pos.y);
    fs.wheel = static_cast<std::int8_t>(m_input.wheelDelta());
    fs.locked = m_window.relativeMouseMode() ? 1 : 0;
    return fs;
}

void Game::preloadNetInputs() {
    // The unified delay rule (design D3's no-prediction): EVERY player
    // (the local included) is simulated from the ring's D-frames-ago
    // frame state — the local fast-path's absence is the desync's
    // structural wall.
    const std::size_t F = m_frameCount;
    for (std::size_t i = 0; i < m_players.size(); ++i) {
        const FrameState& fs = m_frameRings[i][ringSlot(F)];
        m_netInputs[i].preloadFrame(fs.keysDown, fs.keysPressed,
                                    fs.buttonsDown, fs.buttonsPressed,
                                    fs.buttonsReleased, fs.mouseDx,
                                    fs.mouseDy, fs.mouseAx, fs.mouseAy,
                                    fs.wheel);
        m_sessions[i]->inputLocked = fs.locked != 0;
    }
}

std::string Game::netFramePre(double& dt) {
    const std::size_t F = m_frameCount;
    if (m_netIsHost) {
        std::string err = m_net->poll();
        if (!err.empty()) {
            // The client's EOF (the R19's removal — the session
            // continues solo; the error's return shape: the empty
            // string after the removal = the loop's continue).
            if (m_players.size() > 1) {
                netRemovePlayer(1, err);
                return {};
            }
            return err;
        }
        // The client's INPUT packets (the frame-filing by number — the
        // host consumes them D frames later via the ring's read).
        NetPacket pkt;
        while (m_net->nextPacket(NetType::Input, pkt)) {
            std::uint32_t f = 0;
            FrameState fs {};
            std::uint64_t h = 0;
            if (!unpackInput(pkt.payload.data(), pkt.payload.size(), f, fs,
                             h)) {
                return "the client's INPUT payload is corrupt (the "
                       "stream's desync — the named exit)";
            }
            m_frameRings[1][f % kRingDepth] = fs;
            if (f > m_clientFrameMax) {
                m_clientFrameMax = f;
            }
            if (h != 0 && f % kNetHashInterval == 0 &&
                m_hostHashRing[f % kNetHashInterval] != h) {
                return "hash desync at frame " + std::to_string(f) +
                       " (the host " + hex(m_hostHashRing[f % kNetHashInterval]) +
                       " / the client " + hex(h) + ")";
            }
        }
        // The lockstep barrier (the mutual wait's host half): the
        // host's sim of frame F consumes the client's FrameState[F-D]
        // — a fast headless host racing the client's connect (the
        // 0.3-s start skew) would eat the not-yet-sent input as the
        // cold-zero, the silent-desync root. Block until the client's
        // INPUT[F-D] lands (the client's post(F-D) — its pre(F-D)
        // needed the host's FRAME[F-D], already sent: no deadlock,
        // the causal chain's F-D < F). F < D = the cold ring's zero
        // (both machines' cold start, the no-barrier frames).
        if (F >= kNetInputDelay) {
            for (;;) {
                if (static_cast<std::uint32_t>(F - kNetInputDelay) <=
                    m_clientFrameMax) {
                    break;
                }
                err = m_net->poll();
                if (!err.empty()) {
                    if (m_players.size() > 1) {
                        netRemovePlayer(1, err);
                        return {};
                    }
                    return err;
                }
                NetPacket ipkt;
                bool got = false;
                while (m_net->nextPacket(NetType::Input, ipkt)) {
                    got = true;
                    std::uint32_t f = 0;
                    FrameState fs {};
                    std::uint64_t h = 0;
                    if (!unpackInput(ipkt.payload.data(), ipkt.payload.size(),
                                     f, fs, h)) {
                        return "the client's INPUT payload is corrupt (the "
                               "stream's desync — the named exit)";
                    }
                    m_frameRings[1][f % kRingDepth] = fs;
                    if (f > m_clientFrameMax) {
                        m_clientFrameMax = f;
                    }
                    if (h != 0 && f % kNetHashInterval == 0 &&
                        m_hostHashRing[f % kNetHashInterval] != h) {
                        return "hash desync at frame " + std::to_string(f) +
                               " (the host " +
                               hex(m_hostHashRing[f % kNetHashInterval]) +
                               " / the client " + hex(h) + ")";
                    }
                }
                if (!got) {
                    // No packet yet: the blocking wait (the timeout's
                    // named error — the peer's stall).
                    err = m_net->waitPacket(ipkt, 10);
                    if (!err.empty()) {
                        if (m_players.size() > 1) {
                            netRemovePlayer(1, "the lockstep's timeout (" +
                                               err + ")");
                            return {};
                        }
                        return err;
                    }
                    if (ipkt.type != NetType::Input) {
                        continue; // a stray type (the 12b's fixed pair's
                                   // belt — re-loop)
                    }
                    std::uint32_t f = 0;
                    FrameState fs {};
                    std::uint64_t h = 0;
                    if (!unpackInput(ipkt.payload.data(), ipkt.payload.size(),
                                     f, fs, h)) {
                        return "the client's INPUT payload is corrupt (the "
                               "stream's desync — the named exit)";
                    }
                    m_frameRings[1][f % kRingDepth] = fs;
                    if (f > m_clientFrameMax) {
                        m_clientFrameMax = f;
                    }
                    if (h != 0 && f % kNetHashInterval == 0 &&
                        m_hostHashRing[f % kNetHashInterval] != h) {
                        return "hash desync at frame " + std::to_string(f) +
                               " (the host " +
                               hex(m_hostHashRing[f % kNetHashInterval]) +
                               " / the client " + hex(h) + ")";
                    }
                }
            }
        }
        // The host's own capture (the ring's fill — the dt stays the
        // host's wall value: the broadcast's source).
        m_frameRings[0][F % kRingDepth] = captureLocalFrameState();
    } else {
        // The client's wait (the design's "the client blocks the sim on
        // the packet" — the render hold's demo precedent; the loopback
        // gate's instant arrival, the LAN's RTT/2 headroom under D).
        NetPacket pkt;
        std::string err = m_net->waitPacket(pkt, 5);
        if (!err.empty()) {
            return err; // the D2's all-exit (the host is the session)
        }
        std::uint32_t f = 0;
        double pdt = 0.0;
        std::vector<FrameState> states;
        std::uint64_t h = 0;
        if (!unpackFrame(pkt.payload.data(), pkt.payload.size(), f, pdt,
                         states, h)) {
            return "the host's FRAME payload is corrupt (the stream's "
                   "desync — the named exit)";
        }
        if (f != F) {
            return "frame out of order (the host " + std::to_string(f) +
                   " / the local " + std::to_string(F) + ")";
        }
        if (states.size() < m_players.size()) {
            return "the FRAME's player count is short (the 12b's fixed "
                   "pair's shape)";
        }
        // The host's capture is for frame f-D (the packet's frame law);
        // the sim of frame f reads it from the ringSlot(f) slot (the
        // host-side fill's twin — the f%3 store was the 1-frame-late
        // read, the f150 desync's gate-catch root).
        m_frameRings[0][ringSlot(f)] = states[0]; // the host's state
        dt = pdt; // the broadcast's dt (the raw f64 bits — the sim's
                  // ONLY time source, the design's R2 resolution)
        if (h != 0) {
            m_pendingHostHash = h;
            m_pendingHostHashFrame = f;
        }
        // The client's own capture (the INPUT's frame-N payload).
        m_frameRings[1][F % kRingDepth] = captureLocalFrameState();
    }
    preloadNetInputs();
    return {};
}

std::string Game::netFramePost(double dt, std::size_t F) {
    if (m_netIsHost) {
        std::uint64_t h = 0;
        if (F % kNetHashInterval == 0) {
            h = netComputeHash();
            m_hostHashRing[F % kNetHashInterval] = h;
        }
        if (F % 300 == 0) {
            MC_LOG_DEBUG("net hash f" + std::to_string(F) + " " +
                         hex(netComputeHash()));
        }
        FrameState states[kNetMaxPlayers] {};
        for (std::size_t i = 0; i < m_players.size(); ++i) {
            states[i] = m_frameRings[i][ringSlot(F)];
        }
        std::string err =
            m_net->send(NetType::Frame, packFrame(static_cast<std::uint32_t>(F),
                                                  dt, states,
                                                  static_cast<std::uint8_t>(
                                                      m_players.size()),
                                                  h));
        if (!err.empty()) {
            // The send's EPIPE / the RST (the client's clean exit — the
            // kernel's reset, the poll's EOF's twin): the R19's
            // removal, the session's solo continuation.
            if (m_players.size() > 1) {
                netRemovePlayer(1, "the host's send failed (" + err + ")");
                return {};
            }
            return err;
        }
        return {};
    }
    // The client's INPUT (the frame-N capture + the hash's carry).
    std::uint64_t h = 0;
    if (F % kNetHashInterval == 0) {
        h = netComputeHash();
        if (m_pendingHostHashFrame == F && m_pendingHostHash != h) {
            return "hash desync at frame " + std::to_string(F) +
                   " (the client " + hex(h) + " / the host " +
                   hex(m_pendingHostHash) + ")";
        }
        m_pendingHostHashFrame = ~0u;
    }
    if (F % 300 == 0) {
        MC_LOG_DEBUG("net hash f" + std::to_string(F) + " " + hex(h));
    }
    const FrameState& fs = m_frameRings[1][F % kRingDepth];
    std::string err =
        m_net->send(NetType::Input, packInput(static_cast<std::uint32_t>(F),
                                              fs, h));
    if (!err.empty()) {
        return err;
    }
    return {};
}

std::uint64_t Game::netComputeHash() {
    return netStateHash(*this);
}

} // namespace mc
