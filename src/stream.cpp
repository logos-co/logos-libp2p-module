#include "plugin.h"

#include <cstring>

// ---------------------------------------------------------------------------
// Stream registry helpers
// ---------------------------------------------------------------------------

uint64_t Libp2pModuleImpl::addStream(libp2p_stream_t* stream) {
    auto id = m_nextStreamId.fetch_add(1);
    std::unique_lock<std::shared_mutex> lock(m_streamsLock);
    m_streams[id] = stream;
    return id;
}

libp2p_stream_t* Libp2pModuleImpl::getStream(uint64_t id) const {
    std::shared_lock<std::shared_mutex> lock(m_streamsLock);
    auto it = m_streams.find(id);
    return (it != m_streams.end()) ? it->second : nullptr;
}

libp2p_stream_t* Libp2pModuleImpl::removeStream(uint64_t id) {
    std::unique_lock<std::shared_mutex> lock(m_streamsLock);
    auto it = m_streams.find(id);
    if (it == m_streams.end()) return nullptr;
    auto* stream = it->second;
    m_streams.erase(it);
    return stream;
}

bool Libp2pModuleImpl::hasStream(uint64_t id) const {
    std::shared_lock<std::shared_mutex> lock(m_streamsLock);
    return m_streams.find(id) != m_streams.end();
}

// ---------------------------------------------------------------------------
// Stream operations
// ---------------------------------------------------------------------------

StdLogosResult Libp2pModuleImpl::streamClose(uint64_t streamId) {
    if (!ctx || streamId == 0 || !hasStream(streamId))
        return {false, {}, "Invalid stream"};

    auto* stream = getStream(streamId);
    if (!stream) return {false, {}, "Stream not found"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_stream_close(ctx, stream,
                                  &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to close stream"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::streamCloseWithEOF(uint64_t streamId) {
    if (!ctx || streamId == 0 || !hasStream(streamId))
        return {false, {}, "Invalid stream"};

    auto* stream = getStream(streamId);
    if (!stream) return {false, {}, "Stream not found"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_stream_close(ctx, stream,
                                  &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to close stream with EOF"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::streamRelease(uint64_t streamId) {
    if (!ctx || streamId == 0 || !hasStream(streamId))
        return {false, {}, "Invalid stream"};

    auto* stream = getStream(streamId);
    if (!stream) return {false, {}, "Stream not found"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_stream_release(ctx, stream,
                                    &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to release stream"}; }

    removeStream(streamId);

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::streamReadExactly(uint64_t streamId, size_t len) {
    if (!ctx || streamId == 0) return {false, {}, "Invalid stream"};

    auto* stream = getStream(streamId);
    if (!stream) return {false, {}, "Stream not found"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_stream_readExactly(ctx, stream, len,
                                        &Libp2pModuleImpl::promiseBufferCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to read from stream"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, std::string(r.buffer.begin(), r.buffer.end()), ""};
}

StdLogosResult Libp2pModuleImpl::streamReadLp(uint64_t streamId, size_t maxSize) {
    if (!ctx || streamId == 0) return {false, {}, "Invalid stream"};

    auto* stream = getStream(streamId);
    if (!stream) return {false, {}, "Stream not found"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_stream_readLp(ctx, stream, maxSize,
                                   &Libp2pModuleImpl::promiseBufferCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to read LP from stream"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, std::string(r.buffer.begin(), r.buffer.end()), ""};
}

StdLogosResult Libp2pModuleImpl::streamWrite(uint64_t streamId, const std::string& data) {
    if (!ctx || streamId == 0) return {false, {}, "Invalid stream"};

    auto* stream = getStream(streamId);
    if (!stream) return {false, {}, "Stream not found"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_stream_write(ctx, stream,
                                  reinterpret_cast<uint8_t*>(const_cast<char*>(data.data())),
                                  data.size(),
                                  &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to write to stream"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::streamWriteLp(uint64_t streamId, const std::string& data) {
    if (!ctx || streamId == 0) return {false, {}, "Invalid stream"};

    auto* stream = getStream(streamId);
    if (!stream) return {false, {}, "Stream not found"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_stream_writeLp(ctx, stream,
                                    reinterpret_cast<uint8_t*>(const_cast<char*>(data.data())),
                                    data.size(),
                                    &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to write LP to stream"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}
