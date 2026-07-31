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
    req.serviceData = nimffiBytes(serviceData);
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
    req.serviceData = nimffiBytes(serviceData);
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
/// `services` maps a service id to its advertised payload, which is arbitrary
/// bytes: it is carried as `bstr` and reaches the record byte for byte, with no
/// UTF-8 round-trip in between.
StdLogosResult Libp2pModuleImpl::createXpr(
    const std::vector<std::string>& addrs,
    const std::map<std::string, std::vector<uint8_t>>& services,
    uint64_t seqNo)
{
    auto addrsFfi = toNimFfiStrs(addrs);

    std::vector<ServiceInfoEntry> serviceEntries;
    serviceEntries.reserve(services.size());
    for (const auto& [id, data] : services) {
        ServiceInfoEntry entry{};
        entry.id = nimffi_str(id.c_str());
        entry.data = nimffiBytes(data);
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
/// result. `services` comes back as `[{id, data}]` — the shape discoLookup and
/// discoRandomLookup also use — with each `data` base64-encoded, since it is
/// arbitrary bytes that may not be valid UTF-8 and this result is untyped JSON.
/// Feeding it back into createXpr means rebuilding the map and base64-decoding.
StdLogosResult Libp2pModuleImpl::decodeXpr(const std::string& xpr) {
    if (xpr.empty()) return {false, {}, "decodeXpr: empty XPR"};

    std::string bytes;
    try {
        bytes = base64Decode(xpr);
    } catch (const std::invalid_argument& e) {
        return {false, {}, std::string("decodeXpr: invalid base64: ") + e.what()};
    }

    DecodeXprRequest req{};
    req.encoded = nimffiBytes(bytes);

    return callStaticWith("Failed to decode XPR",
        [&](SyncPromise* p) {
            return libp2p_static_decode_xpr(&req, &Libp2pModuleImpl::cbRecord, p);
        },
        [](const SyncResult& r) -> StdLogosResult {
            if (!r.data.is_object()) {
                return {false, {}, "decodeXpr: no record decoded"};
            }
            return {true, r.data, ""};
        });
}
