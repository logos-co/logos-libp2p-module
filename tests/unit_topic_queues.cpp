// TopicQueues bounds in isolation (no Libp2pModuleImpl, links without libp2p.so).

#include <logos_test.h>
#include <topic_queues.h>

#include <string>

LOGOS_TEST(topic_queues_drops_newest_over_message_bound) {
    TopicQueues queues;
    queues.setBounds(2, 4096);

    LOGOS_ASSERT_TRUE(queues.push("t", "one"));
    LOGOS_ASSERT_TRUE(queues.push("t", "two"));
    LOGOS_ASSERT_FALSE(queues.push("t", "three"));

    std::string out;
    LOGOS_ASSERT_TRUE(queues.pop("t", 0, out));
    LOGOS_ASSERT_TRUE(out == "one");
    LOGOS_ASSERT_TRUE(queues.pop("t", 0, out));
    LOGOS_ASSERT_TRUE(out == "two");
    LOGOS_ASSERT_FALSE(queues.pop("t", 0, out));
}

LOGOS_TEST(topic_queues_drops_newest_over_byte_bound) {
    TopicQueues queues;
    queues.setBounds(1024, 8);

    LOGOS_ASSERT_TRUE(queues.push("t", std::string(5, 'a')));
    LOGOS_ASSERT_FALSE(queues.push("t", std::string(4, 'b')));
    LOGOS_ASSERT_TRUE(queues.push("t", std::string(3, 'c')));
}

// The byte bound holds on an empty queue too, so a payload larger than the
// bound never enters the backlog.
LOGOS_TEST(topic_queues_drops_oversized_message_on_empty_queue) {
    TopicQueues queues;
    queues.setBounds(1024, 64);

    LOGOS_ASSERT_FALSE(queues.push("t", std::string(4096, 'y')));

    std::string out;
    LOGOS_ASSERT_FALSE(queues.pop("t", 0, out));
}

LOGOS_TEST(topic_queues_pop_frees_bytes_for_the_next_push) {
    TopicQueues queues;
    queues.setBounds(1024, 8);

    LOGOS_ASSERT_TRUE(queues.push("t", std::string(8, 'a')));
    LOGOS_ASSERT_FALSE(queues.push("t", "b"));

    std::string out;
    LOGOS_ASSERT_TRUE(queues.pop("t", 0, out));
    LOGOS_ASSERT_TRUE(queues.push("t", std::string(8, 'c')));
}

LOGOS_TEST(topic_queues_disabled_when_a_bound_is_zero) {
    TopicQueues byMessages;
    byMessages.setBounds(0, 4096);
    LOGOS_ASSERT_FALSE(byMessages.push("t", "dropped"));

    TopicQueues byBytes;
    byBytes.setBounds(1024, 0);
    LOGOS_ASSERT_FALSE(byBytes.push("t", "dropped"));
}

LOGOS_TEST(topic_queues_bounds_are_per_topic) {
    TopicQueues queues;
    queues.setBounds(1, 4096);

    LOGOS_ASSERT_TRUE(queues.push("a", "first"));
    LOGOS_ASSERT_FALSE(queues.push("a", "second"));
    LOGOS_ASSERT_TRUE(queues.push("b", "first"));
}

// A node that cycles topics must not grow a map entry and two metric series per
// topic it ever subscribed to, so an entry that counted no drop is erased.
LOGOS_TEST(topic_queues_release_forgets_a_topic_that_dropped_nothing) {
    TopicQueues queues;
    queues.setBounds(1024, 4096);

    for (int i = 0; i < 100; ++i) {
        const std::string topic = "topic-" + std::to_string(i);
        LOGOS_ASSERT_TRUE(queues.push(topic, "payload"));
        queues.release(topic);
    }
    LOGOS_ASSERT_EQ(queues.topicCount(), size_t(0));
    LOGOS_ASSERT_TRUE(queues.metrics().empty());
}

LOGOS_TEST(topic_queues_release_all_forgets_topics_that_dropped_nothing) {
    TopicQueues queues;
    queues.setBounds(1, 4096);

    LOGOS_ASSERT_TRUE(queues.push("quiet", "payload"));
    LOGOS_ASSERT_TRUE(queues.push("noisy", "payload"));
    LOGOS_ASSERT_FALSE(queues.push("noisy", "over the bound"));
    queues.releaseAll();

    LOGOS_ASSERT_EQ(queues.topicCount(), size_t(1));
}

LOGOS_TEST(topic_queues_release_frees_payloads_and_keeps_the_drop_counter) {
    TopicQueues queues;
    queues.setBounds(1, 4096);

    LOGOS_ASSERT_TRUE(queues.push("t", "kept"));
    LOGOS_ASSERT_FALSE(queues.push("t", "dropped"));
    queues.release("t");

    std::string out;
    LOGOS_ASSERT_FALSE(queues.pop("t", 0, out));
    LOGOS_ASSERT_EQ(queues.topicCount(), size_t(1));

    double depth = -1.0;
    double dropped = -1.0;
    for (const auto& m : queues.metrics()) {
        if (m.name == "libp2p_module_gossipsub_queue_depth") depth = m.value;
        if (m.name == "libp2p_module_gossipsub_queue_dropped_total") dropped = m.value;
    }
    LOGOS_ASSERT_EQ(depth, 0.0);
    LOGOS_ASSERT_EQ(dropped, 1.0);
}
