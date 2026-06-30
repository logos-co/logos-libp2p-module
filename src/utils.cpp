#include "utils.h"

#include <stdexcept>

std::string base64Encode(const std::vector<uint8_t>& data) {
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

std::string hexEncode(const uint8_t* data, size_t len) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    if (!data) return out;
    out.resize(len * 2);
    for (size_t i = 0; i < len; ++i) {
        out[2 * i]     = kDigits[data[i] >> 4];
        out[2 * i + 1] = kDigits[data[i] & 0x0f];
    }
    return out;
}

static int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    throw std::invalid_argument("hex string contains a non-hex character");
}

std::vector<uint8_t> decodeHex(const std::string& hex) {
    if (hex.size() % 2 != 0) {
        throw std::invalid_argument("hex string has an odd length");
    }
    std::vector<uint8_t> out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        out.push_back(static_cast<uint8_t>((hexNibble(hex[i]) << 4) | hexNibble(hex[i + 1])));
    }
    return out;
}
