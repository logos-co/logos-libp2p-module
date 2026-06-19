#include "plugin.h"

#include <algorithm>
#include <cstring>

StdLogosResult Libp2pModuleImpl::gossipsubPublish(
    const std::string& topic, const std::string& data)
{
    return callSync("Failed to publish", [&](SyncPromise* p) {
        // cbinding signature is non-const but data is copied before enqueue.
        return libp2p_gossipsub_publish(
            ctx, topic.c_str(),
            reinterpret_cast<uint8_t*>(const_cast<char*>(data.data())),
            data.size(),
            &Libp2pModuleImpl::promiseCallback, p);
    });
}

// Surface async (un)subscribe outcomes as gossipsubResult events. Drops the
// ctx from the registry when the ack fires for an unsubscribing ctx.
void Libp2pModuleImpl::gossipsubResultCallback(int ret, const char* msg, size_t len, void* userData) {
    auto* subCtx = static_cast<SubscribeCtx*>(userData);
    if (!subCtx || !subCtx->instance) return;

    auto* instance = subCtx->instance;
    const std::string topic = subCtx->topic;
    const bool unsubscribing = subCtx->unsubscribing.load(std::memory_order_acquire);

    nlohmann::json j;
    j["topic"] = topic;
    j["result"] = ret;
    j["message"] = (msg && len > 0) ? std::string(msg, len) : std::string();
    instance->emitEventSafe("gossipsubResult", j.dump());

    if (unsubscribing && ret == RET_OK) {
        std::lock_guard<std::mutex> lock(instance->m_subscribeContextsLock);
        auto& vec = instance->m_subscribeContexts;
        vec.erase(std::remove_if(vec.begin(), vec.end(),
            [subCtx](const std::unique_ptr<SubscribeCtx>& c) { return c.get() == subCtx; }),
            vec.end());
    }
}

StdLogosResult Libp2pModuleImpl::gossipsubSubscribe(const std::string& topic) {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto subCtx = std::make_unique<SubscribeCtx>();
    subCtx->instance = this;
    subCtx->topic = topic;

    int ret = libp2p_gossipsub_subscribe(
        ctx, topic.c_str(),
        &Libp2pModuleImpl::topicHandler,
        &Libp2pModuleImpl::gossipsubResultCallback,
        subCtx.get());

    if (ret != RET_OK) {
        return {false, {}, "Failed to subscribe"};
    }

    {
        std::lock_guard<std::mutex> lock(m_subscribeContextsLock);
        m_subscribeContexts.push_back(std::move(subCtx));
    }
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::gossipsubUnsubscribe(const std::string& topic) {
    if (!ctx) return {false, {}, "No libp2p context"};

    SubscribeCtx* ctxPtr = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_subscribeContextsLock);
        auto it = std::find_if(m_subscribeContexts.begin(), m_subscribeContexts.end(),
            [&](const std::unique_ptr<SubscribeCtx>& c) {
                return c->topic == topic
                    && !c->unsubscribing.load(std::memory_order_acquire);
            });
        if (it == m_subscribeContexts.end()) {
            return {false, {}, "Not subscribed to topic"};
        }
        ctxPtr = it->get();
        ctxPtr->unsubscribing.store(true, std::memory_order_release);
    }

    int ret = libp2p_gossipsub_unsubscribe(
        ctx, topic.c_str(),
        &Libp2pModuleImpl::topicHandler,
        &Libp2pModuleImpl::gossipsubResultCallback,
        ctxPtr);

    if (ret != RET_OK) {
        ctxPtr->unsubscribing.store(false, std::memory_order_release);
        return {false, {}, "Failed to unsubscribe (ret=" + std::to_string(ret) + ")"};
    }
    return {true, {}, ""};
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
