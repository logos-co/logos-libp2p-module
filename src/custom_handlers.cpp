#include "plugin.h"

using json = nlohmann::json;

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

    json j;
    j["streamId"] = streamId;
    j["proto"] = protoStr;
    self->emitEventSafe("protocolStream", j.dump());
}

void Libp2pModuleImpl::mountCompleteCallback(int ret, const char* msg, size_t len,
                                              void* userData) {
    auto* hCtx = static_cast<ProtocolHandlerCtx*>(userData);
    SyncResult r;
    r.ok = (ret == RET_OK);
    r.message = (msg && len > 0) ? std::string(msg, len) : std::string();
    hCtx->mountPromise->set_value(std::move(r));
    delete hCtx->mountPromise;
    hCtx->mountPromise = nullptr;
}

StdLogosResult Libp2pModuleImpl::mountProtocol(const std::string& proto) {
    if (!ctx) return {false, {}, "No libp2p context"};
    if (proto.empty()) return {false, {}, "Protocol string is empty"};
    // Without emitEvent, protocolHandler would register a stream that no caller
    // could ever read, close, or release — leaking the stream.
    if (!emitEvent) return {false, {}, "emitEvent must be set before mounting a protocol"};

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
