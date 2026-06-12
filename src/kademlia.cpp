#include "plugin.h"

#include <cstring>

using json = nlohmann::json;

StdLogosResult Libp2pModuleImpl::kadFindNode(const std::string& peerId) {
    return callSyncWith("Failed to find node",
        [&](SyncPromise* p) {
            return libp2p_kad_find_node(ctx, peerId.c_str(),
                                        &Libp2pModuleImpl::promisePeersCallback, p);
        },
        [](const SyncResult& r) -> StdLogosResult {
            if (r.message.empty()) return {true, json::array(), ""};
            return parseJsonResponse(r.message, "kadFindNode");
        });
}

StdLogosResult Libp2pModuleImpl::kadPutValue(const std::string& key, const std::string& value) {
    return callSync("Failed to put value", [&](SyncPromise* p) {
        return libp2p_kad_put_value(
            ctx,
            reinterpret_cast<const uint8_t*>(key.data()), key.size(),
            reinterpret_cast<const uint8_t*>(value.data()), value.size(),
            &Libp2pModuleImpl::promiseCallback, p);
    });
}

StdLogosResult Libp2pModuleImpl::kadGetValue(const std::string& key, int quorum) {
    return callSyncWith("Failed to get value",
        [&](SyncPromise* p) {
            return libp2p_kad_get_value(
                ctx,
                reinterpret_cast<const uint8_t*>(key.data()), key.size(),
                quorum,
                &Libp2pModuleImpl::promiseBufferCallback, p);
        },
        [](const SyncResult& r) -> StdLogosResult {
            return {true, std::string(r.buffer.begin(), r.buffer.end()), ""};
        });
}

StdLogosResult Libp2pModuleImpl::kadAddProvider(const std::string& cid) {
    return callSync("Failed to add provider", [&](SyncPromise* p) {
        return libp2p_kad_add_provider(ctx, cid.c_str(),
                                       &Libp2pModuleImpl::promiseCallback, p);
    });
}

StdLogosResult Libp2pModuleImpl::kadStartProviding(const std::string& cid) {
    return callSync("Failed to start providing", [&](SyncPromise* p) {
        return libp2p_kad_start_providing(ctx, cid.c_str(),
                                          &Libp2pModuleImpl::promiseCallback, p);
    });
}

StdLogosResult Libp2pModuleImpl::kadStopProviding(const std::string& cid) {
    return callSync("Failed to stop providing", [&](SyncPromise* p) {
        return libp2p_kad_stop_providing(ctx, cid.c_str(),
                                         &Libp2pModuleImpl::promiseCallback, p);
    });
}

StdLogosResult Libp2pModuleImpl::kadGetProviders(const std::string& cid) {
    return callSyncWith("Failed to get providers",
        [&](SyncPromise* p) {
            return libp2p_kad_get_providers(ctx, cid.c_str(),
                                            &Libp2pModuleImpl::promiseProvidersCallback, p);
        },
        [](const SyncResult& r) -> StdLogosResult {
            if (r.message.empty()) return {true, json::array(), ""};
            return parseJsonResponse(r.message, "kadGetProviders");
        });
}

StdLogosResult Libp2pModuleImpl::kadGetRandomRecords() {
    return callSyncWith("Failed to get random records",
        [&](SyncPromise* p) {
            return libp2p_kad_random_records(ctx,
                                             &Libp2pModuleImpl::promiseRandomRecordsCallback, p);
        },
        [](const SyncResult& r) -> StdLogosResult {
            if (r.message.empty()) return {true, json::array(), ""};
            return parseJsonResponse(r.message, "kadGetRandomRecords");
        });
}
