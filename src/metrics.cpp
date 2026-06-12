#include "plugin.h"

using json = nlohmann::json;

LogosMap Libp2pModuleImpl::collectMetrics() {
    // Pass through nim-libp2p's whole metrics registry. It returns a JSON array
    // of metric series; before start (no ctx) or on error the call fails and we
    // return an empty-but-well-formed metrics array.
    std::string text;
    auto res = callSyncWith("Failed to collect metrics",
        [&](SyncPromise* p) {
            return libp2p_collect_metrics(ctx, &Libp2pModuleImpl::promiseCallback, p);
        },
        [](const SyncResult& r) -> StdLogosResult {
            return {true, r.message, ""};
        });
    if (res.success && res.value.is_string()) {
        text = res.value.get<std::string>();
    }

    const auto parsed = json::parse(text, nullptr, false);
    std::vector<Metric> series;
    if (parsed.is_array()) {
        series = parsed.get<std::vector<Metric>>();
    }

    json payload;
    payload["metrics"] = series;
    return payload;
}
