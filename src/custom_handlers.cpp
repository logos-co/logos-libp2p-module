#include "plugin.h"

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Custom Protocol Handlers
// ---------------------------------------------------------------------------

static void mountProtocolResultCallback(int ret, const char* msg, size_t len, void* userData) {
    (void)ret; (void)msg; (void)len; (void)userData;
}

void Libp2pModuleImpl::protocolHandler(
    libp2p_ctx_t* /*ctx*/, libp2p_stream_t* stream,
    const char* proto, size_t protoLen, void* userData)
{
    auto* handlerCtx = static_cast<ProtocolHandlerCtx*>(userData);
    if (!handlerCtx || !handlerCtx->instance || !stream) return;

    auto* self = handlerCtx->instance;
    std::string protoStr = (proto && protoLen > 0) ? std::string(proto, protoLen) : std::string();

    uint64_t streamId = self->addStream(stream);

    json j;
    j["streamId"] = streamId;
    j["proto"] = protoStr;
    self->emitEventSafe("protocolStream", j.dump());
}

StdLogosResult Libp2pModuleImpl::mountProtocol(const std::string& proto) {
    if (!ctx || !m_started) return {false, {}, "No libp2p context"};
    if (proto.empty()) return {false, {}, "Protocol string is empty"};

    auto handlerCtx = std::make_unique<ProtocolHandlerCtx>();
    handlerCtx->instance = this;
    handlerCtx->proto = proto;

    int ret = libp2p_mount_protocol(
        ctx, proto.c_str(),
        &Libp2pModuleImpl::protocolHandler,
        &mountProtocolResultCallback,
        handlerCtx.get());

    if (ret != RET_OK) {
        return {false, {}, "Failed to mount protocol"};
    }

    m_protocolHandlerContexts.push_back(std::move(handlerCtx));
    return {true, {}, ""};
}
