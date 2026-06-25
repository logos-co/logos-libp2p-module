#include <logos_test.h>
#include <plugin.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <unistd.h>

namespace {
// Sets LIBP2P_MODULE_CONFIG for the test body and restores it on scope exit so
// later default-constructed plugins don't pick up a stale config.
struct ScopedModuleConfig {
    explicit ScopedModuleConfig(const std::string& value) {
        setenv("LIBP2P_MODULE_CONFIG", value.c_str(), 1);
    }
    ~ScopedModuleConfig() { unsetenv("LIBP2P_MODULE_CONFIG"); }
};
}

LOGOS_TEST(config_from_json_overlay) {
    auto j = nlohmann::json::parse(R"({
        "addrs": ["/ip4/0.0.0.0/tcp/9000"],
        "bootstrapNodes": [
            {"peerId": "16Uiu2bootstrap", "addrs": ["/ip4/1.2.3.4/tcp/9000", "/ip4/1.2.3.4/udp/9000/quic-v1"]}
        ],
        "transport": "quic",
        "maxConnections": 200,
        "mountServiceDiscovery": false
    })");

    Libp2pModuleOptions opts;
    libp2p_module_config::apply(j, opts);

    LOGOS_ASSERT_EQ(opts.addrs.size(), 1u);
    LOGOS_ASSERT_TRUE(opts.addrs[0] == "/ip4/0.0.0.0/tcp/9000");
    LOGOS_ASSERT_EQ(opts.bootstrapNodes.size(), 1u);
    LOGOS_ASSERT_TRUE(opts.bootstrapNodes[0].first == "16Uiu2bootstrap");
    LOGOS_ASSERT_EQ(opts.bootstrapNodes[0].second.size(), 2u);
    LOGOS_ASSERT_EQ(opts.transport, LIBP2P_TRANSPORT_QUIC);
    LOGOS_ASSERT_EQ(opts.maxConnections, 200);
    LOGOS_ASSERT_FALSE(opts.mountServiceDiscovery);
    // Untouched keys keep their defaults.
    LOGOS_ASSERT_TRUE(opts.mountKad);
    LOGOS_ASSERT_EQ(opts.maxInConnections, 25);
}

LOGOS_TEST(config_from_json_key_type_and_priv_key) {
    auto j = nlohmann::json::parse(R"({
        "keyType": "ed25519",
        "privKey": "0a0bFF"
    })");

    Libp2pModuleOptions opts;
    libp2p_module_config::apply(j, opts);

    LOGOS_ASSERT_EQ(opts.keyType, LIBP2P_PK_ED25519);
    LOGOS_ASSERT_EQ(opts.privKey.size(), 3u);
    LOGOS_ASSERT_EQ(opts.privKey[0], 0x0au);
    LOGOS_ASSERT_EQ(opts.privKey[1], 0x0bu);
    LOGOS_ASSERT_EQ(opts.privKey[2], 0xffu);
}

LOGOS_TEST(config_key_type_defaults_to_secp256k1) {
    Libp2pModuleOptions opts;
    LOGOS_ASSERT_EQ(opts.keyType, LIBP2P_PK_SECP256K1);
    LOGOS_ASSERT_TRUE(opts.privKey.empty());
}

LOGOS_TEST(config_load_invalid_priv_key_hex_returns_defaults) {
    ScopedModuleConfig cfg(R"({"privKey": "zz", "maxConnections": 200})");

    Libp2pModuleOptions opts = Libp2pModuleOptions::load();
    LOGOS_ASSERT_TRUE(opts.privKey.empty());
    LOGOS_ASSERT_EQ(opts.maxConnections, 50);
}

LOGOS_TEST(config_load_odd_priv_key_hex_returns_defaults) {
    ScopedModuleConfig cfg(R"({"privKey": "abc"})");

    Libp2pModuleOptions opts = Libp2pModuleOptions::load();
    LOGOS_ASSERT_TRUE(opts.privKey.empty());
}

// A supplied private key must yield the same peer ID across constructions.
LOGOS_TEST(config_priv_key_stable_peer_identity) {
    Libp2pModuleImpl keyGen;
    auto keyRes = keyGen.newPrivateKey();
    LOGOS_ASSERT_TRUE(keyRes.success);
    std::string raw = keyRes.value.get<std::string>();
    std::vector<uint8_t> priv(raw.begin(), raw.end());

    std::string firstPeerId;
    {
        Libp2pModuleImpl plugin(Libp2pModuleOptions{ .privKey = priv });
        LOGOS_ASSERT_TRUE(plugin.start().success);
        auto info = plugin.peerInfo();
        LOGOS_ASSERT_TRUE(info.success);
        firstPeerId = info.value["peerId"].get<std::string>();
        LOGOS_ASSERT_TRUE(plugin.stop().success);
    }

    Libp2pModuleImpl plugin(Libp2pModuleOptions{ .privKey = priv });
    LOGOS_ASSERT_TRUE(plugin.start().success);
    auto info = plugin.peerInfo();
    LOGOS_ASSERT_TRUE(info.success);
    LOGOS_ASSERT_TRUE(info.value["peerId"].get<std::string>() == firstPeerId);
    LOGOS_ASSERT_TRUE(plugin.stop().success);
}

LOGOS_TEST(config_from_json_partial_keeps_defaults) {
    auto j = nlohmann::json::parse(R"({"maxConnsPerPeer": 4})");

    Libp2pModuleOptions opts;
    libp2p_module_config::apply(j, opts);

    LOGOS_ASSERT_EQ(opts.maxConnsPerPeer, 4);
    LOGOS_ASSERT_TRUE(opts.addrs.empty());
    LOGOS_ASSERT_TRUE(opts.bootstrapNodes.empty());
    LOGOS_ASSERT_EQ(opts.transport, LIBP2P_TRANSPORT_TCP);
}

LOGOS_TEST(config_load_unset_returns_defaults) {
    unsetenv("LIBP2P_MODULE_CONFIG");
    Libp2pModuleOptions opts = Libp2pModuleOptions::load();
    LOGOS_ASSERT_TRUE(opts.addrs.empty());
    LOGOS_ASSERT_TRUE(opts.bootstrapNodes.empty());
    LOGOS_ASSERT_EQ(opts.transport, LIBP2P_TRANSPORT_TCP);
}

LOGOS_TEST(config_load_inline_json) {
    ScopedModuleConfig cfg(R"({"addrs": ["/ip4/0.0.0.0/tcp/7000"], "transport": "quic"})");

    Libp2pModuleOptions opts = Libp2pModuleOptions::load();
    LOGOS_ASSERT_EQ(opts.addrs.size(), 1u);
    LOGOS_ASSERT_TRUE(opts.addrs[0] == "/ip4/0.0.0.0/tcp/7000");
    LOGOS_ASSERT_EQ(opts.transport, LIBP2P_TRANSPORT_QUIC);
}

LOGOS_TEST(config_load_inline_json_leading_whitespace) {
    ScopedModuleConfig cfg("  \n\t{\"transport\": \"quic\"}");

    Libp2pModuleOptions opts = Libp2pModuleOptions::load();
    LOGOS_ASSERT_EQ(opts.transport, LIBP2P_TRANSPORT_QUIC);
}

LOGOS_TEST(config_load_from_file) {
    char path[] = "/tmp/libp2p_module_test_config.XXXXXX";
    int fd = mkstemp(path);
    LOGOS_ASSERT_TRUE(fd != -1);
    close(fd);
    {
        std::ofstream f(path);
        f << R"({"bootstrapNodes": [{"peerId": "16Uiu2file", "addrs": ["/ip4/9.9.9.9/tcp/9000"]}]})";
    }
    ScopedModuleConfig cfg(path);

    Libp2pModuleOptions opts = Libp2pModuleOptions::load();
    LOGOS_ASSERT_EQ(opts.bootstrapNodes.size(), 1u);
    LOGOS_ASSERT_TRUE(opts.bootstrapNodes[0].first == "16Uiu2file");
    LOGOS_ASSERT_TRUE(opts.bootstrapNodes[0].second[0] == "/ip4/9.9.9.9/tcp/9000");

    std::remove(path);
}

LOGOS_TEST(config_load_invalid_json_returns_defaults) {
    ScopedModuleConfig cfg("{not valid json");

    Libp2pModuleOptions opts = Libp2pModuleOptions::load();
    LOGOS_ASSERT_TRUE(opts.addrs.empty());
    LOGOS_ASSERT_TRUE(opts.bootstrapNodes.empty());
    LOGOS_ASSERT_EQ(opts.transport, LIBP2P_TRANSPORT_TCP);
}

// Valid JSON but a wrong-typed field must not throw out of construction.
LOGOS_TEST(config_load_wrong_field_type_returns_defaults) {
    ScopedModuleConfig cfg(R"({"maxConnections": "not-a-number"})");

    Libp2pModuleOptions opts = Libp2pModuleOptions::load();
    LOGOS_ASSERT_EQ(opts.maxConnections, 50);
    LOGOS_ASSERT_TRUE(opts.addrs.empty());
}

LOGOS_TEST(config_options_defaults) {
    Libp2pModuleOptions opts;
    LOGOS_ASSERT_TRUE(opts.addrs.empty());
    LOGOS_ASSERT_TRUE(opts.bootstrapNodes.empty());
    LOGOS_ASSERT_EQ(opts.transport, LIBP2P_TRANSPORT_TCP);
    LOGOS_ASSERT_FALSE(opts.autonat);
    LOGOS_ASSERT_FALSE(opts.autonatV2);
    LOGOS_ASSERT_FALSE(opts.autonatV2Server);
    LOGOS_ASSERT_FALSE(opts.circuitRelay);
    LOGOS_ASSERT_EQ(opts.maxConnections, 50);
    LOGOS_ASSERT_EQ(opts.maxInConnections, 25);
    LOGOS_ASSERT_EQ(opts.maxOutConnections, 25);
    LOGOS_ASSERT_EQ(opts.maxConnsPerPeer, 1);
    LOGOS_ASSERT_TRUE(opts.gossipsubTriggerSelf);
}

LOGOS_TEST(config_options_designated_init) {
    Libp2pModuleOptions opts{ .circuitRelay = true };
    LOGOS_ASSERT_TRUE(opts.circuitRelay);
    LOGOS_ASSERT_TRUE(opts.addrs.empty());
    LOGOS_ASSERT_TRUE(opts.bootstrapNodes.empty());
    LOGOS_ASSERT_EQ(opts.transport, LIBP2P_TRANSPORT_TCP);
    LOGOS_ASSERT_FALSE(opts.autonat);
    LOGOS_ASSERT_FALSE(opts.autonatV2);
    LOGOS_ASSERT_FALSE(opts.autonatV2Server);
}

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
