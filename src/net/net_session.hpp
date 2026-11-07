// V3-12b: the TCP TRANSPORT (design §2.2: the POSIX BSD socket, NO net
// thread — the main loop polls at the frame head, the Input pump's
// single-consumer twin). The per-connection byte queue reassembles the
// TCP stream into whole packets (the R16's half-packet / coalesced-
// packet tolerance — the feed/drain seam is the unit test's surface:
// the socket never enters the codec's tests).
//
// The blocking shape: startup (the accept / the connect / the
// handshake) is the one-shot blocking part; the frame loop's poll() is
// a zero-timeout select + the read-only drain (the send is the full-
// loop write — the 2-player LAN's per-frame traffic is a few hundred
// bytes, the kernel buffer never backs up in the PoC's RTT).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "net/net_proto.hpp"

namespace mc {

// One received packet (the type + the payload — the consumer unpacks
// through net_proto's decoders immediately; the session owns the
// payload's lifetime until the queue is drained).
struct NetPacket {
    NetType type;
    std::vector<std::uint8_t> payload;
};

class NetSession {
public:
    NetSession() = default;
    ~NetSession();
    NetSession(const NetSession&) = delete;
    NetSession& operator=(const NetSession&) = delete;

    // -- The startup (the handshake — before the frame loop) ---------
    // V3-12c2: the host's persistent LISTENER (socket + bind +
    // listen, the port's single owner — the initial window's
    // attempts + the mid-session rejoin's per-frame accepts all run
    // over this fd, the backlog 8's connect queue). The fd; -1 =
    // the named error (the S6's port face, logged).
    static int openListener(std::uint16_t port);
    // V3-12c2: the accept + the HELLO's receive over the persistent
    // listener (the listener fd's NON-owning — the caller owns it;
    // the session owns the accepted connection). timeoutMs 0 = the
    // rejoin's per-frame non-blocking attempt (no pending connect =
    // the quiet "timed out" face, the caller's ignore); > 0 = the
    // window's retry attempt. "" on success (outHello filled, the
    // session connected), the named error otherwise.
    std::string acceptFromListener(int listenFd, NetHello& outHello,
                                   int timeoutMs);
    // Client: the connect + the HELLO's send + the WELCOME's receive
    // (a REJECT = the named refusal surfaced to the caller, the
    // version-lie's exit-1 precedent). "" on success.
    std::string startClient(const std::string& host, std::uint16_t port,
                            const NetHello& hello, NetWelcome& out);

    // -- The frame loop ------------------------------------------------
    // The frame-head poll: the zero-timeout select + the read-only
    // drain (the peer's packets queue for the nextPacket's consume).
    // Returns "" while connected; the fatal error (the EOF / the
    // transport's gone — the D2's exit signal, the R13's host-single-
    // point) otherwise.
    std::string poll();
    // The blocking next-packet wait (the client's frame-block on the
    // host's FRAME — the timeout's named error, the design's "the
    // client blocks the sim on the packet"). The FIRST queued / read
    // packet wins (the caller dispatches on the type).
    std::string waitPacket(NetPacket& out, int timeoutSec);
    // Consume the next queued packet of a type (false = none).
    bool nextPacket(NetType type, NetPacket& out);
    // Send a whole packet (the magic + the type + the len + the
    // payload). The full-loop write (the EPIPE's named error — the
    // peer's gone mid-send, the poll's EOF's twin).
    std::string send(NetType type, const std::vector<std::uint8_t>& payload);
    bool connected() const {
        return m_sock >= 0;
    }
    int fd() const { return m_sock; }
    void close();
    // V3-12c1 (the N-host's barrier): the multi-connection select —
    // which of the fds is readable within the timeout (-1 = none).
    // The host's lockstep barrier's wait face (a single waitPacket
    // blocks on ONE connection — the N>2's per-remote watermark needs
    // the fan-in).
    static std::vector<int> selectReady(const std::vector<int>& fds,
                                        int timeoutMs);

    // -- The reassembly seam (the R16's unit-test surface) ------------
    // Feed raw bytes (a split stream) + drain the whole packets —
    // the socket path's parseQueue twin, the deterministic byte
    // stream's tolerance proof.
    void feed(const std::uint8_t* data, std::size_t n);
    void feed(const std::string& data) {
        feed(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
    }
    std::vector<NetPacket> drainPackets();

private:
    int m_sock = -1;
    bool m_streamCorrupt = false; // the magic's broke face (the named error)
    std::vector<std::uint8_t> m_rx; // the reassembly buffer
    std::vector<NetPacket> m_queue; // the whole packets, in stream order
    // The stream's parse (the header's magic + the type + the len's
    // bound check — a corrupt stream = the fatal error, the S6's
    // magic face's wire twin).
    void parse();
    std::string sendRaw(const std::vector<std::uint8_t>& bytes);
};

} // namespace mc
