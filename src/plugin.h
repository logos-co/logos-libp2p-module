#pragma once

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <map>
#include <future>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <sstream>
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
    bool mountServiceDiscovery = true;

    /// Builds options from the LIBP2P_MODULE_CONFIG deployment config (codegen
    /// default-constructs a loaded module). See readme; absent/invalid → defaults.
    static Libp2pModuleOptions load();
};

namespace libp2p_module_config {

/// Reads LIBP2P_MODULE_CONFIG: inline JSON when the first non-space char is '{',
/// otherwise a path to a JSON file. Returns "" when unset or unreadable.
inline std::string readSource() {
    const char* cfg = std::getenv("LIBP2P_MODULE_CONFIG");
    if (!cfg || !*cfg) {
        return "";
    }
    std::string value(cfg);
    auto firstChar = value.find_first_not_of(" \t\r\n");
    if (firstChar != std::string::npos && value[firstChar] == '{') {
        return value;
    }
    std::ifstream f(value);
    if (!f) {
        fprintf(stderr, "libp2p_module: cannot read config file %s\n", value.c_str());
        return "";
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

inline int parseTransport(const nlohmann::json& j, int fallback) {
    auto it = j.find("transport");
    if (it == j.end() || !it->is_string()) {
        return fallback;
    }
    std::string t = it->get<std::string>();
    if (t == "tcp") return LIBP2P_TRANSPORT_TCP;
    if (t == "quic" || t == "quic-v1") return LIBP2P_TRANSPORT_QUIC;
    return fallback;
}

/// Overlays present keys onto `o`. Throws nlohmann type_error on a wrong-typed
/// field; load() catches it and falls back to defaults.
inline void apply(const nlohmann::json& j, Libp2pModuleOptions& o) {
    if (!j.is_object()) {
        return;
    }
    o.addrs = j.value("addrs", o.addrs);
    if (auto it = j.find("bootstrapNodes"); it != j.end() && it->is_array()) {
        o.bootstrapNodes.clear();
        for (const auto& n : *it) {
            o.bootstrapNodes.emplace_back(n.value("peerId", std::string{}),
                                          n.value("addrs", std::vector<std::string>{}));
        }
    }
    o.transport = parseTransport(j, o.transport);
    o.autonat = j.value("autonat", o.autonat);
    o.autonatV2 = j.value("autonatV2", o.autonatV2);
    o.autonatV2Server = j.value("autonatV2Server", o.autonatV2Server);
    o.circuitRelay = j.value("circuitRelay", o.circuitRelay);
    o.circuitRelayClient = j.value("circuitRelayClient", o.circuitRelayClient);
    o.maxConnections = j.value("maxConnections", o.maxConnections);
    o.maxInConnections = j.value("maxInConnections", o.maxInConnections);
    o.maxOutConnections = j.value("maxOutConnections", o.maxOutConnections);
    o.maxConnsPerPeer = j.value("maxConnsPerPeer", o.maxConnsPerPeer);
    o.gossipsubTriggerSelf = j.value("gossipsubTriggerSelf", o.gossipsubTriggerSelf);
    o.mountGossipsub = j.value("mountGossipsub", o.mountGossipsub);
    o.mountKad = j.value("mountKad", o.mountKad);
    o.mountServiceDiscovery = j.value("mountServiceDiscovery", o.mountServiceDiscovery);
}

} // namespace libp2p_module_config

inline Libp2pModuleOptions Libp2pModuleOptions::load() {
    std::string raw = libp2p_module_config::readSource();
    if (raw.empty()) {
        return {};
    }
    auto j = nlohmann::json::parse(raw, nullptr, false);
    if (j.is_discarded()) {
        fprintf(stderr, "libp2p_module: ignoring invalid LIBP2P_MODULE_CONFIG\n");
        return {};
    }
    Libp2pModuleOptions opts;
    try {
        libp2p_module_config::apply(j, opts);
    } catch (const std::exception& e) {
        fprintf(stderr, "libp2p_module: ignoring invalid LIBP2P_MODULE_CONFIG: %s\n", e.what());
        return {};
    }
    return opts;
}

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

// Inverse of base64Encode; feeds base64'd buffers (e.g. createXpr output) back
// into byte-input APIs like decodeXpr.
inline std::string base64Decode(const std::string& in) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int lookup[256];
    for (int& v : lookup) v = -1;
    for (int i = 0; i < 64; ++i) lookup[static_cast<unsigned char>(kAlphabet[i])] = i;

    std::string out;
    out.reserve(in.size() / 4 * 3);
    uint32_t buf = 0;
    int bits = 0;
    for (char c : in) {
        if (c == '=') break;
        int val = lookup[static_cast<unsigned char>(c)];
        if (val < 0) continue;
        buf = (buf << 6) | static_cast<uint32_t>(val);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((buf >> bits) & 0xff));
        }
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

// One metrics series, mirroring the JSON that nim-libp2p's libp2p_collect_metrics
// emits. Cbind sends labels as `[{"name":..,"value":..}, ...]`; we flatten into
// a map because openmetrics-module's renderer expects `labels` as a flat
// `{key:value}` object (openmetrics_format.cpp:82). `timestamp` is Unix
// milliseconds from the registry; 0 means "unset" and is dropped on output.
struct Metric {
    std::string name;
    std::string type;
    std::string help;
    std::map<std::string, std::string> labels;
    double value = 0.0;
    int64_t timestamp = 0;
};

inline void from_json(const nlohmann::json& j, Metric& m) {
    j.at("name").get_to(m.name);
    j.at("type").get_to(m.type);
    j.at("help").get_to(m.help);
    j.at("value").get_to(m.value);
    m.labels.clear();
    if (auto it = j.find("labels"); it != j.end() && it->is_array()) {
        for (const auto& lp : *it) {
            m.labels.emplace(lp.at("name").get<std::string>(),
                             lp.at("value").get<std::string>());
        }
    }
    if (auto it = j.find("timestamp"); it != j.end() && it->is_number_integer()) {
        m.timestamp = it->get<int64_t>();
    } else {
        m.timestamp = 0;
    }
}

inline void to_json(nlohmann::json& j, const Metric& m) {
    j = nlohmann::json{
        {"name", m.name},
        {"type", m.type},
        {"help", m.help},
        {"labels", m.labels},
        {"value", m.value},
    };
    if (m.timestamp != 0) j["timestamp"] = m.timestamp;
}

class Libp2pModuleImpl {
public:
    Libp2pModuleImpl(const Libp2pModuleOptions& options = Libp2pModuleOptions::load());
    ~Libp2pModuleImpl();

    std::function<void(const std::string& eventName, const std::string& data)> emitEvent;

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
            // A synchronous failure (failWithMsg) fires the callback, which owns
            // and deletes p, before returning the error; checkLibParams returns
            // without firing it. Reclaim p only when the callback didn't run.
            if (f.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
                delete p;
            }
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
