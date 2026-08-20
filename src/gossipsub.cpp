#include "plugin.h"

StdLogosResult Libp2pModuleImpl::gossipsubPublish(
    const std::string& topic, const std::string& data)
{
    PublishRequest req{};
    req.topic = nimffi_str(topic.c_str());
    req.data = nimffiBytes(data);
    return callSync("Failed to publish", [&](SyncPromise* p) {
        return libp2p_ctx_gossipsub_publish(ctx, &req, &Libp2pModuleImpl::cbPublish, p);
    });
}

StdLogosResult Libp2pModuleImpl::gossipsubSubscribe(const std::string& topic) {
    if (!hasCtx()) return {false, {}, "No libp2p context"};
    // Delivered messages surface through the on_pubsub_message listener, which
    // needs the emit snapshot published to forward gossipsubMessage events.
    publishEmitEvent();
    return callSync("Failed to subscribe", [&](SyncPromise* p) {
        return libp2p_ctx_gossipsub_subscribe(ctx, nimffi_str(topic.c_str()),
                                              &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::gossipsubUnsubscribe(const std::string& topic) {
    if (!hasCtx()) return {false, {}, "No libp2p context"};
    auto res = callSync("Failed to unsubscribe", [&](SyncPromise* p) {
        return libp2p_ctx_gossipsub_unsubscribe(ctx, nimffi_str(topic.c_str()),
                                                &Libp2pModuleImpl::cbBool, p);
    });
    if (res.success) {
        m_topicQueues.release(topic);
    }
    return res;
}

StdLogosResult Libp2pModuleImpl::gossipsubNextMessage(const std::string& topic, int64_t timeoutMs) {
    std::string msg;
    if (!m_topicQueues.pop(topic, timeoutMs, msg)) {
        return {false, {}, "timeout waiting for message"};
    }
    return {true, msg, ""};
}
