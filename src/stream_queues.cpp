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
        it = it->second.retire() ? std::next(it) : m_protocols.erase(it);
    }
}

// A scrape samples under the lock and formats outside it, so it never holds the enqueue path.
std::vector<Metric> InboundStreamQueues::metrics() const {
    struct Sample {
        std::string proto;
        size_t depth;
        uint64_t dropped;
    };
    std::vector<Sample> samples;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        samples.reserve(m_protocols.size());
        for (const auto& [proto, p] : m_protocols) {
            samples.push_back(Sample{proto, p.streams.size(), p.dropped});
        }
    }

    std::vector<Metric> series;
    series.reserve(samples.size() * 2);
    for (auto& s : samples) {
        series.push_back(Metric{"libp2p_module_protocol_stream_queue_depth", "gauge",
                                "inbound streams waiting in the per-protocol accept queue",
                                {{"proto", s.proto}}, static_cast<double>(s.depth)});
        series.push_back(Metric{"libp2p_module_protocol_stream_dropped_total", "counter",
                                "inbound streams dropped because the accept queue was full",
                                {{"proto", std::move(s.proto)}},
                                static_cast<double>(s.dropped)});
    }
    return series;
}

bool InboundStreamQueues::Protocol::retire() {
    streams.clear();
    return dropped != 0;
}
