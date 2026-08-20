#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include "metric.h"

// A handle, not a stream: the Nim side keeps the stream open until the host releases it.
struct InboundStream {
    uint64_t streamId = 0;
    std::string peerId;
};

inline constexpr size_t kMaxInboundStreamsPerProtocol = 1024;

class InboundStreamQueues {
public:
    /// Over the cap the newest stream is dropped and counted; the caller releases it.
    bool push(const std::string& proto, InboundStream stream);

    bool pop(const std::string& proto, int64_t timeoutMs, InboundStream& out);

    /// Drops a stream the host released before anyone accepted it.
    bool remove(uint64_t streamId);

    /// Drops the queued handles once the node or the context is gone, which is what frees the streams.
    void releaseAll();

    std::vector<Metric> metrics() const;

private:
    struct Protocol {
        std::deque<InboundStream> streams;
        uint64_t dropped = 0;

        bool retire();
    };

    mutable std::mutex m_mutex;
    std::condition_variable m_cond;
    std::unordered_map<std::string, Protocol> m_protocols;
};
