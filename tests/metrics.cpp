#include <logos_test.h>
#include <plugin.h>

#include <map>
#include <set>
#include <string>
#include <vector>

namespace {
LogosMap collect() {
    Libp2pModuleImpl plugin;
    return plugin.collectMetrics();
}

const PrometheusSample* find(const std::vector<PrometheusSample>& samples,
                             const std::string& name,
                             const std::map<std::string, std::string>& labels = {}) {
    for (const auto& s : samples) {
        if (s.name == name && s.labels == labels) return &s;
    }
    return nullptr;
}
}

LOGOS_TEST(collect_metrics_payload_has_metrics_array) {
    auto res = collect();
    LOGOS_ASSERT_TRUE(res.is_object());
    LOGOS_ASSERT_TRUE(res.contains("metrics"));
    LOGOS_ASSERT_TRUE(res["metrics"].is_array());
}

LOGOS_TEST(collect_metrics_entries_match_openmetrics_schema) {
    auto res = collect();

    const std::set<std::string> validTypes = {
        "counter", "gauge", "histogram", "summary"
    };

    for (const auto& m : res["metrics"]) {
        LOGOS_ASSERT_TRUE(m.is_object());

        LOGOS_ASSERT_TRUE(m.contains("name"));
        LOGOS_ASSERT_TRUE(m["name"].is_string());
        LOGOS_ASSERT_FALSE(m["name"].get<std::string>().empty());

        LOGOS_ASSERT_TRUE(m.contains("type"));
        LOGOS_ASSERT_TRUE(m["type"].is_string());
        LOGOS_ASSERT_TRUE(validTypes.count(m["type"].get<std::string>()));

        LOGOS_ASSERT_TRUE(m.contains("help"));
        LOGOS_ASSERT_TRUE(m["help"].is_string());

        LOGOS_ASSERT_TRUE(m.contains("value"));
        LOGOS_ASSERT_TRUE(m["value"].is_number());

        if (m.contains("labels")) {
            LOGOS_ASSERT_TRUE(m["labels"].is_object());
        }
    }
}

LOGOS_TEST(parse_prometheus_resolves_type_and_help_from_meta) {
    const auto out = parsePrometheusText(
        "# HELP libp2p_peers Number of connected peers\n"
        "# TYPE libp2p_peers gauge\n"
        "libp2p_peers 3\n");
    LOGOS_ASSERT_EQ(out.size(), 1u);
    const auto* s = find(out, "libp2p_peers");
    LOGOS_ASSERT_TRUE(s != nullptr);
    LOGOS_ASSERT_EQ(s->value, 3.0);
    LOGOS_ASSERT_TRUE(s->type == "gauge");
    LOGOS_ASSERT_TRUE(s->help == "Number of connected peers");
}

LOGOS_TEST(parse_prometheus_preserves_labels_without_summing) {
    const auto out = parsePrometheusText(
        "# TYPE libp2p_open_streams gauge\n"
        "libp2p_open_streams{dir=\"in\"} 4\n"
        "libp2p_open_streams{dir=\"out\"} 6\n");
    LOGOS_ASSERT_EQ(out.size(), 2u);
    const auto* in = find(out, "libp2p_open_streams", {{"dir", "in"}});
    const auto* out_ = find(out, "libp2p_open_streams", {{"dir", "out"}});
    LOGOS_ASSERT_TRUE(in != nullptr && out_ != nullptr);
    LOGOS_ASSERT_EQ(in->value, 4.0);
    LOGOS_ASSERT_EQ(out_->value, 6.0);
}

LOGOS_TEST(parse_prometheus_keeps_counter_total_drops_created) {
    const auto out = parsePrometheusText(
        "# TYPE libp2p_failed_dials counter\n"
        "libp2p_failed_dials_total 7\n"
        "libp2p_failed_dials_created 1700000000\n");
    LOGOS_ASSERT_EQ(out.size(), 1u);
    const auto* total = find(out, "libp2p_failed_dials_total");
    LOGOS_ASSERT_TRUE(total != nullptr);
    LOGOS_ASSERT_EQ(total->value, 7.0);
    LOGOS_ASSERT_TRUE(total->type == "counter");
    LOGOS_ASSERT_TRUE(find(out, "libp2p_failed_dials_created") == nullptr);
}

LOGOS_TEST(parse_prometheus_keeps_histogram_component_series) {
    const auto out = parsePrometheusText(
        "# TYPE cd_message_duration_ms histogram\n"
        "cd_message_duration_ms_bucket{le=\"10\"} 2\n"
        "cd_message_duration_ms_bucket{le=\"+Inf\"} 5\n"
        "cd_message_duration_ms_sum 42\n"
        "cd_message_duration_ms_count 5\n");
    LOGOS_ASSERT_EQ(out.size(), 4u);
    const auto* bucket = find(out, "cd_message_duration_ms_bucket", {{"le", "10"}});
    const auto* sum = find(out, "cd_message_duration_ms_sum");
    LOGOS_ASSERT_TRUE(bucket != nullptr && sum != nullptr);
    LOGOS_ASSERT_EQ(bucket->value, 2.0);
    LOGOS_ASSERT_TRUE(bucket->type == "histogram");
    LOGOS_ASSERT_TRUE(sum->type == "histogram");
}

LOGOS_TEST(parse_prometheus_handles_quoted_label_values) {
    const auto out = parsePrometheusText(
        "# TYPE m gauge\n"
        "m{agent=\"go-libp2p, v0.1\",note=\"a\\\"b\"} 1\n");
    LOGOS_ASSERT_EQ(out.size(), 1u);
    const auto* s = find(out, "m", {{"agent", "go-libp2p, v0.1"}, {"note", "a\"b"}});
    LOGOS_ASSERT_TRUE(s != nullptr);
    LOGOS_ASSERT_EQ(s->value, 1.0);
}

LOGOS_TEST(parse_prometheus_skips_comments_and_blank_lines) {
    const auto out = parsePrometheusText(
        "# HELP libp2p_peers Number of connected peers\n"
        "# TYPE libp2p_peers gauge\n"
        "\n"
        "libp2p_peers 2\n");
    LOGOS_ASSERT_EQ(out.size(), 1u);
    LOGOS_ASSERT_EQ(find(out, "libp2p_peers")->value, 2.0);
}

LOGOS_TEST(parse_prometheus_ignores_trailing_timestamp) {
    const auto out = parsePrometheusText(
        "# TYPE libp2p_peers gauge\n"
        "libp2p_peers 5 1700000000000\n");
    LOGOS_ASSERT_EQ(find(out, "libp2p_peers")->value, 5.0);
}

LOGOS_TEST(parse_prometheus_drops_non_numeric_and_handles_float) {
    const auto out = parsePrometheusText(
        "# TYPE libp2p_bogus gauge\n"
        "# TYPE libp2p_ratio gauge\n"
        "libp2p_bogus notanumber\n"
        "libp2p_ratio 1.5\n");
    LOGOS_ASSERT_TRUE(find(out, "libp2p_bogus") == nullptr);
    LOGOS_ASSERT_EQ(find(out, "libp2p_ratio")->value, 1.5);
}
