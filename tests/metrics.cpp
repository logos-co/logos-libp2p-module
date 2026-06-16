#include <logos_test.h>
#include <plugin.h>

#include <set>
#include <string>

using json = nlohmann::json;

namespace {
LogosMap collect() {
    Libp2pModuleImpl plugin;
    return plugin.collectMetrics();
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

LOGOS_TEST(metric_from_json_basic_fields) {
    const auto j = json::parse(R"({
        "name": "libp2p_peers",
        "type": "gauge",
        "help": "Number of connected peers",
        "labels": [],
        "value": 3,
        "timestamp": 0
    })");
    const auto m = j.get<Metric>();
    LOGOS_ASSERT_TRUE(m.name == "libp2p_peers");
    LOGOS_ASSERT_TRUE(m.type == "gauge");
    LOGOS_ASSERT_TRUE(m.help == "Number of connected peers");
    LOGOS_ASSERT_EQ(m.value, 3.0);
    LOGOS_ASSERT_TRUE(m.labels.empty());
    LOGOS_ASSERT_EQ(m.timestamp, 0.0);
}

LOGOS_TEST(metric_from_json_flattens_label_pair_array) {
    // cbind emits labels as a sequence of {name,value} pairs, but openmetrics-
    // module's renderer wants a flat {key:value} object. Verify the flattening.
    const auto j = json::parse(R"({
        "name": "libp2p_open_streams",
        "type": "gauge",
        "help": "",
        "labels": [
            {"name": "dir", "value": "in"},
            {"name": "proto", "value": "ping"}
        ],
        "value": 4,
        "timestamp": 0
    })");
    const auto m = j.get<Metric>();
    LOGOS_ASSERT_EQ(m.labels.size(), 2u);
    LOGOS_ASSERT_TRUE(m.labels.at("dir") == "in");
    LOGOS_ASSERT_TRUE(m.labels.at("proto") == "ping");
}

LOGOS_TEST(metric_from_json_handles_missing_labels) {
    const auto j = json::parse(R"({
        "name": "x", "type": "gauge", "help": "", "value": 0
    })");
    const auto m = j.get<Metric>();
    LOGOS_ASSERT_TRUE(m.labels.empty());
    LOGOS_ASSERT_EQ(m.timestamp, 0.0);
}

LOGOS_TEST(metric_from_json_records_timestamp) {
    const auto j = json::parse(R"({
        "name": "x", "type": "gauge", "help": "",
        "labels": [], "value": 1, "timestamp": 1700000000.5
    })");
    const auto m = j.get<Metric>();
    LOGOS_ASSERT_EQ(m.timestamp, 1700000000.5);
}

LOGOS_TEST(metric_to_json_emits_labels_as_object) {
    // openmetrics_format.cpp:82 requires labels.is_object(); round-trip must
    // produce that shape, not the inbound array-of-pairs.
    Metric m;
    m.name = "libp2p_peers";
    m.type = "gauge";
    m.help = "Number of connected peers";
    m.labels = {{"dir", "in"}};
    m.value = 7;
    const json j = m;
    LOGOS_ASSERT_TRUE(j["labels"].is_object());
    LOGOS_ASSERT_TRUE(j["labels"]["dir"].get<std::string>() == "in");
    LOGOS_ASSERT_FALSE(j.contains("timestamp"));
}

LOGOS_TEST(metric_to_json_keeps_nonzero_timestamp) {
    Metric m;
    m.name = "x";
    m.type = "gauge";
    m.timestamp = 1700000000.0;
    const json j = m;
    LOGOS_ASSERT_TRUE(j.contains("timestamp"));
    LOGOS_ASSERT_EQ(j["timestamp"].get<double>(), 1700000000.0);
}

LOGOS_TEST(metric_array_round_trips_through_payload_shape) {
    // End-to-end: take a cbind-shaped JSON array, parse it to vector<Metric>,
    // wrap it as payload["metrics"], and check the result matches what
    // openmetrics-module expects as input.
    const auto in = json::parse(R"([
        {"name":"libp2p_peers","type":"gauge","help":"peers",
         "labels":[],"value":2,"timestamp":0},
        {"name":"libp2p_failed_dials_total","type":"counter","help":"fails",
         "labels":[{"name":"err","value":"timeout"}],"value":9,"timestamp":0}
    ])");
    const auto series = in.get<std::vector<Metric>>();
    json payload;
    payload["metrics"] = series;
    LOGOS_ASSERT_EQ(payload["metrics"].size(), 2u);
    LOGOS_ASSERT_TRUE(payload["metrics"][1]["labels"].is_object());
    LOGOS_ASSERT_TRUE(
        payload["metrics"][1]["labels"]["err"].get<std::string>() == "timeout");
}
