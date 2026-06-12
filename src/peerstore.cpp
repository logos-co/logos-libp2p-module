#include "plugin.h"

#include <cstring>

using json = nlohmann::json;

StdLogosResult Libp2pModuleImpl::peerstoreGetPeers() {
    return callSyncWith("Failed to get peers",
        [&](SyncPromise* p) {
            return libp2p_peerstore_get_peers(ctx,
                                               &Libp2pModuleImpl::promisePeersCallback, p);
        },
        [](const SyncResult& r) -> StdLogosResult {
            if (r.message.empty()) return {true, json::array(), ""};
            return parseJsonResponse(r.message, "peerstoreGetPeers");
        });
}

StdLogosResult Libp2pModuleImpl::peerstoreGetPeerInfo(const std::string& peerId) {
    return callSyncWith("Failed to get peer info",
        [&](SyncPromise* p) {
            return libp2p_peerstore_get_peer_info(ctx, peerId.c_str(),
                                                   &Libp2pModuleImpl::promisePeerStoreEntryCallback, p);
        },
        [](const SyncResult& r) -> StdLogosResult {
            if (r.message.empty()) return {true, json::object(), ""};
            return parseJsonResponse(r.message, "peerstoreGetPeerInfo");
        });
}

StdLogosResult Libp2pModuleImpl::peerstoreAddPeer(
    const std::string& peerId,
    const std::vector<std::string>& addrs,
    const std::vector<std::string>& protos)
{
    std::vector<const char*> addrsPtr;
    addrsPtr.reserve(addrs.size());
    for (const auto& a : addrs) addrsPtr.push_back(a.c_str());

    std::vector<const char*> protosPtr;
    protosPtr.reserve(protos.size());
    for (const auto& s : protos) protosPtr.push_back(s.c_str());

    return callSync("Failed to add peer", [&](SyncPromise* p) {
        return libp2p_peerstore_add_peer(
            ctx, peerId.c_str(),
            addrsPtr.empty() ? nullptr : addrsPtr.data(), addrsPtr.size(),
            protosPtr.empty() ? nullptr : protosPtr.data(), protosPtr.size(),
            &Libp2pModuleImpl::promiseCallback, p);
    });
}

StdLogosResult Libp2pModuleImpl::peerstoreSetPeerAddresses(
    const std::string& peerId,
    const std::vector<std::string>& addrs)
{
    std::vector<const char*> addrsPtr;
    addrsPtr.reserve(addrs.size());
    for (const auto& a : addrs) addrsPtr.push_back(a.c_str());

    return callSync("Failed to set peer addresses", [&](SyncPromise* p) {
        return libp2p_peerstore_set_peer_addresses(
            ctx, peerId.c_str(),
            addrsPtr.empty() ? nullptr : addrsPtr.data(), addrsPtr.size(),
            &Libp2pModuleImpl::promiseCallback, p);
    });
}

StdLogosResult Libp2pModuleImpl::peerstoreSetPeerProtocols(
    const std::string& peerId,
    const std::vector<std::string>& protos)
{
    std::vector<const char*> protosPtr;
    protosPtr.reserve(protos.size());
    for (const auto& s : protos) protosPtr.push_back(s.c_str());

    return callSync("Failed to set peer protocols", [&](SyncPromise* p) {
        return libp2p_peerstore_set_peer_protocols(
            ctx, peerId.c_str(),
            protosPtr.empty() ? nullptr : protosPtr.data(), protosPtr.size(),
            &Libp2pModuleImpl::promiseCallback, p);
    });
}

StdLogosResult Libp2pModuleImpl::peerstoreDeletePeer(const std::string& peerId) {
    return callSync("Failed to delete peer", [&](SyncPromise* p) {
        return libp2p_peerstore_delete_peer(ctx, peerId.c_str(),
                                             &Libp2pModuleImpl::promiseCallback, p);
    });
}
