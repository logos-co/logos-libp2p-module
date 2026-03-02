#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QByteArray>
#include <QtCore/QString>

#include "interface.h"

/**
 * Basic peer information used by libp2p operations.
 */
struct PeerInfo {
    /// Peer ID (multihash string)
    QString peerId;

    /// Multiaddresses associated with the peer
    QList<QString> addrs;
};
Q_DECLARE_METATYPE(PeerInfo);
Q_DECLARE_METATYPE(QList<PeerInfo>);

/**
 * Service record advertised by a peer (used in extended peer records).
 */
struct ServiceInfo {
    /// Service identifier
    QString id;

    /// Arbitrary service data
    QByteArray data;
};
Q_DECLARE_METATYPE(ServiceInfo);
Q_DECLARE_METATYPE(QList<ServiceInfo>);

/**
 * Extended peer record returned by certain DHT queries.
 */
struct ExtendedPeerRecord {
    /// Peer ID
    QString peerId;

    /// Record sequence number
    uint64_t seqNo;

    /// Known peer addresses
    QList<QString> addrs;

    /// Advertised services
    QList<ServiceInfo> services;
};
Q_DECLARE_METATYPE(ExtendedPeerRecord);
Q_DECLARE_METATYPE(QList<ExtendedPeerRecord>);

/**
 * Generic result returned by synchronous API calls.
 */
struct Libp2pResult {
    /// True if the operation succeeded
    bool ok = false;

    /// Error message if ok == false
    QString error;

    /// Optional returned data
    QVariant data;
};

Q_DECLARE_METATYPE(Libp2pResult);

/**
 * Qt interface exposed by the libp2p Logos module.
 *
 * Provides asynchronous (QString request id) and synchronous
 * (Libp2pResult) variants of networking, stream, and DHT operations.
 */
class Libp2pModuleInterface : public PluginInterface
{
public:
    virtual ~Libp2pModuleInterface() {}

    /* ---------------- Async usage ----------------

    All asynchronous methods return a UUID string representing the request.
    To wait for completion:

    1. Call the async method and store the returned UUID:
           QString reqId = plugin->libp2pStart();

    2. Connect to the `response` signal:
           connect(plugin, &Libp2pModuleInterface::response,
                   this, [reqId](const QString &op, int result, const QString &msg, const QVariant &data){
                       if (op == reqId) {
                           // operation completed, handle result
                       }
                   });

    3. Optionally, implement a timeout or event loop if blocking behavior is desired.
    */

    /* ----------- Libp2p Core ----------- */

    /// Starts the libp2p node.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString libp2pStart() = 0;

    /// Stops the libp2p node.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString libp2pStop() = 0;

    /// Returns the public key for the libp2p node
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString libp2pPublicKey() = 0;

    /// Generates a new libp2p private key
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString libp2pNewPrivateKey() = 0;

    /* ----------- Sync core functions ----------- */
    /// data: none
    Q_INVOKABLE virtual Libp2pResult syncLibp2pStart() = 0;
    /// data: none
    Q_INVOKABLE virtual Libp2pResult syncLibp2pStop() = 0;
    /// data: QByteArray (libp2p public key)
    Q_INVOKABLE virtual Libp2pResult syncLibp2pPublicKey() = 0;
    /// data: QByteArray (libp2p private key)
    Q_INVOKABLE virtual Libp2pResult syncLibp2pNewPrivateKey() = 0;

    /* ----------- Connectivity ----------- */

    /// Connect to a peer using known multiaddresses.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString connectPeer(
        const QString &peerId,
        const QList<QString> multiaddrs,
        int64_t timeoutMs = -1
    ) = 0;

    /// Disconnect from a peer.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString disconnectPeer(const QString &peerId) = 0;

    /// Returns information about the local peer.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString peerInfo() = 0;

    /// Returns currently connected peers.
    /// Possible Direction values: Direction_In, Direction_Out.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString connectedPeers(int direction = Direction_In) = 0;

    /// Opens a stream using a protocol.
    /// Returns a UUID string identifying this request.
    /// Upon finishing, dial calls connectionCallback which returns a streamId.
    Q_INVOKABLE virtual QString dial(const QString &peerId, const QString &proto) = 0;

    /* ----------- Sync Connectivity ----------- */

    /// data: none
    Q_INVOKABLE virtual Libp2pResult syncConnectPeer(const QString &peerId, const QList<QString> multiaddrs, int64_t timeoutMs = -1) = 0;
    /// data: none
    Q_INVOKABLE virtual Libp2pResult syncDisconnectPeer(const QString &peerId) = 0;
    /// data: PeerInfo
    Q_INVOKABLE virtual Libp2pResult syncPeerInfo() = 0;
    /// data: QList<PeerInfo>
    Q_INVOKABLE virtual Libp2pResult syncConnectedPeers(int direction = 0) = 0;
    /// data: uint64_t (streamId)
    Q_INVOKABLE virtual Libp2pResult syncDial(const QString &peerId, const QString &proto) = 0;

    /* ----------- Streams ----------- */

    /// Read exactly len bytes from a stream identified by a streamId.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString streamReadExactly(uint64_t streamId, size_t len) = 0;

    /// Read length-prefixed data from a stream identified by a streamId.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString streamReadLp(uint64_t streamId, size_t maxSize) = 0;

    /// Write length-prefixed data to a stream identified by a streamId.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString streamWrite(uint64_t streamId, const QByteArray &data) = 0;

    /// Write length-prefixed data to a stream identified by a streamId.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString streamWriteLp(uint64_t streamId, const QByteArray &data) = 0;

    /// Close the stream identified by a streamId.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString streamClose(uint64_t streamId) = 0;

    /// Close the stream identified by a streamId with EOF.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString streamCloseWithEOF(uint64_t streamId) = 0;

    /// Release the stream identified by a streamId from the registry.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString streamRelease(uint64_t streamId) = 0;

    /* ----------- Sync Streams ----------- */

    /// data: QByteArray
    Q_INVOKABLE virtual Libp2pResult syncStreamReadExactly(uint64_t streamId, size_t len) = 0;
    /// data: QByteArray
    Q_INVOKABLE virtual Libp2pResult syncStreamReadLp(uint64_t streamId, size_t maxSize) = 0;
    /// data: none
    Q_INVOKABLE virtual Libp2pResult syncStreamWrite(uint64_t streamId, const QByteArray &data) = 0;
    /// data: none
    Q_INVOKABLE virtual Libp2pResult syncStreamWriteLp(uint64_t streamId, const QByteArray &data) = 0;
    /// data: none
    Q_INVOKABLE virtual Libp2pResult syncStreamClose(uint64_t streamId) = 0;
    /// data: none
    Q_INVOKABLE virtual Libp2pResult syncStreamCloseWithEOF(uint64_t streamId) = 0;
    /// data: none
    Q_INVOKABLE virtual Libp2pResult syncStreamRelease(uint64_t streamId) = 0;

    /* ----------- Gossipsub ----------- */

    /// Publish data to a topic
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString gossipsubPublish(
        const QString &topic,
        const QByteArray &data
    ) = 0;

    /// Subscribe to a topic
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString gossipsubSubscribe(
        const QString &topic
    ) = 0;

    /// Unsubscribe from a topic
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString gossipsubUnsubscribe(
        const QString &topic
    ) = 0;

    /* ----------- Sync Gossipsub ----------- */

    /// data: none
    Q_INVOKABLE virtual Libp2pResult syncGossipsubPublish(
        const QString &topic,
        const QByteArray &data
    ) = 0;
    /// data: none
    Q_INVOKABLE virtual Libp2pResult syncGossipsubSubscribe(
        const QString &topic
    ) = 0;
    /// data: none
    Q_INVOKABLE virtual Libp2pResult syncGossipsubUnsubscribe(
        const QString &topic
    ) = 0;
    /// data: QByteArray (message)
    Q_INVOKABLE virtual Libp2pResult syncGossipsubNextMessage(
        const QString &topic,
        int timeoutMs
    ) = 0;

    /* ----------- Kademlia ----------- */

    /// Converts a key to a CID.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString toCid(const QByteArray &key) = 0;

    /// Finds a node in the DHT.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString kadFindNode(const QString &peerId) = 0;

    /// Stores a value in the DHT.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString kadPutValue(const QByteArray &key, const QByteArray &value) = 0;

    /// Retrieves a value from the DHT.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString kadGetValue(const QByteArray &key, int quorum = -1) = 0;

    /// Announces a provider for a CID.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString kadAddProvider(const QString &cid) = 0;

    /// Starts providing content for a CID.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString kadStartProviding(const QString &cid) = 0;

    /// Stops providing content for a CID.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString kadStopProviding(const QString &cid) = 0;

    /// Returns providers for a CID.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString kadGetProviders(const QString &cid) = 0;

    /// Returns random DHT records.
    /// Returns a UUID string identifying this request.
    Q_INVOKABLE virtual QString kadGetRandomRecords() = 0;


    /* ----------- Sync Kademlia ----------- */

    /// data: QString (CID)
    Q_INVOKABLE virtual Libp2pResult syncToCid(const QByteArray &key) = 0;
    /// data: QList<QString> (peer IDs)
    Q_INVOKABLE virtual Libp2pResult syncKadFindNode(const QString &peerId) = 0;
    /// data: none
    Q_INVOKABLE virtual Libp2pResult syncKadPutValue(const QByteArray &key, const QByteArray &value) = 0;
    /// data: QByteArray
    Q_INVOKABLE virtual Libp2pResult syncKadGetValue(const QByteArray &key, int quorum = -1) = 0;
    /// data: none
    Q_INVOKABLE virtual Libp2pResult syncKadAddProvider(const QString &cid) = 0;
    /// data: QList<PeerInfo>
    Q_INVOKABLE virtual Libp2pResult syncKadGetProviders(const QString &cid) = 0;
    /// data: none
    Q_INVOKABLE virtual Libp2pResult syncKadStartProviding(const QString &cid) = 0;
    /// data: none
    Q_INVOKABLE virtual Libp2pResult syncKadStopProviding(const QString &cid) = 0;
    /// data: QList<ExtendedPeerRecord>
    Q_INVOKABLE virtual Libp2pResult syncKadGetRandomRecords() = 0;


    /* ----------- Mix Network ----------- */

    /// Generates a new Curve25519 private key for mix networking.
    Q_INVOKABLE virtual QByteArray mixGeneratePrivKey() = 0;

    /// Derives the public key from a given Curve25519 private key.
    Q_INVOKABLE virtual QByteArray mixPublicKey(const QByteArray &privKey) = 0;

    /// Establishes a mix connection to a peer through a multiaddr and protocol.
    Q_INVOKABLE virtual QString mixDial(
        const QString &peerId,
        const QString &multiaddr,
        const QString &proto
    ) = 0;

    /// Establishes a mix connection expecting a reply with SURBs.
    Q_INVOKABLE virtual QString mixDialWithReply(
        const QString &peerId,
        const QString &multiaddr,
        const QString &proto,
        int expectReply,
        uint8_t numSurbs
    ) = 0;

    /// Registers how payloads should be read for a protocol at the mix destination.
    Q_INVOKABLE virtual QString mixRegisterDestReadBehavior(
        const QString &proto,
        int behavior,
        uint32_t sizeParam
    ) = 0;

    /// Sets node information used by the mix layer.
    Q_INVOKABLE virtual QString mixSetNodeInfo(
        const QString &multiaddr,
        const QByteArray &mixPrivKey
    ) = 0;

    /// Adds a node to the mix node pool.
    Q_INVOKABLE virtual QString mixNodepoolAdd(
        const QString &peerId,
        const QString &multiaddr,
        const QByteArray &mixPubKey,
        const QByteArray &libp2pPubKey
    ) = 0;

    /* ----------- Sync Mix Network ----------- */

    /// data: uint64_t (streamId)
    Q_INVOKABLE virtual Libp2pResult syncMixDial(
        const QString &peerId,
        const QString &multiaddr,
        const QString &proto
    ) = 0;

    /// data: uint64_t (streamId)
    Q_INVOKABLE virtual Libp2pResult syncMixDialWithReply(
        const QString &peerId,
        const QString &multiaddr,
        const QString &proto,
        int expectReply,
        uint8_t numSurbs
    ) = 0;

    /// data: none
    Q_INVOKABLE virtual Libp2pResult syncMixRegisterDestReadBehavior(
        const QString &proto,
        int behavior,
        uint32_t sizeParam
    ) = 0;

    /// data: none
    Q_INVOKABLE virtual Libp2pResult syncMixSetNodeInfo(
        const QString &multiaddr,
        const QByteArray &mixPrivKey
    ) = 0;

    /// data: none
    Q_INVOKABLE virtual Libp2pResult syncMixNodepoolAdd(
        const QString &peerId,
        const QString &multiaddr,
        const QByteArray &mixPubKey,
        const QByteArray &libp2pPubKey
    ) = 0;

    /// Registers the event callback used for async responses.
    Q_INVOKABLE virtual bool setEventCallback() = 0;

signals:
    /**
     * Generic async response emitted when an operation completes.
     */
    void response(
        const QString &operation,
        int result,
        const QString &message,
        const QVariant &data);
};

#define Libp2pModuleInterface_iid "org.logos.Libp2pModuleInterface"
Q_DECLARE_INTERFACE(Libp2pModuleInterface, Libp2pModuleInterface_iid)

