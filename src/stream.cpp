#include "plugin.h"

StdLogosResult Libp2pModuleImpl::streamReadExactly(uint64_t streamId, uint64_t len) {
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
    return callSync("Failed to release stream", [&](SyncPromise* p) {
        return libp2p_ctx_stream_release(ctx, streamId, &Libp2pModuleImpl::cbBool, p);
    });
}
