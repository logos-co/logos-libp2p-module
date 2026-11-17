#include "plugin.h"

#include "logos_codec.h"

using json = nlohmann::json;

namespace {
// {peerId, seqNo, addrs, services:[{id, data}]}. Service `data` is arbitrary
// bytes, so it is base64-encoded to keep the JSON valid UTF-8.
//
// This is deliberately NOT keyed by id to mirror the `{tstr: bstr}` map
// createXpr takes: the same shape is returned by discoLookup and
// discoRandomLookup, whose records come from remote peers and may legitimately
// repeat a service id — an object would drop all but the last. A caller
// round-tripping a decoded record back into createXpr rebuilds the map itself,
// base64-decoding each `data` as it goes.
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

void Libp2pModuleImpl::cbPublish(int ec, const PublishResponse* reply, const char* em, void* ud) {
    auto r = replyBase(ec, em);
    if (r.ok && reply) r.data = reply->peerCount;
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

// Listeners run on the Nim event thread: an exception escaping one crosses the FFI boundary and terminates.
void Libp2pModuleImpl::onIncomingStream(const IncomingStreamEvent* evt, void* ud) {
    auto* gate = static_cast<ListenerGate*>(ud);
    if (!gate || !evt) return;
    std::shared_lock<std::shared_mutex> guard(gate->lock);
    auto* self = gate->owner;
    if (!self) return;
    try {
        OutboxEvent e;
        e.kind = OutboxEvent::Kind::Stream;
        e.name = nfStr(evt->proto);
        e.peerId = nfStr(evt->peerId);
        e.streamId = evt->streamId;
        if (!self->m_inboundStreams.push(e.name, InboundStream{e.streamId, e.peerId})) {
            self->releaseStreamNoWait(e.streamId);
            return;
        }
        self->m_outbox.push(std::move(e));
    } catch (...) {}
}

void Libp2pModuleImpl::onPubsubMessage(const PubsubMessageEvent* evt, void* ud) {
    auto* gate = static_cast<ListenerGate*>(ud);
    if (!gate || !evt) return;
    std::shared_lock<std::shared_mutex> guard(gate->lock);
    auto* self = gate->owner;
    if (!self) return;
    try {
        OutboxEvent e;
        e.name = nfStr(evt->topic);
        e.data = nfBytes(evt->data);
        self->m_outbox.push(std::move(e));
    } catch (...) {}
}

void Libp2pModuleImpl::onNotResponding(int, const char*, size_t, void* ud) {
    auto* gate = static_cast<ListenerGate*>(ud);
    if (!gate) return;
    std::shared_lock<std::shared_mutex> guard(gate->lock);
    if (!gate->owner) return;
    gate->owner->m_stuck = true;
    fprintf(stderr, "libp2p_module: libp2p not responding\n");
}

void Libp2pModuleImpl::runEventWorker() {
    OutboxEvent e;
    while (m_outbox.pop(e)) {
        try {
            deliverEvent(e);
        } catch (...) {}
    }
}

void Libp2pModuleImpl::deliverEvent(const OutboxEvent& e) {
    json j;
    if (e.kind == OutboxEvent::Kind::Stream) {
        j["streamId"] = e.streamId;
        j["proto"] = e.name;
        j["peerId"] = e.peerId;
        emitEventSafe("protocolStream", j.dump());
        return;
    }

    m_topicQueues.push(e.name, std::string(e.data.begin(), e.data.end()));
    j["topic"] = e.name;
    j["data"] = logos::bytesToJson(e.data);
    emitEventSafe("gossipsubMessage", j.dump());
}
