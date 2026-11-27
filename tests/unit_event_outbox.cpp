// EventOutbox bounds in isolation (no Libp2pModuleImpl, links without libp2p.so).

#include <logos_test.h>
#include <event_outbox.h>

#include <atomic>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {
const size_t kFixed = OutboxEvent{}.bytes();

OutboxEvent pubsub(const std::string& topic, size_t size) {
    OutboxEvent e;
    e.name = topic;
    e.data.assign(size, 'x');
    return e;
}

double dropped(const EventOutbox& outbox) {
    return outbox.metrics()[1].value;
}
}

LOGOS_TEST(event_outbox_drops_newest_over_event_bound) {
    EventOutbox outbox(2, 4096);

    LOGOS_ASSERT_TRUE(outbox.push(pubsub("a", 1)));
    LOGOS_ASSERT_TRUE(outbox.push(pubsub("b", 1)));
    LOGOS_ASSERT_FALSE(outbox.push(pubsub("c", 1)));
    LOGOS_ASSERT_TRUE(dropped(outbox) == 1);

    OutboxEvent out;
    LOGOS_ASSERT_TRUE(outbox.pop(out));
    LOGOS_ASSERT_TRUE(out.name == "a");
    LOGOS_ASSERT_TRUE(outbox.pop(out));
    LOGOS_ASSERT_TRUE(out.name == "b");
}

LOGOS_TEST(event_outbox_drops_newest_over_byte_bound) {
    EventOutbox outbox(1024, 2 * kFixed + 16);

    LOGOS_ASSERT_TRUE(outbox.push(pubsub("t", 9)));
    LOGOS_ASSERT_FALSE(outbox.push(pubsub("t", 9)));
    LOGOS_ASSERT_TRUE(outbox.push(pubsub("t", 5)));
    LOGOS_ASSERT_FALSE(outbox.push(pubsub("t", 4096)));
    LOGOS_ASSERT_TRUE(dropped(outbox) == 2);
}

LOGOS_TEST(event_outbox_frees_bytes_on_pop_and_clear) {
    EventOutbox outbox(1024, kFixed + 16);

    LOGOS_ASSERT_TRUE(outbox.push(pubsub("t", 15)));
    OutboxEvent out;
    LOGOS_ASSERT_TRUE(outbox.pop(out));
    LOGOS_ASSERT_TRUE(outbox.push(pubsub("t", 15)));
    outbox.clear();
    LOGOS_ASSERT_TRUE(outbox.push(pubsub("t", 15)));
}

LOGOS_TEST(event_outbox_close_wakes_a_blocked_pop) {
    EventOutbox outbox;
    bool popped = true;
    std::atomic<bool> entered{false};
    std::thread worker([&] {
        OutboxEvent out;
        entered = true;
        popped = outbox.pop(out);
    });
    while (!entered.load()) {
        std::this_thread::yield();
    }
    outbox.close();
    worker.join();

    LOGOS_ASSERT_FALSE(popped);
    LOGOS_ASSERT_FALSE(outbox.push(pubsub("t", 1)));
}

LOGOS_TEST(event_outbox_exports_depth_and_drops) {
    EventOutbox outbox(1, 4096);
    outbox.push(pubsub("t", 1));
    outbox.push(pubsub("t", 1));

    auto series = outbox.metrics();
    LOGOS_ASSERT_TRUE(series.size() == 2);
    LOGOS_ASSERT_TRUE(series[0].name == "libp2p_module_event_outbox_depth");
    LOGOS_ASSERT_TRUE(series[0].value == 1.0);
    LOGOS_ASSERT_TRUE(series[1].name == "libp2p_module_event_outbox_dropped_total");
    LOGOS_ASSERT_TRUE(series[1].value == 1.0);
}

LOGOS_TEST(event_outbox_close_discards_queued_events) {
    EventOutbox outbox;
    outbox.push(pubsub("t", 1));
    outbox.close();

    OutboxEvent out;
    LOGOS_ASSERT_FALSE(outbox.pop(out));
    LOGOS_ASSERT_TRUE(outbox.metrics()[0].value == 0.0);
}

LOGOS_TEST(event_outbox_push_after_close_is_not_counted_as_drop) {
    EventOutbox outbox;
    outbox.close();

    LOGOS_ASSERT_FALSE(outbox.push(pubsub("t", 1)));
    LOGOS_ASSERT_TRUE(dropped(outbox) == 0);
}

LOGOS_TEST(event_outbox_clear_empties_queue_and_stays_open) {
    EventOutbox outbox;
    outbox.push(pubsub("a", 1));
    outbox.push(pubsub("b", 1));
    outbox.clear();
    LOGOS_ASSERT_TRUE(outbox.metrics()[0].value == 0.0);

    LOGOS_ASSERT_TRUE(outbox.push(pubsub("c", 1)));
    OutboxEvent out;
    LOGOS_ASSERT_TRUE(outbox.pop(out));
    LOGOS_ASSERT_TRUE(out.name == "c");
}

LOGOS_TEST(event_outbox_pop_returns_every_field_of_a_stream_event) {
    EventOutbox outbox;
    OutboxEvent in;
    in.kind = OutboxEvent::Kind::Stream;
    in.name = "/proto/1";
    in.peerId = "peer";
    in.streamId = 42;
    in.data = {1, 2, 3};
    outbox.push(in);

    OutboxEvent out;
    LOGOS_ASSERT_TRUE(outbox.pop(out));
    LOGOS_ASSERT_TRUE(out.kind == OutboxEvent::Kind::Stream);
    LOGOS_ASSERT_TRUE(out.name == "/proto/1");
    LOGOS_ASSERT_TRUE(out.peerId == "peer");
    LOGOS_ASSERT_TRUE(out.streamId == 42);
    LOGOS_ASSERT_TRUE((out.data == std::vector<uint8_t>{1, 2, 3}));
}

LOGOS_TEST(event_outbox_counts_peer_id_in_byte_bound) {
    EventOutbox outbox(1024, kFixed + 10);
    OutboxEvent e;
    e.peerId.assign(10, 'p');

    LOGOS_ASSERT_TRUE(outbox.push(e));
    LOGOS_ASSERT_FALSE(outbox.push(pubsub("", 1)));
    LOGOS_ASSERT_TRUE(dropped(outbox) == 1);
}

LOGOS_TEST(event_outbox_zero_event_bound_drops_everything) {
    EventOutbox outbox(0, 4096);

    LOGOS_ASSERT_FALSE(outbox.push(pubsub("t", 0)));
    LOGOS_ASSERT_TRUE(dropped(outbox) == 1);
}

LOGOS_TEST(event_outbox_depth_falls_after_pop) {
    EventOutbox outbox;
    outbox.push(pubsub("a", 1));
    outbox.push(pubsub("b", 1));
    OutboxEvent out;
    outbox.pop(out);

    LOGOS_ASSERT_TRUE(outbox.metrics()[0].value == 1.0);
}

LOGOS_TEST(event_outbox_concurrent_producers_deliver_in_per_producer_order) {
    constexpr int kProducers = 4;
    constexpr int kPerProducer = 500;
    EventOutbox outbox(kProducers * kPerProducer, 64 * 1024 * 1024);

    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&outbox, p] {
            for (int i = 0; i < kPerProducer; ++i) {
                OutboxEvent e;
                e.streamId = static_cast<uint64_t>(i);
                e.name = std::to_string(p);
                outbox.push(std::move(e));
            }
        });
    }
    std::vector<int> next(kProducers, 0);
    bool ordered = true;
    for (int n = 0; n < kProducers * kPerProducer; ++n) {
        OutboxEvent out;
        if (!outbox.pop(out)) {
            ordered = false;
            break;
        }
        const int p = std::stoi(out.name);
        ordered = ordered && out.streamId == static_cast<uint64_t>(next[p]++);
    }
    for (auto& t : producers) t.join();

    LOGOS_ASSERT_TRUE(ordered);
    LOGOS_ASSERT_TRUE(dropped(outbox) == 0);
}

LOGOS_TEST(event_outbox_clear_starts_a_new_generation) {
    EventOutbox outbox;
    outbox.push(pubsub("a", 1));
    OutboxEvent out;
    LOGOS_ASSERT_TRUE(outbox.pop(out));
    LOGOS_ASSERT_TRUE(out.generation == outbox.generation());

    outbox.clear();
    LOGOS_ASSERT_TRUE(out.generation != outbox.generation());
}

LOGOS_TEST(event_outbox_drop_topic_removes_only_that_topic) {
    EventOutbox outbox;
    outbox.push(pubsub("a", 1));
    outbox.push(pubsub("b", 2));
    outbox.push(pubsub("a", 3));

    outbox.dropTopic("a");

    OutboxEvent out;
    LOGOS_ASSERT_TRUE(outbox.pop(out));
    LOGOS_ASSERT_TRUE(out.name == "b");
    LOGOS_ASSERT_TRUE(!outbox.stale(out));
    LOGOS_ASSERT_TRUE(outbox.metrics()[0].value == 0);
}

LOGOS_TEST(event_outbox_drop_topic_marks_a_popped_event_stale) {
    EventOutbox outbox;
    outbox.push(pubsub("a", 1));
    outbox.push(pubsub("b", 1));
    OutboxEvent popped;
    LOGOS_ASSERT_TRUE(outbox.pop(popped));
    LOGOS_ASSERT_TRUE(!outbox.stale(popped));

    outbox.dropTopic("a");

    LOGOS_ASSERT_TRUE(outbox.stale(popped));
}

LOGOS_TEST(event_outbox_events_pushed_after_drop_topic_are_fresh) {
    EventOutbox outbox;
    outbox.dropTopic("a");
    outbox.push(pubsub("a", 1));

    OutboxEvent out;
    LOGOS_ASSERT_TRUE(outbox.pop(out));
    LOGOS_ASSERT_TRUE(!outbox.stale(out));
}

LOGOS_TEST(event_outbox_drop_topic_keeps_stream_events) {
    EventOutbox outbox;
    OutboxEvent stream;
    stream.kind = OutboxEvent::Kind::Stream;
    stream.name = "a";
    outbox.push(stream);

    outbox.dropTopic("a");

    OutboxEvent out;
    LOGOS_ASSERT_TRUE(outbox.pop(out));
    LOGOS_ASSERT_TRUE(!outbox.stale(out));
}

LOGOS_TEST(event_outbox_clear_after_drop_topic_keeps_old_events_stale) {
    EventOutbox outbox;
    outbox.push(pubsub("a", 1));
    OutboxEvent popped;
    LOGOS_ASSERT_TRUE(outbox.pop(popped));

    outbox.dropTopic("a");
    outbox.clear();

    LOGOS_ASSERT_TRUE(outbox.stale(popped));
}

LOGOS_TEST(event_outbox_drop_topic_prunes_unreferenced_topics) {
    EventOutbox outbox;
    outbox.push(pubsub("popped", 1));
    outbox.push(pubsub("queued", 1));
    OutboxEvent popped;
    LOGOS_ASSERT_TRUE(outbox.pop(popped));
    outbox.dropTopic("popped");
    outbox.dropTopic("queued");
    outbox.push(pubsub("queued", 2));

    for (int i = 0; i < 3000; ++i) outbox.dropTopic("t" + std::to_string(i));

    LOGOS_ASSERT_TRUE(outbox.trackedTopics() < 1100);
    LOGOS_ASSERT_TRUE(outbox.stale(popped));
    OutboxEvent out;
    LOGOS_ASSERT_TRUE(outbox.pop(out));
    LOGOS_ASSERT_TRUE(out.name == "queued");
    LOGOS_ASSERT_TRUE(!outbox.stale(out));
}

LOGOS_TEST(event_outbox_carries_a_dropped_count) {
    EventOutbox outbox;
    outbox.addDropped(3);
    LOGOS_ASSERT_TRUE(outbox.dropped() == 3);
    LOGOS_ASSERT_TRUE(dropped(outbox) == 3);
}
