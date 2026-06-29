#include "plugin.h"

#include <cstring>

using json = nlohmann::json;

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
        [](const SyncResult& r) { return jsonResult(r, json::array()); });
}

StdLogosResult Libp2pModuleImpl::discoRandomLookup() {
    return callSyncWith("Failed to random lookup",
        [&](SyncPromise* p) {
            return libp2p_service_disco_random_lookup(ctx,
                                                     &Libp2pModuleImpl::promiseRandomRecordsCallback, p);
        },
        [](const SyncResult& r) { return jsonResult(r, json::array()); });
}

/// Builds and signs the node's own Extended Peer Record, returning the signed
/// protobuf bytes. Empty `addrs` uses the listen addresses; `seqNo` 0 uses now.
StdLogosResult Libp2pModuleImpl::createXpr(
    const std::vector<std::string>& addrs,
    const std::vector<std::pair<std::string, std::string>>& services,
    uint64_t seqNo)
{
    std::vector<const char*> addrPtrs;
    addrPtrs.reserve(addrs.size());
    for (const auto& addr : addrs) {
        addrPtrs.push_back(addr.c_str());
    }

    std::vector<Libp2pServiceInfo> serviceInfos;
    serviceInfos.reserve(services.size());
    for (const auto& [id, data] : services) {
        serviceInfos.push_back(Libp2pServiceInfo{
            .id = const_cast<char*>(id.c_str()),
            .data = data.empty() ? nullptr : reinterpret_cast<const uint8_t*>(data.data()),
            .dataLen = data.size()});
    }

    return callSyncWith("Failed to create XPR",
        [&](SyncPromise* p) {
            return libp2p_create_xpr(
                ctx, addrPtrs.empty() ? nullptr : addrPtrs.data(), addrPtrs.size(),
                serviceInfos.empty() ? nullptr : serviceInfos.data(), serviceInfos.size(),
                seqNo, &Libp2pModuleImpl::promiseBufferCallback, p);
        },
        bufferToResult);
}
