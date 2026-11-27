#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
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
    uint64_t generation = 0;
    uint64_t topicGeneration = 0;

    size_t bytes() const {
        return sizeof(kind) + sizeof(streamId) + sizeof(generation) + name.size() + peerId.size() + data.size();
    }
};

// Listeners only copy into it, so a slow host emit cannot stall the Nim event thread.
class EventOutbox {
public:
    // maxBytes caps the payload bytes, not the container and allocator overhead.
    explicit EventOutbox(size_t maxEvents = 4096, size_t maxBytes = 64 * 1024 * 1024)
        : m_maxEvents(maxEvents), m_maxBytes(maxBytes) {}

    bool push(const OutboxEvent& event);

    /// Blocks until an event arrives; false once close() runs.
    bool pop(OutboxEvent& out);

    /// Drops the backlog and starts a new generation, so an event popped earlier reads as stale.
    void clear();
    void close();

    /// Removes the queued events of a topic and marks the ones already popped as stale, leaving other topics alone.
    void dropTopic(const std::string& topic);

    /// True when clear() or dropTopic() retired the event after it was pushed.
    bool stale(const OutboxEvent& event) const;

    size_t trackedTopics() const;

    uint64_t generation() const;
    uint64_t dropped() const;
    void addDropped(uint64_t count);

    std::vector<Metric> metrics() const;

private:
    mutable std::mutex m_mutex;
    std::condition_variable m_cond;
    std::deque<OutboxEvent> m_events;
    size_t m_bytes = 0;
    uint64_t m_dropped = 0;
    uint64_t m_generation = 0;
    uint64_t m_topicCounter = 0;
    // Only dropped topics. clear() empties it, and dropTopic() prunes it past kMaxTrackedTopics.
    std::unordered_map<std::string, uint64_t> m_topicGenerations;
    // The one consumer delivers a single event at a time, so only the last popped pubsub topic can still be in flight.
    std::string m_lastPopped;

    static constexpr size_t kMaxTrackedTopics = 1024;
    void pruneTopicGenerations();
    bool m_closed = false;

    const size_t m_maxEvents;
    const size_t m_maxBytes;
};
