#include "plugin.h"

#include <charconv>

using json = nlohmann::json;

namespace {

uint64_t asStreamId(const json& v) {
    if (v.is_number_unsigned()) return v.get<uint64_t>();
    if (v.is_number_integer()) return 0;
    if (v.is_string()) {
        const std::string& s = v.get_ref<const std::string&>();
        uint64_t out = 0;
        auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
        if (ec == std::errc{} && p == s.data() + s.size()) return out;
    }
    return 0;
}

bool parseBlob(const std::string& argsJson, const char* what, json& out, StdLogosResult& err) {
    out = json::parse(argsJson, nullptr, false);
    if (!out.is_object()) {
        err = {false, {}, std::string(what) + ": invalid JSON object arg"};
        return false;
    }
    return true;
}

constexpr uint64_t kDefaultReadMax = 1u << 20;

uint64_t asReadMax(const json& a) {
    auto it = a.find("maxSize");
    if (it == a.end() || !it->is_number_unsigned()) return kDefaultReadMax;
    uint64_t v = it->get<uint64_t>();
    return v == 0 ? kDefaultReadMax : v;
}

}  // namespace

StdLogosResult Libp2pModuleImpl::protocolRequest(const std::string& argsJson) {
    json a;
    StdLogosResult err;
    if (!parseBlob(argsJson, "protocolRequest", a, err)) return err;

    std::string peerId, proto, requestB64;
    std::vector<std::string> multiaddrs;
    int64_t timeoutMs = 0;
    uint64_t maxSize = kDefaultReadMax;
    bool expectResponse = true;
    try {
        peerId = a.at("peerId").get<std::string>();
        proto = a.at("proto").get<std::string>();
        requestB64 = a.at("requestB64").get<std::string>();
        if (a.contains("multiaddrs"))
            for (const auto& m : a["multiaddrs"]) multiaddrs.push_back(m.get<std::string>());
        timeoutMs = a.value("timeoutMs", static_cast<int64_t>(0));
        maxSize = asReadMax(a);
        expectResponse = a.value("expectResponse", true);
    } catch (...) {
        return {false, {},
                "protocolRequest: bad args (need {peerId,proto,multiaddrs?,requestB64,timeoutMs?,maxSize?,expectResponse?})"};
    }

    std::string requestBytes;
    try {
        requestBytes = base64Decode(requestB64);
    } catch (const std::exception& e) {
        return {false, {}, std::string("protocolRequest: bad requestB64: ") + e.what()};
    }

    if (!multiaddrs.empty()) {
        auto c = connectPeer(peerId, multiaddrs, timeoutMs > 0 ? timeoutMs : kDefaultOpTimeoutMs);
        if (!c.success) return {false, {}, "protocolRequest: connect failed: " + c.error};
    }

    auto d = dial(peerId, proto);
    if (!d.success) return {false, {}, "protocolRequest: dial failed: " + d.error};
    uint64_t streamId = 0;
    try { streamId = d.value.get<uint64_t>(); } catch (...) {}
    if (streamId == 0) return {false, {}, "protocolRequest: dial returned no stream"};

    auto w = streamWriteLp(streamId, requestBytes);
    if (!w.success) {
        streamRelease(streamId);
        return {false, {}, "protocolRequest: write failed: " + w.error};
    }

    if (!expectResponse) {
        streamCloseWithEOF(streamId);
        streamRelease(streamId);
        return {true, json::object(), ""};
    }

    StreamReadLpRequest readReq{};
    readReq.streamId = streamId;
    readReq.maxSize = static_cast<int64_t>(maxSize);
    auto r = callSyncWith("Failed to read LP from stream",
        [&](SyncPromise* p) {
            return libp2p_ctx_stream_read_lp(ctx, &readReq, &Libp2pModuleImpl::cbRead, p);
        },
        bufferToResult, awaitTimeoutFor(timeoutMs));
    streamRelease(streamId);
    if (!r.success) return {false, {}, "protocolRequest: read failed: " + r.error};

    json out;
    out["responseB64"] = r.value;
    return {true, out, ""};
}

StdLogosResult Libp2pModuleImpl::streamReadLpJson(const std::string& argsJson) {
    json a;
    StdLogosResult err;
    if (!parseBlob(argsJson, "streamReadLpJson", a, err)) return err;
    if (!a.contains("streamId")) return {false, {}, "streamReadLpJson: missing streamId"};

    uint64_t streamId = 0, maxSize = kDefaultReadMax;
    int64_t timeoutMs = 0;
    try {
        streamId = asStreamId(a["streamId"]);
        maxSize = asReadMax(a);
        timeoutMs = a.value("timeoutMs", static_cast<int64_t>(0));
    } catch (...) {
        return {false, {}, "streamReadLpJson: bad args (need {streamId, maxSize?, timeoutMs?})"};
    }

    StreamReadLpRequest readReq{};
    readReq.streamId = streamId;
    readReq.maxSize = static_cast<int64_t>(maxSize);
    auto r = callSyncWith("Failed to read LP from stream",
        [&](SyncPromise* p) {
            return libp2p_ctx_stream_read_lp(ctx, &readReq, &Libp2pModuleImpl::cbRead, p);
        },
        bufferToResult, awaitTimeoutFor(timeoutMs));
    if (!r.success) return r;

    json out;
    out["dataB64"] = r.value;
    return {true, out, ""};
}

StdLogosResult Libp2pModuleImpl::streamWriteLpJson(const std::string& argsJson) {
    json a;
    StdLogosResult err;
    if (!parseBlob(argsJson, "streamWriteLpJson", a, err)) return err;
    if (!a.contains("streamId")) return {false, {}, "streamWriteLpJson: missing streamId"};

    uint64_t streamId = asStreamId(a["streamId"]);
    std::string dataBytes;
    try {
        dataBytes = base64Decode(a.at("dataB64").get<std::string>());
    } catch (const std::exception& e) {
        return {false, {}, std::string("streamWriteLpJson: missing or bad dataB64: ") + e.what()};
    }

    auto w = streamWriteLp(streamId, dataBytes);
    if (!w.success) return w;
    return {true, json::object(), ""};
}

StdLogosResult Libp2pModuleImpl::streamCloseJson(const std::string& argsJson) {
    json a;
    StdLogosResult err;
    if (!parseBlob(argsJson, "streamCloseJson", a, err)) return err;
    if (!a.contains("streamId")) return {false, {}, "streamCloseJson: missing streamId"};
    return streamClose(asStreamId(a["streamId"]));
}

StdLogosResult Libp2pModuleImpl::streamReleaseJson(const std::string& argsJson) {
    json a;
    StdLogosResult err;
    if (!parseBlob(argsJson, "streamReleaseJson", a, err)) return err;
    if (!a.contains("streamId")) return {false, {}, "streamReleaseJson: missing streamId"};
    return streamRelease(asStreamId(a["streamId"]));
}

StdLogosResult Libp2pModuleImpl::protocolAcceptStream(const std::string& argsJson) {
    json a;
    StdLogosResult err;
    if (!parseBlob(argsJson, "protocolAcceptStream", a, err)) return err;

    std::string proto;
    int64_t timeoutMs = 0;
    try {
        proto = a.at("proto").get<std::string>();
        timeoutMs = a.value("timeoutMs", static_cast<int64_t>(0));
    } catch (...) {
        return {false, {}, "protocolAcceptStream: bad args (need {proto, timeoutMs?})"};
    }

    std::unique_lock<std::mutex> lock(m_inboundStreamMutex);
    auto hasStream = [&] {
        auto it = m_inboundStreamQueues.find(proto);
        return it != m_inboundStreamQueues.end() && !it->second.empty();
    };
    if (!hasStream()) {
        auto wait = std::chrono::milliseconds(timeoutMs > 0 ? timeoutMs : 0);
        if (!m_inboundStreamCond.wait_for(lock, wait, hasStream)) {
            return {false, {}, "timeout waiting for inbound stream"};
        }
    }
    auto it = m_inboundStreamQueues.find(proto);
    if (it == m_inboundStreamQueues.end() || it->second.empty()) {
        return {false, {}, "no inbound stream"};
    }
    uint64_t streamId = it->second.front();
    it->second.pop_front();
    lock.unlock();

    return {true, json{{"streamId", streamId}, {"proto", proto}}, ""};
}
