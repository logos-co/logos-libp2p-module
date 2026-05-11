#include "plugin.h"

#include <cstring>

using json = nlohmann::json;

// ---------------------------------------------------------------------------
// Peerstore
// ---------------------------------------------------------------------------

StdLogosResult Libp2pModuleImpl::peerstoreGetPeers() {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_peerstore_get_peers(ctx,
                                          &Libp2pModuleImpl::promisePeersCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to get peers"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    if (r.message.empty()) return {true, json::array(), ""};
    return {true, json::parse(r.message), ""};
}

StdLogosResult Libp2pModuleImpl::peerstoreGetPeerInfo(const std::string& peerId) {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_peerstore_get_peer_info(ctx, peerId.c_str(),
                                              &Libp2pModuleImpl::promisePeerStoreEntryCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to get peer info"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    if (r.message.empty()) return {true, {}, ""};
    return {true, json::parse(r.message), ""};
}

StdLogosResult Libp2pModuleImpl::peerstoreAddPeer(
    const std::string& peerId,
    const std::vector<std::string>& addrs,
    const std::vector<std::string>& protos)
{
    if (!ctx) return {false, {}, "No libp2p context"};

    std::vector<const char*> addrsPtr;
    addrsPtr.reserve(addrs.size());
    for (const auto& a : addrs) addrsPtr.push_back(a.c_str());

    std::vector<const char*> protosPtr;
    protosPtr.reserve(protos.size());
    for (const auto& p : protos) protosPtr.push_back(p.c_str());

    auto* pr = new SyncPromise();
    auto f = pr->get_future();
    int ret = libp2p_peerstore_add_peer(
        ctx, peerId.c_str(),
        addrsPtr.empty() ? nullptr : addrsPtr.data(), addrsPtr.size(),
        protosPtr.empty() ? nullptr : protosPtr.data(), protosPtr.size(),
        &Libp2pModuleImpl::promiseCallback, pr);
    if (ret != RET_OK) { delete pr; return {false, {}, "Failed to add peer"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::peerstoreSetPeerAddresses(
    const std::string& peerId,
    const std::vector<std::string>& addrs)
{
    if (!ctx) return {false, {}, "No libp2p context"};

    std::vector<const char*> addrsPtr;
    addrsPtr.reserve(addrs.size());
    for (const auto& a : addrs) addrsPtr.push_back(a.c_str());

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_peerstore_set_peer_addresses(
        ctx, peerId.c_str(),
        addrsPtr.empty() ? nullptr : addrsPtr.data(), addrsPtr.size(),
        &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to set peer addresses"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::peerstoreSetPeerProtocols(
    const std::string& peerId,
    const std::vector<std::string>& protos)
{
    if (!ctx) return {false, {}, "No libp2p context"};

    std::vector<const char*> protosPtr;
    protosPtr.reserve(protos.size());
    for (const auto& p : protos) protosPtr.push_back(p.c_str());

    auto* pr = new SyncPromise();
    auto f = pr->get_future();
    int ret = libp2p_peerstore_set_peer_protocols(
        ctx, peerId.c_str(),
        protosPtr.empty() ? nullptr : protosPtr.data(), protosPtr.size(),
        &Libp2pModuleImpl::promiseCallback, pr);
    if (ret != RET_OK) { delete pr; return {false, {}, "Failed to set peer protocols"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::peerstoreDeletePeer(const std::string& peerId) {
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_peerstore_delete_peer(ctx, peerId.c_str(),
                                            &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to delete peer"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}
