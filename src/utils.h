#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

extern "C" {
#include "lib/libp2p.h"
}

// Borrows each string's c_str() into the const char* array the cbinding layer
// expects. The returned pointers alias `in`, which must outlive them.
inline std::vector<const char*> toCStringPtrs(const std::vector<std::string>& in) {
    std::vector<const char*> out;
    out.reserve(in.size());
    for (const auto& s : in) out.push_back(s.c_str());
    return out;
}

// Collects a cbinding's (char** arr, len) pair into a JSON array, skipping nulls.
inline nlohmann::json cStrArrayToJson(const char* const* arr, size_t len) {
    nlohmann::json out = nlohmann::json::array();
    if (!arr) return out;
    for (size_t i = 0; i < len; ++i) {
        if (arr[i]) out.push_back(arr[i]);
    }
    return out;
}

// Flattens a Libp2pPeerInfo into {peerId, addrs}, the shape every peer-bearing
// callback returns.
inline nlohmann::json peerInfoToJson(const Libp2pPeerInfo& info) {
    nlohmann::json j;
    j["peerId"] = info.peerId ? info.peerId : "";
    j["addrs"] = cStrArrayToJson(info.addrs, info.addrsLen);
    return j;
}

// Encodes raw bytes as base64.
std::string base64Encode(const std::vector<uint8_t>& data);

// Encodes raw bytes as lowercase hex.
std::string hexEncode(const uint8_t* data, size_t len);
