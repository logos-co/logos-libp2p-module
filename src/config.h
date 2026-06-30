#pragma once

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

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

    // Raw private key bytes for a stable peer identity; empty generates a fresh key.
    std::vector<uint8_t> privKey = {};
    // Scheme for the generated key; ignored when privKey is supplied.
    int keyType = LIBP2P_PK_SECP256K1;

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

inline int parseKeyType(const nlohmann::json& j, int fallback) {
    auto it = j.find("keyType");
    if (it == j.end() || !it->is_string()) {
        return fallback;
    }
    std::string t = it->get<std::string>();
    if (t == "rsa") return LIBP2P_PK_RSA;
    if (t == "ed25519") return LIBP2P_PK_ED25519;
    if (t == "secp256k1") return LIBP2P_PK_SECP256K1;
    if (t == "ecdsa") return LIBP2P_PK_ECDSA;
    return fallback;
}

inline int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    throw std::invalid_argument("privKey contains a non-hex character");
}

/// Decodes a hex string into bytes. Throws std::invalid_argument on odd length
/// or a non-hex character; load() catches it and falls back to defaults.
inline std::vector<uint8_t> decodeHex(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        throw std::invalid_argument("privKey hex length must be even");
    }
    std::vector<uint8_t> out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        out.push_back(static_cast<uint8_t>((hexNibble(hex[i]) << 4) | hexNibble(hex[i + 1])));
    }
    return out;
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
    o.keyType = parseKeyType(j, o.keyType);
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
