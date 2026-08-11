#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "utils.h"

struct Libp2pModuleOptions {
    std::vector<std::string> addrs = {};
    std::vector<std::pair<std::string, std::vector<std::string>>> bootstrapNodes = {};
    TransportType transport = TRANSPORT_TYPE_TCP;
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

    // Bounds on the per-topic backlog gossipsubNextMessage() drains; either at
    // 0 disables it. Keep the byte bound above gossipsubMaxMessageSize, since a
    // larger message never fits. See TopicQueues.
    size_t gossipsubQueueMaxMessages = 1024;
    size_t gossipsubQueueMaxBytes = 4 * 1024 * 1024;

    // Ingress limits nim-libp2p applies; 0 leaves each one at the core default.
    // The rate limit needs both bytes and interval, and it only counts hits
    // until gossipsubDisconnectPeerAboveRateLimit enforces it.
    int64_t gossipsubMaxMessageSize = 0;
    int64_t gossipsubOverheadRateLimitBytes = 0;
    int64_t gossipsubOverheadRateLimitIntervalMs = 0;
    bool gossipsubDisconnectPeerAboveRateLimit = false;

    // Raw private key bytes for a stable peer identity; empty generates a fresh key.
    std::vector<uint8_t> privKey = {};

    /// Builds options from the LIBP2P_MODULE_CONFIG deployment config (codegen
    /// default-constructs a loaded module). See readme; absent/invalid → defaults.
    static Libp2pModuleOptions load();

    /// Builds options from a JSON config string (the createNode argument).
    /// Sets ok=false on invalid JSON or a wrong-typed field; never throws. When
    /// err is non-null, it receives the reason on failure (empty on success).
    static Libp2pModuleOptions fromJson(const std::string& raw, bool& ok, std::string* err = nullptr);
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

inline TransportType parseTransport(const nlohmann::json& j, TransportType fallback) {
    auto it = j.find("transport");
    if (it == j.end() || !it->is_string()) {
        return fallback;
    }
    std::string t = it->get<std::string>();
    if (t == "tcp") return TRANSPORT_TYPE_TCP;
    if (t == "quic" || t == "quic-v1") return TRANSPORT_TYPE_QUIC;
    return fallback;
}

/// is_number_unsigned() rejects negatives and floats in one check; a negative
/// queue bound read straight into size_t would wrap into a huge positive one,
/// and nim-libp2p refuses a negative ingress limit. The range check keeps a
/// value above the target type from truncating into a smaller bound, or from
/// wrapping into the negative the sign check just rejected.
template <typename T>
T parseNonNegative(const nlohmann::json& j, const char* key, T fallback) {
    auto it = j.find(key);
    if (it == j.end()) {
        return fallback;
    }
    if (!it->is_number_unsigned()) {
        throw std::invalid_argument(std::string(key) + " must be a non-negative integer");
    }
    const auto raw = it->get<uint64_t>();
    if (raw > static_cast<uint64_t>(std::numeric_limits<T>::max())) {
        throw std::invalid_argument(std::string(key) + " is out of range");
    }
    return static_cast<T>(raw);
}

/// Overlays present keys onto `o`. Throws nlohmann type_error on a wrong-typed
/// field, or std::invalid_argument on an out-of-range one; load() catches both
/// and falls back to defaults.
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
    if (auto it = j.find("privKey"); it != j.end()) {
        if (!it->is_string()) {
            throw std::invalid_argument("privKey must be a string");
        }
        o.privKey = decodeHex(it->get<std::string>());
    }
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
    o.gossipsubQueueMaxMessages =
        parseNonNegative(j, "gossipsubQueueMaxMessages", o.gossipsubQueueMaxMessages);
    o.gossipsubQueueMaxBytes =
        parseNonNegative(j, "gossipsubQueueMaxBytes", o.gossipsubQueueMaxBytes);
    o.gossipsubMaxMessageSize =
        parseNonNegative(j, "gossipsubMaxMessageSize", o.gossipsubMaxMessageSize);
    o.gossipsubOverheadRateLimitBytes =
        parseNonNegative(j, "gossipsubOverheadRateLimitBytes", o.gossipsubOverheadRateLimitBytes);
    o.gossipsubOverheadRateLimitIntervalMs = parseNonNegative(
        j, "gossipsubOverheadRateLimitIntervalMs", o.gossipsubOverheadRateLimitIntervalMs);
    o.gossipsubDisconnectPeerAboveRateLimit = j.value(
        "gossipsubDisconnectPeerAboveRateLimit", o.gossipsubDisconnectPeerAboveRateLimit);
}

} // namespace libp2p_module_config

inline Libp2pModuleOptions Libp2pModuleOptions::fromJson(const std::string& raw, bool& ok, std::string* err) {
    ok = true;
    if (err) err->clear();
    auto j = nlohmann::json::parse(raw, nullptr, false);
    if (j.is_discarded()) {
        ok = false;
        if (err) *err = "malformed JSON";
        return {};
    }
    Libp2pModuleOptions opts;
    try {
        libp2p_module_config::apply(j, opts);
    } catch (const std::exception& e) {
        fprintf(stderr, "libp2p_module: invalid config: %s\n", e.what());
        ok = false;
        if (err) *err = e.what();
        return {};
    }
    return opts;
}

inline Libp2pModuleOptions Libp2pModuleOptions::load() {
    std::string raw = libp2p_module_config::readSource();
    if (raw.empty()) {
        return {};
    }
    bool ok = false;
    std::string err;
    auto opts = fromJson(raw, ok, &err);
    if (!ok) {
        fprintf(stderr, "libp2p_module: ignoring invalid LIBP2P_MODULE_CONFIG: %s\n", err.c_str());
        return {};
    }
    return opts;
}
