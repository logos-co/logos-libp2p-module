#include "plugin.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <sstream>
#include <thread>

using json = nlohmann::json;

namespace {
std::string defaultListenAddr(TransportType transport) {
    if (transport == TRANSPORT_TYPE_QUIC) {
        return "/ip4/127.0.0.1/udp/0/quic-v1";
    }
    return "/ip4/127.0.0.1/tcp/0";
}

// Tears a context down and reports a failed teardown. The context is gone
// either way, but a non-OK code means the Nim side could not stop cleanly and
// its worker threads may still be live, which is worth a line in the log.
void destroyContextChecked(LibP2PCtx* c) {
    int rc = libp2p_ctx_destroy(c);
    if (rc != NIMFFI_RET_OK) {
        fprintf(stderr, "libp2p_module: libp2p_ctx_destroy failed (rc=%d)\n", rc);
    }
}

// Pulls the port out of a multiaddr (the segment after /tcp/ or /udp/).
bool extractPort(const std::string& multiaddr, int& out) {
    std::stringstream ss(multiaddr);
    std::string token, prev;
    while (std::getline(ss, token, '/')) {
        if ((prev == "tcp" || prev == "udp") && !token.empty()) {
            try {
                out = std::stoi(token);
                return true;
            } catch (...) {
                return false;
            }
        }
        prev = token;
    }
    return false;
}

// A create that times out leaves the reply pending, and a late reply hands back
// a context nobody owns. Keep the future alive on a detached thread so the
// context still gets destroyed if it ever arrives.
void reapLateContext(std::future<SyncResult> f) {
    std::thread([f = std::move(f)]() mutable {
        auto r = f.get();
        if (r.newCtx) destroyContextChecked(r.newCtx);
    }).detach();
}

constexpr char kModuleVersion[] = "1.0.0";

std::atomic<int64_t> g_requestedLogLevel{LOG_LEVEL_DEBUG};
}

void Libp2pModuleImpl::publishEmitEvent() {
    std::unique_lock<std::shared_mutex> lock(m_emitEventLock);
    m_emitEventSnapshot = emitEvent;
}

void Libp2pModuleImpl::emitEventSafe(const std::string& name, const std::string& data) const {
    EmitEventFn fn;
    {
        std::shared_lock<std::shared_mutex> lock(m_emitEventLock);
        fn = m_emitEventSnapshot;
    }
    if (!fn) {
        return;
    }
    fn(name, data);
}

Libp2pModuleImpl::Libp2pModuleImpl(const Libp2pModuleOptions& options)
    : ctx(nullptr)
{
    applyOptions(options);
}

void Libp2pModuleImpl::applyOptions(const Libp2pModuleOptions& options) {
    m_libp2pConfig = Libp2pConfig{};
    m_addrs.clear();
    m_addrsFfi.clear();
    m_bootstrapPeerIds.clear();
    m_bootstrapAddrs.clear();
    m_bootstrapAddrsFfi.clear();
    m_bootstrapNodes.clear();

    m_privKey.assign(options.privKey.begin(), options.privKey.end());

    m_libp2pConfig.mountGossipsub = options.mountGossipsub;
    m_libp2pConfig.gossipsubTriggerSelf = options.gossipsubTriggerSelf;
    m_libp2pConfig.mountKad = options.mountKad;
    m_libp2pConfig.mountServiceDiscovery = options.mountServiceDiscovery;

    m_libp2pConfig.muxer = MUXER_TYPE_MPLEX;
    m_libp2pConfig.transport = options.transport;

    m_libp2pConfig.maxConnections = options.maxConnections;
    m_libp2pConfig.maxIn = options.maxInConnections;
    m_libp2pConfig.maxOut = options.maxOutConnections;
    m_libp2pConfig.maxConnsPerPeer = options.maxConnsPerPeer;

    m_libp2pConfig.circuitRelay = options.circuitRelay;
    m_libp2pConfig.circuitRelayClient = options.circuitRelayClient;
    m_libp2pConfig.autonat = options.autonat;
    m_libp2pConfig.autonatV2 = options.autonatV2;
    m_libp2pConfig.autonatV2Server = options.autonatV2Server;

    m_addrs = options.addrs;
    if (m_addrs.empty()) {
        m_addrs.push_back(defaultListenAddr(options.transport));
    }
    m_addrsFfi = toNimFfiStrs(m_addrs);
    m_libp2pConfig.addrs = LibP2PSeq_Str{m_addrsFfi.data(), m_addrsFfi.size()};

    if (!options.bootstrapNodes.empty()) {
        const size_t n = options.bootstrapNodes.size();
        m_bootstrapPeerIds.reserve(n);
        m_bootstrapAddrs.reserve(n);
        m_bootstrapAddrsFfi.reserve(n);
        m_bootstrapNodes.reserve(n);

        for (const auto& [peerId, addrs] : options.bootstrapNodes) {
            m_bootstrapPeerIds.push_back(peerId);
            m_bootstrapAddrs.push_back(addrs);
        }
        for (size_t i = 0; i < n; ++i) {
            m_bootstrapAddrsFfi.push_back(toNimFfiStrs(m_bootstrapAddrs[i]));
        }
        for (size_t i = 0; i < n; ++i) {
            BootstrapNode node{};
            node.peerId = nimffi_str(m_bootstrapPeerIds[i].c_str());
            node.multiaddrs =
                LibP2PSeq_Str{m_bootstrapAddrsFfi[i].data(), m_bootstrapAddrsFfi[i].size()};
            m_bootstrapNodes.push_back(node);
        }
        m_libp2pConfig.bootstrapNodes =
            LibP2PSeq_BootstrapNode{m_bootstrapNodes.data(), m_bootstrapNodes.size()};
    }
}

SyncResult Libp2pModuleImpl::spawnContext(Libp2pConfig& cfg) {
    auto* p = new SyncPromise();
    auto f = p->get_future();

    int ret = libp2p_ctx_create(&cfg, &Libp2pModuleImpl::cbCreate, p);
    if (ret != 0) {
        if (f.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            delete p;
        }
        SyncResult r;
        r.message = "failed to submit (ret=" + std::to_string(ret) + ")";
        return r;
    }

    auto r = awaitResult(f, kNewContextTimeoutMs);
    if (!r.ok) {
        if (f.valid()) reapLateContext(std::move(f));
        return r;
    }
    if (!r.newCtx) {
        r.ok = false;
        r.message = "no context returned";
    }
    return r;
}

StdLogosResult Libp2pModuleImpl::createContext() {
    m_initError.clear();

    // An empty privKey leaves the seq nil, so the Nim side generates a fresh
    // identity; a supplied key gives a stable peer id across restarts.
    m_libp2pConfig.privKey =
        NimFfiBytes{m_privKey.empty() ? nullptr : m_privKey.data(), m_privKey.size()};
    m_libp2pConfig.logLevel = g_requestedLogLevel.load();

    auto r = spawnContext(m_libp2pConfig);
    if (!r.ok) {
        m_initError = "libp2p_ctx_create failed: " + r.message;
        fprintf(stderr, "libp2p_module: %s\n", m_initError.c_str());
        return {false, {}, m_initError};
    }

    ctx = r.newCtx;

    // Register listeners before start so incoming protocol streams and pubsub
    // messages are delivered once the node is running. Destroying the context
    // frees the listener boxes.
    libp2p_ctx_add_on_incoming_stream_listener(ctx, &Libp2pModuleImpl::onIncomingStream, this);
    libp2p_ctx_add_on_pubsub_message_listener(ctx, &Libp2pModuleImpl::onPubsubMessage, this);

    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::createNode(const std::string& config) {
    bool ok = false;
    std::string err;
    auto options = Libp2pModuleOptions::fromJson(config, ok, &err);
    if (!ok) {
        std::string msg = "createNode: invalid config: " + err;
        fprintf(stderr, "libp2p_module: %s\n", msg.c_str());
        return {false, {}, msg};
    }
    if (ctx) {
        std::string msg = "createNode: node already created";
        fprintf(stderr, "libp2p_module: %s\n", msg.c_str());
        return {false, {}, msg};
    }
    applyOptions(options);
    return createContext();
}

void Libp2pModuleImpl::setLogLevel(LogLevel level) {
    g_requestedLogLevel.store(static_cast<int64_t>(level));
}

void Libp2pModuleImpl::destroyContext() {
    if (!ctx) {
        return;
    }
    // Synchronous: runs the Nim destructor (which drops the node and its
    // streams) and frees the C-side context wrapper and listener boxes.
    destroyContextChecked(ctx);
    ctx = nullptr;
}

Libp2pModuleImpl::~Libp2pModuleImpl() {
    try {
        destroyContext();
    } catch (...) {}
}

StdLogosResult Libp2pModuleImpl::start() {
    if (!ctx) {
        auto created = createContext();
        if (!created.success) return created;
    }
    publishEmitEvent();
    return callSync("Failed to start libp2p", [&](SyncPromise* p) {
        return libp2p_ctx_start(ctx, &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::stop() {
    return callSync("Failed to stop libp2p", [&](SyncPromise* p) {
        return libp2p_ctx_stop(ctx, &Libp2pModuleImpl::cbBool, p);
    });
}

bool Libp2pModuleImpl::ok() {
    return ctx != nullptr;
}

StdLogosResult Libp2pModuleImpl::status() {
    if (ctx) return {true, {}, ""};
    return {false, {}, m_initError.empty() ? "libp2p not initialized" : m_initError};
}

StdLogosResult Libp2pModuleImpl::newPrivateKey(const std::string& scheme) {
    KeyScheme parsed{};
    if (!parseKeyScheme(scheme, parsed)) {
        return {false, {}, "Unknown key scheme '" + scheme +
                           "'; expected rsa, ed25519, secp256k1 or ecdsa"};
    }
    if (!ctx) {
        auto created = createContext();
        if (!created.success) return created;
    }
    NewPrivateKeyRequest req{};
    req.scheme = static_cast<int64_t>(parsed);
    return callSyncWith("Failed to generate private key",
        [&](SyncPromise* p) {
            return libp2p_ctx_new_private_key(ctx, &req, &Libp2pModuleImpl::cbBytes, p);
        },
        [](const SyncResult& r) -> StdLogosResult {
            return {true, hexEncode(r.buffer.data(), r.buffer.size()), ""};
        });
}

StdLogosResult Libp2pModuleImpl::publicKey() {
    return callSyncWith("Failed to get public key",
        [&](SyncPromise* p) {
            return libp2p_ctx_public_key(ctx, &Libp2pModuleImpl::cbBytes, p);
        },
        bufferToResult);
}

StdLogosResult Libp2pModuleImpl::toCid(const std::string& key) {
    if (key.empty()) return {false, {}, "Key is empty"};
    CreateCidRequest req{};
    req.version = CID_VERSION_V1;
    req.multicodec = nimffi_str("dag-pb");
    req.hash = nimffi_str("sha2-256");
    req.data = nimffiBytes(key);
    return callStaticWith("Failed to create CID",
        [&](SyncPromise* p) {
            return libp2p_static_create_cid(&req, &Libp2pModuleImpl::cbStr, p);
        },
        [](const SyncResult& r) -> StdLogosResult {
            return {true, r.message, ""};
        });
}

StdLogosResult Libp2pModuleImpl::connectPeer(
    const std::string& peerId,
    const std::vector<std::string>& multiaddrs,
    int64_t timeoutMs)
{
    auto addrsFfi = toNimFfiStrs(multiaddrs);

    ConnectRequest req{};
    req.peerId = nimffi_str(peerId.c_str());
    req.multiaddrs = LibP2PSeq_Str{addrsFfi.data(), addrsFfi.size()};
    req.timeoutMs = timeoutMs;

    return callSync("Failed to connect", [&](SyncPromise* p) {
        return libp2p_ctx_connect(ctx, &req, &Libp2pModuleImpl::cbBool, p);
    }, awaitTimeoutFor(timeoutMs));
}

StdLogosResult Libp2pModuleImpl::disconnectPeer(const std::string& peerId) {
    return callSync("Failed to disconnect", [&](SyncPromise* p) {
        return libp2p_ctx_disconnect(ctx, nimffi_str(peerId.c_str()),
                                     &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::peerInfo() {
    return callSyncWith("Failed to get peer info",
        [&](SyncPromise* p) {
            return libp2p_ctx_peer_info(ctx, &Libp2pModuleImpl::cbPeerInfo, p);
        },
        [](const SyncResult& r) { return jsonResult(r, json::object()); });
}

StdLogosResult Libp2pModuleImpl::nodeInfoBoundPorts() {
    auto info = peerInfo();
    if (!info.success) return info;
    json ports = json::array();
    for (const auto& addr : info.value.value("addrs", json::array())) {
        int port = 0;
        if (addr.is_string() && extractPort(addr.get<std::string>(), port)) {
            ports.push_back(port);
        }
    }
    return {true, ports, ""};
}

StdLogosResult Libp2pModuleImpl::getNodeInfo(const std::string& field) {
    if (field == "Version") return {true, kModuleVersion, ""};
    if (field == "MyBoundPorts") return nodeInfoBoundPorts();
    if (field == "PeerId") {
        auto info = peerInfo();
        if (!info.success) return info;
        return {true, info.value.value("peerId", std::string{}), ""};
    }
    if (field == "Multiaddrs") {
        auto info = peerInfo();
        if (!info.success) return info;
        return {true, info.value.value("addrs", json::array()), ""};
    }
    return {false, {}, "unknown field: " + field};
}

StdLogosResult Libp2pModuleImpl::connectedPeers(int64_t direction) {
    // The binding takes a PeerDirection enum, so an out-of-range ordinal can no
    // longer reach the Nim side to be rejected there; screen it here instead.
    if (direction != PEER_DIRECTION_INBOUND && direction != PEER_DIRECTION_OUTBOUND) {
        return {false, {}, "Failed to get connected peers: invalid direction: " +
                           std::to_string(direction)};
    }
    auto dir = static_cast<PeerDirection>(direction);
    return callSyncWith("Failed to get connected peers",
        [&](SyncPromise* p) {
            return libp2p_ctx_connected_peers(ctx, dir,
                                              &Libp2pModuleImpl::cbPeers, p);
        },
        [](const SyncResult& r) { return jsonResult(r, json::array()); });
}

StdLogosResult Libp2pModuleImpl::dial(const std::string& peerId, const std::string& proto) {
    DialRequest req{};
    req.peerId = nimffi_str(peerId.c_str());
    req.proto = nimffi_str(proto.c_str());
    return callSyncWith("Failed to dial",
        [&](SyncPromise* p) {
            return libp2p_ctx_dial(ctx, &req, &Libp2pModuleImpl::cbDial, p);
        },
        [](const SyncResult& r) -> StdLogosResult {
            if (r.data.is_number()) return {true, r.data, ""};
            return {true, 0, ""};
        });
}

StdLogosResult Libp2pModuleImpl::circuitRelayReserve(
    const std::string& relayPeerId,
    const std::vector<std::string>& relayAddrs)
{
    auto addrsFfi = toNimFfiStrs(relayAddrs);

    CircuitRelayReserveRequest req{};
    req.relayPeerId = nimffi_str(relayPeerId.c_str());
    req.relayAddrs = LibP2PSeq_Str{addrsFfi.data(), addrsFfi.size()};

    return callSyncWith("Failed to reserve relay",
        [&](SyncPromise* p) {
            return libp2p_ctx_circuit_relay_reserve(ctx, &req,
                                                    &Libp2pModuleImpl::cbReservation, p);
        },
        [](const SyncResult& r) { return jsonResult(r, json::array()); });
}

StdLogosResult Libp2pModuleImpl::dialCircuitRelay(
    const std::string& dstPeerId,
    const std::string& multiaddr,
    const std::string& proto)
{
    DialCircuitRelayRequest req{};
    req.peerId = nimffi_str(dstPeerId.c_str());
    req.multiaddr = nimffi_str(multiaddr.c_str());
    req.proto = nimffi_str(proto.c_str());
    return callSyncWith("Failed to dial circuit relay",
        [&](SyncPromise* p) {
            return libp2p_ctx_dial_circuit_relay(ctx, &req,
                                                 &Libp2pModuleImpl::cbDial, p);
        },
        [](const SyncResult& r) -> StdLogosResult {
            if (r.data.is_number()) return {true, r.data, ""};
            return {true, 0, ""};
        });
}
