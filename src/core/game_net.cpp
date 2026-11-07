// V3-12b/c: the Game's NET hooks (the handshake's named refusal, the
// frame's poll/capture/preload/send, the hash exchange, the player add
// / remove, the c1 chat relay + the lobby names). The loop's wiring is
// game_init.cpp's runGameplayLoop (the m_netActive gate — the solo
// loop is the pre-V3-12b sequence, the structural-immunity root).
//
// The c1 N-generalization (the 12b fixed pair -> the protocol's
// kNetMaxPlayers): the host's m_nets = one NetSession per remote
// player (slot 1..N-1); the client's m_nets = the single host
// connection. The per-remote barrier watermarks + the rings generalize
// by slot; the FRAME's states array carries the CURRENT player count
// (the u8's wire read).
#include "core/game.hpp"

#include <cstdio>

#include "core/demo.hpp"
#include "core/game_local.hpp"
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

// The stall timeout (the c1's named heartbeat face — R13's detection:
// the host's barrier's wait / the client's frame-wait both run on it;
// a stall = the peer's dead / hung, the named exit / removal).
constexpr double kNetStallTimeoutSec = 10.0;

// The handshake's HELLO-vs-host compare (the named rejection's shared
// logic — the c1's N-client accept loop's per-client face).
std::string checkHello(const NetHello& hello, std::uint64_t hostAsset,
                       std::uint64_t hostCfg, int hostW, int hostH,
                       float hostUi, bool hostCreative) {
    if (hello.version != TM_VERSION) {
        return "version mismatch (the host " + std::string(TM_VERSION) +
               " / the client " + hello.version + ")";
    }
    if (hello.sim != kDemoSimVersion) {
        return "sim version mismatch (the host " +
               std::to_string(kDemoSimVersion) + " / the client " +
               std::to_string(hello.sim) + ")";
    }
    if (hello.assetFNV != hostAsset) {
        return "asset fingerprint mismatch (the data-driven content is "
               "the sim's semantics — the two machines' assets/data "
               "diverge)";
    }
    if (hello.simConfigHash != hostCfg) {
        return "sim config parity mismatch (a sim-relevant config key "
               "diverges — the render.* is exempt, the dt-broadcast's "
               "normalization)";
    }
    if (hello.windowW != static_cast<std::uint16_t>(hostW) ||
        hello.windowH != static_cast<std::uint16_t>(hostH) ||
        hello.uiScale != hostUi) {
        return "window parity mismatch (the R8's panel cell lands — "
               "the two machines' window / ui.scale must agree)";
    }
    if (hostCreative) {
        return "creative multi is disabled in the net session (the "
               "D11's text-relay + panel-parity complexity is the "
               "deferred face)";
    }
    return {};
}

} // namespace

// The session-end's send tolerance (the gate's --frames N's final
// frame: the peer's frame-limit break closes the socket before the
// last INPUT / FRAME lands — the broken pipe's named-but-harmless
// face: the simulation COMPLETED on both sides, the exit is clean).
// 0 frames = run-until-closed (the tolerance never arms).
bool Game::finalFrame(std::size_t F) const {
    return m_args.frames > 0 && F + 1 == static_cast<std::size_t>(m_args.frames);
}

Result<void> Game::netClientHandshake() {
    // The four fingerprints (the R5's version lock — the machines'
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
    m_nets.push_back(std::make_unique<NetSession>());
    NetWelcome welcome;
    err = m_nets.back()->startClient(m_args.connectHost, m_args.connectPort,
                                     hello, welcome);
    if (!err.empty()) {
        m_nets.clear();
        return MC_FAIL(err);
    }
    if (welcome.players < 2 || welcome.slot == 0 ||
        welcome.slot >= welcome.players) {
        m_nets.clear();
        return MC_FAIL("net: the WELCOME's player shape is off (the "
                       "slot must be 1..players-1)");
    }
    m_netActive = true;
    m_netIsHost = false;
    m_localSlot = static_cast<int>(welcome.slot);
    m_netSeedOverride = welcome.seed;
    // The player set = the WELCOME's count (all slots 0..players-1; the
    // local one is the user's, the rest are the host's + the other
    // clients' — the lockstep simulates EVERY player locally).
    const std::size_t n = welcome.players;
    m_frameRings.assign(n, std::vector<FrameState>(kRingDepth, FrameState {}));
    m_netInputs.resize(n);
    m_remoteFrameMax.assign(n, 0);
    m_playerNames.assign(n, "player");
    m_playerNames[0] = "host";
    MC_LOG_INFO("net: joined the host (slot " + std::to_string(welcome.slot) +
                " of " + std::to_string(n) + ", the world seed " +
                std::to_string(welcome.seed) + ")");
    return {};
}

Result<void> Game::netHostHandshake() {
    // The host's fingerprints (the comparison's baseline — each
    // client's HELLO rides the same computation).
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
    const auto uiScaleParity = uiScaleFromConfig(m_config);
    if (!uiScaleParity.ok()) {
        return MC_FAIL(uiScaleParity.error().describe());
    }
    const std::int64_t seed = m_config.getInt64("world.seed", 0);
    // The 12b's world shape: the FRESH one (the client's zero-transfer
    // worldgen). A host with a LOADED save (no --new) = the save-form
    // WELCOME — the c2's reconnect snapshot's reserved face, the named
    // reject.
    if (m_args.saveSlot.has_value() && !m_args.newWorld) {
        return MC_FAIL("net: the save-form WELCOME is the 12c's face "
                       "(the host's --save without --new)");
    }
    // The c1's accept window (up to the protocol's cap — the first
    // client's long accept is the gate's shape, the rest ride a short
    // window: a timeout closes the window, the N fixes BEFORE the loop
    // = every client's WELCOME must carry the SAME count). The window
    // close's belt: the accepted clients' WELCOMES carry their own
    // accept-time count — a later joiner sees a BIGGER count than the
    // earlier's, the shape mismatch. The c1's ruling: the accept
    // window is FIXED (kNetAcceptWindowSec) and every WELCOME carries
    // the count the window WILL settle to — implemented as: accept all
    // window-joins first, THEN send every WELCOME (the HELLOs queue,
    // the welcome flush is post-window).
    struct PendingClient {
        std::unique_ptr<NetSession> session;
        NetHello hello;
    };
    std::vector<PendingClient> pending;
    // The window's short accept attempt (250 ms — a quiet attempt
    // RETRIES until the 3-s deadline: a single quiet break closed the
    // window early, the c2 connecting ~1 s after the c1 got refused,
    // the PITS 411's gate-catch) + the deadline (the window's close:
    // the N fixes before any WELCOME flushes).
    constexpr int kNetWindowAttemptMs = 250;
    constexpr double kNetAcceptWindowSec = 3.0;
    double windowStart = -1.0;
    for (;;) {
        if (!pending.empty()) {
            // The window's close check (every pending HELLO settled).
            if (windowStart < 0.0) {
                windowStart = nowSeconds();
            }
            const bool windowFull =
                pending.size() >= kNetMaxPlayers - 1;
            const bool windowClosed =
                nowSeconds() - windowStart >= kNetAcceptWindowSec;
            if (windowFull || windowClosed) {
                break;
            }
            // The window's short accept attempt (the next client's
            // HELLO joins the window).
            auto session = std::make_unique<NetSession>();
            NetHello hello;
            err = session->startHost(m_args.listenPort, hello,
                                     kNetWindowAttemptMs);
            if (!err.empty()) {
                if (nowSeconds() - windowStart < kNetAcceptWindowSec) {
                    continue; // the quiet attempt: retry to the deadline
                }
                break; // the window's close: the N fixes
            }
            pending.push_back({std::move(session), hello});
            continue;
        }
        // The first client (the long blocking accept — the gate's
        // shape: the host waits for a player to join).
        auto session = std::make_unique<NetSession>();
        NetHello hello;
        err = session->startHost(m_args.listenPort, hello,
                                 static_cast<int>(kNetStallTimeoutSec *
                                                  1000.0));
        if (!err.empty()) {
            m_nets.clear();
            return MC_FAIL(err);
        }
        pending.push_back({std::move(session), hello});
    }
    if (pending.empty()) {
        return MC_FAIL("net: no client joined the accept window (the "
                       "host's solo shape is the --listen-less run)");
    }
    // The HELLO checks (the named rejection — a rejected client's
    // connection closes, the window's N settles on the accepted).
    std::vector<PendingClient> accepted;
    for (auto& pc : pending) {
        const std::string reject =
            checkHello(pc.hello, asset, cfg, m_window.requestedWidth(),
                       m_window.requestedHeight(), *uiScaleParity,
                       m_player != nullptr && m_player->isCreative());
        if (!reject.empty()) {
            (void)pc.session->send(NetType::Reject, packReject(reject));
            pc.session->close();
            MC_LOG_INFO("net: a client was rejected (" + reject + ")");
            continue;
        }
        accepted.push_back(std::move(pc));
    }
    if (accepted.empty()) {
        return MC_FAIL("net: every client was rejected (the N's shape "
                       "is empty — the named exit)");
    }
    // The WELCOME flush (every accepted client sees the SAME N — the
    // lockstep's premise's belt).
    const std::size_t n = accepted.size() + 1;
    for (std::size_t i = 0; i < accepted.size(); ++i) {
        NetWelcome welcome;
        welcome.fresh = true;
        welcome.seed = seed;
        welcome.slot = static_cast<std::uint8_t>(i + 1);
        welcome.players = static_cast<std::uint8_t>(n);
        err = accepted[i].session->send(NetType::Welcome, packWelcome(welcome));
        if (!err.empty()) {
            accepted[i].session->close();
            return MC_FAIL(err);
        }
        m_nets.push_back(std::move(accepted[i].session));
        const std::string name =
            accepted[i].hello.name.empty() ? "player" : accepted[i].hello.name;
        m_playerNames.push_back(name);
        netAddPlayer(i + 1);
        MC_LOG_INFO("net: client '" + name + "' joined (slot " +
                    std::to_string(i + 1) + " of " + std::to_string(n) +
                    ", the world seed " + std::to_string(seed) + ")");
    }
    m_netActive = true;
    m_netIsHost = true;
    m_localSlot = 0;
    m_frameRings.assign(n, std::vector<FrameState>(kRingDepth, FrameState {}));
    m_netInputs.resize(n);
    m_remoteFrameMax.assign(n, 0);
    m_playerNames.insert(m_playerNames.begin(), "host");
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
        if (m_frameRings.size() < slot + 1) {
            m_frameRings.push_back(
                std::vector<FrameState>(kRingDepth, FrameState {}));
        }
        if (m_netInputs.size() < slot + 1) {
            m_netInputs.emplace_back();
        }
        if (m_remoteFrameMax.size() < slot + 1) {
            m_remoteFrameMax.push_back(0);
        }
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
    // rule), the dismount, the effect clear, the vector's erase. The
    // c1's N-generalization: the slot's net connection (host side)
    // closes; a client's own removal = the D2's exit.
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
    if (slot < m_frameRings.size()) {
        m_frameRings.erase(m_frameRings.begin() +
                           static_cast<std::ptrdiff_t>(slot));
    }
    if (slot < m_netInputs.size()) {
        m_netInputs.erase(m_netInputs.begin() +
                          static_cast<std::ptrdiff_t>(slot));
    }
    if (slot < m_remoteFrameMax.size()) {
        m_remoteFrameMax.erase(m_remoteFrameMax.begin() +
                               static_cast<std::ptrdiff_t>(slot));
    }
    if (slot < m_playerNames.size()) {
        m_playerNames.erase(m_playerNames.begin() +
                            static_cast<std::ptrdiff_t>(slot));
    }
    m_player = m_players[0].get(); // the alias re-pin (the primary's belt)
    // The host's slot's connection (the m_nets index = slot - 1).
    if (m_netIsHost && slot > 0 && slot - 1 < m_nets.size()) {
        m_nets[slot - 1]->close();
        m_nets.erase(m_nets.begin() + static_cast<std::ptrdiff_t>(slot - 1));
    }
    if (slot == static_cast<std::size_t>(m_localSlot)) {
        m_netActive = false; // the client's own removal (the D2's exit)
        m_nets.clear();
    }
}

void Game::netConsumeRemote(std::size_t r) {
    // The c1's per-remote packet consumption (the poll + the INPUT's
    // file + the CHAT's relay — the barrier's select's fan-in face;
    // the pre-loop's poll rides it too). The removal's index
    // bookkeeping is the caller's (the r-1 shift).
    if (r >= m_nets.size()) {
        return;
    }
    const std::size_t slot = r + 1;
    std::string err = m_nets[r]->poll();
    if (!err.empty()) {
        netRemovePlayer(slot, err);
        return;
    }
    NetPacket pkt;
    while (m_nets[r]->nextPacket(NetType::Input, pkt)) {
        std::uint32_t f = 0;
        FrameState fs {};
        std::uint64_t h = 0;
        if (!unpackInput(pkt.payload.data(), pkt.payload.size(), f, fs, h)) {
            netRemovePlayer(slot, "the INPUT's payload is corrupt (the "
                                  "stream's desync — the named exit)");
            return;
        }
        if (slot < m_frameRings.size()) {
            m_frameRings[slot][f % kRingDepth] = fs;
            if (f >= m_remoteFrameMax[slot]) {
                m_remoteFrameMax[slot] = f;
            }
        }
        if (h != 0 && f % kNetHashInterval == 0 &&
            m_hostHashRing[f % kNetHashInterval] != h) {
            MC_LOG_ERROR("hash desync at frame " + std::to_string(f) +
                         " (the host " +
                         hex(m_hostHashRing[f % kNetHashInterval]) +
                         " / the slot-" + std::to_string(slot) + " " +
                         hex(h) + ")");
            netRemovePlayer(slot, "the hash's desync (the named exit)");
            return;
        }
    }
    // The c1's CHAT relay (client -> host): the rebroadcast to the
    // OTHER clients + the local display (presentation — never the
    // sim, the hash's exclusion by construction).
    NetPacket cchat;
    while (m_nets[r]->nextPacket(NetType::Chat, cchat)) {
        std::uint8_t cslot = 0;
        std::string ctext;
        if (unpackChat(cchat.payload.data(), cchat.payload.size(), cslot,
                       ctext)) {
            displayChat(static_cast<int>(r + 1), ctext);
            for (std::size_t o = 0; o < m_nets.size(); ++o) {
                if (o != r) {
                    (void)m_nets[o]->send(NetType::Chat,
                                          packChat(static_cast<std::uint8_t>(
                                                      r + 1),
                                                  ctext));
                }
            }
        }
    }
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
        // The c1's per-remote poll + file (the m_nets index = slot-1).
        for (std::size_t r = 0; r < m_nets.size(); ++r) {
            netConsumeRemote(r);
            if (r < m_nets.size() &&
                m_nets[r] == nullptr) { // (belt — the close's null)
                --r;
            }
        }
        // The lockstep barrier (the mutual wait's host half): the
        // host's sim of frame F consumes EVERY remote's
        // FrameState[F-D] — a fast headless host racing a remote's
        // connect (the 0.3-s start skew) would eat the not-yet-sent
        // input as the cold-zero, the silent-desync root. Block until
        // each remote's INPUT[F-D] lands (the remote's post(F-D) — its
        // pre(F-D) needed the host's FRAME[F-D], already sent: no
        // deadlock, the causal chain's F-D < F). F < D = the cold
        // ring's zero (both machines' cold start, the no-barrier
        // frames). The c1's N-generalization: the multi-connection
        // select (any remote's packet advances the watermarks) + the
        // stall timeout (R13's heartbeat — a remote that never lands
        // its INPUT[N-D] is removed, the session continues).
        if (F >= kNetInputDelay && m_players.size() > 1) {
            double stallStart = -1.0;
            std::uint32_t lastMinWm =
                (m_players.size() > 1) ? m_remoteFrameMax[1] : 0u;
            for (std::size_t slot = 2; slot < m_players.size(); ++slot) {
                if (m_remoteFrameMax[slot] < lastMinWm) {
                    lastMinWm = m_remoteFrameMax[slot];
                }
            }
            for (;;) {
                bool allReady = true;
                for (std::size_t slot = 1; slot < m_players.size(); ++slot) {
                    if (m_remoteFrameMax[slot] <
                        static_cast<std::uint32_t>(F - kNetInputDelay)) {
                        allReady = false;
                        break;
                    }
                }
                if (allReady || m_players.size() == 1) {
                    break;
                }
                std::vector<int> fds;
                for (auto& sess : m_nets) {
                    fds.push_back(sess->fd());
                }
                const auto readyFds = NetSession::selectReady(fds, 1000);
                for (int rfd : readyFds) {
                    for (std::size_t r = 0; r < m_nets.size(); ++r) {
                        if (m_nets[r]->fd() == rfd) {
                            netConsumeRemote(r);
                            break;
                        }
                    }
                }
                // The stall's heartbeat (R13): the LAGGING remote's
                // watermark (the min over remotes) makes no progress
                // for the timeout = it is dead — the named removal,
                // the session's continuation. A healthy peer's
                // progress must not mask the lagger's silence (the
                // any-ready check's 2-forever deadlock's gate-catch
                // shape, fixed to the min-watermark's advance).
                std::uint32_t minWm = m_remoteFrameMax[1];
                for (std::size_t slot = 2; slot < m_players.size();
                     ++slot) {
                    if (m_remoteFrameMax[slot] < minWm) {
                        minWm = m_remoteFrameMax[slot];
                    }
                }
                if (minWm > lastMinWm) {
                    lastMinWm = minWm;
                    stallStart = -1.0;
                } else if (stallStart < 0.0) {
                    stallStart = nowSeconds();
                }
                if (stallStart >= 0.0 &&
                    nowSeconds() - stallStart >= kNetStallTimeoutSec) {
                    // Remove the LAGGING remote (the min watermark —
                    // the index order's deterministic pick).
                    std::size_t lagging = 1;
                    for (std::size_t slot = 2; slot < m_players.size();
                         ++slot) {
                        if (slot < m_remoteFrameMax.size() &&
                            m_remoteFrameMax[slot] <
                                m_remoteFrameMax[lagging]) {
                            lagging = slot;
                        }
                    }
                    netRemovePlayer(
                        lagging,
                        "the lockstep's stall timeout (the R13's "
                        "heartbeat — no INPUT for " +
                            std::to_string(static_cast<int>(kNetStallTimeoutSec)) +
                            " s)");
                    stallStart = -1.0;
                    if (m_players.size() > 1) {
                        lastMinWm = m_remoteFrameMax[1];
                        for (std::size_t slot = 2;
                             slot < m_players.size(); ++slot) {
                            if (m_remoteFrameMax[slot] < lastMinWm) {
                                lastMinWm = m_remoteFrameMax[slot];
                            }
                        }
                    }
                    if (m_players.size() == 1) {
                        break;
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
        std::string err = m_nets[0]->waitPacket(pkt,
                                                static_cast<int>(
                                                    kNetStallTimeoutSec));
        if (!err.empty()) {
            return err; // the D2's all-exit (the host is the session)
        }
        if (pkt.type == NetType::Chat) {
            // The host's rebroadcast (the peer's chat relayed): the
            // local display (presentation — the sim's exclusion).
            std::uint8_t cslot = 0;
            std::string ctext;
            if (unpackChat(pkt.payload.data(), pkt.payload.size(), cslot,
                           ctext)) {
                displayChat(static_cast<int>(cslot), ctext);
            }
            return netFramePre(dt); // the CHAT is not the frame's packet
        }
        if (pkt.type != NetType::Frame) {
            return "the host's packet type is off (the FRAME's "
                   "face — the named exit)";
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
            return "the FRAME's player count is short (the N's shape)";
        }
        // The remotes' captures (the packet's frame law: states[i] is
        // the capture for frame f-D; the sim of frame f reads it from
        // the ringSlot(f) slot — the 12b's 1-frame-late gate-catch's
        // fix generalizes by slot).
        for (std::size_t i = 0; i < states.size(); ++i) {
            if (i != static_cast<std::size_t>(m_localSlot)) {
                m_frameRings[i][ringSlot(f)] = states[i];
            }
        }
        dt = pdt; // the broadcast's dt (the raw f64 bits — the sim's
                  // ONLY time source, the design's R2 resolution)
        if (h != 0) {
            m_pendingHostHash = h;
            m_pendingHostHashFrame = f;
        }
        // The client's own capture (the INPUT's frame-N payload).
        m_frameRings[static_cast<std::size_t>(m_localSlot)][F % kRingDepth] =
            captureLocalFrameState();
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
        // The FRAME to EVERY client (the states array = the current
        // player count's ring reads).
        std::vector<FrameState> states(m_players.size());
        for (std::size_t i = 0; i < m_players.size(); ++i) {
            states[i] = m_frameRings[i][ringSlot(F)];
        }
        const auto payload = packFrame(static_cast<std::uint32_t>(F), dt,
                                       states.data(),
                                       static_cast<std::uint8_t>(
                                           m_players.size()),
                                       h);
        for (std::size_t r = 0; r < m_nets.size(); ++r) {
            std::string err = m_nets[r]->send(NetType::Frame, payload);
            if (!err.empty()) {
                if (finalFrame(F)) {
                    return {}; // the peer's clean exit won the race
                }
                // The send's EPIPE / the RST (the client's clean exit —
                // the kernel's reset, the poll's EOF's twin): the R19's
                // removal, the session's continuation.
                netRemovePlayer(r + 1, "the host's send failed (" + err +
                                            ")");
                return {};
            }
        }
        return {};
    }
    // The client's INPUT (the frame-N capture + the hash's carry) to
    // the host's single connection.
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
    const FrameState& fs =
        m_frameRings[static_cast<std::size_t>(m_localSlot)][F % kRingDepth];
    std::string err =
        m_nets[0]->send(NetType::Input,
                        packInput(static_cast<std::uint32_t>(F), fs, h));
    if (!err.empty()) {
        if (finalFrame(F)) {
            return {}; // the peer's clean exit won the race
        }
        return err;
    }
    return {};
}

void Game::netSendChat(const std::string& text) {
    // The client's chat send (the T-key's Enter — the c1's presentation
    // face: the host relays, never the sim's input, the FrameState's
    // textLen-0 reservation's distinct-packet ruling).
    if (!m_netActive || m_netIsHost) {
        return;
    }
    const std::string clean = latinOnly(text);
    if (clean.empty()) {
        return;
    }
    (void)m_nets[0]->send(
        NetType::Chat,
        packChat(static_cast<std::uint8_t>(m_localSlot), clean));
}

void Game::displayChat(int slot, const std::string& text) {
    // The local display (the INFO log + the render's chat overlay —
    // the presentation layer, the hash's exclusion by construction).
    const std::string who =
        (slot >= 0 && slot < static_cast<int>(m_playerNames.size()))
            ? m_playerNames[static_cast<std::size_t>(slot)]
            : std::to_string(slot);
    MC_LOG_INFO("net chat[" + who + "]: " + text);
    m_chatLines.push_back(ChatLine {static_cast<int>(m_frameCount), who,
                                    text});
    while (m_chatLines.size() > 8) {
        m_chatLines.erase(m_chatLines.begin());
    }
}

std::uint64_t Game::netComputeHash() {
    return netStateHash(*this);
}

} // namespace mc
