#include "plugin.h"

#include <chrono>

StdLogosResult Libp2pModuleImpl::gossipsubPublish(
    const std::string& topic, const std::string& data)
{
    PublishRequest req{};
    req.topic = nimffi_str(topic.c_str());
    req.data = nimffiBytes(data);
    return callSync("Failed to publish", [&](SyncPromise* p) {
        return libp2p_ctx_gossipsub_publish(ctx, &req, &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::gossipsubSubscribe(const std::string& topic) {
    if (!ctx) return {false, {}, "No libp2p context"};
    // Delivered messages surface through the on_pubsub_message listener, which
    // needs the emit snapshot published to forward gossipsubMessage events.
    publishEmitEvent();
    return callSync("Failed to subscribe", [&](SyncPromise* p) {
        return libp2p_ctx_gossipsub_subscribe(ctx, nimffi_str(topic.c_str()),
                                              &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::gossipsubUnsubscribe(const std::string& topic) {
    if (!ctx) return {false, {}, "No libp2p context"};
    return callSync("Failed to unsubscribe", [&](SyncPromise* p) {
        return libp2p_ctx_gossipsub_unsubscribe(ctx, nimffi_str(topic.c_str()),
                                                &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::gossipsubNextMessage(const std::string& topic, int64_t timeoutMs) {
    std::unique_lock<std::mutex> lock(m_queueMutex);

    // .find() avoids inserting an empty queue for every polled topic.
    auto hasMessage = [&] {
        auto it = m_topicQueues.find(topic);
        return it != m_topicQueues.end() && !it->second.empty();
    };

    if (!hasMessage()) {
        if (!m_queueCond.wait_for(lock, std::chrono::milliseconds(timeoutMs), hasMessage)) {
            return {false, {}, "timeout waiting for message"};
        }
    }

    auto it = m_topicQueues.find(topic);
    if (it == m_topicQueues.end() || it->second.empty()) {
        return {false, {}, "queue is empty"};
    }
    std::string msg = std::move(it->second.front());
    it->second.pop();
    return {true, msg, ""};
}
