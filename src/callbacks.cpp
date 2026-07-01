#include "plugin.h"

using json = nlohmann::json;

namespace {
json extendedRecordToJson(const Libp2pExtendedPeerRecord& rec) {
    json out;
    out["peerId"] = rec.peerId ? rec.peerId : "";
    out["seqNo"] = rec.seqNo;

    json addrs = json::array();
    if (rec.addrs) {
        for (size_t a = 0; a < rec.addrsLen; ++a) {
            if (rec.addrs[a]) addrs.push_back(rec.addrs[a]);
        }
    }
    out["addrs"] = addrs;

    json services = json::array();
    if (rec.services) {
        for (size_t s = 0; s < rec.servicesLen; ++s) {
            json svc;
            svc["id"] = rec.services[s].id ? rec.services[s].id : "";
            std::vector<uint8_t> data;
            if (rec.services[s].data && rec.services[s].dataLen > 0) {
                data.assign(rec.services[s].data,
                            rec.services[s].data + rec.services[s].dataLen);
            }
            svc["data"] = base64Encode(data);
            services.push_back(svc);
        }
    }
    out["services"] = services;
    return out;
}
}  // namespace

void Libp2pModuleImpl::promisePeerInfoCallback(
    int ret, const Libp2pPeerInfo* info,
    const char* msg, size_t len, void* userData)
{
    auto r = basicResult(ret, msg, len);

    if (r.ok && info) {
        r.data = peerInfoToJson(*info);
    }

    finishPromise(static_cast<SyncPromise*>(userData), std::move(r));
}

void Libp2pModuleImpl::promisePeerStoreEntryCallback(
    int ret, const Libp2pPeerStoreEntry* entry,
    const char* msg, size_t len, void* userData)
{
    auto r = basicResult(ret, msg, len);

    if (r.ok && entry) {
        json j;
        j["peerId"] = entry->peerId ? entry->peerId : "";
        j["addrs"] = cStrArrayToJson(entry->addrs, entry->addrsLen);
        j["protocols"] = cStrArrayToJson(entry->protocols, entry->protocolsLen);
        j["publicKey"] = hexEncode(entry->publicKey, entry->publicKeyLen);
        j["agentVersion"] = entry->agentVersion ? entry->agentVersion : "";
        j["protoVersion"] = entry->protoVersion ? entry->protoVersion : "";
        r.data = std::move(j);
    }

    finishPromise(static_cast<SyncPromise*>(userData), std::move(r));
}

void Libp2pModuleImpl::promisePeersCallback(
    int ret, const char** peerIds, size_t peerIdsLen,
    const char* msg, size_t len, void* userData)
{
    auto r = basicResult(ret, msg, len);

    if (r.ok && peerIds && peerIdsLen > 0) {
        r.data = cStrArrayToJson(peerIds, peerIdsLen);
    }

    finishPromise(static_cast<SyncPromise*>(userData), std::move(r));
}

void Libp2pModuleImpl::promiseProvidersCallback(
    int ret, const Libp2pPeerInfo* providers, size_t providersLen,
    const char* msg, size_t len, void* userData)
{
    auto r = basicResult(ret, msg, len);

    if (r.ok && providers && providersLen > 0) {
        json arr = json::array();
        for (size_t i = 0; i < providersLen; ++i) {
            arr.push_back(peerInfoToJson(providers[i]));
        }
        r.data = std::move(arr);
    }

    finishPromise(static_cast<SyncPromise*>(userData), std::move(r));
}

void Libp2pModuleImpl::promiseConnectionCallback(
    int ret, libp2p_stream_t* stream,
    const char* msg, size_t len, void* userData)
{
    auto r = basicResult(ret, msg, len);
    if (r.ok && stream) {
        r.stream = stream;
    }

    finishPromise(static_cast<SyncPromise*>(userData), std::move(r));
}

void Libp2pModuleImpl::promiseReservationCallback(
    int ret, const char** addrs, size_t addrsLen, uint64_t expireTime,
    const char* msg, size_t len, void* userData)
{
    (void)expireTime;
    auto r = basicResult(ret, msg, len);

    if (r.ok && addrs && addrsLen > 0) {
        r.data = cStrArrayToJson(addrs, addrsLen);
    }

    finishPromise(static_cast<SyncPromise*>(userData), std::move(r));
}

void Libp2pModuleImpl::promiseRandomRecordsCallback(
    int ret, const Libp2pExtendedPeerRecord* records, size_t recordsLen,
    const char* msg, size_t len, void* userData)
{
    auto r = basicResult(ret, msg, len);

    if (r.ok && records && recordsLen > 0) {
        json arr = json::array();
        for (size_t i = 0; i < recordsLen; ++i) {
            arr.push_back(extendedRecordToJson(records[i]));
        }
        r.data = std::move(arr);
    }

    finishPromise(static_cast<SyncPromise*>(userData), std::move(r));
}

void Libp2pModuleImpl::promiseExtendedPeerRecordCallback(
    int ret, const Libp2pExtendedPeerRecord* record,
    const char* msg, size_t len, void* userData)
{
    auto* p = static_cast<SyncPromise*>(userData);
    SyncResult r;
    r.ok = (ret == RET_OK);

    if (ret == RET_OK && record) {
        r.message = extendedRecordToJson(*record).dump();
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
