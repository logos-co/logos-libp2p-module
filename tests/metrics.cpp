#include <logos_test.h>
#include <plugin.h>

#include <set>
#include <string>

namespace {
StdLogosResult collect() {
    Libp2pModuleImpl plugin;
    return plugin.collectMetrics();
}
}

LOGOS_TEST(collect_metrics_returns_success) {
    auto res = collect();
    LOGOS_ASSERT_TRUE(res.success);
    LOGOS_ASSERT_TRUE(res.error.empty());
}

LOGOS_TEST(collect_metrics_payload_has_metrics_array) {
    auto res = collect();
    LOGOS_ASSERT_TRUE(res.value.is_object());
    LOGOS_ASSERT_TRUE(res.value.contains("metrics"));
    LOGOS_ASSERT_TRUE(res.value["metrics"].is_array());
    LOGOS_ASSERT_TRUE(res.value["metrics"].size() > 0);
}

LOGOS_TEST(collect_metrics_entries_match_openmetrics_schema) {
    auto res = collect();

    const std::set<std::string> validTypes = {
        "counter", "gauge", "histogram", "summary"
    };

    for (const auto& m : res.value["metrics"]) {
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

LOGOS_TEST(collect_metrics_counter_names_carry_total_suffix) {
    auto res = collect();

    for (const auto& m : res.value["metrics"]) {
        if (m["type"].get<std::string>() != "counter") continue;
        const auto name = m["name"].get<std::string>();
        const std::string suffix = "_total";
        const bool hasTotalSuffix =
            name.size() >= suffix.size() &&
            name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0;
        LOGOS_ASSERT_TRUE(hasTotalSuffix);
    }
}

LOGOS_TEST(collect_metrics_includes_expected_libp2p_series) {
    auto res = collect();

    std::set<std::string> names;
    for (const auto& m : res.value["metrics"]) {
        names.insert(m["name"].get<std::string>());
    }

    for (const auto* expected : {
        "libp2p_peers",
        "libp2p_open_streams",
        "libp2p_dial_attempts_total",
        "libp2p_successful_dials_total",
        "libp2p_failed_dials_total",
        "libp2p_pubsub_peers",
        "libp2p_pubsub_topics",
    }) {
        LOGOS_ASSERT_TRUE(names.count(expected));
    }
}
