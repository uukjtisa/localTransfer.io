// ================================================================
//  localTransfer.io  –  mdns.cpp
//  A small multicast-DNS responder: answers A queries for
//  <name>.local and advertises _http._tcp so the server can be found
//  at http://localtransfer-io.local/ without touching the router.
//
//  Wire format notes (RFC 1035 + RFC 6762):
//    * mDNS reuses the DNS message format on UDP 224.0.0.251:5353.
//    * Names are length-prefixed labels terminated by a zero byte.
//      We never emit compression pointers — responses are tiny and
//      every resolver accepts uncompressed names.
//    * Response records set the top bit of the CLASS field
//      (0x8000, "cache flush") to say this is authoritative.
//    * A query with the top bit of QCLASS set is a "unicast reply
//      requested" (QU) question; we answer those directly to the
//      sender as well as to the group.
// ================================================================
#include "mdns.h"
#include "utils.h"

// Order matters: the first entry is the one advertised in logs and SRV.
std::vector<std::string> g_mdnsNames = {
    "localTransfer.io.local",   // the headline name; multi-label, Apple-friendly
    "localtransfer-io.local",   // single label — what Windows reliably resolves
    "localtransfer.local",      // plainest fallback
};

static std::atomic<bool> g_mdnsRun{false};
static std::thread*      g_mdnsThread = nullptr;
static SOCKET            g_mdnsSock   = INVALID_SOCKET;
static int               g_mdnsPort   = 0;   // the HTTP port we advertise

static const char* MDNS_GROUP = "224.0.0.251";
static const uint16_t MDNS_PORT = 5353;

std::string mdnsFullName() { return g_mdnsNames.empty() ? "localtransfer.local" : g_mdnsNames[0]; }

// ── byte helpers ────────────────────────────────────────────────
static void put16(std::vector<uint8_t>& b, uint16_t v) { b.push_back(v >> 8); b.push_back(v & 0xFF); }
static void put32(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back((v >> 24) & 0xFF); b.push_back((v >> 16) & 0xFF);
    b.push_back((v >> 8) & 0xFF);  b.push_back(v & 0xFF);
}
// Encode "a.b.c" as length-prefixed labels + terminating zero.
static void putName(std::vector<uint8_t>& b, const std::string& name) {
    size_t start = 0;
    while (start < name.size()) {
        size_t dot = name.find('.', start);
        if (dot == std::string::npos) dot = name.size();
        size_t len = dot - start;
        if (len > 63) len = 63;
        b.push_back((uint8_t)len);
        b.insert(b.end(), name.begin() + start, name.begin() + start + len);
        start = dot + 1;
    }
    b.push_back(0);
}

// Read a name at offset, following compression pointers. Returns false on
// malformed input; advances 'off' past the name only when it was inline.
static bool readName(const uint8_t* buf, size_t len, size_t& off, std::string& out) {
    out.clear();
    size_t cur = off;
    bool jumped = false;
    int guard = 0;
    while (cur < len) {
        if (++guard > 128) return false;              // pointer loop
        uint8_t l = buf[cur];
        if (l == 0) { if (!jumped) off = cur + 1; return true; }
        if ((l & 0xC0) == 0xC0) {                     // compression pointer
            if (cur + 1 >= len) return false;
            size_t ptr = ((l & 0x3F) << 8) | buf[cur + 1];
            if (!jumped) off = cur + 2;
            jumped = true;
            if (ptr >= len) return false;
            cur = ptr;
            continue;
        }
        if (cur + 1 + l > len) return false;
        if (!out.empty()) out += '.';
        out.append((const char*)buf + cur + 1, l);
        cur += 1 + l;
    }
    return false;
}

// One literal label (dots and all), then the service type as normal labels.
static void putInstName(std::vector<uint8_t>& b, const std::string& inst, const std::string& type) {
    size_t len = inst.size() > 63 ? 63 : inst.size();
    b.push_back((uint8_t)len);
    b.insert(b.end(), inst.begin(), inst.begin() + len);
    putName(b, type);
}

static bool iequals(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    return true;
}

// The address we hand out. Prefer a real LAN address over an APIPA
// (169.254.x) one, which is what you get from a disconnected adapter.
static uint32_t primaryIPv4() {
    auto ips = getLocalIPs();
    std::string best;
    for (auto& ip : ips) {
        if (ip.rfind("169.254.", 0) == 0) continue;
        if (ip == "127.0.0.1") continue;
        best = ip; break;
    }
    if (best.empty() && !ips.empty()) best = ips[0];
    if (best.empty()) best = "127.0.0.1";
    in_addr a{};
    inet_pton(AF_INET, best.c_str(), &a);
    return ntohl(a.s_addr);
}

// ── record builders ─────────────────────────────────────────────
static void addA(std::vector<uint8_t>& b, const std::string& name, uint32_t ip, uint32_t ttl) {
    putName(b, name);
    put16(b, 1);            // TYPE A
    put16(b, 0x8001);       // CLASS IN + cache-flush
    put32(b, ttl);
    put16(b, 4);
    put32(b, ip);
}
static void addPtr(std::vector<uint8_t>& b, const std::string& name,
                   const std::string& instLabel, const std::string& instType, uint32_t ttl) {
    putName(b, name);
    put16(b, 12);           // TYPE PTR
    put16(b, 0x0001);       // shared record: no cache-flush bit
    put32(b, ttl);
    std::vector<uint8_t> rd; putInstName(rd, instLabel, instType);
    put16(b, (uint16_t)rd.size());
    b.insert(b.end(), rd.begin(), rd.end());
}
static void addSrv(std::vector<uint8_t>& b, const std::string& instLabel, const std::string& instType,
                   const std::string& host, uint16_t port, uint32_t ttl) {
    putInstName(b, instLabel, instType);
    put16(b, 33);           // TYPE SRV
    put16(b, 0x8001);
    put32(b, ttl);
    std::vector<uint8_t> rd;
    put16(rd, 0); put16(rd, 0); put16(rd, port);
    putName(rd, host);
    put16(b, (uint16_t)rd.size());
    b.insert(b.end(), rd.begin(), rd.end());
}
static void addTxt(std::vector<uint8_t>& b, const std::string& instLabel, const std::string& instType,
                   const std::vector<std::string>& kv, uint32_t ttl) {
    putInstName(b, instLabel, instType);
    put16(b, 16);           // TYPE TXT
    put16(b, 0x8001);
    put32(b, ttl);
    std::vector<uint8_t> rd;
    for (auto& s : kv) { rd.push_back((uint8_t)s.size()); rd.insert(rd.end(), s.begin(), s.end()); }
    if (rd.empty()) rd.push_back(0);
    put16(b, (uint16_t)rd.size());
    b.insert(b.end(), rd.begin(), rd.end());
}

// Full announcement: A + PTR + SRV + TXT. ttl 0 makes it a goodbye packet.
static std::vector<uint8_t> buildAnnounce(uint32_t ttl) {
    const std::string svcType = "_http._tcp.local";
    const std::string instLbl = "localTransfer";      // one label: no dots
    const uint32_t    ip      = primaryIPv4();

    std::vector<uint8_t> b;
    put16(b, 0);            // ID 0 for responses
    put16(b, 0x8400);       // QR=1, AA=1
    put16(b, 0);            // QDCOUNT
    put16(b, (uint16_t)(g_mdnsNames.size() + 3));   // A per name + PTR + SRV + TXT
    put16(b, 0); put16(b, 0);
    for (const auto& n : g_mdnsNames) addA(b, n, ip, ttl);
    addPtr(b, svcType, instLbl, svcType, ttl);
    addSrv(b, instLbl, svcType, mdnsFullName(), (uint16_t)g_mdnsPort, ttl);
    addTxt(b, instLbl, svcType, { "path=/", "app=localTransfer.io" }, ttl);
    return b;
}

// Answer to a specific A question.
static std::vector<uint8_t> buildAReply(uint16_t id, const std::string& askedFor) {
    std::vector<uint8_t> b;
    put16(b, id);
    put16(b, 0x8400);
    put16(b, 0);
    put16(b, 1);
    put16(b, 0); put16(b, 0);
    addA(b, askedFor, primaryIPv4(), 120);   // echo the exact name queried
    return b;
}

// ── responder loop ──────────────────────────────────────────────
static void mdnsLoop() {
    sockaddr_in grp{};
    grp.sin_family = AF_INET;
    grp.sin_port   = htons(MDNS_PORT);
    inet_pton(AF_INET, MDNS_GROUP, &grp.sin_addr);

    auto sendTo = [&](const std::vector<uint8_t>& pkt, const sockaddr_in& dst) {
        if (g_mdnsSock == INVALID_SOCKET || pkt.empty()) return;
        sendto(g_mdnsSock, (const char*)pkt.data(), (int)pkt.size(), 0,
               (const sockaddr*)&dst, sizeof(dst));
    };

    // Announce twice a second apart, as recommended, so caches pick us up
    // without waiting for a query.
    sendTo(buildAnnounce(120), grp);
    Sleep(900);
    if (g_mdnsRun) sendTo(buildAnnounce(120), grp);

    const std::string svcType = "_http._tcp.local";

    std::vector<uint8_t> buf(2048);
    int lastAnnounce = 0;

    while (g_mdnsRun) {
        fd_set rd; FD_ZERO(&rd); FD_SET(g_mdnsSock, &rd);
        timeval tv{1, 0};
        int r = select(0, &rd, nullptr, nullptr, &tv);
        if (!g_mdnsRun) break;

        // Re-announce every ~60 s so late joiners find us.
        if (++lastAnnounce >= 60) { lastAnnounce = 0; sendTo(buildAnnounce(120), grp); }

        if (r <= 0 || !FD_ISSET(g_mdnsSock, &rd)) continue;

        sockaddr_in from{}; int fromLen = sizeof(from);
        int n = recvfrom(g_mdnsSock, (char*)buf.data(), (int)buf.size(), 0,
                         (sockaddr*)&from, &fromLen);
        if (n < 12) continue;

        const uint8_t* p = buf.data();
        uint16_t id      = (p[0] << 8) | p[1];
        uint16_t flags   = (p[2] << 8) | p[3];
        uint16_t qdcount = (p[4] << 8) | p[5];
        if (flags & 0x8000) continue;               // a response, not a query

        size_t off = 12;
        bool wantA = false, wantSvc = false, unicast = false;
        std::string asked;

        for (uint16_t q = 0; q < qdcount && off < (size_t)n; ++q) {
            std::string qname;
            if (!readName(p, (size_t)n, off, qname)) break;
            if (off + 4 > (size_t)n) break;
            uint16_t qtype  = (p[off] << 8) | p[off + 1];
            uint16_t qclass = (p[off + 2] << 8) | p[off + 3];
            off += 4;
            if (qclass & 0x8000) unicast = true;     // QU bit

            if (qtype == 1 || qtype == 255) {
                for (const auto& n : g_mdnsNames) {
                    if (iequals(qname, n)) { wantA = true; asked = qname; break; }
                }
            }
            if (iequals(qname, svcType) && (qtype == 12 || qtype == 255)) wantSvc = true;
        }

        if (!wantA && !wantSvc) continue;

        std::vector<uint8_t> reply = wantSvc ? buildAnnounce(120) : buildAReply(id, asked);
        sendTo(reply, grp);
        if (unicast) sendTo(reply, from);
        Log(L_VERB, "mDNS: answered a query for " + (wantSvc ? svcType : asked));
    }

    // Goodbye: TTL 0 tells resolvers to drop us immediately.
    sendTo(buildAnnounce(0), grp);
}

// ── lifecycle ───────────────────────────────────────────────────
void mdnsStart(int servicePort) {
    if (g_mdnsRun) return;
    g_mdnsPort = servicePort;

    g_mdnsSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_mdnsSock == INVALID_SOCKET) { Log(L_WARN, "mDNS: socket() failed; .local name unavailable"); return; }

    // Several responders share 5353 (Windows itself runs one), so this must
    // not be exclusive.
    BOOL yes = TRUE;
    setsockopt(g_mdnsSock, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port        = htons(MDNS_PORT);
    if (bind(g_mdnsSock, (sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        Log(L_WARN, "mDNS: could not bind UDP 5353; .local name unavailable");
        closesocket(g_mdnsSock); g_mdnsSock = INVALID_SOCKET; return;
    }

    ip_mreq mreq{};
    inet_pton(AF_INET, MDNS_GROUP, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (setsockopt(g_mdnsSock, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) == SOCKET_ERROR) {
        Log(L_WARN, "mDNS: could not join 224.0.0.251; .local name unavailable");
        closesocket(g_mdnsSock); g_mdnsSock = INVALID_SOCKET; return;
    }
    int ttl = 255;   // required by RFC 6762
    setsockopt(g_mdnsSock, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&ttl, sizeof(ttl));

    g_mdnsRun = true;
    g_mdnsThread = new std::thread(mdnsLoop);
    const std::string suffix = (servicePort == 80 ? "/" : (":" + std::to_string(servicePort) + "/"));
    Log(L_OK, "mDNS: http://" + mdnsFullName() + suffix);
    std::string others;
    for (size_t i = 1; i < g_mdnsNames.size(); ++i) others += (others.empty() ? "" : ", ") + g_mdnsNames[i];
    if (!others.empty()) Log(L_INFO, "  also answering: " + others);
    // Measured, not assumed: Windows resolves only single-label .local names,
    // so the dotted form works on Apple devices but not from a Windows box.
    Log(L_INFO, "  Apple devices resolve all of the above.");
    Log(L_INFO, "  Windows resolves only the single-label names (localtransfer-io.local).");
    Log(L_INFO, "  Chrome on Android resolves none of them — use http://<ip>" + suffix + " there.");
}

void mdnsStop() {
    if (!g_mdnsRun) return;
    g_mdnsRun = false;
    if (g_mdnsSock != INVALID_SOCKET) { closesocket(g_mdnsSock); g_mdnsSock = INVALID_SOCKET; }
    if (g_mdnsThread) { g_mdnsThread->join(); delete g_mdnsThread; g_mdnsThread = nullptr; }
}
