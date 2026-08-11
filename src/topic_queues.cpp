#include "topic_queues.h"

#include <chrono>
#include <iterator>
#include <utility>

void TopicQueues::setBounds(size_t maxMessages, size_t maxBytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxMessages = maxMessages;
    m_maxBytes = maxBytes;
}

bool TopicQueues::push(const std::string& topic, std::string payload) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_maxMessages == 0 || m_maxBytes == 0) {
        return false;
    }
    auto& t = m_topics[topic];
    // Subtract instead of adding, so a huge payload cannot wrap the sum.
    const bool exceedsByteBound =
        payload.size() > m_maxBytes || t.bytes > m_maxBytes - payload.size();
    if (t.messages.size() >= m_maxMessages || exceedsByteBound) {
        ++t.dropped;
        return false;
    }
    t.bytes += payload.size();
    t.messages.push(std::move(payload));
    m_cond.notify_all();
    return true;
}

bool TopicQueues::pop(const std::string& topic, int64_t timeoutMs, std::string& out) {
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
    t.bytes -= out.size();
    return true;
}

void TopicQueues::release(const std::string& topic) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_topics.find(topic);
    if (it != m_topics.end() && !it->second.retire()) {
        m_topics.erase(it);
    }
}

void TopicQueues::releaseAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_topics.begin(); it != m_topics.end();) {
        it = it->second.retire() ? std::next(it) : m_topics.erase(it);
    }
}

// A scrape must not hold the enqueue path while it builds its series, so it
// takes one cheap sample per topic under the lock and formats outside it.
std::vector<Metric> TopicQueues::metrics() const {
    struct Sample {
        std::string topic;
        size_t depth;
        uint64_t dropped;
    };
    std::vector<Sample> samples;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        samples.reserve(m_topics.size());
        for (const auto& [topic, t] : m_topics) {
            samples.push_back(Sample{topic, t.messages.size(), t.dropped});
        }
    }

    std::vector<Metric> series;
    series.reserve(samples.size() * 2);
    for (auto& s : samples) {
        series.push_back(Metric{"libp2p_module_gossipsub_queue_depth", "gauge",
                                "messages waiting in the per-topic poll queue",
                                {{"topic", s.topic}}, static_cast<double>(s.depth)});
        series.push_back(Metric{"libp2p_module_gossipsub_queue_dropped_total", "counter",
                                "messages dropped because the per-topic poll queue was full",
                                {{"topic", std::move(s.topic)}},
                                static_cast<double>(s.dropped)});
    }
    return series;
}

size_t TopicQueues::topicCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_topics.size();
}

bool TopicQueues::Topic::retire() {
    messages = {};
    bytes = 0;
    return dropped != 0;
}
