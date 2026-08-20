#include "stream_queues.h"

#include <chrono>
#include <iterator>
#include <utility>

bool InboundStreamQueues::push(const std::string& proto, InboundStream stream) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto& p = m_protocols[proto];
        if (p.streams.size() >= kMaxInboundStreamsPerProtocol) {
            ++p.dropped;
            return false;
        }
        p.streams.push_back(std::move(stream));
    }
    m_cond.notify_all();
    return true;
}

bool InboundStreamQueues::pop(const std::string& proto, int64_t timeoutMs, InboundStream& out) {
    std::unique_lock<std::mutex> lock(m_mutex);
    auto ready = [&] {
        auto it = m_protocols.find(proto);
        return it != m_protocols.end() && !it->second.streams.empty();
    };
    if (!m_cond.wait_for(lock, std::chrono::milliseconds(timeoutMs), ready)) {
        return false;
    }
    auto& p = m_protocols.find(proto)->second;
    out = std::move(p.streams.front());
    p.streams.pop_front();
    return true;
}

bool InboundStreamQueues::remove(uint64_t streamId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& [proto, p] : m_protocols) {
        for (auto it = p.streams.begin(); it != p.streams.end(); ++it) {
            if (it->streamId == streamId) {
                p.streams.erase(it);
                return true;
            }
        }
    }
    return false;
}

void InboundStreamQueues::releaseAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto it = m_protocols.begin(); it != m_protocols.end();) {
        const bool keepForDropCounter = it->second.retire();
        it = keepForDropCounter ? std::next(it) : m_protocols.erase(it);
    }
}

std::vector<Metric> InboundStreamQueues::metrics() const {
    std::vector<QueueSample> samples;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        samples.reserve(m_protocols.size());
        for (const auto& [proto, p] : m_protocols) {
            samples.push_back(QueueSample{proto, p.streams.size(), p.dropped});
        }
    }

    return queueSeries(std::move(samples),
        QueueSeriesNames{"proto",
                         "libp2p_module_protocol_stream_queue_depth",
                         "inbound streams waiting in the per-protocol accept queue",
                         "libp2p_module_protocol_stream_dropped_total",
                         "inbound streams dropped because the accept queue was full"});
}

bool InboundStreamQueues::Protocol::retire() {
    streams.clear();
    return dropped != 0;
}
