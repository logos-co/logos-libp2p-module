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

// False means the Nim side could not stop cleanly, so its worker threads may still be live.
bool destroyContextChecked(LibP2PCtx* c) {
    int rc = libp2p_ctx_destroy(c);
    if (rc != NIMFFI_RET_OK) {
        fprintf(stderr, "libp2p_module: libp2p_ctx_destroy failed (rc=%d)\n", rc);
        return false;
    }
    return true;
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

constexpr char kModuleVersion[] = "2.0.0";

std::atomic<int64_t> g_requestedLogLevel{LOG_LEVEL_DEBUG};
}

// Decrements under the lock: once it is released the waiter may delete the gate.
void endSubmit(ListenerGate* gate) {
    std::lock_guard<std::mutex> lock(gate->drainLock);
    if (gate->submits.fetch_sub(1) == 1) gate->drained.notify_all();
}

namespace {
bool waitSubmits(ListenerGate* gate) {
    std::unique_lock<std::mutex> lock(gate->drainLock);
    return gate->drained.wait_for(lock, std::chrono::milliseconds(kSubmitDrainMs),
                                  [&] { return gate->submits.load() == 0; });
}
}

ListenerGate* Libp2pModuleImpl::beginSubmit(LibP2PCtx*& ctx) {
    std::shared_lock<std::shared_mutex> lock(m_ctxLock);
    if (!this->ctx || !m_gate) return nullptr;
    m_gate->submits.fetch_add(1);
    ctx = this->ctx;
    return m_gate;
}

bool Libp2pModuleImpl::onEventWorker() const {
    return std::this_thread::get_id() == pipe()->workerId.load();
}

// The worker skips the wait: the delivery in progress is its own.
bool Libp2pModuleImpl::waitDelivery(bool bounded) {
    if (onEventWorker()) return true;
    auto current = pipe();
    if (!bounded) {
        std::lock_guard<std::timed_mutex> drained(current->deliverLock);
        return true;
    }
    if (current->deliverLock.try_lock_for(std::chrono::milliseconds(kSubmitDrainMs))) {
        current->deliverLock.unlock();
        return true;
    }
    fprintf(stderr, "libp2p_module: an emitEvent handler is still running; not waiting for it\n");
    return false;
}

std::shared_ptr<EventPipe> Libp2pModuleImpl::pipe() const {
    std::lock_guard<std::mutex> lock(m_pipeLock);
    return m_pipe;
}

void Libp2pModuleImpl::replaceEventWorker() {
    auto old = pipe();
    auto fresh = std::make_shared<EventPipe>();
    fresh->topics.setBounds(old->topics.maxMessages(), old->topics.maxBytes());
    fresh->deliveryFailures = old->deliveryFailures.load();
    fresh->outbox.addDropped(old->outbox.dropped());
    {
        std::unique_lock<std::shared_mutex> lock(old->emitLock);
        fresh->emitFn = std::move(old->emitFn);
        old->emitFn = {};
    }
    old->outbox.close();

    m_eventWorker.detach();
    {
        std::lock_guard<std::mutex> lock(m_pipeLock);
        m_pipe = fresh;
    }
    m_eventWorker = std::thread([fresh] { fresh->run(); });
}

void Libp2pModuleImpl::stopReleaseThread() {
    std::unique_lock<std::mutex> lock(m_releaseWorker->lock);
    m_releaseWorker->stop = true;
    m_releaseWorker->cond.notify_all();
    const bool exited = m_releaseWorker->cond.wait_for(
        lock, std::chrono::milliseconds(kSubmitDrainMs), [&] { return m_releaseWorker->exited; });
    lock.unlock();
    if (exited) {
        m_releaseThread.join();
        return;
    }
    fprintf(stderr, "libp2p_module: a stream release is still blocked in libp2p; detaching the release thread\n");
    m_releaseThread.detach();
}

void Libp2pModuleImpl::runReleases(std::shared_ptr<ReleaseWorker> worker) {
    std::unique_lock<std::mutex> lock(worker->lock);
    while (true) {
        worker->cond.wait(lock, [&] { return worker->wake || worker->stop; });
        if (worker->stop) break;
        worker->wake = false;

        LibP2PCtx* ctx = nullptr;
        ListenerGate* gate = beginSubmit(ctx);
        lock.unlock();
        if (gate) {
            SubmitScope scope{gate};
            try {
                flushReleases(gate, ctx);
            } catch (const std::exception& ex) {
                fprintf(stderr, "libp2p_module: stream release failed: %s\n", ex.what());
            } catch (...) {
                fprintf(stderr, "libp2p_module: stream release failed\n");
            }
        }
        lock.lock();
    }
    worker->exited = true;
    worker->cond.notify_all();
}

std::unique_lock<std::mutex> Libp2pModuleImpl::lockCreate() {
    if (onEventWorker()) return std::unique_lock<std::mutex>(m_createLock, std::try_to_lock);
    return std::unique_lock<std::mutex>(m_createLock);
}

void Libp2pModuleImpl::publishEmitEvent() {
    auto current = pipe();
    std::unique_lock<std::shared_mutex> lock(current->emitLock);
    if (m_closing) return;
    current->emitFn = emitEvent;
}

Libp2pModuleImpl::Libp2pModuleImpl(const Libp2pModuleOptions& options)
    : ctx(nullptr)
{
    applyOptions(options);
    m_eventWorker = std::thread([pipe = m_pipe] { pipe->run(); });
    m_releaseThread = std::thread([this, worker = m_releaseWorker] { runReleases(worker); });
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

    pipe()->topics.setBounds(options.gossipsubQueueMaxMessages,
                            options.gossipsubQueueMaxBytes);

    m_libp2pConfig.gossipsub.mount = options.mountGossipsub;
    m_libp2pConfig.gossipsub.triggerSelf = options.gossipsubTriggerSelf;
    m_libp2pConfig.gossipsub.maxMessageSize = options.gossipsubMaxMessageSize;
    m_libp2pConfig.gossipsub.overheadRateLimit.bytes = options.gossipsubOverheadRateLimitBytes;
    m_libp2pConfig.gossipsub.overheadRateLimit.intervalMs =
        options.gossipsubOverheadRateLimitIntervalMs;
    m_libp2pConfig.gossipsub.disconnectPeerAboveRateLimit =
        options.gossipsubDisconnectPeerAboveRateLimit;
    m_libp2pConfig.mountKad = options.mountKad;
    m_libp2pConfig.serviceDiscovery.mount = options.mountServiceDiscovery;
    m_libp2pConfig.serviceDiscovery.disableXprPublishing = options.mountServiceDiscovery;

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
    m_libp2pConfig.natPortMappingAuto = options.natPortMappingAuto;
    m_libp2pConfig.natPortMappingUpnp = options.natPortMappingUpnp;
    m_libp2pConfig.natPortMappingNatPmp = options.natPortMappingNatPmp;
    m_natExplicitIp = options.natExplicitIp;
    m_libp2pConfig.natExplicitIp = nimffi_str(m_natExplicitIp.c_str());
    m_libp2pConfig.natDiscoveryTimeoutMs = options.natDiscoveryTimeoutMs;
    m_libp2pConfig.natMappingTimeoutMs = options.natMappingTimeoutMs;
    m_libp2pConfig.natReachabilityV1 = options.natReachabilityV1;
    m_libp2pConfig.natReachabilityV2 = options.natReachabilityV2;
    m_libp2pConfig.natReachabilityScheduleIntervalMs =
        options.natReachabilityScheduleIntervalMs;
    m_libp2pConfig.natHolePunching = options.natHolePunching;
    m_libp2pConfig.natHolePunchingMaxNumRelays = options.natHolePunchingMaxNumRelays;
    m_libp2pConfig.natHolePunchingScheduleIntervalMs =
        options.natHolePunchingScheduleIntervalMs;

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
        SyncResult r;
        r.message = submitFailure(p, f, ret);
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

    std::unique_lock<std::shared_mutex> lock(m_ctxLock);
    ctx = r.newCtx;

    m_gate = new ListenerGate();
    m_gate->owner = this;

    // Registered before start so incoming protocol streams and pubsub messages are delivered once the node runs; destroying the context frees the listener boxes.
    libp2p_ctx_add_on_incoming_stream_listener(ctx, &Libp2pModuleImpl::onIncomingStream, m_gate);
    libp2p_ctx_add_on_pubsub_message_listener(ctx, &Libp2pModuleImpl::onPubsubMessage, m_gate);
    // The binding has no typed wrapper for nim-ffi's liveness event.
    libp2p_add_event_listener(ctx->ptr, "not_responding", &Libp2pModuleImpl::onNotResponding, m_gate);

    return {true, {}, ""};
}

bool Libp2pModuleImpl::hasCtx() const {
    std::shared_lock<std::shared_mutex> lock(m_ctxLock);
    return ctx != nullptr;
}

StdLogosResult Libp2pModuleImpl::ensureContext() {
    if (hasCtx()) return {true, {}, ""};
    auto lock = lockCreate();
    if (!lock.owns_lock()) return {false, {}, "a create or rebuild is in progress"};
    if (m_closing) return {false, {}, "module is shutting down"};
    if (hasCtx()) return {true, {}, ""};
    return createContext();
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
    auto lock = lockCreate();
    if (!lock.owns_lock()) return {false, {}, "createNode: a create or rebuild is in progress"};
    if (m_closing) return {false, {}, "createNode: module is shutting down"};
    if (hasCtx()) {
        std::string msg = "createNode: node already created";
        fprintf(stderr, "libp2p_module: %s\n", msg.c_str());
        return {false, {}, msg};
    }
    applyOptions(options);
    return createContext();
}

StdLogosResult Libp2pModuleImpl::rebuild() {
    auto lock = lockCreate();
    if (!lock.owns_lock()) return {false, {}, "rebuild: a create or rebuild is in progress"};
    if (m_closing) return {false, {}, "rebuild: module is shutting down"};
    if (!destroyContext()) replaceEventWorker();
    return createContext();
}

StdLogosResult Libp2pModuleImpl::setLogLevel(const std::string& level) {
    LogLevel parsed{};
    if (!parseLogLevel(level, parsed)) {
        return {false, {}, "Unknown log level '" + level +
                           "'; expected none, trace, debug, info, notice, warn, error or fatal"};
    }
    g_requestedLogLevel.store(static_cast<int64_t>(parsed));
    return {true, {}, ""};
}

// Waits out a listener that is already running, and stops any later one before it touches the module. A listener holds the gate lock only while it copies into the C++ queues, never across a submit, so the wait is short.
void Libp2pModuleImpl::detachListeners(ListenerGate* gate) {
    std::unique_lock<std::shared_mutex> lock(gate->lock);
    gate->owner = nullptr;
}

bool Libp2pModuleImpl::destroyContext() {
    ListenerGate* gate = nullptr;
    LibP2PCtx* doomed = nullptr;
    {
        std::unique_lock<std::shared_mutex> lock(m_ctxLock);
        gate = m_gate;
        doomed = ctx;
        m_gate = nullptr;
        ctx = nullptr;
    }
    if (!gate) {
        return true;
    }

    const bool drained = waitSubmits(gate);

    // Before the teardown, so the Nim event thread never sits in a module callback while the teardown waits to join it.
    detachListeners(gate);
    m_stuck = false;

    // A submit still blocked in nim-ffi waits on this context's signals, so freeing them would be a use-after-free.
    if (!drained) {
        fprintf(stderr, "libp2p_module: a call is still blocked in libp2p; abandoning the context\n");
    }

    // Synchronous: runs the Nim destructor (dropping the node and its streams) and frees the C-side context wrapper and listener boxes.
    const bool stopped = drained && destroyContextChecked(doomed);

    // A failed or abandoned teardown leaves the listener boxes, and the thread that reads them, alive: the gate they point at has to stay.
    if (stopped) {
        delete gate;
    }

    pipe()->outbox.clear();
    const bool delivered = waitDelivery(true);
    pipe()->topics.releaseAll();
    m_inboundStreams.releaseAll();
    return delivered;
}

Libp2pModuleImpl::~Libp2pModuleImpl() {
    m_closing = true;
    {
        // A handler still running reads its own copy of emitFn; clearing it here stops any later callback, including eventDeliveryFailed.
        auto current = pipe();
        std::unique_lock<std::shared_mutex> lock(current->emitLock);
        current->emitFn = {};
    }
    stopReleaseThread();
    try {
        auto lock = lockCreate();
        if (lock.owns_lock()) destroyContext();
    } catch (...) {}
    pipe()->outbox.close();
    // A handler that never returns, or that destroys the module itself, must not hang the destructor; the shared pipe keeps what it still touches alive.
    if (!onEventWorker() && pipe()->waitExit(kSubmitDrainMs)) {
        m_eventWorker.join();
        return;
    }
    fprintf(stderr, "libp2p_module: an emitEvent handler is still running; detaching the event worker\n");
    m_eventWorker.detach();
}

StdLogosResult Libp2pModuleImpl::start() {
    auto ready = ensureContext();
    if (!ready.success) return ready;

    publishEmitEvent();
    return callSync("Failed to start libp2p", [&](LibP2PCtx* ctx, SyncPromise* p) {
        return libp2p_ctx_start(ctx, &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::stop() {
    auto res = callSync("Failed to stop libp2p", [&](LibP2PCtx* ctx, SyncPromise* p) {
        return libp2p_ctx_stop(ctx, &Libp2pModuleImpl::cbBool, p);
    });
    // A node that failed to stop is still delivering, so it keeps its backlog.
    if (res.success) {
        pipe()->outbox.clear();
        waitDelivery(false);
        pipe()->topics.releaseAll();
        m_inboundStreams.releaseAll();
    }
    return res;
}

bool Libp2pModuleImpl::ok() {
    return hasCtx() && !m_stuck;
}

StdLogosResult Libp2pModuleImpl::status() {
    if (m_stuck) return {false, {}, "libp2p not responding; call rebuild, then start"};
    if (hasCtx()) return {true, {}, ""};
    return {false, {}, m_initError.empty() ? "libp2p not initialized" : m_initError};
}

StdLogosResult Libp2pModuleImpl::newPrivateKey(const std::string& scheme) {
    KeyScheme parsed{};
    if (!parseKeyScheme(scheme, parsed)) {
        return {false, {}, "Unknown key scheme '" + scheme +
                           "'; expected rsa, ed25519, secp256k1 or ecdsa"};
    }
    NewPrivateKeyRequest req{};
    req.scheme = static_cast<int64_t>(parsed);
    return callStaticWith("Failed to generate private key",
        [&](SyncPromise* p) {
            return libp2p_static_new_private_key(&req, &Libp2pModuleImpl::cbBytes, p);
        },
        [](const SyncResult& r) -> StdLogosResult {
            return {true, hexEncode(r.buffer.data(), r.buffer.size()), ""};
        });
}

StdLogosResult Libp2pModuleImpl::publicKey() {
    return callSyncWith("Failed to get public key",
        [&](LibP2PCtx* ctx, SyncPromise* p) {
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

    return callSync("Failed to connect", [&](LibP2PCtx* ctx, SyncPromise* p) {
        return libp2p_ctx_connect(ctx, &req, &Libp2pModuleImpl::cbBool, p);
    }, awaitTimeoutFor(timeoutMs));
}

StdLogosResult Libp2pModuleImpl::disconnectPeer(const std::string& peerId) {
    return callSync("Failed to disconnect", [&](LibP2PCtx* ctx, SyncPromise* p) {
        return libp2p_ctx_disconnect(ctx, nimffi_str(peerId.c_str()),
                                     &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::peerInfo() {
    return callSyncWith("Failed to get peer info",
        [&](LibP2PCtx* ctx, SyncPromise* p) {
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
        [&](LibP2PCtx* ctx, SyncPromise* p) {
            return libp2p_ctx_connected_peers(ctx, dir,
                                              &Libp2pModuleImpl::cbPeers, p);
        },
        [](const SyncResult& r) { return jsonResult(r, json::array()); });
}

StdLogosResult Libp2pModuleImpl::dial(const std::string& peerId, const std::string& proto) {
    return dialWithAddrs(peerId, {}, proto, false, 0);
}

StdLogosResult Libp2pModuleImpl::dialWithAddrs(
    const std::string& peerId,
    const std::vector<std::string>& multiaddrs,
    const std::string& proto,
    bool forceDial,
    int64_t timeoutMs)
{
    auto addrsFfi = toNimFfiStrs(multiaddrs);

    DialRequest req{};
    req.peerId = nimffi_str(peerId.c_str());
    req.proto = nimffi_str(proto.c_str());
    req.multiaddrs = LibP2PSeq_Str{addrsFfi.data(), addrsFfi.size()};
    req.forceDial = forceDial;
    req.timeoutMs = timeoutMs;

    return callSyncWith("Failed to dial",
        [&](LibP2PCtx* ctx, SyncPromise* p) {
            return libp2p_ctx_dial(ctx, &req, &Libp2pModuleImpl::cbDial, p);
        },
        [](const SyncResult& r) -> StdLogosResult {
            if (r.data.is_number()) return {true, r.data, ""};
            return {true, 0, ""};
        },
        awaitTimeoutFor(timeoutMs));
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
        [&](LibP2PCtx* ctx, SyncPromise* p) {
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
        [&](LibP2PCtx* ctx, SyncPromise* p) {
            return libp2p_ctx_dial_circuit_relay(ctx, &req,
                                                 &Libp2pModuleImpl::cbDial, p);
        },
        [](const SyncResult& r) -> StdLogosResult {
            if (r.data.is_number()) return {true, r.data, ""};
            return {true, 0, ""};
        });
}
