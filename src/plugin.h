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

// The nim-ffi generated header is header-only C: it declares the exported Nim
// symbols inside its own `extern "C"` block and exposes the async API as
// `static inline` wrappers plus C++-linkage callback typedefs. It must NOT be
// wrapped in an extra `extern "C"` here, or the reply-callback typedefs would
// take C linkage and no longer match the C++ static callbacks we pass in.
#include "lib/libp2p.h"

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
    LibP2PCtx* newCtx = nullptr;
};

using SyncPromise = std::promise<SyncResult>;

// Resolves and reclaims a heap SyncPromise. Every libp2p callback owns its
// promise and ends by handing the result back through here.
inline void finishPromise(SyncPromise* p, SyncResult r) {
    p->set_value(std::move(r));
    delete p;
}

// Seeds a SyncResult from the (err_code, err_msg) pair every nim-ffi reply
// callback receives. err_msg is a NUL-terminated copy owned by the binding and
// valid only during the callback. err_code 0 (NIMFFI_RET_OK) means success.
inline SyncResult replyBase(int errCode, const char* errMsg) {
    SyncResult r;
    r.ok = (errCode == NIMFFI_RET_OK);
    if (!r.ok) r.message = errMsg ? std::string(errMsg) : std::string();
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

// Wraps a resolved buffer as a successful result. Buffers are raw bytes
// (publicKey/kadGetValue/stream reads), so they're base64-encoded to keep
// `value` a valid UTF-8 JSON string.
inline StdLogosResult bufferToResult(const SyncResult& r) {
    return {true, base64Encode(r.buffer), ""};
}

// Hex-encodes the buffer so private keys match the hex `privKey` config format.
inline StdLogosResult bufferToHexResult(const SyncResult& r) {
    return {true, hexEncode(r.buffer.data(), r.buffer.size()), ""};
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

private:
    LibP2PCtx* ctx = nullptr;
    Libp2pConfig m_libp2pConfig = {};

    // Set when construction fails; surfaced through status() since the
    // constructor cannot signal failure to the codegen default-constructor.
    std::string m_initError;

    // Backing storage the Libp2pConfig's NimFfiStr/seq views borrow from; it
    // must outlive every libp2p_ctx_create call. Built once in applyOptions and
    // never mutated afterwards, so the views stay valid.
    std::vector<std::string> m_addrs;
    std::vector<NimFfiStr> m_addrsFfi;

    std::vector<std::string> m_bootstrapPeerIds;
    std::vector<std::vector<std::string>> m_bootstrapAddrs;
    std::vector<std::vector<NimFfiStr>> m_bootstrapAddrsFfi;
    std::vector<BootstrapNode> m_bootstrapNodes;

    SecureBytes m_privKey;
    int m_keyType = LIBP2P_PK_SECP256K1;

    SyncResult generatePrivateKey(int scheme);

    // The Nim side owns stream lifetimes and hands out opaque uint64 stream
    // ids; the wrapper forwards them verbatim, so no local stream table.

    std::mutex m_queueMutex;
    std::condition_variable m_queueCond;
    std::unordered_map<std::string, std::queue<std::string>> m_topicQueues;

    void applyOptions(const Libp2pModuleOptions& options);
    StdLogosResult createContext();
    void destroyContext();
    StdLogosResult nodeInfoBoundPorts();

    // Reply trampolines: one per generated response type. Each turns the typed
    // (err_code, reply, err_msg) callback into a SyncResult and resolves the
    // promise handed in as user_data.
    static void cbBool(int ec, const bool* reply, const char* em, void* ud);
    static void cbBytes(int ec, const NimFfiBytes* reply, const char* em, void* ud);
    static void cbStr(int ec, const NimFfiStr* reply, const char* em, void* ud);
    static void cbRead(int ec, const ReadResponse* reply, const char* em, void* ud);
    static void cbCreate(int ec, LibP2PCtx* newCtx, const char* em, void* ud);
    static void cbPeerInfo(int ec, const PeerInfoResponse* reply, const char* em, void* ud);
    static void cbPeers(int ec, const PeersResponse* reply, const char* em, void* ud);
    static void cbDial(int ec, const DialResponse* reply, const char* em, void* ud);
    static void cbProviders(int ec, const ProvidersResponse* reply, const char* em, void* ud);
    static void cbRecords(int ec, const ExtendedRecordsResponse* reply, const char* em, void* ud);
    static void cbRecord(int ec, const ExtendedPeerRecordEntry* reply, const char* em, void* ud);
    static void cbReservation(int ec, const ReservationResponse* reply, const char* em, void* ud);
    static void cbPeerStoreEntry(int ec, const PeerStoreEntryResponse* reply, const char* em, void* ud);

    // Event listeners registered on the context; the Nim side pushes here.
    static void onIncomingStream(const IncomingStreamEvent* evt, void* ud);
    static void onPubsubMessage(const PubsubMessageEvent* evt, void* ud);

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
        if (ret != 0) {
            // A submit-time failure (encode/OOM/missing-callback) fires the
            // reply callback synchronously — which owns and deletes p — before
            // returning non-zero. Reclaim p only in the (unexpected) case the
            // callback didn't run, so we never double-free.
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
};
