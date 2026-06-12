#include "plugin.h"

using json = nlohmann::json;

LogosMap Libp2pModuleImpl::collectMetrics() {
    struct Series {
        const char* name;
        const char* type;
        const char* help;
    };
    // TODO: read from nim-libp2p's metrics registry via FFI.
    static constexpr Series kSeries[] = {
        {"libp2p_peers",                  "gauge",   "Number of connected peers"},
        {"libp2p_open_streams",           "gauge",   "Number of currently open streams"},
        {"libp2p_dial_attempts_total",    "counter", "Total number of dial attempts"},
        {"libp2p_successful_dials_total", "counter", "Number of successful dials"},
        {"libp2p_failed_dials_total",     "counter", "Number of failed dials"},
        {"libp2p_pubsub_peers",           "gauge",   "Number of peers connected to pubsub"},
        {"libp2p_pubsub_topics",          "gauge",   "Number of subscribed pubsub topics"},
    };

    json metrics = json::array();
    for (const auto& s : kSeries) {
        metrics.push_back({
            {"name",  s.name},
            {"type",  s.type},
            {"help",  s.help},
            {"value", 0},
        });
    }

    json payload;
    payload["metrics"] = std::move(metrics);
    return payload;
}
