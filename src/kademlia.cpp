#include "plugin.h"

#include <cstring>

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Kademlia
// ---------------------------------------------------------------------------

StdLogosResult Libp2pModuleImpl::kadFindNode(const std::string& peerId) {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_kad_find_node(ctx, peerId.c_str(),
                                   &Libp2pModuleImpl::promisePeersCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to find node (ret=" + std::to_string(ret) + ")"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    if (r.message.empty()) return {true, json::array(), ""};
    return parseJsonResponse(r.message, "kadFindNode");
}

StdLogosResult Libp2pModuleImpl::kadPutValue(const std::string& key, const std::string& value) {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_kad_put_value(
        ctx,
        reinterpret_cast<const uint8_t*>(key.data()), key.size(),
        reinterpret_cast<const uint8_t*>(value.data()), value.size(),
        &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to put value"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::kadGetValue(const std::string& key, int quorum) {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_kad_get_value(
        ctx,
        reinterpret_cast<const uint8_t*>(key.data()), key.size(),
        quorum,
        &Libp2pModuleImpl::promiseBufferCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to get value"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, std::string(r.buffer.begin(), r.buffer.end()), ""};
}

StdLogosResult Libp2pModuleImpl::kadAddProvider(const std::string& cid) {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_kad_add_provider(ctx, cid.c_str(),
                                      &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to add provider"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::kadStartProviding(const std::string& cid) {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_kad_start_providing(ctx, cid.c_str(),
                                         &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to start providing"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::kadStopProviding(const std::string& cid) {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_kad_stop_providing(ctx, cid.c_str(),
                                        &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to stop providing"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::kadGetProviders(const std::string& cid) {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_kad_get_providers(ctx, cid.c_str(),
                                       &Libp2pModuleImpl::promiseProvidersCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to get providers (ret=" + std::to_string(ret) + ")"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    if (r.message.empty()) return {true, json::array(), ""};
    return parseJsonResponse(r.message, "kadGetProviders");
}

StdLogosResult Libp2pModuleImpl::kadGetRandomRecords() {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_kad_random_records(ctx,
                                        &Libp2pModuleImpl::promiseRandomRecordsCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to get random records (ret=" + std::to_string(ret) + ")"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    if (r.message.empty()) return {true, json::array(), ""};
    return parseJsonResponse(r.message, "kadGetRandomRecords");
}
