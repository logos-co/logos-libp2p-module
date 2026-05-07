#include "plugin.h"

#include <cstring>

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Service Discovery
// ---------------------------------------------------------------------------

StdLogosResult Libp2pModuleImpl::discoStart() {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_service_disco_start(ctx,
                                         &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to start discovery"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::discoStop() {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_service_disco_stop(ctx,
                                        &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to stop discovery"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::discoStartAdvertising(
    const std::string& serviceId,
    const std::string& serviceData)
{
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_service_disco_start_advertising(
        ctx, serviceId.c_str(),
        serviceData.empty() ? nullptr : reinterpret_cast<const uint8_t*>(serviceData.data()),
        serviceData.size(),
        &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to start advertising"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::discoStopAdvertising(const std::string& serviceId) {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_service_disco_stop_advertising(ctx, serviceId.c_str(),
                                                    &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to stop advertising"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::discoStartDiscovering(const std::string& serviceId) {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_service_disco_start_discovering(ctx, serviceId.c_str(),
                                                     &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to start discovering"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::discoStopDiscovering(const std::string& serviceId) {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_service_disco_stop_discovering(ctx, serviceId.c_str(),
                                                    &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to stop discovering"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::discoLookup(
    const std::string& serviceId,
    const std::string& serviceData)
{
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_service_disco_lookup(
        ctx, serviceId.c_str(),
        serviceData.empty() ? nullptr : reinterpret_cast<const uint8_t*>(serviceData.data()),
        serviceData.size(),
        &Libp2pModuleImpl::promiseRandomRecordsCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to lookup"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    if (r.message.empty()) return {true, json::array(), ""};
    return {true, json::parse(r.message), ""};
}

StdLogosResult Libp2pModuleImpl::discoRandomLookup() {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_service_disco_random_lookup(ctx,
                                                 &Libp2pModuleImpl::promiseRandomRecordsCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to random lookup"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    if (r.message.empty()) return {true, json::array(), ""};
    return {true, json::parse(r.message), ""};
}
