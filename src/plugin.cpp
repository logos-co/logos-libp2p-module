#include "plugin.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Promise-based callbacks — each resolves a heap-allocated SyncPromise
// ---------------------------------------------------------------------------

void Libp2pModuleImpl::promiseCallback(int ret, const char* msg, size_t len, void* userData) {
    auto* p = static_cast<SyncPromise*>(userData);
    SyncResult r;
    r.ok = (ret == RET_OK);
    r.message = (msg && len > 0) ? std::string(msg, len) : std::string();
    p->set_value(std::move(r));
    delete p;
}

void Libp2pModuleImpl::promiseBufferCallback(int ret, const uint8_t* data, size_t dataLen,
                                             const char* msg, size_t len, void* userData) {
    auto* p = static_cast<SyncPromise*>(userData);
    SyncResult r;
    r.ok = (ret == RET_OK);
    r.message = (msg && len > 0) ? std::string(msg, len) : std::string();
    if (data && dataLen > 0) {
        r.buffer.assign(data, data + dataLen);
    }
    p->set_value(std::move(r));
    delete p;
}

void Libp2pModuleImpl::emitEventSafe(const std::string& name, const std::string& data) const {
    if (emitEvent) {
        emitEvent(name, data);
    }
}

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

Libp2pModuleImpl::Libp2pModuleImpl(const Libp2pModuleOptions& options)
    : ctx(nullptr)
{
    std::memset(&m_libp2pConfig, 0, sizeof(m_libp2pConfig));

    m_libp2pConfig.mount_gossipsub = options.mountGossipsub ? 1 : 0;
    m_libp2pConfig.gossipsub_trigger_self = options.gossipsubTriggerSelf ? 1 : 0;

    m_libp2pConfig.max_connections    = options.maxConnections;
    m_libp2pConfig.max_in             = options.maxInConnections;
    m_libp2pConfig.max_out            = options.maxOutConnections;
    m_libp2pConfig.max_conns_per_peer = options.maxConnsPerPeer;

    m_libp2pConfig.autonat = options.autonat ? 1 : 0;
    m_libp2pConfig.autonat_v2 = options.autonatV2 ? 1 : 0;
    m_libp2pConfig.autonat_v2_server = options.autonatV2Server ? 1 : 0;
    m_libp2pConfig.circuit_relay = options.circuitRelay ? 1 : 0;
    m_libp2pConfig.circuit_relay_client = options.circuitRelayClient ? 1 : 0;

    m_libp2pConfig.transport = options.transport;

    // Listen addresses
    m_addrs = options.addrs;
    if (!m_addrs.empty()) {
        m_addrsPtr.reserve(m_addrs.size());
        for (const auto& addr : m_addrs) {
            m_addrsPtr.push_back(addr.c_str());
        }
        m_libp2pConfig.addrs = m_addrsPtr.data();
        m_libp2pConfig.addrsLen = static_cast<int>(m_addrsPtr.size());
    }

    // Bootstrap nodes
    if (!options.bootstrapNodes.empty()) {
        m_peerIdStorage.reserve(options.bootstrapNodes.size());
        m_addrStorage.reserve(options.bootstrapNodes.size());
        m_addrPtrStorage.reserve(options.bootstrapNodes.size());
        m_bootstrapCNodes.reserve(options.bootstrapNodes.size());

        for (const auto& [peerId, addrs] : options.bootstrapNodes) {
            m_peerIdStorage.push_back(peerId);
            m_addrStorage.push_back(addrs);

            std::vector<const char*> ptrs;
            ptrs.reserve(addrs.size());
            for (const auto& a : m_addrStorage.back()) {
                ptrs.push_back(a.c_str());
            }
            m_addrPtrStorage.push_back(std::move(ptrs));

            libp2p_bootstrap_node_t node{};
            node.peerId = m_peerIdStorage.back().c_str();
            node.multiaddrs = m_addrPtrStorage.back().data();
            node.multiaddrsLen = static_cast<int>(m_addrPtrStorage.back().size());
            m_bootstrapCNodes.push_back(node);
        }

        m_libp2pConfig.kad_bootstrap_nodes = m_bootstrapCNodes.data();
        m_libp2pConfig.kad_bootstrap_nodes_len = static_cast<int>(m_bootstrapCNodes.size());
    }

    m_libp2pConfig.mount_kad = options.mountKad ? 1 : 0;
    m_libp2pConfig.mount_service_discovery = options.mountServiceDiscovery ? 1 : 0;

    // Generate private key
    auto keyResult = newPrivateKey();
    if (!keyResult.success) {
        fprintf(stderr, "libp2p_new_private_key failed: %s\n", keyResult.error.c_str());
        return;
    }
    std::string keyStr = keyResult.value.get<std::string>();
    m_privKey.assign(keyStr.begin(), keyStr.end());

    m_libp2pConfig.priv_key.data = m_privKey.data();
    m_libp2pConfig.priv_key.dataLen = static_cast<int>(m_privKey.size());

    // Call libp2p_new
    auto* p = new SyncPromise();
    auto f = p->get_future();

    ctx = libp2p_new(&m_libp2pConfig, &Libp2pModuleImpl::promiseCallback, p);

    auto r = awaitResult(f, 5000);
    if (!r.ok) {
        fprintf(stderr, "libp2p_new failed: %s\n", r.message.c_str());
    }

    if (!ctx) {
        fprintf(stderr, "libp2p_new returned null context\n");
    }
}

// ---------------------------------------------------------------------------
// Destructor
// ---------------------------------------------------------------------------

Libp2pModuleImpl::~Libp2pModuleImpl() {
    // Release all streams
    std::vector<uint64_t> streamIds;
    {
        std::shared_lock<std::shared_mutex> lock(m_streamsLock);
        for (const auto& [id, _] : m_streams) {
            streamIds.push_back(id);
        }
    }
    for (uint64_t sid : streamIds) {
        streamRelease(sid);
    }

    // Destroy libp2p context
    if (ctx) {
        auto* p = new SyncPromise();
        auto f = p->get_future();

        libp2p_destroy(ctx, &Libp2pModuleImpl::promiseCallback, p);

        awaitResult(f, 5000);
        ctx = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

StdLogosResult Libp2pModuleImpl::start() {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_start(ctx, &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to start libp2p"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::stop() {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_stop(ctx, &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to stop libp2p"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::publicKey() {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_public_key(ctx, &Libp2pModuleImpl::promiseBufferCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to get public key"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, std::string(r.buffer.begin(), r.buffer.end()), ""};
}

StdLogosResult Libp2pModuleImpl::newPrivateKey() {
    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_new_private_key(LIBP2P_PK_SECP256K1,
                                     &Libp2pModuleImpl::promiseBufferCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to generate private key"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, std::string(r.buffer.begin(), r.buffer.end()), ""};
}

// ---------------------------------------------------------------------------
// Helper: toCid
// ---------------------------------------------------------------------------

StdLogosResult Libp2pModuleImpl::toCid(const std::string& key) {
    if (key.empty()) return {false, {}, "Key is empty"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_create_cid(
        1, "dag-pb", "sha2-256",
        reinterpret_cast<const uint8_t*>(key.data()), key.size(),
        &Libp2pModuleImpl::promiseCallback, p
    );
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to create CID"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, r.message, ""};
}

// ---------------------------------------------------------------------------
// Event Callback
// ---------------------------------------------------------------------------

void Libp2pModuleImpl::eventCallback(int ret, const char* msg, size_t len, void* userData) {
    auto* self = static_cast<Libp2pModuleImpl*>(userData);
    if (!self) return;

    std::string message = (msg && len > 0) ? std::string(msg, len) : std::string();
    json j;
    j["result"] = ret;
    j["message"] = message;
    self->emitEventSafe("libp2pEvent", j.dump());
}

bool Libp2pModuleImpl::setEventCallback() {
    if (!ctx) return false;
    libp2p_set_event_callback(ctx, &Libp2pModuleImpl::eventCallback, this);
    return true;
}

// ---------------------------------------------------------------------------
// Connectivity
// ---------------------------------------------------------------------------

StdLogosResult Libp2pModuleImpl::connectPeer(
    const std::string& peerId,
    const std::vector<std::string>& multiaddrs,
    int64_t timeoutMs)
{
    if (!ctx) return {false, {}, "No libp2p context"};

    std::vector<const char*> addrPtrs;
    addrPtrs.reserve(multiaddrs.size());
    for (const auto& addr : multiaddrs) {
        addrPtrs.push_back(addr.c_str());
    }

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_connect(ctx, peerId.c_str(), addrPtrs.data(),
                             static_cast<int>(addrPtrs.size()), timeoutMs,
                             &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to connect"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::disconnectPeer(const std::string& peerId) {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_disconnect(ctx, peerId.c_str(),
                                &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to disconnect"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::peerInfo() {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_peerinfo(ctx, &Libp2pModuleImpl::promisePeerInfoCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to get peer info (ret=" + std::to_string(ret) + ")"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return parseJsonResponse(r.message, "peerInfo");
}

StdLogosResult Libp2pModuleImpl::connectedPeers(int direction) {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_connected_peers(ctx, direction,
                                     &Libp2pModuleImpl::promisePeersCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to get connected peers (ret=" + std::to_string(ret) + ")"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    if (r.message.empty()) return {true, json::array(), ""};
    return parseJsonResponse(r.message, "connectedPeers");
}

StdLogosResult Libp2pModuleImpl::dial(const std::string& peerId, const std::string& proto) {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_dial(ctx, peerId.c_str(), proto.c_str(),
                          &Libp2pModuleImpl::promiseConnectionCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to dial"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};

    auto* stream = static_cast<libp2p_stream_t*>(r.extra);
    if (stream) {
        uint64_t streamId = addStream(stream);
        return {true, streamId, ""};
    }
    return {true, 0, ""};
}

// ---------------------------------------------------------------------------
// Circuit Relay
// ---------------------------------------------------------------------------

StdLogosResult Libp2pModuleImpl::circuitRelayReserve(
    const std::string& relayPeerId,
    const std::vector<std::string>& relayAddrs)
{
    if (!ctx) return {false, {}, "No libp2p context"};

    std::vector<const char*> addrPtrs;
    addrPtrs.reserve(relayAddrs.size());
    for (const auto& addr : relayAddrs) {
        addrPtrs.push_back(addr.c_str());
    }

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_circuit_relay_reserve(ctx, relayPeerId.c_str(),
                                           addrPtrs.data(),
                                           static_cast<int>(addrPtrs.size()),
                                           &Libp2pModuleImpl::promiseReservationCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to reserve relay (ret=" + std::to_string(ret) + ")"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    if (r.message.empty()) return {true, json::array(), ""};
    return parseJsonResponse(r.message, "circuitRelayReserve");
}

StdLogosResult Libp2pModuleImpl::dialCircuitRelay(
    const std::string& dstPeerId,
    const std::string& multiaddr,
    const std::string& proto)
{
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_dial_circuit_relay(ctx, dstPeerId.c_str(),
                                        multiaddr.c_str(), proto.c_str(),
                                        &Libp2pModuleImpl::promiseConnectionCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to dial circuit relay"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};

    auto* stream = static_cast<libp2p_stream_t*>(r.extra);
    if (stream) {
        uint64_t streamId = addStream(stream);
        return {true, streamId, ""};
    }
    return {true, 0, ""};
}
