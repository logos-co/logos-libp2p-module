#include "utils.h"

#include <array>
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

std::string base64Decode(const std::string& in) {
    static const auto lookup = [] {
        static constexpr char kAlphabet[] =
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::array<int, 256> t;
        t.fill(-1);
        for (int i = 0; i < 64; ++i) t[static_cast<unsigned char>(kAlphabet[i])] = i;
        return t;
    }();

    if (in.size() % 4 != 0) {
        throw std::invalid_argument("base64Decode: length is not a multiple of 4");
    }

    std::string out;
    out.reserve(in.size() / 4 * 3);
    uint32_t buf = 0;
    int bits = 0;
    size_t pad = 0;
    for (size_t i = 0; i < in.size(); ++i) {
        char c = in[i];
        if (c == '=') {
            if (i < in.size() - 2 || ++pad > 2) {
                throw std::invalid_argument("base64Decode: misplaced padding");
            }
            continue;
        }
        if (pad > 0) {
            throw std::invalid_argument("base64Decode: data after padding");
        }
        int val = lookup[static_cast<unsigned char>(c)];
        if (val < 0) {
            throw std::invalid_argument("base64Decode: invalid character");
        }
        buf = (buf << 6) | static_cast<uint32_t>(val);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((buf >> bits) & 0xff));
        }
    }
    if ((buf & ((1u << bits) - 1)) != 0) {
        throw std::invalid_argument("base64Decode: non-canonical trailing bits");
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
