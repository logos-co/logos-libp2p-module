// Library-backed config tests (start a real node). Pure parsing: unit_config.cpp.

#include <logos_test.h>
#include <plugin.h>

#include <string>

LOGOS_TEST(config_custom_listen_address) {
    Libp2pModuleImpl plugin(Libp2pModuleOptions{ .addrs = {"/ip6/::1/tcp/0"} });
    auto startRes = plugin.start();
    if (!startRes.success) return; // IPv6 not available in sandbox

    auto res = plugin.peerInfo();
    LOGOS_ASSERT_TRUE(res.success);

    auto addrs = res.value["addrs"];

    bool hasIp6 = false;
    for (const auto& addr : addrs) {
        if (addr.get<std::string>().find("/ip6/::1") != std::string::npos) {
            hasIp6 = true;
            break;
        }
    }
    LOGOS_ASSERT_TRUE(hasIp6);

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(config_gossipsub_trigger_self_enabled) {
    Libp2pModuleImpl plugin;
    LOGOS_ASSERT_TRUE(plugin.start().success);

    std::string topic = "self-test";
    LOGOS_ASSERT_TRUE(plugin.gossipsubSubscribe(topic).success);

    std::string payload = "hello";
    LOGOS_ASSERT_TRUE(plugin.gossipsubPublish(topic, payload).success);

    auto res = plugin.gossipsubNextMessage(topic, 1000);
    LOGOS_ASSERT_TRUE(res.success);
    LOGOS_ASSERT_TRUE(res.value.get<std::string>() == payload);

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(config_gossipsub_trigger_self_disabled) {
    Libp2pModuleImpl plugin(Libp2pModuleOptions{ .gossipsubTriggerSelf = false });
    LOGOS_ASSERT_TRUE(plugin.start().success);

    std::string topic = "self-test";
    LOGOS_ASSERT_TRUE(plugin.gossipsubSubscribe(topic).success);
    LOGOS_ASSERT_TRUE(plugin.gossipsubPublish(topic, "hello").success);

    auto res = plugin.gossipsubNextMessage(topic, 500);
    LOGOS_ASSERT_FALSE(res.success);

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(config_tcp_transport) {
    Libp2pModuleImpl plugin;
    LOGOS_ASSERT_TRUE(plugin.start().success);

    auto res = plugin.peerInfo();
    LOGOS_ASSERT_TRUE(res.success);

    auto addrs = res.value["addrs"];

    bool hasTcp = false;
    for (const auto& addr : addrs) {
        if (addr.get<std::string>().find("/tcp/") != std::string::npos) {
            hasTcp = true;
            break;
        }
    }
    LOGOS_ASSERT_TRUE(hasTcp);

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(config_quic_transport) {
    Libp2pModuleImpl plugin(Libp2pModuleOptions{ .transport = LIBP2P_TRANSPORT_QUIC });
    LOGOS_ASSERT_TRUE(plugin.start().success);

    auto res = plugin.peerInfo();
    LOGOS_ASSERT_TRUE(res.success);

    auto addrs = res.value["addrs"];

    bool hasQuic = false;
    for (const auto& addr : addrs) {
        if (addr.get<std::string>().find("/quic-v1") != std::string::npos) {
            hasQuic = true;
            break;
        }
    }
    LOGOS_ASSERT_TRUE(hasQuic);

    LOGOS_ASSERT_TRUE(plugin.stop().success);
}
