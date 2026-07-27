#include "plugin.h"

using json = nlohmann::json;

static constexpr size_t kMaxInboundStreamsPerProto = 1024;

bool Libp2pModuleImpl::enqueueInboundStream(const std::string& proto,
                                            libp2p_stream_t* stream, uint64_t streamId) {
    bool queued = false;
    {
        std::lock_guard<std::mutex> lock(m_inboundStreamMutex);
        auto& q = m_inboundStreamQueues[proto];
        if (q.size() < kMaxInboundStreamsPerProto) {
            q.push_back(streamId);
            queued = true;
        }
    }
    if (!queued) {
        removeStream(streamId);
        libp2p_stream_release(ctx, stream,
                              +[](int, const char*, size_t, void*) {}, nullptr);
        return false;
    }
    m_inboundStreamCond.notify_all();
    return true;
}

void Libp2pModuleImpl::protocolHandler(
    libp2p_ctx_t* /*ctx*/, libp2p_stream_t* stream,
    const char* proto, size_t protoLen, void* userData)
{
    auto* handlerCtx = static_cast<ProtocolHandlerCtx*>(userData);
    if (!handlerCtx || !handlerCtx->instance || !stream) return;

    auto* self = handlerCtx->instance;
    std::string protoStr = (proto && protoLen > 0)
        ? std::string(proto, protoLen)
        : handlerCtx->proto;

    uint64_t streamId = self->addStream(stream);
    if (!self->enqueueInboundStream(protoStr, stream, streamId)) return;

    json j;
    j["streamId"] = streamId;
    j["proto"] = protoStr;
    self->emitEventSafe("protocolStream", j.dump());
}

void Libp2pModuleImpl::mountCompleteCallback(int ret, const char* msg, size_t len,
                                              void* userData) {
    auto* hCtx = static_cast<ProtocolHandlerCtx*>(userData);
    if (!hCtx || !hCtx->mountPromise) return;
    hCtx->mountPromise->set_value(basicResult(ret, msg, len));
    delete hCtx->mountPromise;
    hCtx->mountPromise = nullptr;
}

StdLogosResult Libp2pModuleImpl::mountProtocol(const std::string& proto) {
    if (!ctx) return {false, {}, "No libp2p context"};
    if (proto.empty()) return {false, {}, "Protocol string is empty"};
    publishEmitEvent();

    auto handlerCtx = std::make_unique<ProtocolHandlerCtx>();
    handlerCtx->instance = this;
    handlerCtx->proto = proto;
    handlerCtx->mountPromise = new SyncPromise();
    auto f = handlerCtx->mountPromise->get_future();

    int ret = libp2p_mount_protocol(
        ctx, proto.c_str(),
        &Libp2pModuleImpl::protocolHandler,
        &Libp2pModuleImpl::mountCompleteCallback,
        handlerCtx.get());

    if (ret != RET_OK) {
        delete handlerCtx->mountPromise;
        handlerCtx->mountPromise = nullptr;
        return {false, {}, "Failed to mount protocol"};
    }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};

    {
        std::lock_guard<std::mutex> lock(m_protocolHandlersLock);
        m_protocolHandlerContexts.push_back(std::move(handlerCtx));
    }
    return {true, {}, ""};
}
