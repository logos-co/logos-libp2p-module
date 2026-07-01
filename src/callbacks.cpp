#include "plugin.h"

using json = nlohmann::json;

namespace {
// {peerId, seqNo, addrs, services:[{id, data}]}. Service `data` is arbitrary
// bytes, so it is base64-encoded to keep the JSON valid UTF-8.
json recordEntryToJson(const ExtendedPeerRecordEntry& rec) {
    json out;
    out["peerId"] = nfStr(rec.peerId);
    out["seqNo"] = rec.seqNo;
    out["addrs"] = seqStrToJson(rec.addrs);

    json services = json::array();
    if (rec.services.data) {
        for (size_t i = 0; i < rec.services.len; ++i) {
            const auto& s = rec.services.data[i];
            json svc;
            svc["id"] = nfStr(s.id);
            svc["data"] = base64Encode(nfBytes(s.data));
            services.push_back(svc);
        }
    }
    out["services"] = services;
    return out;
}

json providerToJson(const ProviderInfo& p) {
    json j;
    j["peerId"] = nfStr(p.peerId);
    j["addrs"] = seqStrToJson(p.addrs);
    return j;
}
}  // namespace

void Libp2pModuleImpl::cbBool(int ec, const bool*, const char* em, void* ud) {
    finishPromise(static_cast<SyncPromise*>(ud), replyBase(ec, em));
}

void Libp2pModuleImpl::cbBytes(int ec, const NimFfiBytes* reply, const char* em, void* ud) {
    auto r = replyBase(ec, em);
    if (r.ok && reply) r.buffer = nfBytes(*reply);
    finishPromise(static_cast<SyncPromise*>(ud), std::move(r));
}

void Libp2pModuleImpl::cbStr(int ec, const NimFfiStr* reply, const char* em, void* ud) {
    auto r = replyBase(ec, em);
    if (r.ok && reply) r.message = nfStr(*reply);
    finishPromise(static_cast<SyncPromise*>(ud), std::move(r));
}

void Libp2pModuleImpl::cbRead(int ec, const ReadResponse* reply, const char* em, void* ud) {
    auto r = replyBase(ec, em);
    if (r.ok && reply) r.buffer = nfBytes(reply->data);
    finishPromise(static_cast<SyncPromise*>(ud), std::move(r));
}

void Libp2pModuleImpl::cbCreate(int ec, LibP2PCtx* newCtx, const char* em, void* ud) {
    auto r = replyBase(ec, em);
    if (r.ok) r.newCtx = newCtx;
    finishPromise(static_cast<SyncPromise*>(ud), std::move(r));
}

void Libp2pModuleImpl::cbPeerInfo(int ec, const PeerInfoResponse* reply, const char* em, void* ud) {
    auto r = replyBase(ec, em);
    if (r.ok && reply) r.data = peerInfoToJson(*reply);
    finishPromise(static_cast<SyncPromise*>(ud), std::move(r));
}

void Libp2pModuleImpl::cbPeers(int ec, const PeersResponse* reply, const char* em, void* ud) {
    auto r = replyBase(ec, em);
    if (r.ok && reply) r.data = seqStrToJson(reply->peerIds);
    finishPromise(static_cast<SyncPromise*>(ud), std::move(r));
}

void Libp2pModuleImpl::cbDial(int ec, const DialResponse* reply, const char* em, void* ud) {
    auto r = replyBase(ec, em);
    if (r.ok && reply) r.data = reply->streamId;
    finishPromise(static_cast<SyncPromise*>(ud), std::move(r));
}

void Libp2pModuleImpl::cbProviders(int ec, const ProvidersResponse* reply, const char* em, void* ud) {
    auto r = replyBase(ec, em);
    if (r.ok && reply && reply->providers.data) {
        json arr = json::array();
        for (size_t i = 0; i < reply->providers.len; ++i) {
            arr.push_back(providerToJson(reply->providers.data[i]));
        }
        r.data = std::move(arr);
    }
    finishPromise(static_cast<SyncPromise*>(ud), std::move(r));
}

void Libp2pModuleImpl::cbRecords(int ec, const ExtendedRecordsResponse* reply, const char* em, void* ud) {
    auto r = replyBase(ec, em);
    if (r.ok && reply && reply->records.data) {
        json arr = json::array();
        for (size_t i = 0; i < reply->records.len; ++i) {
            arr.push_back(recordEntryToJson(reply->records.data[i]));
        }
        r.data = std::move(arr);
    }
    finishPromise(static_cast<SyncPromise*>(ud), std::move(r));
}

void Libp2pModuleImpl::cbRecord(int ec, const ExtendedPeerRecordEntry* reply, const char* em, void* ud) {
    auto r = replyBase(ec, em);
    if (r.ok && reply) r.data = recordEntryToJson(*reply);
    finishPromise(static_cast<SyncPromise*>(ud), std::move(r));
}

void Libp2pModuleImpl::cbReservation(int ec, const ReservationResponse* reply, const char* em, void* ud) {
    auto r = replyBase(ec, em);
    if (r.ok && reply) r.data = seqStrToJson(reply->addrs);
    finishPromise(static_cast<SyncPromise*>(ud), std::move(r));
}

void Libp2pModuleImpl::cbPeerStoreEntry(
    int ec, const PeerStoreEntryResponse* reply, const char* em, void* ud)
{
    auto r = replyBase(ec, em);
    if (r.ok && reply) {
        json j;
        j["peerId"] = nfStr(reply->peerId);
        j["addrs"] = seqStrToJson(reply->addrs);
        j["protocols"] = seqStrToJson(reply->protocols);
        auto pk = nfBytes(reply->publicKey);
        j["publicKey"] = hexEncode(pk.data(), pk.size());
        j["agentVersion"] = nfStr(reply->agentVersion);
        j["protoVersion"] = nfStr(reply->protoVersion);
        r.data = std::move(j);
    }
    finishPromise(static_cast<SyncPromise*>(ud), std::move(r));
}

// Event listeners run on the Nim dispatch thread through a C trampoline, so a
// C++ exception escaping here would cross the FFI boundary and terminate. Guard
// the whole body — a serialization failure (e.g. non-UTF-8 pubsub payload in
// the emitted event) must not take the process down.
void Libp2pModuleImpl::onIncomingStream(const IncomingStreamEvent* evt, void* ud) {
    auto* self = static_cast<Libp2pModuleImpl*>(ud);
    if (!self || !evt) return;
    try {
        json j;
        j["streamId"] = evt->streamId;
        j["proto"] = nfStr(evt->proto);
        self->emitEventSafe("protocolStream", j.dump());
    } catch (...) {}
}

void Libp2pModuleImpl::onPubsubMessage(const PubsubMessageEvent* evt, void* ud) {
    auto* self = static_cast<Libp2pModuleImpl*>(ud);
    if (!self || !evt) return;
    try {
        std::string topic = nfStr(evt->topic);
        auto bytes = nfBytes(evt->data);
        std::string payload(bytes.begin(), bytes.end());
        {
            std::lock_guard<std::mutex> lock(self->m_queueMutex);
            self->m_topicQueues[topic].push(payload);
            self->m_queueCond.notify_all();
        }
        json j;
        j["topic"] = topic;
        j["data"] = payload;
        self->emitEventSafe("gossipsubMessage", j.dump());
    } catch (...) {}
}
