#pragma once

#include <atomic>
#include <chrono>
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "logos_json.h"
#include "logos_result.h"

// The nim-ffi generated header is header-only C: it declares the exported Nim
// symbols inside its own `extern "C"` block and exposes the async API as
// `static inline` wrappers plus C++-linkage callback typedefs. It must NOT be
// wrapped in an extra `extern "C"` here, or the reply-callback typedefs would
// take C linkage and no longer match the C++ static callbacks we pass in.
#include <libp2p.h>

#include "config.h"
#include "metric.h"
#include "stream_queues.h"
#include "topic_queues.h"
#include "utils.h"

// Timeouts (milliseconds) for the sync-over-async libp2p bridge. nim-ffi never
// cancels a handler, so these bound the C++ wait only: a call that outlives its
// timeout keeps running and still resolves (and reclaims) its promise later.
inline constexpr int kDefaultOpTimeoutMs = 10000;
inline constexpr int kNewContextTimeoutMs = 5000;
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

// libp2p logs treat levels as inclusive minimum thresholds:
// `Trace` emits trace and above, `Debug` emits debug and above, etc.
// `None` is the lowest threshold, so it emits all logs; use `Fatal` for the
// quietest built-in threshold.
enum class LogLevel : int64_t {
    None = LOG_LEVEL_NONE,     // All logs.
    Trace = LOG_LEVEL_TRACE,   // Trace and above.
    Debug = LOG_LEVEL_DEBUG,   // Debug and above.
    Info = LOG_LEVEL_INFO,     // Info and above.
    Notice = LOG_LEVEL_NOTICE, // Notice and above.
    Warn = LOG_LEVEL_WARN,     // Warn and above.
    Error = LOG_LEVEL_ERROR,   // Error and above.
    Fatal = LOG_LEVEL_FATAL,   // Fatal only.
};

// Maps a level name onto LogLevel. The name is what crosses the module
// boundary — LIDL has no enum, and the numeric LOG_LEVEL_* values belong to the
// Nim binding, so they are no contract for a caller in another language.
inline bool parseLogLevel(const std::string& name, LogLevel& out) {
    if (name == "none") { out = LogLevel::None; return true; }
    if (name == "trace") { out = LogLevel::Trace; return true; }
    if (name == "debug") { out = LogLevel::Debug; return true; }
    if (name == "info") { out = LogLevel::Info; return true; }
    if (name == "notice") { out = LogLevel::Notice; return true; }
    if (name == "warn") { out = LogLevel::Warn; return true; }
    if (name == "error") { out = LogLevel::Error; return true; }
    if (name == "fatal") { out = LogLevel::Fatal; return true; }
    return false;
}

enum class KeyScheme : int64_t {
    Rsa = KEY_SCHEME_RSA,
    Ed25519 = KEY_SCHEME_ED25519,
    Secp256k1 = KEY_SCHEME_SECP256K1,
    Ecdsa = KEY_SCHEME_ECDSA,
};

// Maps a scheme name onto KeyScheme, matching the vocabulary metadata.json
// documents for the `keyType` config key. The name is what crosses the module
// boundary — LIDL has no enum, and the numeric KEY_SCHEME_* values belong to
// the Nim binding, so they are no contract for a caller in another language.
inline bool parseKeyScheme(const std::string& name, KeyScheme& out) {
    if (name == "rsa") { out = KeyScheme::Rsa; return true; }
    if (name == "ed25519") { out = KeyScheme::Ed25519; return true; }
    if (name == "secp256k1") { out = KeyScheme::Secp256k1; return true; }
    if (name == "ecdsa") { out = KeyScheme::Ecdsa; return true; }
    return false;
}

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

    static StdLogosResult setLogLevel(const std::string& level);

    bool ok();
    StdLogosResult status();

    StdLogosResult createNode(const std::string& config);
    StdLogosResult getNodeInfo(const std::string& field);

    StdLogosResult start();
    StdLogosResult stop();
    StdLogosResult newPrivateKey(const std::string& scheme);
    StdLogosResult publicKey();

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

    StdLogosResult protocolRequest(const std::string& argsJson);
    StdLogosResult pingPeer(const std::string& peerId, int64_t timeoutMs);
    StdLogosResult streamReadLpJson(const std::string& argsJson);
    StdLogosResult streamWriteLpJson(const std::string& argsJson);
    StdLogosResult streamCloseJson(const std::string& argsJson);
    StdLogosResult streamReleaseJson(const std::string& argsJson);
    StdLogosResult protocolAcceptStream(const std::string& argsJson);

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
    StdLogosResult discoStartAdvertising(const std::string& serviceId, const std::string& serviceData, const std::string& advertisement);
    StdLogosResult discoStopAdvertising(const std::string& serviceId);
    StdLogosResult discoRegisterInterest(const std::string& serviceId);
    StdLogosResult discoUnregisterInterest(const std::string& serviceId);
    StdLogosResult discoLookup(const std::string& serviceId, const std::string& serviceData);
    StdLogosResult discoRandomLookup();
    StdLogosResult createXpr(const std::vector<std::string>& addrs,
                             const std::map<std::string, std::vector<uint8_t>>& services,
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
    std::string m_natExplicitIp;

    // Creates a context from `cfg` without adopting it as the member `ctx`.
    SyncResult spawnContext(Libp2pConfig& cfg);

    // The Nim side owns stream lifetimes and hands out opaque uint64 stream
    // ids; the wrapper forwards them verbatim, so no local stream table.

    TopicQueues m_topicQueues;

    InboundStreamQueues m_inboundStreams;

    // Releases without waiting, for callers on the Nim dispatch thread that must not block it.
    void releaseStreamNoWait(uint64_t streamId);

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
    static void cbPublish(int ec, const PublishResponse* reply, const char* em, void* ud);
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
        return callStaticWith(errPrefix, std::forward<Invoke>(invoke),
                              std::forward<Transform>(transform), awaitMs);
    }

    // Same dance without the context check, for the `{.ffiStatic.}` bindings:
    // they take no ctx and run on the library's own static context.
    template <class Invoke, class Transform>
    static StdLogosResult callStaticWith(const char* errPrefix, Invoke&& invoke, Transform&& transform,
                                         int awaitMs = kDefaultOpTimeoutMs) {
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
        if (!r.ok) return {false, {}, std::string(errPrefix) + ": " + r.message};
        return transform(r);
    }
};

inline StdLogosResult setLogLevel(const std::string& level) {
    return Libp2pModuleImpl::setLogLevel(level);
}
