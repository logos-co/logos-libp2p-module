#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

#include "metric.h"

struct OutboxEvent {
    enum class Kind { Pubsub, Stream };

    Kind kind = Kind::Pubsub;
    // The gossipsub topic, or the protocol of an incoming stream.
    std::string name;
    std::string peerId;
    uint64_t streamId = 0;
    std::vector<uint8_t> data;

    size_t bytes() const { return name.size() + peerId.size() + data.size(); }
};

// Listeners only copy into it, so a slow host emit cannot stall the Nim event thread.
class EventOutbox {
public:
    explicit EventOutbox(size_t maxEvents = 4096, size_t maxBytes = 64 * 1024 * 1024)
        : m_maxEvents(maxEvents), m_maxBytes(maxBytes) {}

    bool push(OutboxEvent event);

    /// Blocks until an event arrives; false once close() runs.
    bool pop(OutboxEvent& out);

    void clear();
    void close();

    std::vector<Metric> metrics() const;

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cond;
    std::deque<OutboxEvent> m_events;
    size_t m_bytes = 0;
    uint64_t m_dropped = 0;
    bool m_closed = false;

    const size_t m_maxEvents;
    const size_t m_maxBytes;
};
