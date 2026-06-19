#include "plugin.h"

#include <cstring>

uint64_t Libp2pModuleImpl::addStream(libp2p_stream_t* stream) {
    auto id = m_nextStreamId.fetch_add(1);
    auto entry = std::make_shared<StreamEntry>(stream);
    std::unique_lock<std::shared_mutex> lock(m_streamsLock);
    m_streams[id] = std::move(entry);
    return id;
}

std::shared_ptr<Libp2pModuleImpl::StreamEntry>
Libp2pModuleImpl::getStream(uint64_t id) const {
    std::shared_lock<std::shared_mutex> lock(m_streamsLock);
    auto it = m_streams.find(id);
    return (it != m_streams.end()) ? it->second : nullptr;
}

std::shared_ptr<Libp2pModuleImpl::StreamEntry>
Libp2pModuleImpl::removeStream(uint64_t id) {
    std::unique_lock<std::shared_mutex> lock(m_streamsLock);
    auto it = m_streams.find(id);
    if (it == m_streams.end()) return nullptr;
    auto entry = std::move(it->second);
    m_streams.erase(it);
    return entry;
}

StdLogosResult Libp2pModuleImpl::streamClose(uint64_t streamId) {
    return callSyncStream(streamId, "Failed to close stream",
        [&](libp2p_stream_t* s, SyncPromise* p) {
            return libp2p_stream_close(ctx, s, &Libp2pModuleImpl::promiseCallback, p);
        });
}

StdLogosResult Libp2pModuleImpl::streamCloseWithEOF(uint64_t streamId) {
    return callSyncStream(streamId, "Failed to close stream with EOF",
        [&](libp2p_stream_t* s, SyncPromise* p) {
            return libp2p_stream_closeWithEOF(ctx, s, &Libp2pModuleImpl::promiseCallback, p);
        });
}

StdLogosResult Libp2pModuleImpl::streamRelease(uint64_t streamId) {
    if (!ctx || streamId == 0) return {false, {}, "Invalid stream"};

    auto entry = getStream(streamId);
    if (!entry) return {false, {}, "Stream not found"};

    std::unique_lock<std::shared_mutex> opLock(entry->mtx);
    if (entry->released) return {false, {}, "Stream already released"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_stream_release(ctx, entry->ptr,
                                    &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) {
        delete p;
        return {false, {}, "Failed to release stream (ret=" + std::to_string(ret) + ")"};
    }
    entry->released = true;

    auto r = awaitResult(f);

    opLock.unlock();
    removeStream(streamId);

    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::streamReadExactly(uint64_t streamId, uint64_t len) {
    return callSyncStreamWith(streamId, "Failed to read from stream",
        [&](libp2p_stream_t* s, SyncPromise* p) {
            return libp2p_stream_readExactly(ctx, s, len,
                                             &Libp2pModuleImpl::promiseBufferCallback, p);
        },
        [](const SyncResult& r) -> StdLogosResult {
            return {true, std::string(r.buffer.begin(), r.buffer.end()), ""};
        });
}

StdLogosResult Libp2pModuleImpl::streamReadLp(uint64_t streamId, uint64_t maxSize) {
    return callSyncStreamWith(streamId, "Failed to read LP from stream",
        [&](libp2p_stream_t* s, SyncPromise* p) {
            return libp2p_stream_readLp(ctx, s, maxSize,
                                        &Libp2pModuleImpl::promiseBufferCallback, p);
        },
        [](const SyncResult& r) -> StdLogosResult {
            return {true, std::string(r.buffer.begin(), r.buffer.end()), ""};
        });
}

StdLogosResult Libp2pModuleImpl::streamWrite(uint64_t streamId, const std::string& data) {
    return callSyncStream(streamId, "Failed to write to stream",
        [&](libp2p_stream_t* s, SyncPromise* p) {
            return libp2p_stream_write(ctx, s,
                                       reinterpret_cast<const uint8_t*>(data.data()),
                                       data.size(),
                                       &Libp2pModuleImpl::promiseCallback, p);
        });
}

StdLogosResult Libp2pModuleImpl::streamWriteLp(uint64_t streamId, const std::string& data) {
    return callSyncStream(streamId, "Failed to write LP to stream",
        [&](libp2p_stream_t* s, SyncPromise* p) {
            return libp2p_stream_writeLp(ctx, s,
                                         reinterpret_cast<const uint8_t*>(data.data()),
                                         data.size(),
                                         &Libp2pModuleImpl::promiseCallback, p);
        });
}
