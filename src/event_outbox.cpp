#include "event_outbox.h"

#include <algorithm>
#include <utility>

bool EventOutbox::push(const OutboxEvent& event) {
    const size_t size = event.bytes();
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_closed) {
        return false;
    }
    // Subtract instead of adding, so a huge event cannot wrap the sum.
    const bool exceedsByteBound = size > m_maxBytes || m_bytes > m_maxBytes - size;
    if (m_events.size() >= m_maxEvents || exceedsByteBound) {
        ++m_dropped;
        return false;
    }
    m_events.push_back(event);
    m_events.back().generation = m_generation;
    if (event.kind == OutboxEvent::Kind::Pubsub) {
        auto it = m_topicGenerations.find(event.name);
        m_events.back().topicGeneration = it == m_topicGenerations.end() ? 0 : it->second;
    }
    m_bytes += size;
    m_cond.notify_one();
    return true;
}

bool EventOutbox::pop(OutboxEvent& out) {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cond.wait(lock, [&] { return m_closed || !m_events.empty(); });
    if (m_closed) {
        return false;
    }

    out = std::move(m_events.front());
    m_events.pop_front();
    m_bytes -= out.bytes();
    m_lastPopped = out.kind == OutboxEvent::Kind::Pubsub ? out.name : std::string();
    return true;
}

void EventOutbox::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_events.clear();
    m_bytes = 0;
    m_topicGenerations.clear();
    ++m_generation;
}

void EventOutbox::dropTopic(const std::string& topic) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto kept = std::remove_if(m_events.begin(), m_events.end(), [&](const OutboxEvent& e) {
        if (e.kind != OutboxEvent::Kind::Pubsub || e.name != topic) return false;
        m_bytes -= e.bytes();
        return true;
    });
    m_events.erase(kept, m_events.end());
    m_topicGenerations[topic] = ++m_topicCounter;
    if (m_topicGenerations.size() > kMaxTrackedTopics) {
        pruneTopicGenerations();
    }
}

void EventOutbox::pruneTopicGenerations() {
    std::unordered_set<std::string> queued;
    for (const auto& e : m_events) {
        if (e.kind == OutboxEvent::Kind::Pubsub) queued.insert(e.name);
    }
    for (auto it = m_topicGenerations.begin(); it != m_topicGenerations.end();) {
        const bool referenced = it->first == m_lastPopped || queued.count(it->first) > 0;
        it = referenced ? std::next(it) : m_topicGenerations.erase(it);
    }
}

size_t EventOutbox::trackedTopics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_topicGenerations.size();
}

bool EventOutbox::stale(const OutboxEvent& event) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (event.generation != m_generation) return true;
    if (event.kind != OutboxEvent::Kind::Pubsub) return false;
    auto it = m_topicGenerations.find(event.name);
    return (it == m_topicGenerations.end() ? 0 : it->second) != event.topicGeneration;
}

uint64_t EventOutbox::generation() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_generation;
}

uint64_t EventOutbox::dropped() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_dropped;
}

void EventOutbox::addDropped(uint64_t count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_dropped += count;
}

void EventOutbox::close() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_closed = true;
    m_events.clear();
    m_bytes = 0;
    m_cond.notify_all();
}

std::vector<Metric> EventOutbox::metrics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return {
        Metric{"libp2p_module_event_outbox_depth", "gauge",
               "events waiting for the module event worker", {},
               static_cast<double>(m_events.size())},
        Metric{"libp2p_module_event_outbox_dropped_total", "counter",
               "events dropped because the module event outbox was full", {},
               static_cast<double>(m_dropped)},
    };
}
