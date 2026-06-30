// Pure config logic (no Libp2pModuleImpl, links without libp2p.so).

#include <logos_test.h>
#include <plugin.h>

#include <cstdio>
#include <cstdlib>
#include <unistd.h>

namespace {
// Sets LIBP2P_MODULE_CONFIG for the test body and restores it on scope exit so
// later load() calls don't pick up a stale config.
struct ScopedModuleConfig {
    explicit ScopedModuleConfig(const std::string& value) {
        setenv("LIBP2P_MODULE_CONFIG", value.c_str(), 1);
    }
    ~ScopedModuleConfig() { unsetenv("LIBP2P_MODULE_CONFIG"); }
};
}

using json = nlohmann::json;
namespace cfg = libp2p_module_config;

LOGOS_TEST(parse_transport_tcp) {
    auto j = json::parse(R"({"transport": "tcp"})");
    LOGOS_ASSERT_EQ(cfg::parseTransport(j, LIBP2P_TRANSPORT_QUIC), LIBP2P_TRANSPORT_TCP);
}

LOGOS_TEST(parse_transport_quic_and_alias) {
    auto quic = json::parse(R"({"transport": "quic"})");
    auto alias = json::parse(R"({"transport": "quic-v1"})");
    LOGOS_ASSERT_EQ(cfg::parseTransport(quic, LIBP2P_TRANSPORT_TCP), LIBP2P_TRANSPORT_QUIC);
    LOGOS_ASSERT_EQ(cfg::parseTransport(alias, LIBP2P_TRANSPORT_TCP), LIBP2P_TRANSPORT_QUIC);
}

LOGOS_TEST(parse_transport_unknown_keeps_fallback) {
    auto j = json::parse(R"({"transport": "carrier-pigeon"})");
    LOGOS_ASSERT_EQ(cfg::parseTransport(j, LIBP2P_TRANSPORT_TCP), LIBP2P_TRANSPORT_TCP);
    LOGOS_ASSERT_EQ(cfg::parseTransport(j, LIBP2P_TRANSPORT_QUIC), LIBP2P_TRANSPORT_QUIC);
}

LOGOS_TEST(parse_transport_missing_or_wrong_type_keeps_fallback) {
    auto missing = json::parse(R"({})");
    auto wrong = json::parse(R"({"transport": 7})");
    LOGOS_ASSERT_EQ(cfg::parseTransport(missing, LIBP2P_TRANSPORT_QUIC), LIBP2P_TRANSPORT_QUIC);
    LOGOS_ASSERT_EQ(cfg::parseTransport(wrong, LIBP2P_TRANSPORT_QUIC), LIBP2P_TRANSPORT_QUIC);
}

LOGOS_TEST(apply_overlays_present_keys) {
    auto j = json::parse(R"({
        "addrs": ["/ip4/0.0.0.0/tcp/9000"],
        "bootstrapNodes": [
            {"peerId": "16Uiu2bootstrap", "addrs": ["/ip4/1.2.3.4/tcp/9000", "/ip4/1.2.3.4/udp/9000/quic-v1"]}
        ],
        "transport": "quic",
        "maxConnections": 200,
        "mountServiceDiscovery": false
    })");

    Libp2pModuleOptions opts;
    cfg::apply(j, opts);

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

LOGOS_TEST(apply_partial_keeps_defaults) {
    auto j = json::parse(R"({"maxConnsPerPeer": 4})");

    Libp2pModuleOptions opts;
    cfg::apply(j, opts);

    LOGOS_ASSERT_EQ(opts.maxConnsPerPeer, 4);
    LOGOS_ASSERT_TRUE(opts.addrs.empty());
    LOGOS_ASSERT_TRUE(opts.bootstrapNodes.empty());
    LOGOS_ASSERT_EQ(opts.transport, LIBP2P_TRANSPORT_TCP);
}

LOGOS_TEST(apply_non_object_is_noop) {
    Libp2pModuleOptions opts;
    cfg::apply(json::parse(R"([1, 2, 3])"), opts);
    cfg::apply(json("just a string"), opts);
    LOGOS_ASSERT_TRUE(opts.addrs.empty());
    LOGOS_ASSERT_EQ(opts.transport, LIBP2P_TRANSPORT_TCP);
    LOGOS_ASSERT_EQ(opts.maxConnections, 50);
}

LOGOS_TEST(apply_wrong_field_type_throws) {
    auto j = json::parse(R"({"maxConnections": "not-a-number"})");
    Libp2pModuleOptions opts;
    bool threw = false;
    try {
        cfg::apply(j, opts);
    } catch (const nlohmann::json::exception&) {
        threw = true;
    }
    LOGOS_ASSERT_TRUE(threw);
}

LOGOS_TEST(apply_bootstrap_node_missing_fields_defaults_empty) {
    auto j = json::parse(R"({"bootstrapNodes": [{}, {"peerId": "16Uiu2only"}]})");
    Libp2pModuleOptions opts;
    cfg::apply(j, opts);
    LOGOS_ASSERT_EQ(opts.bootstrapNodes.size(), 2u);
    LOGOS_ASSERT_TRUE(opts.bootstrapNodes[0].first.empty());
    LOGOS_ASSERT_TRUE(opts.bootstrapNodes[0].second.empty());
    LOGOS_ASSERT_TRUE(opts.bootstrapNodes[1].first == "16Uiu2only");
    LOGOS_ASSERT_TRUE(opts.bootstrapNodes[1].second.empty());
}

LOGOS_TEST(read_source_unset_returns_empty) {
    unsetenv("LIBP2P_MODULE_CONFIG");
    LOGOS_ASSERT_TRUE(cfg::readSource().empty());
}

LOGOS_TEST(read_source_inline_json_passthrough) {
    ScopedModuleConfig c(R"(  {"transport": "quic"})");
    LOGOS_ASSERT_TRUE(cfg::readSource() == R"(  {"transport": "quic"})");
}

LOGOS_TEST(read_source_missing_file_returns_empty) {
    ScopedModuleConfig c("/no/such/libp2p/config/file.json");
    LOGOS_ASSERT_TRUE(cfg::readSource().empty());
}

LOGOS_TEST(load_unset_returns_defaults) {
    unsetenv("LIBP2P_MODULE_CONFIG");
    Libp2pModuleOptions opts = Libp2pModuleOptions::load();
    LOGOS_ASSERT_TRUE(opts.addrs.empty());
    LOGOS_ASSERT_TRUE(opts.bootstrapNodes.empty());
    LOGOS_ASSERT_EQ(opts.transport, LIBP2P_TRANSPORT_TCP);
}

LOGOS_TEST(load_inline_json) {
    ScopedModuleConfig c(R"({"addrs": ["/ip4/0.0.0.0/tcp/7000"], "transport": "quic"})");
    Libp2pModuleOptions opts = Libp2pModuleOptions::load();
    LOGOS_ASSERT_EQ(opts.addrs.size(), 1u);
    LOGOS_ASSERT_TRUE(opts.addrs[0] == "/ip4/0.0.0.0/tcp/7000");
    LOGOS_ASSERT_EQ(opts.transport, LIBP2P_TRANSPORT_QUIC);
}

LOGOS_TEST(load_inline_json_leading_whitespace) {
    ScopedModuleConfig c("  \n\t{\"transport\": \"quic\"}");
    Libp2pModuleOptions opts = Libp2pModuleOptions::load();
    LOGOS_ASSERT_EQ(opts.transport, LIBP2P_TRANSPORT_QUIC);
}

LOGOS_TEST(load_from_file) {
    char path[] = "/tmp/libp2p_module_unit_config.XXXXXX";
    int fd = mkstemp(path);
    LOGOS_ASSERT_TRUE(fd != -1);
    const std::string contents =
        R"({"bootstrapNodes": [{"peerId": "16Uiu2file", "addrs": ["/ip4/9.9.9.9/tcp/9000"]}]})";
    LOGOS_ASSERT_TRUE(write(fd, contents.data(), contents.size())
                      == static_cast<ssize_t>(contents.size()));
    close(fd);
    ScopedModuleConfig c(path);

    Libp2pModuleOptions opts = Libp2pModuleOptions::load();
    LOGOS_ASSERT_EQ(opts.bootstrapNodes.size(), 1u);
    LOGOS_ASSERT_TRUE(opts.bootstrapNodes[0].first == "16Uiu2file");
    LOGOS_ASSERT_TRUE(opts.bootstrapNodes[0].second[0] == "/ip4/9.9.9.9/tcp/9000");

    std::remove(path);
}

LOGOS_TEST(load_invalid_json_returns_defaults) {
    ScopedModuleConfig c("{not valid json");
    Libp2pModuleOptions opts = Libp2pModuleOptions::load();
    LOGOS_ASSERT_TRUE(opts.addrs.empty());
    LOGOS_ASSERT_TRUE(opts.bootstrapNodes.empty());
    LOGOS_ASSERT_EQ(opts.transport, LIBP2P_TRANSPORT_TCP);
}

// Valid JSON but a wrong-typed field must not throw out of load().
LOGOS_TEST(load_wrong_field_type_returns_defaults) {
    ScopedModuleConfig c(R"({"maxConnections": "not-a-number"})");
    Libp2pModuleOptions opts = Libp2pModuleOptions::load();
    LOGOS_ASSERT_EQ(opts.maxConnections, 50);
    LOGOS_ASSERT_TRUE(opts.addrs.empty());
}

LOGOS_TEST(options_defaults) {
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

LOGOS_TEST(options_designated_init) {
    Libp2pModuleOptions opts{ .circuitRelay = true };
    LOGOS_ASSERT_TRUE(opts.circuitRelay);
    LOGOS_ASSERT_TRUE(opts.addrs.empty());
    LOGOS_ASSERT_EQ(opts.transport, LIBP2P_TRANSPORT_TCP);
    LOGOS_ASSERT_FALSE(opts.autonat);
}

LOGOS_TEST(from_json_valid_overlays_and_sets_ok) {
    bool ok = false;
    auto opts = Libp2pModuleOptions::fromJson(
        R"({"addrs": ["/ip4/0.0.0.0/tcp/9000"], "transport": "quic"})", ok);
    LOGOS_ASSERT_TRUE(ok);
    LOGOS_ASSERT_EQ(opts.addrs.size(), 1u);
    LOGOS_ASSERT_TRUE(opts.addrs[0] == "/ip4/0.0.0.0/tcp/9000");
    LOGOS_ASSERT_EQ(opts.transport, LIBP2P_TRANSPORT_QUIC);
}

LOGOS_TEST(from_json_invalid_json_clears_ok) {
    bool ok = true;
    auto opts = Libp2pModuleOptions::fromJson("{not valid json", ok);
    LOGOS_ASSERT_FALSE(ok);
    LOGOS_ASSERT_TRUE(opts.addrs.empty());
}

LOGOS_TEST(from_json_wrong_field_type_clears_ok) {
    bool ok = true;
    auto opts = Libp2pModuleOptions::fromJson(R"({"maxConnections": "nope"})", ok);
    LOGOS_ASSERT_FALSE(ok);
}

LOGOS_TEST(from_json_reports_error_reason) {
    bool ok = true;
    std::string err;
    Libp2pModuleOptions::fromJson("{not valid json", ok, &err);
    LOGOS_ASSERT_FALSE(ok);
    LOGOS_ASSERT_TRUE(err == "malformed JSON");

    ok = true;
    Libp2pModuleOptions::fromJson(R"({"maxConnections": "nope"})", ok, &err);
    LOGOS_ASSERT_FALSE(ok);
    LOGOS_ASSERT_FALSE(err.empty());

    ok = false;
    err = "stale";
    Libp2pModuleOptions::fromJson(R"({"transport": "quic"})", ok, &err);
    LOGOS_ASSERT_TRUE(ok);
    LOGOS_ASSERT_TRUE(err.empty());
}
