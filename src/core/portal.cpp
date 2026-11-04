// V3-1 Phase A (v3.2.0): the nether portal frame — the pure function
// family (see core/portal.hpp for the model + the bounded-scan
// discipline). No GL, no SDL, no logging: the unit tests own these.

#include "core/portal.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

#include "block/block.hpp"
#include "world/noise.hpp"
#include "world/world.hpp"

namespace mc {

namespace {

// The golden-ratio 64-bit step (the PITS #4 advance discipline — the
// roll state must ADVANCE with the frame before the splitmix64
// finalizer; the same constant the ambience / fire-particle streams use).
constexpr std::uint64_t kGoldenStep = 0x9E3779B97F4A7C15ULL;
// 2^53 — the uniform [0, 1) denominator (the day_cycle starfield
// convention, ambience's twin).
constexpr double kU53 = 9007199254740992.0;
// The scan reach from the event cell on each axis (the frame's far edge
// is at most the max extent - 1 away: a 23-long-edge frame holding the
// event cell at its corner reaches 22).
constexpr int kScanReachU = kPortalMaxLongEdge - 1;
constexpr int kScanReachY = kPortalMaxHeight - 1;

// Reinterpret an (possibly negative) int as 64 bits for hashing (the
// worldgen's u64 — two's-complement is a bijection).
std::uint64_t asU64(int v) {
    return static_cast<std::uint64_t>(static_cast<std::int64_t>(v));
}

// The scan window's grid side (kScanReachU * 2 + 1 = 45; kScanReachY is
// the same value by the 23 / 23 max pair — kept distinct for the
// arithmetic's honesty).
constexpr int kGridSide = kScanReachU * 2 + 1;

// The cell classification of the scan grid (the four interior-legal
// contents + the bad bucket; the obsidian / fire / portal ids decide,
// the air sentinel is the block grid's own convention).
enum class CellClass : std::uint8_t {
    Other = 0,
    Obsidian = 1,
    Fire = 2,
    Portal = 3,
    Air = 4,
};

// The 2D prefix sum over a kGridSide x kGridSide grid (inclusive
// rectangle sums in O(1); row = y, column = u).
struct Prefix2D {
    std::array<std::array<int, kGridSide + 1>, kGridSide + 1> p {};

    void build(const std::array<std::array<int, kGridSide>, kGridSide>& g) {
        for (int y = 0; y < kGridSide; ++y) {
            for (int u = 0; u < kGridSide; ++u) {
                p[y + 1][u + 1] = g[y][u] + p[y][u + 1] + p[y + 1][u] -
                                  p[y][u];
            }
        }
    }
    // The sum over the INCLUSIVE window rectangle (u0..u1, y0..y1).
    int rect(int u0, int y0, int u1, int y1) const {
        if (u0 > u1 || y0 > y1) {
            return 0;
        }
        return p[y1 + 1][u1 + 1] - p[y0][u1 + 1] - p[y1 + 1][u0] +
               p[y0][u0];
    }
    int cell(int u, int y) const { return p[y + 1][u + 1] - p[y][u + 1] -
                                          p[y + 1][u] + p[y][u]; }
};

// One plane of the scan: the grid build + the candidate enumeration.
// `axis` = 0: the plane z = eventCell.z, the long edge along X (u = x);
// `axis` = 1: the plane x = eventCell.x, the long edge along Z (u = z).
void scanPlane(std::vector<PortalFrameStatus>& out, const World& world,
               const BlockPos& eventCell, int axis, BlockID obsidianId,
               BlockID fireId, BlockID portalId, bool allowPortalInterior) {
    const int c = (axis == 0) ? eventCell.x : eventCell.z;
    const int cy = eventCell.y;
    const int plane = (axis == 0) ? eventCell.z : eventCell.x;
    const int uLo = c - kScanReachU;
    const int yLo = cy - kScanReachY;

    // Build the classification grid (out-of-world reads are air / void —
    // World::getBlock's convention, so the window never needs a bounds
    // guard).
    std::array<std::array<int, kGridSide>, kGridSide> gridObs {}, gridFire {},
        gridPortal {}, gridBad {};
    std::array<std::array<CellClass, kGridSide>, kGridSide> cls {};
    for (int wy = 0; wy < kGridSide; ++wy) {
        const int y = yLo + wy;
        for (int wu = 0; wu < kGridSide; ++wu) {
            const int u = uLo + wu;
            const BlockID id =
                (axis == 0) ? world.getBlock(u, y, plane)
                            : world.getBlock(plane, y, u);
            CellClass k = CellClass::Other;
            if (id == BlockID::air()) {
                k = CellClass::Air;
            } else if (id == obsidianId) {
                k = CellClass::Obsidian;
            } else if (id == fireId) {
                k = CellClass::Fire;
            } else if (allowPortalInterior && id == portalId) {
                k = CellClass::Portal;
            }
            cls[wy][wu] = k;
            gridObs[wy][wu] = (k == CellClass::Obsidian) ? 1 : 0;
            gridFire[wy][wu] = (k == CellClass::Fire) ? 1 : 0;
            gridPortal[wy][wu] = (k == CellClass::Portal) ? 1 : 0;
            // Interior-legal = air / fire / (portal when allowed); every
            // other content (obsidian included, and the portal when the
            // strict search is active) disqualifies the interior.
            gridBad[wy][wu] = (k == CellClass::Air || k == CellClass::Fire ||
                               k == CellClass::Portal)
                                  ? 0
                                  : 1;
        }
    }
    Prefix2D sumObs, sumFire, sumPortal, sumBad;
    sumObs.build(gridObs);
    sumFire.build(gridFire);
    sumPortal.build(gridPortal);
    sumBad.build(gridBad);

    // The event cell's window coordinates (it is always inside the window:
    // the reach is the max frame extent - 1).
    const int cu = c - uLo;
    const int cw = cy - yLo;

    // Enumerate every size-valid frame whose bounding box contains the
    // event cell (u0 <= c <= u0 + W - 1, y0 <= cy <= y0 + H - 1) — the
    // candidate count is the bounded ~72k O(1) checks per plane. The
    // frame's coordinates are WORLD; the prefix grids are indexed in
    // WINDOW coordinates (the uLo / yLo offset).
    for (int W = kPortalMinLongEdge; W <= kPortalMaxLongEdge; ++W) {
        for (int u0 = std::max(uLo, c - (W - 1)); u0 <= std::min(c, uLo + kGridSide - 1); ++u0) {
            const int u1 = u0 + W - 1;
            const int gu0 = u0 - uLo;
            const int gu1 = u1 - uLo;
            for (int H = kPortalMinHeight; H <= kPortalMaxHeight; ++H) {
                for (int y0 = std::max(yLo, cy - (H - 1)); y0 <= std::min(cy, yLo + kGridSide - 1); ++y0) {
                    const int y1 = y0 + H - 1;
                    const int gy0 = y0 - yLo;
                    const int gy1 = y1 - yLo;
                    // The perimeter (the bounding box minus the interior
                    // rectangle — non-empty by the size floor).
                    const int perimeterCells = 2 * W + 2 * H - 4;
                    const int perimeterObs =
                        sumObs.rect(gu0, gy0, gu1, gy1) -
                        sumObs.rect(gu0 + 1, gy0 + 1, gu1 - 1, gy1 - 1);
                    const int cornerObs =
                        sumObs.cell(gu0, gy0) + sumObs.cell(gu1, gy0) +
                        sumObs.cell(gu0, gy1) + sumObs.cell(gu1, gy1);
                    const int nonCornerPerimeter = perimeterCells - 4;
                    const bool ringComplete =
                        (perimeterObs - cornerObs) == nonCornerPerimeter;
                    const bool eventOnRing =
                        (c == u0 || c == u1 || cy == y0 || cy == y1);
                    const bool eventIsCorner =
                        (c == u0 || c == u1) && (cy == y0 || cy == y1);
                    const bool eventObsidian =
                        (cls[cw][cu] == CellClass::Obsidian);
                    const bool ringCompletePreEvent =
                        ringComplete ||
                        (eventOnRing && !eventIsCorner && !eventObsidian &&
                         (perimeterObs + 1 - cornerObs) ==
                             nonCornerPerimeter);
                    if (!ringCompletePreEvent) {
                        continue; // not a frame even before the event
                    }
                    // The interior must be all air / fire / (portal).
                    if (sumBad.rect(gu0 + 1, gy0 + 1, gu1 - 1, gy1 - 1) != 0) {
                        continue;
                    }
                    const int interiorFire =
                        sumFire.rect(gu0 + 1, gy0 + 1, gu1 - 1, gy1 - 1);
                    const int interiorPortal =
                        sumPortal.rect(gu0 + 1, gy0 + 1, gu1 - 1, gy1 - 1);
                    PortalFrameStatus st;
                    st.frame = PortalFrame {axis, u0, u1, y0, y1, plane};
                    st.ringComplete = ringComplete;
                    st.ringCompletePreEvent = ringCompletePreEvent;
                    st.eventOnRing = eventOnRing;
                    st.eventIsCorner = eventIsCorner;
                    st.interiorFire = interiorFire;
                    st.interiorPortal = interiorPortal;
                    st.interiorAir =
                        (W - 2) * (H - 2) - interiorFire - interiorPortal;
                    out.push_back(std::move(st));
                }
            }
        }
    }
}

// The deterministic frame selection (the "largest frame" rule): the
// largest area; a tie prefers the interior that already holds a fire
// (the one an adjacent event would activate — the shared-obsidian frames'
// disambiguation); then (axis, lo, y0) ascending.
bool framePreferred(const PortalFrameStatus& a, const PortalFrameStatus& b) {
    if (a.frame.area() != b.frame.area()) {
        return a.frame.area() > b.frame.area();
    }
    if ((a.interiorFire > 0) != (b.interiorFire > 0)) {
        return a.interiorFire > 0;
    }
    if (a.frame.axis != b.frame.axis) {
        return a.frame.axis < b.frame.axis;
    }
    if (a.frame.lo != b.frame.lo) {
        return a.frame.lo < b.frame.lo;
    }
    return a.frame.y0 < b.frame.y0;
}

} // namespace

bool portalFrameSizeValid(const PortalFrame& frame) {
    return frame.longEdge() >= kPortalMinLongEdge &&
           frame.longEdge() <= kPortalMaxLongEdge &&
           frame.height() >= kPortalMinHeight &&
           frame.height() <= kPortalMaxHeight;
}

BlockPos portalFrameCell(const PortalFrame& frame, int u, int y) {
    return (frame.axis == 0) ? BlockPos {u, y, frame.plane}
                             : BlockPos {frame.plane, y, u};
}

bool portalFrameIsCorner(const PortalFrame& frame, const BlockPos& cell) {
    const bool onPlane =
        (frame.axis == 0) ? (cell.z == frame.plane) : (cell.x == frame.plane);
    if (!onPlane) {
        return false;
    }
    const int u = (frame.axis == 0) ? cell.x : cell.z;
    return (u == frame.lo || u == frame.hi) &&
           (cell.y == frame.y0 || cell.y == frame.y1);
}

bool portalFrameOnRing(const PortalFrame& frame, const BlockPos& cell) {
    const bool onPlane =
        (frame.axis == 0) ? (cell.z == frame.plane) : (cell.x == frame.plane);
    if (!onPlane) {
        return false;
    }
    const int u = (frame.axis == 0) ? cell.x : cell.z;
    return (u == frame.lo || u == frame.hi || cell.y == frame.y0 ||
            cell.y == frame.y1) &&
           u >= frame.lo && u <= frame.hi && cell.y >= frame.y0 &&
           cell.y <= frame.y1;
}

bool portalFrameStrictlyInterior(const PortalFrame& frame,
                                 const BlockPos& cell) {
    const bool onPlane =
        (frame.axis == 0) ? (cell.z == frame.plane) : (cell.x == frame.plane);
    if (!onPlane) {
        return false;
    }
    const int u = (frame.axis == 0) ? cell.x : cell.z;
    return u > frame.lo && u < frame.hi && cell.y > frame.y0 &&
           cell.y < frame.y1;
}

std::vector<BlockPos> portalFrameRing(const PortalFrame& frame) {
    std::vector<BlockPos> out;
    out.reserve(2 * frame.longEdge() + 2 * frame.height() - 4);
    for (int y = frame.y0; y <= frame.y1; ++y) {
        for (int u = frame.lo; u <= frame.hi; ++u) {
            if (y == frame.y0 || y == frame.y1 || u == frame.lo ||
                u == frame.hi) {
                out.push_back(portalFrameCell(frame, u, y));
            }
        }
    }
    return out;
}

std::vector<BlockPos> portalFrameInterior(const PortalFrame& frame) {
    std::vector<BlockPos> out;
    out.reserve((frame.longEdge() - 2) * (frame.height() - 2));
    for (int y = frame.y0 + 1; y <= frame.y1 - 1; ++y) {
        for (int u = frame.lo + 1; u <= frame.hi - 1; ++u) {
            out.push_back(portalFrameCell(frame, u, y));
        }
    }
    return out;
}

BlockPos overworldToNether(int x, int y, int z) {
    return BlockPos {floorDiv(x, kPortalMappingScale), y,
                     floorDiv(z, kPortalMappingScale)};
}

bool portalAmbientRoll(std::uint64_t seed, std::uint64_t frame,
                       const BlockPos& cell) {
    // The state advances with the frame (PITS #4: splitmix64 is a pure
    // finalizer — the golden step before it) + the per-cell hash3
    // decorrelation (the fire-particle-roll pattern); the dedicated salt
    // keeps the stream off every other deterministic roll.
    const std::uint64_t state = seed + frame * kGoldenStep +
                                hash3(asU64(cell.x), asU64(cell.y),
                                      asU64(cell.z), kPortalAmbientSalt);
    const double u =
        static_cast<double>(splitmix64(state) >> 11) / kU53;
    return u < 1.0 / (kPortalAmbientIntervalSeconds * 60.0);
}

std::vector<PortalFrameStatus> scanPortalFrames(
    const World& world, const BlockPos& eventCell, BlockID obsidianId,
    BlockID fireId, BlockID portalId, bool allowPortalInterior) {
    std::vector<PortalFrameStatus> out;
    // Both vertical planes through the event cell (axis 0 first — the
    // deterministic plane order the selection's tiebreak relies on).
    scanPlane(out, world, eventCell, 0, obsidianId, fireId, portalId,
              allowPortalInterior);
    scanPlane(out, world, eventCell, 1, obsidianId, fireId, portalId,
              allowPortalInterior);
    std::sort(out.begin(), out.end(),
              [](const PortalFrameStatus& a, const PortalFrameStatus& b) {
                  if (a.frame.axis != b.frame.axis) {
                      return a.frame.axis < b.frame.axis;
                  }
                  if (a.frame.lo != b.frame.lo) {
                      return a.frame.lo < b.frame.lo;
                  }
                  if (a.frame.y0 != b.frame.y0) {
                      return a.frame.y0 < b.frame.y0;
                  }
                  return a.frame.plane < b.frame.plane;
              });
    return out;
}

std::optional<PortalFrame> findFrameAt(const World& world,
                                       const BlockPos& cell,
                                       BlockID obsidianId, BlockID fireId) {
    // The strict search: the interior holds only air / fire (a portal cell
    // disqualifies — an already-activated frame is not "a frame waiting
    // for its fire").
    const std::vector<PortalFrameStatus> candidates =
        scanPortalFrames(world, cell, obsidianId, fireId, BlockID::invalid(),
                         /*allowPortalInterior=*/false);
    const PortalFrameStatus* best = nullptr;
    for (const PortalFrameStatus& st : candidates) {
        if (!st.ringComplete) {
            continue; // complete in the CURRENT world (no pre-event view)
        }
        if (best == nullptr || framePreferred(st, *best)) {
            best = &st;
        }
    }
    if (best == nullptr) {
        return std::nullopt;
    }
    return best->frame;
}

std::optional<PortalFrame> portalValidFrame(
    const World& world, const BlockPos& portalCell, BlockID obsidianId,
    BlockID fireId, BlockID portalId) {
    // The rescan search: the live frame's own portals are interior-legal,
    // and the cell must be STRICTLY interior (a portal on the ring is
    // hand-editing, not a frame).
    const std::vector<PortalFrameStatus> candidates =
        scanPortalFrames(world, portalCell, obsidianId, fireId, portalId,
                         /*allowPortalInterior=*/true);
    const PortalFrameStatus* best = nullptr;
    for (const PortalFrameStatus& st : candidates) {
        if (!st.ringComplete ||
            !portalFrameStrictlyInterior(st.frame, portalCell)) {
            continue;
        }
        if (best == nullptr || framePreferred(st, *best)) {
            best = &st;
        }
    }
    if (best == nullptr) {
        return std::nullopt;
    }
    return best->frame;
}

std::vector<BlockPos> portalActivationPlan(const World& world,
                                           const PortalFrame& frame,
                                           BlockID fireId) {
    std::vector<BlockPos> out;
    for (const BlockPos& p : portalFrameInterior(frame)) {
        const BlockID id = world.getBlock(p.x, p.y, p.z);
        if (id == BlockID::air() || id == fireId) {
            out.push_back(p);
        }
    }
    return out;
}

std::vector<BlockPos> portalDeactivationPlan(const World& world,
                                             const PortalFrame& frame,
                                             BlockID portalId) {
    std::vector<BlockPos> out;
    for (const BlockPos& p : portalFrameInterior(frame)) {
        if (world.getBlock(p.x, p.y, p.z) == portalId) {
            out.push_back(p);
        }
    }
    return out;
}

PortalTransitionDecision portalTransitionDecision(
    const std::vector<PortalFrameStatus>& candidates, BlockID oldId,
    BlockID obsidianId) {
    PortalTransitionDecision d;
    for (const PortalFrameStatus& st : candidates) {
        // Deactivation: an obsidian destruction broke a NON-CORNER ring
        // cell of a frame that was complete pre-event and whose interior
        // holds portal blocks (the corner destruction keeps them —
        // Minecraft-exact). A non-obsidian oldId never deactivates (the
        // fire extinguish's self-sustainability pin).
        if (oldId == obsidianId && !st.ringComplete &&
            st.ringCompletePreEvent && st.eventOnRing && !st.eventIsCorner &&
            st.interiorPortal > 0) {
            d.deactivate.push_back(st);
        }
        // Activation: a currently-complete frame with a live interior fire
        // and no portal yet (both-sides-checked — the design's deviation
        // from the wiki's "fire last" quirk).
        if (st.ringComplete && st.interiorFire > 0 && st.interiorPortal == 0) {
            d.activate.push_back(st);
        }
    }
    return d;
}

PortalEntryStep portalEntryStep(bool inPortal, int armed) {
    if (!inPortal) {
        return PortalEntryStep {0, false}; // leaving cancels immediately
    }
    if (armed <= 0) {
        // The arm frame is the FIRST of the kPortalEntryFrames
        // consecutive in-portal frames (the countdown includes it).
        return PortalEntryStep {kPortalEntryFrames - 1, false};
    }
    const int next = armed - 1;
    return PortalEntryStep {next, next == 0}; // continue; the last frame
}

} // namespace mc
