// Library-backed collectMetrics() test. Pure Metric JSON: unit_metrics.cpp.

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
