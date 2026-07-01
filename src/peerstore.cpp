#include "plugin.h"

using json = nlohmann::json;

StdLogosResult Libp2pModuleImpl::peerstoreGetPeers() {
    return callSyncWith("Failed to get peers",
        [&](SyncPromise* p) {
            return libp2p_ctx_peerstore_get_peers(ctx, &Libp2pModuleImpl::cbPeers, p);
        },
        [](const SyncResult& r) { return jsonResult(r, json::array()); });
}

StdLogosResult Libp2pModuleImpl::peerstoreGetPeerInfo(const std::string& peerId) {
    return callSyncWith("Failed to get peer info",
        [&](SyncPromise* p) {
            return libp2p_ctx_peerstore_get_peer_info(ctx, nimffi_str(peerId.c_str()),
                                                      &Libp2pModuleImpl::cbPeerStoreEntry, p);
        },
        [](const SyncResult& r) { return jsonResult(r, json::object()); });
}

StdLogosResult Libp2pModuleImpl::peerstoreAddPeer(
    const std::string& peerId,
    const std::vector<std::string>& addrs,
    const std::vector<std::string>& protos)
{
    std::vector<NimFfiStr> addrsFfi;
    addrsFfi.reserve(addrs.size());
    for (const auto& a : addrs) addrsFfi.push_back(nimffi_str(a.c_str()));
    std::vector<NimFfiStr> protosFfi;
    protosFfi.reserve(protos.size());
    for (const auto& s : protos) protosFfi.push_back(nimffi_str(s.c_str()));

    AddPeerRequest req{};
    req.peerId = nimffi_str(peerId.c_str());
    req.addrs = LibP2PSeq_Str{addrsFfi.data(), addrsFfi.size()};
    req.protocols = LibP2PSeq_Str{protosFfi.data(), protosFfi.size()};

    return callSync("Failed to add peer", [&](SyncPromise* p) {
        return libp2p_ctx_peerstore_add_peer(ctx, &req, &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::peerstoreSetPeerAddresses(
    const std::string& peerId,
    const std::vector<std::string>& addrs)
{
    std::vector<NimFfiStr> addrsFfi;
    addrsFfi.reserve(addrs.size());
    for (const auto& a : addrs) addrsFfi.push_back(nimffi_str(a.c_str()));

    SetAddressesRequest req{};
    req.peerId = nimffi_str(peerId.c_str());
    req.addrs = LibP2PSeq_Str{addrsFfi.data(), addrsFfi.size()};

    return callSync("Failed to set peer addresses", [&](SyncPromise* p) {
        return libp2p_ctx_peerstore_set_peer_addresses(ctx, &req, &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::peerstoreSetPeerProtocols(
    const std::string& peerId,
    const std::vector<std::string>& protos)
{
    std::vector<NimFfiStr> protosFfi;
    protosFfi.reserve(protos.size());
    for (const auto& s : protos) protosFfi.push_back(nimffi_str(s.c_str()));

    SetProtocolsRequest req{};
    req.peerId = nimffi_str(peerId.c_str());
    req.protocols = LibP2PSeq_Str{protosFfi.data(), protosFfi.size()};

    return callSync("Failed to set peer protocols", [&](SyncPromise* p) {
        return libp2p_ctx_peerstore_set_peer_protocols(ctx, &req, &Libp2pModuleImpl::cbBool, p);
    });
}

StdLogosResult Libp2pModuleImpl::peerstoreDeletePeer(const std::string& peerId) {
    return callSync("Failed to delete peer", [&](SyncPromise* p) {
        return libp2p_ctx_peerstore_delete_peer(ctx, nimffi_str(peerId.c_str()),
                                                &Libp2pModuleImpl::cbBool, p);
    });
}
