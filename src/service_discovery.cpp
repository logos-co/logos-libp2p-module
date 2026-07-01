#include "plugin.h"

using json = nlohmann::json;

StdLogosResult Libp2pModuleImpl::discoStart() {
    return callSync("Failed to start discovery", [&](SyncPromise* p) {
        return libp2p_ctx_service_disco_start(ctx, &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::discoStop() {
    return callSync("Failed to stop discovery", [&](SyncPromise* p) {
        return libp2p_ctx_service_disco_stop(ctx, &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::discoStartAdvertising(
    const std::string& serviceId,
    const std::string& serviceData)
{
    StartAdvertisingRequest req{};
    req.serviceId = nimffi_str(serviceId.c_str());
    req.serviceData = NimFfiBytes{
        reinterpret_cast<uint8_t*>(const_cast<char*>(serviceData.data())), serviceData.size()};
    return callSync("Failed to start advertising", [&](SyncPromise* p) {
        return libp2p_ctx_service_disco_start_advertising(ctx, &req,
                                                          &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::discoStopAdvertising(const std::string& serviceId) {
    return callSync("Failed to stop advertising", [&](SyncPromise* p) {
        return libp2p_ctx_service_disco_stop_advertising(ctx, nimffi_str(serviceId.c_str()),
                                                         &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::discoRegisterInterest(const std::string& serviceId) {
    return callSync("Failed to register interest", [&](SyncPromise* p) {
        return libp2p_ctx_service_disco_register_interest(ctx, nimffi_str(serviceId.c_str()),
                                                          &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::discoUnregisterInterest(const std::string& serviceId) {
    return callSync("Failed to unregister interest", [&](SyncPromise* p) {
        return libp2p_ctx_service_disco_unregister_interest(ctx, nimffi_str(serviceId.c_str()),
                                                            &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::discoLookup(
    const std::string& serviceId,
    const std::string& serviceData)
{
    LookupRequest req{};
    req.serviceId = nimffi_str(serviceId.c_str());
    req.serviceData = NimFfiBytes{
        reinterpret_cast<uint8_t*>(const_cast<char*>(serviceData.data())), serviceData.size()};
    return callSyncWith("Failed to lookup",
        [&](SyncPromise* p) {
            return libp2p_ctx_service_disco_lookup(ctx, &req, &Libp2pModuleImpl::cbRecords, p);
        },
        [](const SyncResult& r) { return jsonResult(r, json::array()); });
}

StdLogosResult Libp2pModuleImpl::discoRandomLookup() {
    return callSyncWith("Failed to random lookup",
        [&](SyncPromise* p) {
            return libp2p_ctx_service_disco_random_lookup(ctx, &Libp2pModuleImpl::cbRecords, p);
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
    std::vector<NimFfiStr> addrsFfi;
    addrsFfi.reserve(addrs.size());
    for (const auto& a : addrs) addrsFfi.push_back(nimffi_str(a.c_str()));

    std::vector<ServiceInfoEntry> serviceEntries;
    serviceEntries.reserve(services.size());
    for (const auto& [id, data] : services) {
        ServiceInfoEntry entry{};
        entry.id = nimffi_str(id.c_str());
        entry.data = NimFfiBytes{
            reinterpret_cast<uint8_t*>(const_cast<char*>(data.data())), data.size()};
        serviceEntries.push_back(entry);
    }

    CreateXprRequest req{};
    req.addrs = LibP2PSeq_Str{addrsFfi.data(), addrsFfi.size()};
    req.services = LibP2PSeq_ServiceInfoEntry{serviceEntries.data(), serviceEntries.size()};
    req.seqNo = seqNo;

    return callSyncWith("Failed to create XPR",
        [&](SyncPromise* p) {
            return libp2p_ctx_create_xpr(ctx, &req, &Libp2pModuleImpl::cbBytes, p);
        },
        bufferToResult);
}

/// Verifies a signed XPR's signature and returns the decoded record (peerId,
/// seqNo, addrs, services). `xpr` is the base64 string produced by createXpr and
/// is decoded back to the signed protobuf bytes here, so createXpr's output can
/// be passed straight in. A bad signature or malformed payload yields a failed
/// result. Each service's `data` is base64-encoded, since it is arbitrary bytes
/// that may not be valid UTF-8.
StdLogosResult Libp2pModuleImpl::decodeXpr(const std::string& xpr) {
    if (xpr.empty()) return {false, {}, "decodeXpr: empty XPR"};

    std::string bytes;
    try {
        bytes = base64Decode(xpr);
    } catch (const std::invalid_argument& e) {
        return {false, {}, std::string("decodeXpr: invalid base64: ") + e.what()};
    }

    DecodeXprRequest req{};
    req.encoded = NimFfiBytes{
        reinterpret_cast<uint8_t*>(const_cast<char*>(bytes.data())), bytes.size()};

    return callSyncWith("Failed to decode XPR",
        [&](SyncPromise* p) {
            return libp2p_ctx_decode_xpr(ctx, &req, &Libp2pModuleImpl::cbRecord, p);
        },
        [](const SyncResult& r) -> StdLogosResult {
            if (!r.data.is_object()) {
                return {false, {}, "decodeXpr: no record decoded"};
            }
            return {true, r.data, ""};
        });
}
