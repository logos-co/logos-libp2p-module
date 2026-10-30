#pragma once

#include <cstdint>
#include <plugin.h>
#include <string>
#include <utility>
#include <vector>

inline std::pair<std::string, std::vector<std::string>> getPeerInfoPair(Libp2pModuleImpl& node) {
    auto res = node.peerInfo();
    auto info = res.value;
    std::string peerId = info["peerId"].get<std::string>();
    std::vector<std::string> addrs;
    for (const auto& a : info["addrs"]) {
        addrs.push_back(a.get<std::string>());
    }
    return {peerId, addrs};
}

inline std::vector<uint8_t> toBytes(const std::string& s) {
    return std::vector<uint8_t>(s.begin(), s.end());
}

inline std::string decodedValue(const StdLogosResult& res) {
    return decodeTaggedBytes(res.value);
}
