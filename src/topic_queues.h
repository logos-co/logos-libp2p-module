#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

#include "metric.h"

// Per-topic backlog that gossipsubNextMessage() drains. Both bounds are needed:
// 1024 messages at the 1 MiB gossipsub message limit is still 1 GiB per topic.
// Overflow drops the newest, leaving a coherent prefix of the stream.
class TopicQueues {
public:
    void setBounds(size_t maxMessages, size_t maxBytes);

    /// Either bound at 0 disables the backlog. A payload larger than the byte
    /// bound never fits, so keep the bound above `gossipsubMaxMessageSize`.
    bool push(const std::string& topic, std::string payload);

    bool pop(const std::string& topic, int64_t timeoutMs, std::string& out);

    /// Frees payloads. The drop counter survives, since resetting a Prometheus
    /// counter reads as a target restart.
    void release(const std::string& topic);

    void releaseAll();

    std::vector<Metric> metrics() const;

    /// Entries held for their drop counter, so a test can prove the map does not
    /// grow one series per topic the node ever subscribed to.
    size_t topicCount() const;

private:
    struct Topic {
        std::queue<std::string> messages;
        size_t bytes = 0;
        uint64_t dropped = 0;

        /// Frees the payloads and reports whether the entry still carries a drop
        /// count worth exporting.
        bool retire();
    };

    mutable std::mutex m_mutex;
    std::condition_variable m_cond;
    // One entry per live topic, plus the ones release() kept for their counter.
    std::unordered_map<std::string, Topic> m_topics;

    size_t m_maxMessages = 1024;
    size_t m_maxBytes = 4 * 1024 * 1024;
};
