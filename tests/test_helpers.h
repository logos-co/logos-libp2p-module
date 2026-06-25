#pragma once

#include <cstdint>
#include <plugin.h>
#include <string>
#include <utility>
#include <vector>

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
