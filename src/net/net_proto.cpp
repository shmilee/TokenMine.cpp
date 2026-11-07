// V3-12b: the net protocol's pure implementation (the codec's byte
// order, the FNV-1a 64, the asset / config fingerprints, the state
// hash's field list). No fd, no thread — the unit tests exercise
// every decoder on synthetic byte streams (the R16's reassembly is
// net_session's, this file's decoders assume a whole payload).
#include "net/net_proto.hpp"

#include <algorithm>
#include <cstring>
#include <filesystem>

#include "core/game.hpp"
#include "core/json.hpp"
#include "entity/breath_cloud.hpp"
#include "entity/entity.hpp"
#include "entity/exp_orb.hpp"
#include "entity/item_entity.hpp"
#include "entity/mob_entity.hpp"
#include "entity/projectile.hpp"
#include "world/chunk_streaming.hpp"

namespace mc {

namespace {
// The little-endian fixed-width writers (the wire order — the host /
// client are the same-arch LAN, but the ORDER is the contract).
void putU8(std::vector<std::uint8_t>& v, std::uint8_t x) {
    v.push_back(x);
}
void putU16(std::vector<std::uint8_t>& v, std::uint16_t x) {
    v.push_back(static_cast<std::uint8_t>(x & 0xFF));
    v.push_back(static_cast<std::uint8_t>((x >> 8) & 0xFF));
}
void putU32(std::vector<std::uint8_t>& v, std::uint32_t x) {
    for (int i = 0; i < 4; ++i) {
        v.push_back(static_cast<std::uint8_t>((x >> (8 * i)) & 0xFF));
    }
}
void putI32(std::vector<std::uint8_t>& v, std::int32_t x) {
    putU32(v, static_cast<std::uint32_t>(x));
}
void putU64(std::vector<std::uint8_t>& v, std::uint64_t x) {
    for (int i = 0; i < 8; ++i) {
        v.push_back(static_cast<std::uint8_t>((x >> (8 * i)) & 0xFF));
    }
}
void putI64(std::vector<std::uint8_t>& v, std::int64_t x) {
    putU64(v, static_cast<std::uint64_t>(x));
}
void putF32(std::vector<std::uint8_t>& v, float x) {
    std::uint32_t bits;
    std::memcpy(&bits, &x, sizeof(bits));
    putU32(v, bits);
}
void putF64(std::vector<std::uint8_t>& v, double x) {
    std::uint64_t bits;
    std::memcpy(&bits, &x, sizeof(bits));
    putU64(v, bits);
}

struct Reader {
    const std::uint8_t* p;
    std::size_t n;
    std::size_t off = 0;
    bool ok = true;
    std::uint8_t u8() {
        if (off + 1 > n) {
            ok = false;
            return 0;
        }
        return p[off++];
    }
    std::uint16_t u16() {
        if (off + 2 > n) {
            ok = false;
            return 0;
        }
        const std::uint16_t v =
            static_cast<std::uint16_t>(p[off] | (p[off + 1] << 8));
        off += 2;
        return v;
    }
    std::uint32_t u32() {
        if (off + 4 > n) {
            ok = false;
            return 0;
        }
        std::uint32_t v = 0;
        for (int i = 0; i < 4; ++i) {
            v |= static_cast<std::uint32_t>(p[off + i]) << (8 * i);
        }
        off += 4;
        return v;
    }
    std::uint64_t u64() {
        if (off + 8 > n) {
            ok = false;
            return 0;
        }
        std::uint64_t v = 0;
        for (int i = 0; i < 8; ++i) {
            v |= static_cast<std::uint64_t>(p[off + i]) << (8 * i);
        }
        off += 8;
        return v;
    }
    std::int32_t i32() {
        return static_cast<std::int32_t>(u32());
    }
    std::int64_t i64() {
        return static_cast<std::int64_t>(u64());
    }
    std::int8_t i8() {
        return static_cast<std::int8_t>(u8());
    }
    float f32() {
        std::uint32_t bits = u32();
        float v;
        std::memcpy(&v, &bits, sizeof(v));
        return v;
    }
    double f64() {
        std::uint64_t bits = u64();
        double v;
        std::memcpy(&v, &bits, sizeof(v));
        return v;
    }
    std::string str16() { // {u16 len, bytes}
        const std::uint16_t len = u16();
        if (off + len > n) {
            ok = false;
            return {};
        }
        std::string s(reinterpret_cast<const char*>(p + off), len);
        off += len;
        return s;
    }
    std::string str8() { // {u8 len, bytes}
        const std::uint8_t len = u8();
        if (off + len > n) {
            ok = false;
            return {};
        }
        std::string s(reinterpret_cast<const char*>(p + off), len);
        off += len;
        return s;
    }
    void skip(std::size_t k) {
        if (off + k > n) {
            ok = false;
            return;
        }
        off += k;
    }
};

// The FrameState's fixed 40-byte layout (the design §2.3's field
// order — the unit test pins the offsets).
void putFrameState(std::vector<std::uint8_t>& v, const FrameState& fs) {
    putU64(v, fs.keysDown);
    putU64(v, fs.keysPressed);
    putU8(v, fs.buttonsDown);
    putU8(v, fs.buttonsPressed);
    putU8(v, fs.buttonsReleased);
    putF32(v, fs.mouseDx);
    putF32(v, fs.mouseDy);
    putU32(v, fs.mouseAx);
    putU32(v, fs.mouseAy);
    putU8(v, static_cast<std::uint8_t>(fs.wheel));
    putU8(v, fs.textLen);
    putU8(v, fs.locked);
    for (std::uint8_t r : fs.reserved) {
        putU8(v, r);
    }
}
bool getFrameState(Reader& r, FrameState& out) {
    out.keysDown = r.u64();
    out.keysPressed = r.u64();
    out.buttonsDown = r.u8();
    out.buttonsPressed = r.u8();
    out.buttonsReleased = r.u8();
    out.mouseDx = r.f32();
    out.mouseDy = r.f32();
    out.mouseAx = r.u32();
    out.mouseAy = r.u32();
    out.wheel = r.i8();
    out.textLen = r.u8();
    out.locked = r.u8();
    for (auto& x : out.reserved) {
        x = r.u8();
    }
    return r.ok;
}

// -- The state-hash's field feeders (the single source — the R15's
// mutation test pokes each one). Every feed is the raw bits (the
// float's f32/f64, the int's native width) — the two machines'
// identical values hash identically, a 1-ulp difference trips.

void feedVec3(Fnv64& h, const Vec3& v) {
    // The RAW f32 bits (a value-cast would eat the fraction — two
    // sub-block positions hashing equal = the R15's silent desync).
    h.feedScalar(__builtin_bit_cast(std::uint32_t, v.x));
    h.feedScalar(__builtin_bit_cast(std::uint32_t, v.y));
    h.feedScalar(__builtin_bit_cast(std::uint32_t, v.z));
}
void feedF64(Fnv64& h, double v) {
    h.feedScalar(__builtin_bit_cast(std::uint64_t, v));
}
void feedF32(Fnv64& h, float v) {
    h.feedScalar(__builtin_bit_cast(std::uint32_t, v));
}

void feedEntities(Fnv64& h, const std::vector<std::unique_ptr<Entity>>& ents) {
    h.feedScalar(static_cast<std::uint64_t>(ents.size()));
    for (const auto& e : ents) { // insertion order (the design's read)
        const Entity* raw = e.get();
        feedVec3(h, raw->position());
        if (const auto* item = dynamic_cast<const ItemEntity*>(raw)) {
            h.feedScalar(std::uint64_t {1}); // the kind tag
            h.feed(item->itemName().data(), item->itemName().size());
            h.feedScalar(static_cast<std::uint64_t>(item->count()));
            feedF32(h, item->damage());
            feedF64(h, item->age());
            feedF64(h, item->pickupDelayRemaining());
        } else if (const auto* orb =
                       dynamic_cast<const ExpOrbEntity*>(raw)) {
            h.feedScalar(std::uint64_t {2});
            h.feedScalar(static_cast<std::uint64_t>(orb->value()));
            h.feedScalar(static_cast<std::uint64_t>(orb->age()));
        } else if (const auto* proj =
                       dynamic_cast<const Projectile*>(raw)) {
            h.feedScalar(std::uint64_t {3});
            feedVec3(h, proj->velocity());
            h.feedScalar(static_cast<std::uint64_t>(proj->stuck()));
            h.feed(proj->ammoItem().data(), proj->ammoItem().size());
            feedF32(h, proj->damage());
            h.feedScalar(static_cast<std::uint64_t>(
                proj->effectId() < 0 ? 0
                : static_cast<std::uint64_t>(proj->effectId())));
            h.feedScalar(static_cast<std::uint64_t>(proj->teleports()));
            h.feedScalar(static_cast<std::uint64_t>(proj->explodes()));
        } else if (const auto* cloud =
                       dynamic_cast<const BreathCloudEntity*>(raw)) {
            h.feedScalar(std::uint64_t {4});
            (void)cloud; // the position above + the frame-locked age
        } else if (const auto* mob =
                       dynamic_cast<const MobEntity*>(raw)) {
            h.feedScalar(std::uint64_t {5});
            h.feedScalar(static_cast<std::uint64_t>(mob->typeId().value));
            h.feedScalar(static_cast<std::uint32_t>(
                __builtin_bit_cast(std::uint32_t, mob->health())));
            h.feedScalar(static_cast<std::uint64_t>(mob->dead()));
            h.feedScalar(static_cast<std::uint64_t>(
                static_cast<std::uint64_t>(mob->state())));
            h.feedScalar(mob->rngState()); // the per-mob stream state
        } else {
            h.feedScalar(std::uint64_t {0}); // unknown kind (defensive)
        }
    }
}

void feedJournal(Fnv64& h, const SaveJournal& j) {
    h.feedScalar(static_cast<std::uint64_t>(j.entries));
    h.feedScalar(static_cast<std::uint64_t>(j.valid));
    // The map's iteration order is the key-sorted deterministic one —
    // the two machines' journals are the sim's output, the hash's job
    // is the divergence tripwire over the cell SETS.
    for (const auto& [col, cells] : j.blocks) {
        h.feedScalar(static_cast<std::uint64_t>(cells.size()));
        for (const BlockPos& c : cells) {
            h.feedScalar(static_cast<std::uint64_t>(c.x));
            h.feedScalar(static_cast<std::uint64_t>(c.y));
            h.feedScalar(static_cast<std::uint64_t>(c.z));
        }
    }
    for (const auto& [col, cells] : j.fluids) {
        h.feedScalar(static_cast<std::uint64_t>(cells.size()));
        for (const BlockPos& c : cells) {
            h.feedScalar(static_cast<std::uint64_t>(c.x));
            h.feedScalar(static_cast<std::uint64_t>(c.y));
            h.feedScalar(static_cast<std::uint64_t>(c.z));
        }
    }
}
} // namespace

std::vector<std::uint8_t> packHello(const NetHello& h) {
    std::vector<std::uint8_t> v;
    putU16(v, static_cast<std::uint16_t>(h.version.size()));
    for (char c : h.version) {
        putU8(v, static_cast<std::uint8_t>(c));
    }
    putI32(v, static_cast<std::int32_t>(h.sim));
    putU64(v, h.assetFNV);
    putU64(v, h.simConfigHash);
    putU16(v, h.windowW);
    putU16(v, h.windowH);
    putF32(v, h.uiScale);
    putU8(v, static_cast<std::uint8_t>(h.name.size()));
    for (char c : h.name) {
        putU8(v, static_cast<std::uint8_t>(c));
    }
    return v;
}

bool unpackHello(const std::uint8_t* p, std::size_t n, NetHello& out) {
    Reader r {p, n};
    out.version = r.str16();
    out.sim = r.i32();
    out.assetFNV = r.u64();
    out.simConfigHash = r.u64();
    out.windowW = r.u16();
    out.windowH = r.u16();
    out.uiScale = r.f32();
    out.name = r.str8();
    return r.ok && out.name.size() <= 12;
}

std::vector<std::uint8_t> packWelcome(const NetWelcome& w) {
    std::vector<std::uint8_t> v;
    putU8(v, w.fresh ? 0 : 1);
    putI64(v, w.seed);
    putU32(v, static_cast<std::uint32_t>(w.journalPayload.size()));
    for (char c : w.journalPayload) {
        putU8(v, static_cast<std::uint8_t>(c));
    }
    putU8(v, w.players);
    putU8(v, w.slot);
    putU8(v, w.delay);
    putU8(v, w.hashInterval);
    return v;
}

bool unpackWelcome(const std::uint8_t* p, std::size_t n, NetWelcome& out) {
    Reader r {p, n};
    out.fresh = r.u8() == 0;
    out.seed = r.i64();
    const std::uint32_t jl = r.u32();
    if (r.off + jl > r.n) {
        return false;
    }
    out.journalPayload.assign(reinterpret_cast<const char*>(p + r.off), jl);
    r.skip(jl);
    out.players = r.u8();
    out.slot = r.u8();
    out.delay = r.u8();
    out.hashInterval = r.u8();
    return r.ok && out.players <= kNetMaxPlayers;
}

std::vector<std::uint8_t> packInput(std::uint32_t frame,
                                    const FrameState& fs, std::uint64_t hash) {
    std::vector<std::uint8_t> v;
    putU32(v, frame);
    putFrameState(v, fs);
    putU64(v, hash);
    return v;
}

bool unpackInput(const std::uint8_t* p, std::size_t n, std::uint32_t& frame,
                 FrameState& out, std::uint64_t& hash) {
    Reader r {p, n};
    frame = r.u32();
    if (!getFrameState(r, out)) {
        return false;
    }
    hash = r.u64();
    return r.ok;
}

std::vector<std::uint8_t> packFrame(std::uint32_t frame, double dt,
                                    const FrameState* states,
                                    std::uint8_t playerCount,
                                    std::uint64_t hash) {
    std::vector<std::uint8_t> v;
    putU32(v, frame);
    putF64(v, dt);
    putU8(v, playerCount);
    for (std::uint8_t i = 0; i < playerCount; ++i) {
        putFrameState(v, states[i]);
    }
    putU64(v, hash);
    return v;
}

bool unpackFrame(const std::uint8_t* p, std::size_t n, std::uint32_t& frame,
                 double& dt, std::vector<FrameState>& states,
                 std::uint64_t& hash) {
    Reader r {p, n};
    frame = r.u32();
    dt = r.f64();
    const std::uint8_t count = r.u8();
    states.clear();
    for (std::uint8_t i = 0; i < count; ++i) {
        FrameState fs {};
        if (!getFrameState(r, fs)) {
            return false;
        }
        states.push_back(fs);
    }
    hash = r.u64();
    return r.ok;
}

std::vector<std::uint8_t> packCtrl(const NetCtrl& c) {
    std::vector<std::uint8_t> v;
    putU8(v, static_cast<std::uint8_t>(c.kind));
    putU8(v, c.slot);
    return v;
}

bool unpackCtrl(const std::uint8_t* p, std::size_t n, NetCtrl& out) {
    Reader r {p, n};
    out.kind = static_cast<NetCtrl::Kind>(r.u8());
    out.slot = r.u8();
    return r.ok;
}

std::vector<std::uint8_t> packReject(const std::string& reason) {
    std::vector<std::uint8_t> v;
    putU16(v, static_cast<std::uint16_t>(reason.size()));
    for (char c : reason) {
        putU8(v, static_cast<std::uint8_t>(c));
    }
    return v;
}

bool unpackReject(const std::uint8_t* p, std::size_t n, std::string& reason) {
    Reader r {p, n};
    reason = r.str16();
    return r.ok;
}

std::string assetFNV(const std::string& assetRoot, std::uint64_t& out) {
    std::error_code ec;
    const std::filesystem::path root = assetRoot;
    if (!std::filesystem::is_directory(root, ec)) {
        return "missing asset root '" + assetRoot + "'";
    }
    // The sorted walk (directory order is filesystem-dependent — the
    // two machines must hash the SAME order: the lexicographic one).
    std::vector<std::filesystem::path> files;
    std::filesystem::recursive_directory_iterator it(root, ec), end;
    while (it != end) {
        if (it->is_regular_file(ec)) {
            files.push_back(it->path());
        }
        ++it;
    }
    std::sort(files.begin(), files.end());
    Fnv64 h;
    h.feedScalar(static_cast<std::uint64_t>(files.size()));
    for (const auto& f : files) {
        const std::string rel =
            std::filesystem::relative(f, root, ec).generic_string();
        h.feed(rel.data(), rel.size());
        std::error_code fec;
        const auto sz =
            std::filesystem::file_size(f, fec);
        h.feedScalar(static_cast<std::uint64_t>(sz));
        // The content (chunked — the textures are the bulk).
        std::FILE* fp = std::fopen(f.c_str(), "rb");
        if (fp == nullptr) {
            return "unreadable asset file '" + rel + "'";
        }
        std::uint8_t buf[8192];
        for (;;) {
            const std::size_t got = std::fread(buf, 1, sizeof(buf), fp);
            if (got > 0) {
                h.feed(buf, got);
            }
            if (got < sizeof(buf)) {
                break;
            }
        }
        std::fclose(fp);
    }
    out = h.digest();
    return {};
}

std::string simConfigHash(const Config& config, std::uint64_t& out) {
    // The fixed list (the sim's actual reader's keys — the
    // Player::Params::fromConfig's player.* family + the world's +
    // the mobs' sim params). A type mismatch (a string where the
    // sim's reader wants a number) = the named error (the two
    // machines' parity includes the value's shape).
    static const char* const kPlayerKeys[] = {
        "width", "height", "eye_height", "walk_speed", "sprint_speed",
        "jump_speed", "gravity", "terminal_velocity", "swim_speed",
        "swim_up_speed", "water_buoyancy", "water_rise_speed",
        "water_sink_speed", "water_terminal_velocity", "lava_swim_speed",
        "lava_swim_up_speed", "lava_gravity", "lava_terminal_velocity",
        "mouse_sensitivity", "max_health", "fall_damage_offset",
        "crouch_height", "crouch_speed_scale", "regen_delay_seconds",
        "regen_interval_seconds", "max_hunger", "hunger_decay_seconds",
        "starve_damage_interval_seconds", "regen_hunger_threshold",
        "sprint_hunger_decay_factor", "lava_damage_interval_seconds",
        "lava_damage", "fire_duration_seconds",
    };
    Fnv64 h;
    h.feedScalar(static_cast<std::uint64_t>(std::size(kPlayerKeys)));
    auto feedKey = [&h, &config](const char* key, bool isBool,
                                 bool isString) {
        h.feed(key, std::strlen(key));
        const Json* j = config.at(key);
        if (j == nullptr) {
            h.feedScalar(std::uint64_t {0xDEAD000000000000ULL}); // the absent sentinel
            return;
        }
        if (isBool) {
            if (!j->isBool()) {
                h.feedScalar(std::uint64_t {0xBAD0000000000000ULL});
                return;
            }
            h.feedScalar(static_cast<std::uint64_t>(j->asBool()));
        } else if (isString) {
            if (!j->isString()) {
                h.feedScalar(std::uint64_t {0xBAD0000000000000ULL});
                return;
            }
            h.feed(j->asString().data(), j->asString().size());
        } else {
            if (!j->isNumber()) {
                h.feedScalar(std::uint64_t {0xBAD0000000000000ULL});
                return;
            }
            const double v = j->asDouble();
            h.feedScalar(__builtin_bit_cast(std::uint64_t, v));
        }
    };
    // world.seed stays OUT: the WELCOME's seed is the host's authority
    // (the client's local config seed is the connect's default, not a
    // parity key — the two machines' world is the host's world).
    feedKey("world.view_distance", false, false);
    feedKey("world.day_length_seconds", false, false);
    feedKey("world.start_time_of_day", false, false);
    feedKey("mobs.enabled", true, false);
    feedKey("mobs.difficulty", false, true);
    for (const char* k : kPlayerKeys) {
        feedKey(("player." + std::string(k)).c_str(), false, false);
    }
    out = h.digest();
    return {};
}

void netFeedPlayer(Fnv64& h, std::size_t i, const Player& p,
                const Game::PlayerSession& s) {
    h.feedScalar(static_cast<std::uint64_t>(i));
    feedVec3(h, p.position());
    feedVec3(h, p.velocity());
    h.feedScalar(static_cast<std::uint32_t>(
        __builtin_bit_cast(std::uint32_t, p.yaw)));
    h.feedScalar(static_cast<std::uint32_t>(
        __builtin_bit_cast(std::uint32_t, p.pitch)));
    h.feedScalar(static_cast<std::uint32_t>(
        __builtin_bit_cast(std::uint32_t, p.health())));
    h.feedScalar(static_cast<std::uint32_t>(
        __builtin_bit_cast(std::uint32_t, p.hunger())));
    h.feedScalar(static_cast<std::uint32_t>(
        __builtin_bit_cast(std::uint32_t, p.saturation())));
    h.feedScalar(static_cast<std::uint64_t>(p.isCreative()));
    h.feedScalar(static_cast<std::uint64_t>(p.isFlying()));
    h.feedScalar(static_cast<std::uint64_t>(p.isDead()));
    h.feedScalar(static_cast<std::uint64_t>(p.xpTotal()));
    h.feedScalar(p.xpSeed());
    feedVec3(h, p.spawnPos());
    // The session's sim timers (the real-dt family — the dt-broadcast
    // makes them identical, the hash verifies it).
    feedF64(h, s.hurtFlash);
    feedF64(h, s.lastSpacePressTime);
    h.feedScalar(static_cast<std::uint64_t>(s.pearlThrowCooldownFrames));
    feedF32(h, s.prevHealth);
    const DrownState& d = s.drown;
    h.feedScalar(static_cast<std::uint64_t>(d.oxygen));
    h.feedScalar(static_cast<std::uint64_t>(d.drownTicks));
    const auto& inv = s.inventory;
    h.feedScalar(static_cast<std::uint64_t>(inv.size()));
    for (std::size_t k = 0; k < inv.size(); ++k) {
        const ItemStack& st = inv.at(k);
        h.feedScalar(st.empty() ? std::uint32_t {0}
                                : static_cast<std::uint32_t>(st.item.value));
        h.feedScalar(st.empty() ? std::uint32_t {0}
                                : static_cast<std::uint32_t>(st.count));
        feedF32(h, st.damage);
    }
    h.feedScalar(static_cast<std::uint64_t>(s.selectedSlot));
    // The effects' instances (the active set — name/tier/remaining).
    h.feedScalar(static_cast<std::uint64_t>(s.effects.count()));
    for (const ActiveEffect& a : s.effects.list()) {
        h.feedScalar(static_cast<std::uint64_t>(a.effect));
        h.feedScalar(static_cast<std::uint64_t>(a.tier));
        h.feedScalar(static_cast<std::uint64_t>(a.remaining));
    }
}


std::uint64_t netStateHash(const Game& g) {
    // The design §2.3's field list (the R15's single source — the
    // mutation test's poke surface). The order is the contract: a
    // reordering is a hash-change without a sim-change (the 12c's
    // version bump, not the 12b's concern).
    Fnv64 h;
    h.feedScalar(static_cast<std::uint64_t>(g.m_frameCount));
    h.feedScalar(static_cast<std::uint64_t>(g.m_dayCycle.timeOfDay()));
    h.feedScalar(static_cast<std::uint64_t>(g.m_weather.raining));
    h.feedScalar(static_cast<std::uint64_t>(g.m_weather.framesLeft));
    h.feedScalar(static_cast<std::uint64_t>(g.m_weather.thunderLeft));
    h.feedScalar(static_cast<std::uint64_t>(g.m_players.size()));
    for (std::size_t i = 0; i < g.m_players.size(); ++i) {
        netFeedPlayer(h, i, *g.m_players[i], *g.m_sessions[i]);
    }
    feedEntities(h, g.m_entities);
    feedEntities(h, g.m_nether.entities);
    feedEntities(h, g.m_end.entities);
    feedJournal(h, g.m_streaming.saveJournal);
    feedJournal(h, g.m_nether.streaming.saveJournal);
    feedJournal(h, g.m_end.streaming.saveJournal);
    h.feedScalar(g.m_mobs.spawnCounter);
    h.feedScalar(g.m_nether.mobs.spawnCounter);
    h.feedScalar(g.m_end.mobs.spawnCounter);
    h.feedScalar(g.m_particleBreakCounter);
    h.feedScalar(static_cast<std::uint64_t>(g.m_particles.size()));
    h.feedScalar(static_cast<std::uint64_t>(g.m_states.fires.size()));
    return h.digest();
}

} // namespace mc
