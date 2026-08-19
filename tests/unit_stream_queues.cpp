// InboundStreamQueues in isolation (no Libp2pModuleImpl, links without libp2p.so).

#include <logos_test.h>
#include <stream_queues.h>

#include <string>

namespace {
double metricValue(const std::vector<Metric>& series, const std::string& name) {
    for (const auto& m : series) {
        if (m.name == name) return m.value;
    }
    return -1.0;
}
}  // namespace

LOGOS_TEST(stream_queues_pops_in_order_with_peer_id) {
    InboundStreamQueues queues;
    LOGOS_ASSERT_TRUE(queues.push("/p/1", InboundStream{1, "peer-a"}));
    LOGOS_ASSERT_TRUE(queues.push("/p/1", InboundStream{2, "peer-b"}));

    InboundStream out;
    LOGOS_ASSERT_TRUE(queues.pop("/p/1", 0, out));
    LOGOS_ASSERT_EQ(out.streamId, static_cast<uint64_t>(1));
    LOGOS_ASSERT_TRUE(out.peerId == "peer-a");
    LOGOS_ASSERT_TRUE(queues.pop("/p/1", 0, out));
    LOGOS_ASSERT_EQ(out.streamId, static_cast<uint64_t>(2));
    LOGOS_ASSERT_TRUE(out.peerId == "peer-b");
    LOGOS_ASSERT_FALSE(queues.pop("/p/1", 0, out));
}

LOGOS_TEST(stream_queues_keep_protocols_apart) {
    InboundStreamQueues queues;
    LOGOS_ASSERT_TRUE(queues.push("/p/1", InboundStream{1, "peer-a"}));

    InboundStream out;
    LOGOS_ASSERT_FALSE(queues.pop("/p/2", 0, out));
    LOGOS_ASSERT_TRUE(queues.pop("/p/1", 0, out));
}

LOGOS_TEST(stream_queues_drop_newest_over_cap) {
    InboundStreamQueues queues;
    for (size_t i = 0; i < kMaxInboundStreamsPerProtocol; ++i) {
        LOGOS_ASSERT_TRUE(queues.push("/p/1", InboundStream{i + 1, "peer"}));
    }
    LOGOS_ASSERT_FALSE(queues.push("/p/1", InboundStream{9999, "peer"}));

    auto series = queues.metrics();
    LOGOS_ASSERT_EQ(metricValue(series, "libp2p_module_protocol_stream_queue_depth"),
                    static_cast<double>(kMaxInboundStreamsPerProtocol));
    LOGOS_ASSERT_EQ(metricValue(series, "libp2p_module_protocol_stream_dropped_total"), 1.0);

    InboundStream out;
    LOGOS_ASSERT_TRUE(queues.pop("/p/1", 0, out));
    LOGOS_ASSERT_EQ(out.streamId, static_cast<uint64_t>(1));
}

LOGOS_TEST(stream_queues_remove_takes_a_stream_out) {
    InboundStreamQueues queues;
    LOGOS_ASSERT_TRUE(queues.push("/p/1", InboundStream{1, "peer-a"}));
    LOGOS_ASSERT_TRUE(queues.push("/p/1", InboundStream{2, "peer-b"}));

    LOGOS_ASSERT_TRUE(queues.remove(1));
    LOGOS_ASSERT_FALSE(queues.remove(1));

    InboundStream out;
    LOGOS_ASSERT_TRUE(queues.pop("/p/1", 0, out));
    LOGOS_ASSERT_EQ(out.streamId, static_cast<uint64_t>(2));
}

// releaseAll frees the streams; a drop counter already reported must survive it.
LOGOS_TEST(stream_queues_release_all_keeps_the_drop_counter) {
    InboundStreamQueues queues;
    for (size_t i = 0; i < kMaxInboundStreamsPerProtocol + 1; ++i) {
        queues.push("/p/1", InboundStream{i + 1, "peer"});
    }
    queues.push("/p/2", InboundStream{1, "peer"});
    queues.releaseAll();

    auto series = queues.metrics();
    LOGOS_ASSERT_EQ(metricValue(series, "libp2p_module_protocol_stream_dropped_total"), 1.0);
    LOGOS_ASSERT_EQ(metricValue(series, "libp2p_module_protocol_stream_queue_depth"), 0.0);
    LOGOS_ASSERT_EQ(series.size(), size_t(2));

    InboundStream out;
    LOGOS_ASSERT_FALSE(queues.pop("/p/1", 0, out));
}
