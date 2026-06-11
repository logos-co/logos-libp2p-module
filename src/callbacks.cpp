#include "plugin.h"

#include <cstring>

using json = nlohmann::json;

void Libp2pModuleImpl::promisePeerInfoCallback(
    int ret, const Libp2pPeerInfo* info,
    const char* msg, size_t len, void* userData)
{
    auto* p = static_cast<SyncPromise*>(userData);
    SyncResult r;
    r.ok = (ret == RET_OK);

    if (ret == RET_OK && info) {
        json j;
        j["peerId"] = info->peerId ? info->peerId : "";
        json addrs = json::array();
        if (info->addrs) {
            for (size_t i = 0; i < info->addrsLen; ++i) {
                if (info->addrs[i]) addrs.push_back(info->addrs[i]);
            }
        }
        j["addrs"] = addrs;
        r.message = j.dump();
    } else {
        r.message = (msg && len > 0) ? std::string(msg, len) : std::string();
    }

    p->set_value(std::move(r));
    delete p;
}

void Libp2pModuleImpl::promisePeerStoreEntryCallback(
    int ret, const Libp2pPeerStoreEntry* entry,
    const char* msg, size_t len, void* userData)
{
    auto* p = static_cast<SyncPromise*>(userData);
    SyncResult r;
    r.ok = (ret == RET_OK);

    if (ret == RET_OK && entry) {
        json j;
        j["peerId"] = entry->peerId ? entry->peerId : "";
        json addrs = json::array();
        if (entry->addrs) {
            for (size_t i = 0; i < entry->addrsLen; ++i) {
                if (entry->addrs[i]) addrs.push_back(entry->addrs[i]);
            }
        }
        j["addrs"] = addrs;
        json protocols = json::array();
        if (entry->protocols) {
            for (size_t i = 0; i < entry->protocolsLen; ++i) {
                if (entry->protocols[i]) protocols.push_back(entry->protocols[i]);
            }
        }
        j["protocols"] = protocols;
        std::string publicKey;
        if (entry->publicKey && entry->publicKeyLen > 0) {
            static constexpr char digits[] = "0123456789abcdef";
            publicKey.resize(entry->publicKeyLen * 2);
            for (size_t i = 0; i < entry->publicKeyLen; ++i) {
                uint8_t b = entry->publicKey[i];
                publicKey[2 * i]     = digits[b >> 4];
                publicKey[2 * i + 1] = digits[b & 0x0f];
            }
        }
        j["publicKey"] = publicKey;
        j["agentVersion"] = entry->agentVersion ? entry->agentVersion : "";
        j["protoVersion"] = entry->protoVersion ? entry->protoVersion : "";
        r.message = j.dump();
    } else {
        r.message = (msg && len > 0) ? std::string(msg, len) : std::string();
    }

    p->set_value(std::move(r));
    delete p;
}

void Libp2pModuleImpl::promisePeersCallback(
    int ret, const char** peerIds, size_t peerIdsLen,
    const char* msg, size_t len, void* userData)
{
    auto* p = static_cast<SyncPromise*>(userData);
    SyncResult r;
    r.ok = (ret == RET_OK);

    if (ret == RET_OK && peerIds && peerIdsLen > 0) {
        json arr = json::array();
        for (size_t i = 0; i < peerIdsLen; ++i) {
            if (peerIds[i]) arr.push_back(peerIds[i]);
        }
        r.message = arr.dump();
    } else {
        r.message = (msg && len > 0) ? std::string(msg, len) : std::string();
    }

    p->set_value(std::move(r));
    delete p;
}

void Libp2pModuleImpl::promiseProvidersCallback(
    int ret, const Libp2pPeerInfo* providers, size_t providersLen,
    const char* msg, size_t len, void* userData)
{
    auto* p = static_cast<SyncPromise*>(userData);
    SyncResult r;
    r.ok = (ret == RET_OK);

    if (ret == RET_OK && providers && providersLen > 0) {
        json arr = json::array();
        for (size_t i = 0; i < providersLen; ++i) {
            json peer;
            peer["peerId"] = providers[i].peerId ? providers[i].peerId : "";
            json addrs = json::array();
            if (providers[i].addrs) {
                for (size_t j2 = 0; j2 < providers[i].addrsLen; ++j2) {
                    if (providers[i].addrs[j2]) addrs.push_back(providers[i].addrs[j2]);
                }
            }
            peer["addrs"] = addrs;
            arr.push_back(peer);
        }
        r.message = arr.dump();
    } else {
        r.message = (msg && len > 0) ? std::string(msg, len) : std::string();
    }

    p->set_value(std::move(r));
    delete p;
}

void Libp2pModuleImpl::promiseConnectionCallback(
    int ret, libp2p_stream_t* stream,
    const char* msg, size_t len, void* userData)
{
    auto* p = static_cast<SyncPromise*>(userData);
    SyncResult r;
    r.ok = (ret == RET_OK);
    r.message = (msg && len > 0) ? std::string(msg, len) : std::string();
    if (ret == RET_OK && stream) {
        r.extra = stream;
    }

    p->set_value(std::move(r));
    delete p;
}

void Libp2pModuleImpl::promiseReservationCallback(
    int ret, const char** addrs, size_t addrsLen, uint64_t expireTime,
    const char* msg, size_t len, void* userData)
{
    (void)expireTime;
    auto* p = static_cast<SyncPromise*>(userData);
    SyncResult r;
    r.ok = (ret == RET_OK);

    if (ret == RET_OK && addrs && addrsLen > 0) {
        json arr = json::array();
        for (size_t i = 0; i < addrsLen; ++i) {
            if (addrs[i]) arr.push_back(addrs[i]);
        }
        r.message = arr.dump();
    } else {
        r.message = (msg && len > 0) ? std::string(msg, len) : std::string();
    }

    p->set_value(std::move(r));
    delete p;
}

void Libp2pModuleImpl::promiseRandomRecordsCallback(
    int ret, const Libp2pExtendedPeerRecord* records, size_t recordsLen,
    const char* msg, size_t len, void* userData)
{
    auto* p = static_cast<SyncPromise*>(userData);
    SyncResult r;
    r.ok = (ret == RET_OK);

    if (ret == RET_OK && records && recordsLen > 0) {
        json arr = json::array();
        for (size_t i = 0; i < recordsLen; ++i) {
            json rec;
            rec["peerId"] = records[i].peerId ? records[i].peerId : "";
            rec["seqNo"] = records[i].seqNo;

            json addrs = json::array();
            if (records[i].addrs) {
                for (size_t a = 0; a < records[i].addrsLen; ++a) {
                    if (records[i].addrs[a]) addrs.push_back(records[i].addrs[a]);
                }
            }
            rec["addrs"] = addrs;

            json services = json::array();
            if (records[i].services) {
                for (size_t s = 0; s < records[i].servicesLen; ++s) {
                    json svc;
                    svc["id"] = records[i].services[s].id ? records[i].services[s].id : "";
                    if (records[i].services[s].data && records[i].services[s].dataLen > 0) {
                        svc["data"] = std::string(
                            reinterpret_cast<const char*>(records[i].services[s].data),
                            records[i].services[s].dataLen);
                    } else {
                        svc["data"] = "";
                    }
                    services.push_back(svc);
                }
            }
            rec["services"] = services;
            arr.push_back(rec);
        }
        r.message = arr.dump();
    } else {
        r.message = (msg && len > 0) ? std::string(msg, len) : std::string();
    }

    p->set_value(std::move(r));
    delete p;
}

void Libp2pModuleImpl::topicHandler(
    const char* topic, uint8_t* data, size_t len, void* userData)
{
    auto* subCtx = static_cast<Libp2pModuleImpl::SubscribeCtx*>(userData);
    if (!subCtx || !subCtx->instance) return;

    auto* self = subCtx->instance;
    std::string topicStr = topic ? topic : "";
    std::string payload(reinterpret_cast<const char*>(data), len);

    {
        std::lock_guard<std::mutex> lock(self->m_queueMutex);
        self->m_topicQueues[topicStr].push(std::move(payload));
        self->m_queueCond.notify_all();
    }

    json j;
    j["topic"] = topicStr;
    j["data"] = std::string(reinterpret_cast<const char*>(data), len);
    self->emitEventSafe("gossipsubMessage", j.dump());
}
