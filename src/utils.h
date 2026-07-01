#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "lib/libp2p.h"
#include "libp2p_consts.h"

// Copies a borrowed NimFfiStr into an owned std::string (empty on null data).
// Response strings are only valid during the callback, so they must be copied.
inline std::string nfStr(const NimFfiStr& s) {
    return (s.data && s.len > 0) ? std::string(s.data, s.len) : std::string();
}

// Copies a borrowed NimFfiBytes into an owned byte vector (binary-safe).
inline std::vector<uint8_t> nfBytes(const NimFfiBytes& b) {
    if (!b.data || b.len == 0) return {};
    return std::vector<uint8_t>(b.data, b.data + b.len);
}

// Collects a NimFfiStr sequence into a JSON array.
inline nlohmann::json seqStrToJson(const LibP2PSeq_Str& seq) {
    nlohmann::json out = nlohmann::json::array();
    if (!seq.data) return out;
    for (size_t i = 0; i < seq.len; ++i) {
        out.push_back(nfStr(seq.data[i]));
    }
    return out;
}

// Flattens a PeerInfoResponse into {peerId, addrs}, the shape every peer-bearing
// callback returns.
inline nlohmann::json peerInfoToJson(const PeerInfoResponse& info) {
    nlohmann::json j;
    j["peerId"] = nfStr(info.peerId);
    j["addrs"] = seqStrToJson(info.addrs);
    return j;
}

// Encodes raw bytes as base64.
std::string base64Encode(const std::vector<uint8_t>& data);

// Inverse of base64Encode. Throws std::invalid_argument on malformed input
// (bad chars, misplaced/excess padding, wrong length, non-canonical tail bits).
std::string base64Decode(const std::string& in);

// Encodes raw bytes as lowercase hex.
std::string hexEncode(const uint8_t* data, size_t len);

// Decodes a hex string (either case) into bytes. Throws std::invalid_argument
// on an odd length or a non-hex character.
std::vector<uint8_t> decodeHex(const std::string& hex);

// Allocator that scrubs memory before releasing it, so secret bytes don't
// linger in the heap after the holding vector is reallocated or destroyed. The
// volatile writes are not elided by the optimizer, unlike a plain memset.
template <class T>
struct ScrubbingAllocator {
    using value_type = T;

    ScrubbingAllocator() noexcept = default;
    template <class U>
    ScrubbingAllocator(const ScrubbingAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) { return std::allocator<T>{}.allocate(n); }

    void deallocate(T* p, std::size_t n) noexcept {
        volatile unsigned char* vp = reinterpret_cast<volatile unsigned char*>(p);
        for (std::size_t i = 0; i < n * sizeof(T); ++i) {
            vp[i] = 0;
        }
        std::allocator<T>{}.deallocate(p, n);
    }
};

template <class T, class U>
bool operator==(const ScrubbingAllocator<T>&, const ScrubbingAllocator<U>&) noexcept {
    return true;
}
template <class T, class U>
bool operator!=(const ScrubbingAllocator<T>&, const ScrubbingAllocator<U>&) noexcept {
    return false;
}

using SecureBytes = std::vector<uint8_t, ScrubbingAllocator<uint8_t>>;
