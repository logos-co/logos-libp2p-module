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

// Non-throwing JSON parse — malformed cbinding output yields a failed result
// instead of propagating an exception.
inline StdLogosResult parseJsonResponse(const std::string& s, const char* errPrefix) {
    auto j = nlohmann::json::parse(s, nullptr, false);
    if (j.is_discarded()) {
        return {false, {}, std::string(errPrefix) + ": invalid JSON"};
    }
    return {true, j, ""};
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

    /* ----------- Custom Protocol Handlers ----------- */

    StdLogosResult mountProtocol(const std::string& proto);

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

#if 0  // mix temporarily disabled — extracted to separate repo, no cbindings yet
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
#endif

    /* ----------- Service Discovery ----------- */

    StdLogosResult discoStart();
    StdLogosResult discoStop();
    StdLogosResult discoStartAdvertising(const std::string& serviceId,
                                         const std::string& serviceData = {});
    StdLogosResult discoStopAdvertising(const std::string& serviceId);
    StdLogosResult discoRegisterInterest(const std::string& serviceId);
    StdLogosResult discoUnregisterInterest(const std::string& serviceId);
    StdLogosResult discoLookup(const std::string& serviceId,
                               const std::string& serviceData = {});
    StdLogosResult discoRandomLookup();

    /* ----------- Peerstore ----------- */

    StdLogosResult peerstoreGetPeers();
    StdLogosResult peerstoreGetPeerInfo(const std::string& peerId);
    StdLogosResult peerstoreAddPeer(const std::string& peerId,
                                    const std::vector<std::string>& addrs,
                                    const std::vector<std::string>& protos = {});
    StdLogosResult peerstoreSetPeerAddresses(const std::string& peerId,
                                             const std::vector<std::string>& addrs);
    StdLogosResult peerstoreSetPeerProtocols(const std::string& peerId,
                                             const std::vector<std::string>& protos);
    StdLogosResult peerstoreDeletePeer(const std::string& peerId);

    /* ----------- Event Callback ----------- */

    bool setEventCallback();

private:
    libp2p_ctx_t* ctx = nullptr;
    libp2p_config_t m_libp2pConfig = {};

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

    // Map guards lookup; entry->mtx guards the pointee. Ops take shared,
    // release takes exclusive and flips `released`.
    struct StreamEntry {
        libp2p_stream_t* ptr;
        mutable std::shared_mutex mtx;
        bool released = false;
        explicit StreamEntry(libp2p_stream_t* p) : ptr(p) {}
    };

    mutable std::shared_mutex m_streamsLock;
    std::unordered_map<uint64_t, std::shared_ptr<StreamEntry>> m_streams;
    std::atomic<uint64_t> m_nextStreamId{1};

    uint64_t addStream(libp2p_stream_t* stream);
    std::shared_ptr<StreamEntry> getStream(uint64_t id) const;
    std::shared_ptr<StreamEntry> removeStream(uint64_t id);

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
    static void promisePeerStoreEntryCallback(int ret, const Libp2pPeerStoreEntry* entry,
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
    static void gossipsubResultCallback(int ret, const char* msg, size_t len, void* userData);
    static void protocolHandler(libp2p_ctx_t* ctx, libp2p_stream_t* stream,
                                const char* proto, size_t protoLen, void* userData);
    static void mountCompleteCallback(int ret, const char* msg, size_t len, void* userData);
    static void eventCallback(int ret, const char* msg, size_t len, void* userData);

    /* ----------- Helpers ----------- */

    void emitEventSafe(const std::string& name, const std::string& data) const;

    // Wraps the new-promise / invoke / await / clean-up dance shared by every
    // sync-over-async libp2p op. `invoke(SyncPromise*)` calls the cbinding and
    // returns its sync ret. The Transform overload maps the resolved SyncResult
    // into the final StdLogosResult.
    template <class Invoke>
    StdLogosResult callSync(const char* errPrefix, Invoke&& invoke) {
        return callSyncWith(errPrefix, std::forward<Invoke>(invoke),
            [](const SyncResult&) -> StdLogosResult { return {true, {}, ""}; });
    }

    template <class Invoke, class Transform>
    StdLogosResult callSyncWith(const char* errPrefix, Invoke&& invoke, Transform&& transform) {
        if (!ctx) return {false, {}, "No libp2p context"};
        auto* p = new SyncPromise();
        auto f = p->get_future();
        int ret = invoke(p);
        if (ret != RET_OK) {
            delete p;
            return {false, {}, std::string(errPrefix) +
                " (ret=" + std::to_string(ret) + ")"};
        }
        auto r = awaitResult(f);
        if (!r.ok) return {false, {}, r.message};
        return transform(r);
    }

    // Stream-op variant: looks up the entry, holds its shared lock across the
    // whole sync-over-async call so a concurrent streamRelease can't free
    // entry->ptr mid-flight. `invoke(libp2p_stream_t*, SyncPromise*)`.
    template <class Invoke>
    StdLogosResult callSyncStream(uint64_t streamId, const char* errPrefix, Invoke&& invoke) {
        return callSyncStreamWith(streamId, errPrefix, std::forward<Invoke>(invoke),
            [](const SyncResult&) -> StdLogosResult { return {true, {}, ""}; });
    }

    template <class Invoke, class Transform>
    StdLogosResult callSyncStreamWith(uint64_t streamId, const char* errPrefix,
                                       Invoke&& invoke, Transform&& transform) {
        if (!ctx || streamId == 0) return {false, {}, "Invalid stream"};
        auto entry = getStream(streamId);
        if (!entry) return {false, {}, "Stream not found"};
        std::shared_lock<std::shared_mutex> opLock(entry->mtx);
        if (entry->released) return {false, {}, "Stream released"};
        return callSyncWith(errPrefix,
            [&](SyncPromise* p) { return invoke(entry->ptr, p); },
            std::forward<Transform>(transform));
    }

    // Subscribe contexts live for the subscription's lifetime; the lock guards
    // concurrent push/lookup and the libp2p worker thread's view of the vector.
    struct SubscribeCtx {
        Libp2pModuleImpl* instance;
        std::string topic;
    };
    std::mutex m_subscribeContextsLock;
    std::vector<std::unique_ptr<SubscribeCtx>> m_subscribeContexts;

    // Protocol handler contexts need to persist for the lifetime of the mounted protocol.
    // libp2p has no unmount API, so contexts live until ~Libp2pModuleImpl() and are not
    // cleared across stop()/start() cycles. m_protocolHandlersLock guards push_back so
    // concurrent mountProtocol calls (and visibility to the libp2p worker thread that
    // invokes protocolHandler) are well-defined.
    struct ProtocolHandlerCtx {
        Libp2pModuleImpl* instance;
        std::string proto;
        SyncPromise* mountPromise = nullptr;
    };
    std::mutex m_protocolHandlersLock;
    std::vector<std::unique_ptr<ProtocolHandlerCtx>> m_protocolHandlerContexts;
};
