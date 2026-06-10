#include "plugin.h"

#include <cstring>

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Service Discovery
// ---------------------------------------------------------------------------

StdLogosResult Libp2pModuleImpl::discoStart() {
    return callSync("Failed to start discovery", [&](SyncPromise* p) {
        return libp2p_service_disco_start(ctx,
                                          &Libp2pModuleImpl::promiseCallback, p);
    });
}

StdLogosResult Libp2pModuleImpl::discoStop() {
    return callSync("Failed to stop discovery", [&](SyncPromise* p) {
        return libp2p_service_disco_stop(ctx,
                                         &Libp2pModuleImpl::promiseCallback, p);
    });
}

StdLogosResult Libp2pModuleImpl::discoStartAdvertising(
    const std::string& serviceId,
    const std::string& serviceData)
{
    return callSync("Failed to start advertising", [&](SyncPromise* p) {
        return libp2p_service_disco_start_advertising(
            ctx, serviceId.c_str(),
            serviceData.empty() ? nullptr : reinterpret_cast<const uint8_t*>(serviceData.data()),
            serviceData.size(),
            &Libp2pModuleImpl::promiseCallback, p);
    });
}

StdLogosResult Libp2pModuleImpl::discoStopAdvertising(const std::string& serviceId) {
    return callSync("Failed to stop advertising", [&](SyncPromise* p) {
        return libp2p_service_disco_stop_advertising(ctx, serviceId.c_str(),
                                                     &Libp2pModuleImpl::promiseCallback, p);
    });
}

StdLogosResult Libp2pModuleImpl::discoRegisterInterest(const std::string& serviceId) {
    return callSync("Failed to register interest", [&](SyncPromise* p) {
        return libp2p_service_disco_register_interest(ctx, serviceId.c_str(),
                                                      &Libp2pModuleImpl::promiseCallback, p);
    });
}

StdLogosResult Libp2pModuleImpl::discoUnregisterInterest(const std::string& serviceId) {
    return callSync("Failed to unregister interest", [&](SyncPromise* p) {
        return libp2p_service_disco_unregister_interest(ctx, serviceId.c_str(),
                                                        &Libp2pModuleImpl::promiseCallback, p);
    });
}

StdLogosResult Libp2pModuleImpl::discoLookup(
    const std::string& serviceId,
    const std::string& serviceData)
{
    return callSyncWith("Failed to lookup",
        [&](SyncPromise* p) {
            return libp2p_service_disco_lookup(
                ctx, serviceId.c_str(),
                serviceData.empty() ? nullptr : reinterpret_cast<const uint8_t*>(serviceData.data()),
                serviceData.size(),
                &Libp2pModuleImpl::promiseRandomRecordsCallback, p);
        },
        [](const SyncResult& r) -> StdLogosResult {
            if (r.message.empty()) return {true, json::array(), ""};
            return parseJsonResponse(r.message, "discoLookup");
        });
}

StdLogosResult Libp2pModuleImpl::discoRandomLookup() {
    return callSyncWith("Failed to random lookup",
        [&](SyncPromise* p) {
            return libp2p_service_disco_random_lookup(ctx,
                                                     &Libp2pModuleImpl::promiseRandomRecordsCallback, p);
        },
        [](const SyncResult& r) -> StdLogosResult {
            if (r.message.empty()) return {true, json::array(), ""};
            return parseJsonResponse(r.message, "discoRandomLookup");
        });
}
