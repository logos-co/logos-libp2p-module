#pragma once

#include <cstdint>
#include <map>
#include <string>

#include <nlohmann/json.hpp>

// One metrics series, mirroring the JSON that nim-libp2p's libp2p_collect_metrics
// emits. Cbind sends labels as `[{"name":..,"value":..}, ...]`; we flatten into
// a map because openmetrics-module's renderer expects `labels` as a flat
// `{key:value}` object (openmetrics_format.cpp:82). `timestamp` is Unix
// milliseconds from the registry; 0 means "unset" and is dropped on output.
struct Metric {
    std::string name;
    std::string type;
    std::string help;
    std::map<std::string, std::string> labels;
    double value = 0.0;
    int64_t timestamp = 0;
};

inline void from_json(const nlohmann::json& j, Metric& m) {
    j.at("name").get_to(m.name);
    j.at("type").get_to(m.type);
    j.at("help").get_to(m.help);
    j.at("value").get_to(m.value);
    m.labels.clear();
    if (auto it = j.find("labels"); it != j.end() && it->is_array()) {
        for (const auto& lp : *it) {
            m.labels.emplace(lp.at("name").get<std::string>(),
                             lp.at("value").get<std::string>());
        }
    }
    if (auto it = j.find("timestamp"); it != j.end() && it->is_number_integer()) {
        m.timestamp = it->get<int64_t>();
    } else {
        m.timestamp = 0;
    }
}

inline void to_json(nlohmann::json& j, const Metric& m) {
    j = nlohmann::json{
        {"name", m.name},
        {"type", m.type},
        {"help", m.help},
        {"labels", m.labels},
        {"value", m.value},
    };
    if (m.timestamp != 0) j["timestamp"] = m.timestamp;
}
