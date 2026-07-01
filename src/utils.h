#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
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
