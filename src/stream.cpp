#include "plugin.h"

namespace {
// The Nim side caps a single read at MAX_READ_BYTES and carries the size as an
// int64, so screening here both honours the cap and keeps a huge uint64 from
// arriving as a negative length.
bool withinReadCap(uint64_t v) {
    return v <= static_cast<uint64_t>(MAX_READ_BYTES);
}

std::string tooLarge(const char* what) {
    return std::string(what) + " exceeds the " + std::to_string(MAX_READ_BYTES) +
           " byte read cap";
}
}  // namespace

StdLogosResult Libp2pModuleImpl::streamReadExactly(uint64_t streamId, uint64_t len) {
    if (!withinReadCap(len)) return {false, {}, tooLarge("Failed to read from stream: length")};
    StreamReadExactlyRequest req{};
    req.streamId = streamId;
    req.numBytes = static_cast<int64_t>(len);
    return callSyncWith("Failed to read from stream",
        [&](SyncPromise* p) {
            return libp2p_ctx_stream_read_exactly(ctx, &req, &Libp2pModuleImpl::cbRead, p);
        },
        bufferToResult);
}

StdLogosResult Libp2pModuleImpl::streamReadLp(uint64_t streamId, uint64_t maxSize) {
    if (!withinReadCap(maxSize)) {
        return {false, {}, tooLarge("Failed to read LP from stream: maxSize")};
    }
    StreamReadLpRequest req{};
    req.streamId = streamId;
    req.maxSize = static_cast<int64_t>(maxSize);
    return callSyncWith("Failed to read LP from stream",
        [&](SyncPromise* p) {
            return libp2p_ctx_stream_read_lp(ctx, &req, &Libp2pModuleImpl::cbRead, p);
        },
        bufferToResult);
}

StdLogosResult Libp2pModuleImpl::streamWrite(uint64_t streamId, const std::string& data) {
    StreamWriteRequest req{};
    req.streamId = streamId;
    req.data = nimffiBytes(data);
    return callSync("Failed to write to stream", [&](SyncPromise* p) {
        return libp2p_ctx_stream_write(ctx, &req, &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::streamWriteLp(uint64_t streamId, const std::string& data) {
    StreamWriteRequest req{};
    req.streamId = streamId;
    req.data = nimffiBytes(data);
    return callSync("Failed to write LP to stream", [&](SyncPromise* p) {
        return libp2p_ctx_stream_write_lp(ctx, &req, &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::streamClose(uint64_t streamId) {
    return callSync("Failed to close stream", [&](SyncPromise* p) {
        return libp2p_ctx_stream_close(ctx, streamId, &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::streamCloseWithEOF(uint64_t streamId) {
    return callSync("Failed to close stream with EOF", [&](SyncPromise* p) {
        return libp2p_ctx_stream_close_with_eof(ctx, streamId, &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::streamRelease(uint64_t streamId) {
    auto res = callSync("Failed to release stream", [&](SyncPromise* p) {
        return libp2p_ctx_stream_release(ctx, streamId, &Libp2pModuleImpl::cbBool, p);
    });
    m_inboundStreams.remove(streamId);
    return res;
}

void Libp2pModuleImpl::releaseStreamNoWait(uint64_t streamId) {
    if (!ctx) return;
    callAsync([&](SyncPromise* p) {
        return libp2p_ctx_stream_release(ctx, streamId, &Libp2pModuleImpl::cbBool, p);
    });
}
