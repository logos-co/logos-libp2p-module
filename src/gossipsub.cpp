#include "plugin.h"

#include <cstring>

// ---------------------------------------------------------------------------
// Gossipsub
// ---------------------------------------------------------------------------

StdLogosResult Libp2pModuleImpl::gossipsubPublish(
    const std::string& topic, const std::string& data)
{
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_gossipsub_publish(
        ctx, topic.c_str(),
        reinterpret_cast<uint8_t*>(const_cast<char*>(data.data())),
        data.size(),
        &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to publish"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

static void gossipsubResultCallback(int ret, const char* msg, size_t len, void* userData) {
    (void)ret; (void)msg; (void)len; (void)userData;
}

StdLogosResult Libp2pModuleImpl::gossipsubSubscribe(const std::string& topic) {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto subCtx = std::make_unique<SubscribeCtx>();
    subCtx->instance = this;

    int ret = libp2p_gossipsub_subscribe(
        ctx, topic.c_str(),
        &Libp2pModuleImpl::topicHandler,
        &gossipsubResultCallback,
        subCtx.get());

    if (ret != RET_OK) {
        return {false, {}, "Failed to subscribe"};
    }

    m_subscribeContexts.push_back(std::move(subCtx));
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::gossipsubUnsubscribe(const std::string& topic) {
    if (!ctx) return {false, {}, "No libp2p context"};

    SubscribeCtx* subCtxPtr = nullptr;
    if (!m_subscribeContexts.empty()) {
        subCtxPtr = m_subscribeContexts.back().get();
    }

    int ret = libp2p_gossipsub_unsubscribe(
        ctx, topic.c_str(),
        &Libp2pModuleImpl::topicHandler,
        &gossipsubResultCallback,
        subCtxPtr);

    if (ret != RET_OK) {
        return {false, {}, "Failed to unsubscribe"};
    }

    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::gossipsubNextMessage(const std::string& topic, int timeoutMs) {
    std::unique_lock<std::mutex> lock(m_queueMutex);

    auto& queue = m_topicQueues[topic];

    if (queue.empty()) {
        if (!m_queueCond.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                                   [&] { return !queue.empty(); })) {
            return {false, {}, "timeout waiting for message"};
        }
    }

    if (!queue.empty()) {
        std::string msg = std::move(queue.front());
        queue.pop();
        return {true, msg, ""};
    }

    return {false, {}, "queue is empty"};
}
