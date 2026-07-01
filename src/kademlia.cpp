#include "plugin.h"

using json = nlohmann::json;

StdLogosResult Libp2pModuleImpl::kadFindNode(const std::string& peerId) {
    return callSyncWith("Failed to find node",
        [&](SyncPromise* p) {
            return libp2p_ctx_kad_find_node(ctx, nimffi_str(peerId.c_str()),
                                            &Libp2pModuleImpl::cbPeers, p);
        },
        [](const SyncResult& r) { return jsonResult(r, json::array()); });
}

StdLogosResult Libp2pModuleImpl::kadPutValue(const std::string& key, const std::string& value) {
    KadPutValueRequest req{};
    req.key = NimFfiBytes{reinterpret_cast<uint8_t*>(const_cast<char*>(key.data())), key.size()};
    req.value = NimFfiBytes{reinterpret_cast<uint8_t*>(const_cast<char*>(value.data())), value.size()};
    return callSync("Failed to put value", [&](SyncPromise* p) {
        return libp2p_ctx_kad_put_value(ctx, &req, &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::kadGetValue(const std::string& key, int64_t quorum) {
    KadGetValueRequest req{};
    req.key = NimFfiBytes{reinterpret_cast<uint8_t*>(const_cast<char*>(key.data())), key.size()};
    req.quorum = quorum;
    return callSyncWith("Failed to get value",
        [&](SyncPromise* p) {
            return libp2p_ctx_kad_get_value(ctx, &req, &Libp2pModuleImpl::cbRead, p);
        },
        bufferToResult);
}

StdLogosResult Libp2pModuleImpl::kadAddProvider(const std::string& cid) {
    return callSync("Failed to add provider", [&](SyncPromise* p) {
        return libp2p_ctx_kad_add_provider(ctx, nimffi_str(cid.c_str()),
                                           &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::kadStartProviding(const std::string& cid) {
    return callSync("Failed to start providing", [&](SyncPromise* p) {
        return libp2p_ctx_kad_start_providing(ctx, nimffi_str(cid.c_str()),
                                              &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::kadStopProviding(const std::string& cid) {
    return callSync("Failed to stop providing", [&](SyncPromise* p) {
        return libp2p_ctx_kad_stop_providing(ctx, nimffi_str(cid.c_str()),
                                             &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::kadGetProviders(const std::string& cid) {
    return callSyncWith("Failed to get providers",
        [&](SyncPromise* p) {
            return libp2p_ctx_kad_get_providers(ctx, nimffi_str(cid.c_str()),
                                                &Libp2pModuleImpl::cbProviders, p);
        },
        [](const SyncResult& r) { return jsonResult(r, json::array()); });
}

StdLogosResult Libp2pModuleImpl::kadGetRandomRecords() {
    return callSyncWith("Failed to get random records",
        [&](SyncPromise* p) {
            return libp2p_ctx_kad_random_records(ctx, &Libp2pModuleImpl::cbRecords, p);
        },
        [](const SyncResult& r) { return jsonResult(r, json::array()); });
}
