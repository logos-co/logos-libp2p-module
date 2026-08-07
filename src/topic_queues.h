#pragma once

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "metric.h"

// Per-topic backlog that gossipsubNextMessage() drains. Both bounds are needed:
// 1024 messages at the 1 MiB gossipsub message limit is still 1 GiB per topic.
// Overflow drops the newest, leaving a coherent prefix of the stream.
class TopicQueues {
public:
    void setBounds(size_t maxMessages, size_t maxBytes) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_maxMessages = maxMessages;
        m_maxBytes = maxBytes;
    }

    /// Either bound at 0 disables the backlog. An empty queue accepts any single
    /// message, so a byte bound below the message size cannot stall a topic.
    bool push(const std::string& topic, std::string payload) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_maxMessages == 0 || m_maxBytes == 0) {
            return false;
        }
        auto& t = m_topics[topic];
        if (t.messages.size() >= m_maxMessages ||
            (!t.messages.empty() && t.bytes + payload.size() > m_maxBytes)) {
            ++t.dropped;
            return false;
        }
        t.bytes += payload.size();
        t.messages.push(std::move(payload));
        m_cond.notify_all();
        return true;
    }

    bool pop(const std::string& topic, int64_t timeoutMs, std::string& out) {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto ready = [&] {
            auto it = m_topics.find(topic);
            return it != m_topics.end() && !it->second.messages.empty();
        };
        if (!m_cond.wait_for(lock, std::chrono::milliseconds(timeoutMs), ready)) {
            return false;
        }
        auto& t = m_topics.find(topic)->second;
        out = std::move(t.messages.front());
        t.messages.pop();
        t.bytes -= std::min(t.bytes, out.size());
        return true;
    }

    /// Frees payloads. The drop counter survives, since resetting a Prometheus
    /// counter reads as a target restart.
    void release(const std::string& topic) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_topics.find(topic);
        if (it != m_topics.end()) {
            it->second.discard();
        }
    }

    void releaseAll() {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& entry : m_topics) {
            entry.second.discard();
        }
    }

    std::vector<Metric> metrics() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<Metric> series;
        series.reserve(m_topics.size() * 2);
        for (const auto& [topic, t] : m_topics) {
            series.push_back(Metric{"libp2p_module_gossipsub_queue_depth", "gauge",
                                    "messages waiting in the per-topic poll queue",
                                    {{"topic", topic}},
                                    static_cast<double>(t.messages.size())});
            series.push_back(Metric{"libp2p_module_gossipsub_queue_dropped_total", "counter",
                                    "messages dropped because the per-topic poll queue was full",
                                    {{"topic", topic}}, static_cast<double>(t.dropped)});
        }
        return series;
    }

private:
    struct Topic {
        std::queue<std::string> messages;
        size_t bytes = 0;
        uint64_t dropped = 0;

        void discard() {
            messages = {};
            bytes = 0;
        }
    };

    mutable std::mutex m_mutex;
    std::condition_variable m_cond;
    // One entry per topic ever pushed to; release() keeps it for the counter.
    std::unordered_map<std::string, Topic> m_topics;

    size_t m_maxMessages = 1024;
    size_t m_maxBytes = 4 * 1024 * 1024;
};
