#include "plugin.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <thread>

using json = nlohmann::json;

namespace {
std::string defaultListenAddr(int transport) {
    if (transport == LIBP2P_TRANSPORT_QUIC) {
        return "/ip4/127.0.0.1/udp/0/quic-v1";
    }
    return "/ip4/127.0.0.1/tcp/0";
}
}

void Libp2pModuleImpl::promiseCallback(int ret, const char* msg, size_t len, void* userData) {
    finishPromise(static_cast<SyncPromise*>(userData), basicResult(ret, msg, len));
}

void Libp2pModuleImpl::promiseBufferCallback(int ret, const uint8_t* data, size_t dataLen,
                                             const char* msg, size_t len, void* userData) {
    auto r = basicResult(ret, msg, len);
    if (data && dataLen > 0) {
        r.buffer.assign(data, data + dataLen);
    }
    finishPromise(static_cast<SyncPromise*>(userData), std::move(r));
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

    m_addrs = options.addrs;
    if (m_addrs.empty()) {
        m_addrs.push_back(defaultListenAddr(options.transport));
    }

    m_addrsPtr.reserve(m_addrs.size());
    for (const auto& addr : m_addrs) {
        m_addrsPtr.push_back(addr.c_str());
    }
    m_libp2pConfig.addrs = m_addrsPtr.data();
    m_libp2pConfig.addrsLen = static_cast<int>(m_addrsPtr.size());

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

    m_privKey.assign(options.privKey.begin(), options.privKey.end());
    if (m_privKey.empty()) {
        auto keyResult = generatePrivateKey(options.keyType);
        if (!keyResult.ok) {
            fprintf(stderr, "libp2p_new_private_key failed: %s\n", keyResult.message.c_str());
            return;
        }
        m_privKey.assign(keyResult.buffer.begin(), keyResult.buffer.end());
    }

    m_libp2pConfig.priv_key.data = m_privKey.data();
    m_libp2pConfig.priv_key.dataLen = static_cast<int>(m_privKey.size());

    auto* p = new SyncPromise();
    auto f = p->get_future();

    ctx = libp2p_new(&m_libp2pConfig, &Libp2pModuleImpl::promiseCallback, p);

    auto r = awaitResult(f, kNewContextTimeoutMs);
    if (!r.ok) {
        m_initError = "libp2p_new failed: " + r.message;
        fprintf(stderr, "libp2p_new failed: %s\n", r.message.c_str());
        ctx = nullptr;
    }

    if (!ctx) {
        if (m_initError.empty()) m_initError = "libp2p_new returned null context";
        fprintf(stderr, "libp2p_new returned null context\n");
    }
}

Libp2pModuleImpl::~Libp2pModuleImpl() {
    try {
        std::vector<uint64_t> streamIds;
        {
            std::shared_lock<std::shared_mutex> lock(m_streamsLock);
            streamIds.reserve(m_streams.size());
            for (const auto& [id, _] : m_streams) {
                streamIds.push_back(id);
            }
        }

        // Bounded worker pool — each streamRelease awaits up to 10s.
        if (!streamIds.empty()) {
            const size_t workers = std::min<size_t>(
                streamIds.size(),
                std::max<unsigned>(std::thread::hardware_concurrency(), 4u));

            std::atomic<size_t> next{0};
            std::vector<std::thread> pool;
            pool.reserve(workers);
            for (size_t i = 0; i < workers; ++i) {
                pool.emplace_back([this, &streamIds, &next] {
                    try {
                        for (;;) {
                            size_t idx = next.fetch_add(1, std::memory_order_relaxed);
                            if (idx >= streamIds.size()) return;
                            streamRelease(streamIds[idx]);
                        }
                    } catch (...) {}
                });
            }
            for (auto& t : pool) {
                if (t.joinable()) t.join();
            }
        }

        if (ctx) {
            auto* p = new SyncPromise();
            auto f = p->get_future();

            libp2p_destroy(ctx, &Libp2pModuleImpl::promiseCallback, p);

            awaitResult(f, kDestroyTimeoutMs);
            ctx = nullptr;
        }
    } catch (...) {}
}

StdLogosResult Libp2pModuleImpl::start() {
    publishEmitEvent();
    return callSync("Failed to start libp2p", [&](SyncPromise* p) {
        return libp2p_start(ctx, &Libp2pModuleImpl::promiseCallback, p);
    });
}

StdLogosResult Libp2pModuleImpl::stop() {
    return callSync("Failed to stop libp2p", [&](SyncPromise* p) {
        return libp2p_stop(ctx, &Libp2pModuleImpl::promiseCallback, p);
    });
}

bool Libp2pModuleImpl::ok() {
    return ctx != nullptr;
}

StdLogosResult Libp2pModuleImpl::status() {
    if (ctx) return {true, {}, ""};
    return {false, {}, m_initError.empty() ? "libp2p not initialized" : m_initError};
}

StdLogosResult Libp2pModuleImpl::publicKey() {
    return callSyncWith("Failed to get public key",
        [&](SyncPromise* p) {
            return libp2p_public_key(ctx, &Libp2pModuleImpl::promiseBufferCallback, p);
        },
        bufferToResult);
}

SyncResult Libp2pModuleImpl::generatePrivateKey(int scheme) {
    // Doesn't need a ctx, so it bypasses callSync's ctx check.
    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_new_private_key(static_cast<libp2p_pk_scheme>(scheme),
                                     &Libp2pModuleImpl::promiseBufferCallback, p);
    if (ret != RET_OK) {
        delete p;
        return {false, "Failed to generate private key (ret=" + std::to_string(ret) + ")", {}, nullptr};
    }
    return awaitResult(f);
}

StdLogosResult Libp2pModuleImpl::newPrivateKey() {
    auto r = generatePrivateKey(LIBP2P_PK_SECP256K1);
    if (!r.ok) return {false, {}, r.message};
    return bufferToHexResult(r);
}

StdLogosResult Libp2pModuleImpl::toCid(const std::string& key) {
    if (key.empty()) return {false, {}, "Key is empty"};
    return callSyncWith("Failed to create CID",
        [&](SyncPromise* p) {
            return libp2p_create_cid(
                1, "dag-pb", "sha2-256",
                reinterpret_cast<const uint8_t*>(key.data()), key.size(),
                &Libp2pModuleImpl::promiseCallback, p);
        },
        [](const SyncResult& r) -> StdLogosResult {
            return {true, r.message, ""};
        });
}

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
    publishEmitEvent();
    libp2p_set_event_callback(ctx, &Libp2pModuleImpl::eventCallback, this);
    return true;
}

StdLogosResult Libp2pModuleImpl::connectPeer(
    const std::string& peerId,
    const std::vector<std::string>& multiaddrs,
    int64_t timeoutMs)
{
    std::vector<const char*> addrPtrs;
    addrPtrs.reserve(multiaddrs.size());
    for (const auto& addr : multiaddrs) {
        addrPtrs.push_back(addr.c_str());
    }
    return callSync("Failed to connect", [&](SyncPromise* p) {
        return libp2p_connect(ctx, peerId.c_str(), addrPtrs.data(),
                              static_cast<int>(addrPtrs.size()), timeoutMs,
                              &Libp2pModuleImpl::promiseCallback, p);
    }, awaitTimeoutFor(timeoutMs));
}

StdLogosResult Libp2pModuleImpl::disconnectPeer(const std::string& peerId) {
    return callSync("Failed to disconnect", [&](SyncPromise* p) {
        return libp2p_disconnect(ctx, peerId.c_str(),
                                 &Libp2pModuleImpl::promiseCallback, p);
    });
}

StdLogosResult Libp2pModuleImpl::peerInfo() {
    return callSyncWith("Failed to get peer info",
        [&](SyncPromise* p) {
            return libp2p_peerinfo(ctx, &Libp2pModuleImpl::promisePeerInfoCallback, p);
        },
        [](const SyncResult& r) { return jsonResult(r, json::object()); });
}

StdLogosResult Libp2pModuleImpl::connectedPeers(int64_t direction) {
    return callSyncWith("Failed to get connected peers",
        [&](SyncPromise* p) {
            return libp2p_connected_peers(ctx, direction,
                                          &Libp2pModuleImpl::promisePeersCallback, p);
        },
        [](const SyncResult& r) { return jsonResult(r, json::array()); });
}

StdLogosResult Libp2pModuleImpl::dial(const std::string& peerId, const std::string& proto) {
    return callSyncWith("Failed to dial",
        [&](SyncPromise* p) {
            return libp2p_dial(ctx, peerId.c_str(), proto.c_str(),
                               &Libp2pModuleImpl::promiseConnectionCallback, p);
        },
        [this](const SyncResult& r) -> StdLogosResult {
            if (r.stream) return {true, addStream(r.stream), ""};
            return {true, 0, ""};
        });
}

StdLogosResult Libp2pModuleImpl::circuitRelayReserve(
    const std::string& relayPeerId,
    const std::vector<std::string>& relayAddrs)
{
    std::vector<const char*> addrPtrs;
    addrPtrs.reserve(relayAddrs.size());
    for (const auto& addr : relayAddrs) {
        addrPtrs.push_back(addr.c_str());
    }
    return callSyncWith("Failed to reserve relay",
        [&](SyncPromise* p) {
            return libp2p_circuit_relay_reserve(ctx, relayPeerId.c_str(),
                                                addrPtrs.data(),
                                                static_cast<int>(addrPtrs.size()),
                                                &Libp2pModuleImpl::promiseReservationCallback, p);
        },
        [](const SyncResult& r) { return jsonResult(r, json::array()); });
}

StdLogosResult Libp2pModuleImpl::dialCircuitRelay(
    const std::string& dstPeerId,
    const std::string& multiaddr,
    const std::string& proto)
{
    return callSyncWith("Failed to dial circuit relay",
        [&](SyncPromise* p) {
            return libp2p_dial_circuit_relay(ctx, dstPeerId.c_str(),
                                             multiaddr.c_str(), proto.c_str(),
                                             &Libp2pModuleImpl::promiseConnectionCallback, p);
        },
        [this](const SyncResult& r) -> StdLogosResult {
            if (r.stream) return {true, addStream(r.stream), ""};
            return {true, 0, ""};
        });
}
