// V3-12b: the TCP transport's implementation (the BSD socket's
// blocking startup + the zero-timeout poll + the stream reassembly).
#include "net/net_session.hpp"

#include <cerrno>
#include <cstring>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>

#include "core/logger.hpp"

namespace mc {

namespace {
// The header's fixed size (the magic u32 + the type u8 + the len u32).
constexpr std::size_t kHeaderSize = 9;
// The packet's wire bound (the design's "packets <= 512B" — the
// WELCOME's journal is the exception, the save document's KB-MB
// one-shot: the bound guards the STREAM PARSE, not the WELCOME's
// payload, which rides its own u32 length's sanity (<= 64MB)).
constexpr std::uint32_t kMaxPacketPayload = 64u * 1024u * 1024u;
// The handshake's read timeout (the accept's / the connect's peer
// silence — the named error, not the hang).
constexpr int kHandshakeTimeoutSec = 10;

std::string errnoStr() {
    return std::strerror(errno);
}

bool readable(int fd, int timeoutSec) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(fd, &set);
    timeval tv {timeoutSec, 0};
    return select(fd + 1, &set, nullptr, nullptr,
                  timeoutSec >= 0 ? &tv : nullptr) > 0;
}
} // namespace

std::vector<int> NetSession::selectReady(const std::vector<int>& fds,
                                         int timeoutMs) {
    std::vector<int> ready;
    if (fds.empty()) {
        return ready;
    }
    int maxfd = -1;
    fd_set set;
    FD_ZERO(&set);
    for (int fd : fds) {
        if (fd < 0) {
            continue;
        }
        FD_SET(fd, &set);
        if (fd > maxfd) {
            maxfd = fd;
        }
    }
    if (maxfd < 0) {
        return ready;
    }
    timeval tv {0, timeoutMs < 0 ? -1 : timeoutMs * 1000};
    const int n = select(maxfd + 1, &set, nullptr, nullptr,
                         timeoutMs < 0 ? nullptr : &tv);
    if (n <= 0) {
        return ready;
    }
    for (int fd : fds) {
        if (fd >= 0 && FD_ISSET(fd, &set)) {
            ready.push_back(fd);
        }
    }
    return ready;
}

NetSession::~NetSession() {
    close();
}

void NetSession::close() {
    if (m_sock >= 0) {
        ::close(m_sock);
        m_sock = -1;
    }
    m_rx.clear();
    m_queue.clear();
}

void NetSession::parse() {
    // The whole-packet drain (the stream order preserved — the
    // lockstep's frame law's premise). A corrupt header (the magic /
    // the oversized len) = the stream desync: clear the queue's
    // trust (the poll surfaces the fatal via the next read's EOF —
    // the named error's wire twin, the S6's magic face).
    for (;;) {
        if (m_rx.size() < kHeaderSize) {
            return;
        }
        std::uint32_t magic = 0;
        for (int i = 0; i < 4; ++i) {
            magic |= static_cast<std::uint32_t>(m_rx[i]) << (8 * i);
        }
        if (magic != kNetMagic) {
            MC_LOG_ERROR("net: the stream's magic broke (the desync's "
                         "named face — the session ends)");
            m_streamCorrupt = true;
            m_rx.clear();
            return;
        }
        const auto type = static_cast<NetType>(m_rx[4]);
        std::uint32_t len = 0;
        for (int i = 0; i < 4; ++i) {
            len |= static_cast<std::uint32_t>(m_rx[5 + i]) << (8 * i);
        }
        if (len > kMaxPacketPayload) {
            MC_LOG_ERROR("net: the packet's length is out of bound (" +
                         std::to_string(len) + " bytes)");
            m_rx.clear();
            return;
        }
        if (m_rx.size() < kHeaderSize + len) {
            return; // the half-packet: wait for the stream's next read
        }
        NetPacket pkt;
        pkt.type = type;
        pkt.payload.assign(m_rx.begin() + kHeaderSize,
                           m_rx.begin() + kHeaderSize + len);
        m_queue.push_back(std::move(pkt));
        m_rx.erase(m_rx.begin(), m_rx.begin() + kHeaderSize + len);
    }
}

void NetSession::feed(const std::uint8_t* data, std::size_t n) {
    m_rx.insert(m_rx.end(), data, data + n);
    parse();
}

std::vector<NetPacket> NetSession::drainPackets() {
    std::vector<NetPacket> out;
    out.swap(m_queue);
    return out;
}

std::string NetSession::sendRaw(const std::vector<std::uint8_t>& bytes) {
    std::size_t sent = 0;
    while (sent < bytes.size()) {
        const ssize_t n = ::send(m_sock, bytes.data() + sent, bytes.size() - sent,
                                 MSG_NOSIGNAL);
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return "net send failed: " + errnoStr();
        }
        sent += static_cast<std::size_t>(n);
    }
    return {};
}

std::string NetSession::send(NetType type,
                             const std::vector<std::uint8_t>& payload) {
    std::vector<std::uint8_t> wire;
    wire.reserve(kHeaderSize + payload.size());
    const std::uint32_t magic = kNetMagic;
    for (int i = 0; i < 4; ++i) {
        wire.push_back(static_cast<std::uint8_t>((magic >> (8 * i)) & 0xFF));
    }
    wire.push_back(static_cast<std::uint8_t>(type));
    const std::uint32_t len = static_cast<std::uint32_t>(payload.size());
    for (int i = 0; i < 4; ++i) {
        wire.push_back(static_cast<std::uint8_t>((len >> (8 * i)) & 0xFF));
    }
    wire.insert(wire.end(), payload.begin(), payload.end());
    return sendRaw(wire);
}

std::string NetSession::waitPacket(NetPacket& out, int timeoutSec) {
    // The handshake's blocking read (the timeout's named error — the
    // peer's silence is the connect's failure face). The FIRST packet
    // wins (the host sends exactly one reply to the HELLO — the
    // WELCOME or the REJECT; the caller dispatches on the type).
    for (;;) {
        if (!m_queue.empty()) {
            out = std::move(m_queue.front());
            m_queue.erase(m_queue.begin());
            return {};
        }
        if (m_streamCorrupt) {
            return "the stream's magic broke (the desync's named face)";
        }
        if (!readable(m_sock, timeoutSec)) {
            return "net timed out (no packet in " +
                   std::to_string(timeoutSec) + " s)";
        }
        char buf[8192];
        const ssize_t n = ::recv(m_sock, buf, sizeof(buf), 0);
        if (n == 0) {
            return "net: the peer closed during the handshake";
        }
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            return "net recv failed: " + errnoStr();
        }
        feed(reinterpret_cast<const std::uint8_t*>(buf),
             static_cast<std::size_t>(n));
    }
}

int NetSession::openListener(std::uint16_t port) {
    const int listenFd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0) {
        MC_LOG_ERROR("net listen socket: " + errnoStr());
        return -1;
    }
    int on = 1;
    ::setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);
    if (::bind(listenFd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) <
        0) {
        MC_LOG_ERROR("net bind : " + errnoStr());
        ::close(listenFd);
        return -1;
    }
    // The backlog 8 (the protocol's player cap — the mid-session
    // rejoin's queue's belt, the initial window's shape).
    if (::listen(listenFd, 8) < 0) {
        MC_LOG_ERROR("net listen: " + errnoStr());
        ::close(listenFd);
        return -1;
    }
    MC_LOG_DEBUG("net: listening on port " + std::to_string(port));
    return listenFd;
}

std::string NetSession::acceptFromListener(int listenFd, NetHello& outHello,
                                           int timeoutMs) {
    // The accept's select (the timeout's ms — the first client's
    // 10-s blocking accept / the accept window's 250-ms attempt /
    // the rejoin's per-frame 0-ms quiet attempt: a no-pending = the
    // "timed out" face, the caller's ignore).
    sockaddr_in peer {};
    socklen_t plen = sizeof(peer);
    for (;;) {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(listenFd, &set);
        timeval tv {timeoutMs / 1000, (timeoutMs % 1000) * 1000};
        const int sel = ::select(listenFd + 1, &set, nullptr, nullptr, &tv);
        if (sel < 0) {
            return "net accept select: " + errnoStr();
        }
        if (sel == 0) {
            return "net accept timed out (no client in " +
                   std::to_string(timeoutMs) + " ms)";
        }
        m_sock = ::accept(listenFd, reinterpret_cast<sockaddr*>(&peer), &plen);
        if (m_sock < 0) {
            continue; // the spurious wakeup: re-select
        }
        break;
    }
    char ip[INET_ADDRSTRLEN] = "?";
    ::inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip));
    MC_LOG_INFO("net: client connected from " + std::string(ip) + ":" +
                std::to_string(ntohs(peer.sin_port)));
    // The HELLO's blocking receive (the four fingerprints' arrival —
    // the game's comparison + the WELCOME / the REJECT's send is the
    // caller's act).
    NetPacket pkt;
    std::string err = waitPacket(pkt, kHandshakeTimeoutSec);
    if (!err.empty()) {
        close();
        return err;
    }
    if (pkt.type != NetType::Hello ||
        !unpackHello(pkt.payload.data(), pkt.payload.size(), outHello)) {
        close();
        return "net: the HELLO's payload is corrupt (the named exit)";
    }
    return {};
}

std::string NetSession::startClient(const std::string& host, std::uint16_t port,
                                    const NetHello& hello, NetWelcome& out) {
    m_sock = ::socket(AF_INET, SOCK_STREAM, 0);
    if (m_sock < 0) {
        return "net socket: " + errnoStr();
    }
    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        const std::string err = "net: bad host address '" + host + "'";
        close();
        return err;
    }
    if (::connect(m_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) <
        0) {
        const std::string err = "net connect " + host + ":" +
                                std::to_string(port) + ": " + errnoStr();
        close();
        return err;
    }
    MC_LOG_INFO("net: connected to " + host + ":" + std::to_string(port));
    std::string e = send(NetType::Hello, packHello(hello));
    if (!e.empty()) {
        close();
        return e;
    }
    NetPacket pkt;
    e = waitPacket(pkt, kHandshakeTimeoutSec);
    if (!e.empty()) {
        close();
        return e;
    }
    if (pkt.type == NetType::Reject) {
        std::string reason;
        unpackReject(pkt.payload.data(), pkt.payload.size(), reason);
        close();
        return "net rejected: " + (reason.empty() ? std::string("(unparsed)")
                                                  : reason);
    }
    if (pkt.type != NetType::Welcome ||
        !unpackWelcome(pkt.payload.data(), pkt.payload.size(), out)) {
        close();
        return "net: the WELCOME's payload is corrupt";
    }
    return {};
}

std::string NetSession::poll() {
    if (m_sock < 0) {
        return "net: the session is closed";
    }
    if (!readable(m_sock, 0)) {
        return {}; // the zero-timeout: nothing yet (the frame's own pace)
    }
    char buf[8192];
    const ssize_t n = ::recv(m_sock, buf, sizeof(buf), 0);
    if (n == 0) {
        return "net: the peer disconnected (the EOF — the CTRL's wire twin)";
    }
    if (n < 0) {
        if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
            return {};
        }
        return "net recv failed: " + errnoStr();
    }
    feed(reinterpret_cast<const std::uint8_t*>(buf), static_cast<std::size_t>(n));
    return {};
}

bool NetSession::nextPacket(NetType type, NetPacket& out) {
    for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
        if (it->type == type) {
            out = std::move(*it);
            m_queue.erase(it);
            return true;
        }
    }
    return false;
}

} // namespace mc
