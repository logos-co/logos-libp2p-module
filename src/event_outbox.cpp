#include "event_outbox.h"

#include <utility>

bool EventOutbox::push(OutboxEvent event) {
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
    event.generation = m_generation;
    m_events.push_back(std::move(event));
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
    return true;
}

void EventOutbox::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_events.clear();
    m_bytes = 0;
    ++m_generation;
}

uint64_t EventOutbox::generation() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_generation;
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
