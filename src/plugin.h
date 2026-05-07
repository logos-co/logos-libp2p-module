#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <queue>

#include <nlohmann/json.hpp>

#ifndef LOGOS_RESULT_H_INCLUDED
#define LOGOS_RESULT_H_INCLUDED
struct StdLogosResult {
    bool success = false;
    nlohmann::json value;
    std::string error;
};
#endif

extern "C" {
#include "lib/libp2p.h"
}

struct Libp2pModuleOptions {
    std::vector<std::string> addrs = {};
    std::vector<std::pair<std::string, std::vector<std::string>>> bootstrapNodes = {};
    int transport = LIBP2P_TRANSPORT_TCP;
    bool autonat = false;
    bool autonatV2 = false;
    bool autonatV2Server = false;
    bool circuitRelay = false;
    bool circuitRelayClient = false;
    int maxConnections = 50;
    int maxInConnections = 25;
    int maxOutConnections = 25;
    int maxConnsPerPeer = 1;
    bool gossipsubTriggerSelf = true;
    bool mountGossipsub = true;
    bool mountKad = true;
    bool mountMix = false;
    bool mountServiceDiscovery = false;
};

// Result type for internal sync-over-async operations.
struct SyncResult {
    bool ok = false;
    std::string message;
    std::vector<uint8_t> buffer;
    void* extra = nullptr;
};

using SyncPromise = std::promise<SyncResult>;

// Awaits a future with timeout. Returns a failed result on timeout.
inline SyncResult awaitResult(std::future<SyncResult>& f, int timeoutMs = 10000) {
    if (f.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::ready) {
        return f.get();
    }
    return {false, "timeout", {}, nullptr};
}

class Libp2pModuleImpl {
public:
    explicit Libp2pModuleImpl(const Libp2pModuleOptions& options = {});
    ~Libp2pModuleImpl();

    std::function<void(const std::string& eventName, const std::string& data)> emitEvent;

    /* ----------- Lifecycle ----------- */

    StdLogosResult start();
    StdLogosResult stop();
    StdLogosResult publicKey();
    StdLogosResult newPrivateKey();

    /* ----------- Connectivity ----------- */

    StdLogosResult connectPeer(const std::string& peerId,
                               const std::vector<std::string>& multiaddrs,
                               int64_t timeoutMs = -1);
    StdLogosResult disconnectPeer(const std::string& peerId);
    StdLogosResult peerInfo();
    StdLogosResult connectedPeers(int direction = 0);
    StdLogosResult dial(const std::string& peerId, const std::string& proto);

    /* ----------- Circuit Relay ----------- */

    StdLogosResult circuitRelayReserve(const std::string& relayPeerId,
                                       const std::vector<std::string>& relayAddrs);
    StdLogosResult dialCircuitRelay(const std::string& dstPeerId,
                                    const std::string& multiaddr,
                                    const std::string& proto);

    /* ----------- Streams ----------- */

    StdLogosResult streamReadExactly(uint64_t streamId, size_t len);
    StdLogosResult streamReadLp(uint64_t streamId, size_t maxSize);
    StdLogosResult streamWrite(uint64_t streamId, const std::string& data);
    StdLogosResult streamWriteLp(uint64_t streamId, const std::string& data);
    StdLogosResult streamClose(uint64_t streamId);
    StdLogosResult streamCloseWithEOF(uint64_t streamId);
    StdLogosResult streamRelease(uint64_t streamId);

    /* ----------- Gossipsub ----------- */

    StdLogosResult gossipsubPublish(const std::string& topic, const std::string& data);
    StdLogosResult gossipsubSubscribe(const std::string& topic);
    StdLogosResult gossipsubUnsubscribe(const std::string& topic);
    StdLogosResult gossipsubNextMessage(const std::string& topic, int timeoutMs = 1000);

    /* ----------- Kademlia ----------- */

    StdLogosResult toCid(const std::string& key);
    StdLogosResult kadFindNode(const std::string& peerId);
    StdLogosResult kadPutValue(const std::string& key, const std::string& value);
    StdLogosResult kadGetValue(const std::string& key, int quorum = -1);
    StdLogosResult kadAddProvider(const std::string& cid);
    StdLogosResult kadStartProviding(const std::string& cid);
    StdLogosResult kadStopProviding(const std::string& cid);
    StdLogosResult kadGetProviders(const std::string& cid);
    StdLogosResult kadGetRandomRecords();

    /* ----------- Mix Network ----------- */

    StdLogosResult mixGeneratePrivKey();
    StdLogosResult mixPublicKey(const std::string& privKey);
    StdLogosResult mixDial(const std::string& peerId,
                           const std::string& multiaddr,
                           const std::string& proto);
    StdLogosResult mixDialWithReply(const std::string& peerId,
                                    const std::string& multiaddr,
                                    const std::string& proto,
                                    int expectReply,
                                    uint8_t numSurbs);
    StdLogosResult mixRegisterDestReadBehavior(const std::string& proto,
                                               int behavior,
                                               uint32_t sizeParam);
    StdLogosResult mixSetNodeInfo(const std::string& multiaddr,
                                  const std::string& mixPrivKey);
    StdLogosResult mixNodepoolAdd(const std::string& peerId,
                                  const std::string& multiaddr,
                                  const std::string& mixPubKey,
                                  const std::string& libp2pPubKey);

    /* ----------- Service Discovery ----------- */

    StdLogosResult discoStart();
    StdLogosResult discoStop();
    StdLogosResult discoStartAdvertising(const std::string& serviceId,
                                         const std::string& serviceData = {});
    StdLogosResult discoStopAdvertising(const std::string& serviceId);
    StdLogosResult discoStartDiscovering(const std::string& serviceId);
    StdLogosResult discoStopDiscovering(const std::string& serviceId);
    StdLogosResult discoLookup(const std::string& serviceId,
                               const std::string& serviceData = {});
    StdLogosResult discoRandomLookup();

    /* ----------- Event Callback ----------- */

    bool setEventCallback();

private:
    libp2p_ctx_t* ctx = nullptr;
    libp2p_config_t m_libp2pConfig = {};

    std::atomic<bool> m_destroyDone{false};
    std::atomic<bool> m_newDone{false};

    // Address storage (kept alive for libp2p_config_t pointers)
    std::vector<std::string> m_addrs;
    std::vector<const char*> m_addrsPtr;

    // Bootstrap storage
    std::vector<std::string> m_peerIdStorage;
    std::vector<std::vector<std::string>> m_addrStorage;
    std::vector<std::vector<const char*>> m_addrPtrStorage;
    std::vector<libp2p_bootstrap_node_t> m_bootstrapCNodes;

    // Private key storage
    std::vector<uint8_t> m_privKey;

    /* ----------- Stream Registry ----------- */

    mutable std::shared_mutex m_streamsLock;
    std::unordered_map<uint64_t, libp2p_stream_t*> m_streams;
    std::atomic<uint64_t> m_nextStreamId{1};

    uint64_t addStream(libp2p_stream_t* stream);
    libp2p_stream_t* getStream(uint64_t id) const;
    libp2p_stream_t* removeStream(uint64_t id);
    bool hasStream(uint64_t id) const;

    /* ----------- Gossipsub Message Queue ----------- */

    std::mutex m_queueMutex;
    std::condition_variable m_queueCond;
    std::unordered_map<std::string, std::queue<std::string>> m_topicQueues;

    /* ----------- Promise-based Callbacks ----------- */

    static void promiseCallback(int ret, const char* msg, size_t len, void* userData);
    static void promiseBufferCallback(int ret, const uint8_t* data, size_t dataLen,
                                      const char* msg, size_t len, void* userData);
    static void promisePeerInfoCallback(int ret, const Libp2pPeerInfo* info,
                                        const char* msg, size_t len, void* userData);
    static void promisePeersCallback(int ret, const char** peerIds, size_t peerIdsLen,
                                     const char* msg, size_t len, void* userData);
    static void promiseProvidersCallback(int ret, const Libp2pPeerInfo* providers,
                                         size_t providersLen, const char* msg,
                                         size_t len, void* userData);
    static void promiseConnectionCallback(int ret, libp2p_stream_t* stream,
                                          const char* msg, size_t len, void* userData);
    static void promiseReservationCallback(int ret, const char** addrs, size_t addrsLen,
                                           uint64_t expireTime, const char* msg,
                                           size_t len, void* userData);
    static void promiseRandomRecordsCallback(int ret, const Libp2pExtendedPeerRecord* records,
                                             size_t recordsLen, const char* msg,
                                             size_t len, void* userData);

    static void topicHandler(const char* topic, uint8_t* data, size_t len, void* userData);
    static void eventCallback(int ret, const char* msg, size_t len, void* userData);

    /* ----------- Helpers ----------- */

    void emitEventSafe(const std::string& name, const std::string& data) const;

    // Gossipsub subscribe contexts need to persist for the lifetime of the subscription
    struct SubscribeCtx {
        Libp2pModuleImpl* instance;
    };
    std::vector<std::unique_ptr<SubscribeCtx>> m_subscribeContexts;
};
