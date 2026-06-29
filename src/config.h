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
