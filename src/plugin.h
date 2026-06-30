#pragma once

#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <queue>

#include <nlohmann/json.hpp>

#include "logos_json.h"
#include "logos_result.h"

extern "C" {
#include "lib/libp2p.h"
}

#include "config.h"
#include "metric.h"
#include "utils.h"

// Timeouts (milliseconds) for the sync-over-async libp2p bridge.
inline constexpr int kDefaultOpTimeoutMs = 10000;
inline constexpr int kNewContextTimeoutMs = 5000;
inline constexpr int kDestroyTimeoutMs = 5000;
// Added on top of a caller-supplied op timeout so the C++ await outlives the
// libp2p operation it wraps instead of racing it.
inline constexpr int kAwaitSlackMs = 5000;

// Result type for internal sync-over-async operations.
struct SyncResult {
    bool ok = false;
    std::string message;
    std::vector<uint8_t> buffer;
    nlohmann::json data;
    libp2p_stream_t* stream = nullptr;
};

using SyncPromise = std::promise<SyncResult>;

// Resolves and reclaims a heap SyncPromise. Every libp2p callback owns its
// promise and ends by handing the result back through here.
inline void finishPromise(SyncPromise* p, SyncResult r) {
    p->set_value(std::move(r));
    delete p;
}

// Seeds a SyncResult from the (ret, msg) pair every cbinding callback receives.
inline SyncResult basicResult(int ret, const char* msg, size_t len) {
    SyncResult r;
    r.ok = (ret == RET_OK);
    r.message = (msg && len > 0) ? std::string(msg, len) : std::string();
    return r;
}

// Awaits a future with timeout. Returns a failed result on timeout.
inline SyncResult awaitResult(std::future<SyncResult>& f, int timeoutMs = kDefaultOpTimeoutMs) {
    if (f.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::ready) {
        return f.get();
    }
    SyncResult r;
    r.message = "timeout";
    return r;
}

inline std::string base64Encode(const std::vector<uint8_t>& data) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    size_t i = 0;
    for (; i + 3 <= data.size(); i += 3) {
        uint32_t n = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out.push_back(kAlphabet[(n >> 18) & 0x3f]);
        out.push_back(kAlphabet[(n >> 12) & 0x3f]);
        out.push_back(kAlphabet[(n >> 6) & 0x3f]);
        out.push_back(kAlphabet[n & 0x3f]);
    }
    if (i < data.size()) {
        uint32_t n = data[i] << 16;
        bool hasTwo = i + 1 < data.size();
        if (hasTwo) n |= data[i + 1] << 8;
        out.push_back(kAlphabet[(n >> 18) & 0x3f]);
        out.push_back(kAlphabet[(n >> 12) & 0x3f]);
        out.push_back(hasTwo ? kAlphabet[(n >> 6) & 0x3f] : '=');
        out.push_back('=');
    }
    return out;
}

// Inverse of base64Encode. Throws std::invalid_argument on malformed input
// (bad chars, misplaced/excess padding, wrong length, non-canonical tail bits).
inline std::string base64Decode(const std::string& in) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int lookup[256];
    for (int& v : lookup) v = -1;
    for (int i = 0; i < 64; ++i) lookup[static_cast<unsigned char>(kAlphabet[i])] = i;

    if (in.size() % 4 != 0) {
        throw std::invalid_argument("base64Decode: length is not a multiple of 4");
    }

    std::string out;
    out.reserve(in.size() / 4 * 3);
    uint32_t buf = 0;
    int bits = 0;
    size_t pad = 0;
    for (size_t i = 0; i < in.size(); ++i) {
        char c = in[i];
        if (c == '=') {
            if (i < in.size() - 2 || ++pad > 2) {
                throw std::invalid_argument("base64Decode: misplaced padding");
            }
            continue;
        }
        if (pad > 0) {
            throw std::invalid_argument("base64Decode: data after padding");
        }
        int val = lookup[static_cast<unsigned char>(c)];
        if (val < 0) {
            throw std::invalid_argument("base64Decode: invalid character");
        }
        buf = (buf << 6) | static_cast<uint32_t>(val);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((buf >> bits) & 0xff));
        }
    }
    if ((buf & ((1u << bits) - 1)) != 0) {
        throw std::invalid_argument("base64Decode: non-canonical trailing bits");
    }
    return out;
}

// Wraps a resolved buffer as a successful result. Buffers are raw bytes
// (publicKey/kadGetValue/stream reads), so they're base64-encoded to keep
// `value` a valid UTF-8 JSON string.
inline StdLogosResult bufferToResult(const SyncResult& r) {
    return {true, base64Encode(r.buffer), ""};
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

// Await long enough to outlast a caller-supplied op timeout, falling back to the
// default when the op carries no timeout of its own.
inline int awaitTimeoutFor(int64_t opTimeoutMs) {
    if (opTimeoutMs <= 0) return kDefaultOpTimeoutMs;
    int64_t v = opTimeoutMs + kAwaitSlackMs;
    return v > INT_MAX ? INT_MAX : static_cast<int>(v);
}

// Maps a resolved SyncResult's structured payload into a result, substituting an
// empty default when the callback produced no data (e.g. ok with zero items).
inline StdLogosResult jsonResult(const SyncResult& r, nlohmann::json emptyDefault) {
    if (r.data.is_null()) return {true, std::move(emptyDefault), ""};
    return {true, r.data, ""};
}

class Libp2pModuleImpl {
public:
    Libp2pModuleImpl(const Libp2pModuleOptions& options = Libp2pModuleOptions::load());
    ~Libp2pModuleImpl();

    std::function<void(const std::string& eventName, const std::string& data)> emitEvent;

    bool ok();
    StdLogosResult status();

    StdLogosResult createNode(const std::string& config);
    StdLogosResult getNodeInfo(const std::string& field);

    StdLogosResult start();
    StdLogosResult stop();
    StdLogosResult publicKey();
    StdLogosResult newPrivateKey();

    StdLogosResult connectPeer(const std::string& peerId, const std::vector<std::string>& multiaddrs, int64_t timeoutMs);
    StdLogosResult disconnectPeer(const std::string& peerId);
    StdLogosResult peerInfo();
    StdLogosResult connectedPeers(int64_t direction);
    StdLogosResult dial(const std::string& peerId, const std::string& proto);

    StdLogosResult circuitRelayReserve(const std::string& relayPeerId, const std::vector<std::string>& relayAddrs);
    StdLogosResult dialCircuitRelay(const std::string& dstPeerId, const std::string& multiaddr, const std::string& proto);

    StdLogosResult mountProtocol(const std::string& proto);

    StdLogosResult streamReadExactly(uint64_t streamId, uint64_t len);
    StdLogosResult streamReadLp(uint64_t streamId, uint64_t maxSize);
    StdLogosResult streamWrite(uint64_t streamId, const std::string& data);
    StdLogosResult streamWriteLp(uint64_t streamId, const std::string& data);
    StdLogosResult streamClose(uint64_t streamId);
    StdLogosResult streamCloseWithEOF(uint64_t streamId);
    StdLogosResult streamRelease(uint64_t streamId);

    StdLogosResult gossipsubPublish(const std::string& topic, const std::string& data);
    StdLogosResult gossipsubSubscribe(const std::string& topic);
    StdLogosResult gossipsubUnsubscribe(const std::string& topic);
    StdLogosResult gossipsubNextMessage(const std::string& topic, int64_t timeoutMs);

    StdLogosResult toCid(const std::string& key);
    StdLogosResult kadFindNode(const std::string& peerId);
    StdLogosResult kadPutValue(const std::string& key, const std::string& value);
    StdLogosResult kadGetValue(const std::string& key, int64_t quorum);
    StdLogosResult kadAddProvider(const std::string& cid);
    StdLogosResult kadStartProviding(const std::string& cid);
    StdLogosResult kadStopProviding(const std::string& cid);
    StdLogosResult kadGetProviders(const std::string& cid);
    StdLogosResult kadGetRandomRecords();

    StdLogosResult discoStart();
    StdLogosResult discoStop();
    StdLogosResult discoStartAdvertising(const std::string& serviceId, const std::string& serviceData);
    StdLogosResult discoStopAdvertising(const std::string& serviceId);
    StdLogosResult discoRegisterInterest(const std::string& serviceId);
    StdLogosResult discoUnregisterInterest(const std::string& serviceId);
    StdLogosResult discoLookup(const std::string& serviceId, const std::string& serviceData);
    StdLogosResult discoRandomLookup();
    StdLogosResult createXpr(const std::vector<std::string>& addrs,
                             const std::vector<std::pair<std::string, std::string>>& services,
                             uint64_t seqNo);
    StdLogosResult decodeXpr(const std::string& xpr);

    StdLogosResult peerstoreGetPeers();
    StdLogosResult peerstoreGetPeerInfo(const std::string& peerId);
    StdLogosResult peerstoreAddPeer(const std::string& peerId, const std::vector<std::string>& addrs, const std::vector<std::string>& protos);
    StdLogosResult peerstoreSetPeerAddresses(const std::string& peerId, const std::vector<std::string>& addrs);
    StdLogosResult peerstoreSetPeerProtocols(const std::string& peerId, const std::vector<std::string>& protos);
    StdLogosResult peerstoreDeletePeer(const std::string& peerId);

    LogosMap collectMetrics();

    /* ----------- Event Callback ----------- */

    bool setEventCallback();

private:
    libp2p_ctx_t* ctx = nullptr;
    libp2p_config_t m_libp2pConfig = {};

    // Set when construction fails; surfaced through status() since the
    // constructor cannot signal failure to the codegen default-constructor.
    std::string m_initError;

    // Address storage (kept alive for libp2p_config_t pointers)
    std::vector<std::string> m_addrs;
    std::vector<const char*> m_addrsPtr;

    std::vector<std::string> m_peerIdStorage;
    std::vector<std::vector<std::string>> m_addrStorage;
    std::vector<std::vector<const char*>> m_addrPtrStorage;
    std::vector<libp2p_bootstrap_node_t> m_bootstrapCNodes;

    std::vector<uint8_t> m_privKey;

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

    std::mutex m_queueMutex;
    std::condition_variable m_queueCond;
    std::unordered_map<std::string, std::queue<std::string>> m_topicQueues;

    void applyOptions(const Libp2pModuleOptions& options);
    StdLogosResult createContext();
    void destroyContext();
    void destroyHandle(libp2p_ctx_t* handle);
    StdLogosResult nodeInfoBoundPorts();

    SyncResult generatePrivateKeyRaw();

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
    static void promiseExtendedPeerRecordCallback(int ret,
                                                  const Libp2pExtendedPeerRecord* record,
                                                  const char* msg, size_t len, void* userData);

    static void topicHandler(const char* topic, uint8_t* data, size_t len, void* userData);
    static void gossipsubResultCallback(int ret, const char* msg, size_t len, void* userData);
    static void protocolHandler(libp2p_ctx_t* ctx, libp2p_stream_t* stream,
                                const char* proto, size_t protoLen, void* userData);
    static void mountCompleteCallback(int ret, const char* msg, size_t len, void* userData);
    static void eventCallback(int ret, const char* msg, size_t len, void* userData);

    using EmitEventFn = std::function<void(const std::string& eventName, const std::string& data)>;

    // Lock-guarded snapshot of `emitEvent`, taken on the caller thread before any worker can emit, so worker threads never read the public field unsynchronized.
    mutable std::shared_mutex m_emitEventLock;
    EmitEventFn m_emitEventSnapshot;
    void publishEmitEvent();
    void emitEventSafe(const std::string& name, const std::string& data) const;

    // Wraps the new-promise / invoke / await / clean-up dance shared by every
    // sync-over-async libp2p op. `invoke(SyncPromise*)` calls the cbinding and
    // returns its sync ret. The Transform overload maps the resolved SyncResult
    // into the final StdLogosResult. `awaitMs` bounds the wait; ops carrying a
    // caller timeout pass awaitTimeoutFor(theirTimeout) so the await outlasts it.
    template <class Invoke>
    StdLogosResult callSync(const char* errPrefix, Invoke&& invoke, int awaitMs = kDefaultOpTimeoutMs) {
        return callSyncWith(errPrefix, std::forward<Invoke>(invoke),
            [](const SyncResult&) -> StdLogosResult { return {true, {}, ""}; }, awaitMs);
    }

    template <class Invoke, class Transform>
    StdLogosResult callSyncWith(const char* errPrefix, Invoke&& invoke, Transform&& transform,
                                int awaitMs = kDefaultOpTimeoutMs) {
        if (!ctx) return {false, {}, "No libp2p context"};
        auto* p = new SyncPromise();
        auto f = p->get_future();
        int ret = invoke(p);
        if (ret != RET_OK) {
            // A synchronous failure (failWithMsg) fires the callback, which owns
            // and deletes p, before returning the error; checkLibParams returns
            // without firing it. Reclaim p only when the callback didn't run.
            if (f.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                delete p;
            }
            return {false, {}, std::string(errPrefix) +
                " (ret=" + std::to_string(ret) + ")"};
        }
        auto r = awaitResult(f, awaitMs);
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
        std::atomic<bool> unsubscribing{false};
    };
    std::mutex m_subscribeContextsLock;
    std::vector<std::unique_ptr<SubscribeCtx>> m_subscribeContexts;

    // Persist for the mounted protocol's lifetime; libp2p has no unmount API, so contexts
    // live until ~Libp2pModuleImpl(). m_protocolHandlersLock guards concurrent push_back.
    struct ProtocolHandlerCtx {
        Libp2pModuleImpl* instance;
        std::string proto;
        SyncPromise* mountPromise = nullptr;
    };
    std::mutex m_protocolHandlersLock;
    std::vector<std::unique_ptr<ProtocolHandlerCtx>> m_protocolHandlerContexts;
};
