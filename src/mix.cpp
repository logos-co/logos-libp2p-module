#include "plugin.h"

#include <cstring>

// ---------------------------------------------------------------------------
// Mix Network
// ---------------------------------------------------------------------------

StdLogosResult Libp2pModuleImpl::mixGeneratePrivKey() {
    if (!ctx) return {false, {}, "No libp2p context"};

    libp2p_curve25519_key_t key{};
    libp2p_mix_generate_priv_key(&key);

    return {true, std::string(reinterpret_cast<const char*>(&key), sizeof(key)), ""};
}

StdLogosResult Libp2pModuleImpl::mixPublicKey(const std::string& privKey) {
    if (!ctx) return {false, {}, "No libp2p context"};

    if (privKey.size() != sizeof(libp2p_curve25519_key_t)) {
        return {false, {}, "Invalid private key size"};
    }

    libp2p_curve25519_key_t inKey{};
    libp2p_curve25519_key_t outKey{};
    memcpy(&inKey, privKey.data(), sizeof(inKey));

    libp2p_mix_public_key(inKey, &outKey);

    return {true, std::string(reinterpret_cast<const char*>(&outKey), sizeof(outKey)), ""};
}

StdLogosResult Libp2pModuleImpl::mixDial(
    const std::string& peerId,
    const std::string& multiaddr,
    const std::string& proto)
{
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_mix_dial(ctx, peerId.c_str(), multiaddr.c_str(), proto.c_str(),
                              &Libp2pModuleImpl::promiseConnectionCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to mix dial"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};

    auto* stream = static_cast<libp2p_stream_t*>(r.extra);
    if (stream) {
        uint64_t streamId = addStream(stream);
        return {true, streamId, ""};
    }
    return {true, 0, ""};
}

StdLogosResult Libp2pModuleImpl::mixDialWithReply(
    const std::string& peerId,
    const std::string& multiaddr,
    const std::string& proto,
    int expectReply,
    uint8_t numSurbs)
{
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_mix_dial_with_reply(ctx, peerId.c_str(), multiaddr.c_str(),
                                         proto.c_str(), expectReply, numSurbs,
                                         &Libp2pModuleImpl::promiseConnectionCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to mix dial with reply"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};

    auto* stream = static_cast<libp2p_stream_t*>(r.extra);
    if (stream) {
        uint64_t streamId = addStream(stream);
        return {true, streamId, ""};
    }
    return {true, 0, ""};
}

StdLogosResult Libp2pModuleImpl::mixRegisterDestReadBehavior(
    const std::string& proto,
    int behavior,
    uint32_t sizeParam)
{
    if (!ctx) return {false, {}, "No libp2p context"};

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_mix_register_dest_read_behavior(
        ctx, proto.c_str(),
        static_cast<Libp2pMixReadBehavior>(behavior), sizeParam,
        &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to register dest read behavior"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::mixSetNodeInfo(
    const std::string& multiaddr,
    const std::string& mixPrivKey)
{
    if (!ctx) return {false, {}, "No libp2p context"};

    if (mixPrivKey.size() != sizeof(libp2p_curve25519_key_t)) {
        return {false, {}, "Invalid key size"};
    }

    libp2p_curve25519_key_t key{};
    memcpy(&key, mixPrivKey.data(), sizeof(key));

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_mix_set_node_info(ctx, multiaddr.c_str(), key,
                                       &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to set node info"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}

StdLogosResult Libp2pModuleImpl::mixNodepoolAdd(
    const std::string& peerId,
    const std::string& multiaddr,
    const std::string& mixPubKey,
    const std::string& libp2pPubKey)
{
    if (!ctx) return {false, {}, "No libp2p context"};

    if (mixPubKey.size() != sizeof(libp2p_curve25519_key_t) ||
        libp2pPubKey.size() != sizeof(libp2p_secp256k1_pubkey_t)) {
        return {false, {}, "Invalid key sizes"};
    }

    libp2p_curve25519_key_t mixKey{};
    libp2p_secp256k1_pubkey_t lpKey{};
    memcpy(&mixKey, mixPubKey.data(), sizeof(mixKey));
    memcpy(&lpKey, libp2pPubKey.data(), sizeof(lpKey));

    auto* p = new SyncPromise();
    auto f = p->get_future();
    int ret = libp2p_mix_nodepool_add(ctx, peerId.c_str(), multiaddr.c_str(),
                                      mixKey, lpKey,
                                      &Libp2pModuleImpl::promiseCallback, p);
    if (ret != RET_OK) { delete p; return {false, {}, "Failed to add to nodepool"}; }

    auto r = awaitResult(f);
    if (!r.ok) return {false, {}, r.message};
    return {true, {}, ""};
}
