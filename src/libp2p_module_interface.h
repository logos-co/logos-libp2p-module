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

    /// Test / example method
    Q_INVOKABLE virtual bool foo(const QString &bar) = 0;

    /* ----------- Start/stop ----------- */

    /// Starts the libp2p node asynchronously.
    Q_INVOKABLE virtual QString libp2pStart() = 0;

    /// Stops the libp2p node asynchronously.
    Q_INVOKABLE virtual QString libp2pStop() = 0;

    /* ----------- Sync Start/stop ----------- */

    /// Starts the libp2p node and blocks until completion.
    Q_INVOKABLE virtual Libp2pResult syncLibp2pStart() = 0;

    /// Stops the libp2p node and blocks until completion.
    Q_INVOKABLE virtual Libp2pResult syncLibp2pStop() = 0;

    /* ----------- Connectivity ----------- */

    /// Connect to a peer using known multiaddresses.
    Q_INVOKABLE virtual QString connectPeer(const QString &peerId, const QList<QString> multiaddrs, int64_t timeoutMs = -1) = 0;

    /// Disconnect from a peer.
    Q_INVOKABLE virtual QString disconnectPeer(const QString &peerId) = 0;

    /// Returns information about the local peer.
    Q_INVOKABLE virtual QString peerInfo() = 0;

    /// Returns currently connected peers.
    Q_INVOKABLE virtual QString connectedPeers(int direction = 0) = 0;

    /// Opens a stream using a protocol.
    Q_INVOKABLE virtual QString dial(const QString &peerId, const QString &proto) = 0;

    /* ----------- Sync Connectivity ----------- */

    /// Synchronous version of connectPeer.
    Q_INVOKABLE virtual Libp2pResult syncConnectPeer(const QString &peerId, const QList<QString> multiaddrs, int64_t timeoutMs = -1) = 0;

    /// Synchronous version of disconnectPeer.
    Q_INVOKABLE virtual Libp2pResult syncDisconnectPeer(const QString &peerId) = 0;

    /// Synchronous version of peerInfo.
    Q_INVOKABLE virtual Libp2pResult syncPeerInfo() = 0;

    /// Synchronous version of connectedPeers.
    Q_INVOKABLE virtual Libp2pResult syncConnectedPeers(int direction = 0) = 0;

    /// Synchronous version of dial.
    Q_INVOKABLE virtual Libp2pResult syncDial(const QString &peerId, const QString &proto) = 0;

    /* ----------- Streams ----------- */

    /// Read exactly len bytes from a stream.
    Q_INVOKABLE virtual QString streamReadExactly(uint64_t streamId, size_t len) = 0;

    /// Read length-prefixed data from a stream.
    Q_INVOKABLE virtual QString streamReadLp(uint64_t streamId, size_t maxSize) = 0;

    /// Write raw data to a stream.
    Q_INVOKABLE virtual QString streamWrite(uint64_t streamId, const QByteArray &data) = 0;

    /// Write length-prefixed data to a stream.
    Q_INVOKABLE virtual QString streamWriteLp(uint64_t streamId, const QByteArray &data) = 0;

    /// Close the stream.
    Q_INVOKABLE virtual QString streamClose(uint64_t streamId) = 0;

    /// Close the stream with EOF.
    Q_INVOKABLE virtual QString streamCloseEOF(uint64_t streamId) = 0;

    /// Release the stream from the registry.
    Q_INVOKABLE virtual QString streamRelease(uint64_t streamId) = 0;

    /* ----------- Sync Streams ----------- */

    Q_INVOKABLE virtual Libp2pResult syncStreamReadExactly(uint64_t streamId, size_t len) = 0;
    Q_INVOKABLE virtual Libp2pResult syncStreamReadLp(uint64_t streamId, size_t maxSize) = 0;
    Q_INVOKABLE virtual Libp2pResult syncStreamWrite(uint64_t streamId, const QByteArray &data) = 0;
    Q_INVOKABLE virtual Libp2pResult syncStreamWriteLp(uint64_t streamId, const QByteArray &data) = 0;
    Q_INVOKABLE virtual Libp2pResult syncStreamClose(uint64_t streamId) = 0;
    Q_INVOKABLE virtual Libp2pResult syncStreamCloseEOF(uint64_t streamId) = 0;
    Q_INVOKABLE virtual Libp2pResult syncStreamRelease(uint64_t streamId) = 0;

    /* ----------- Kademlia ----------- */

    /// Converts a key to a CID.
    Q_INVOKABLE virtual QString toCid(const QByteArray &key) = 0;

    /// Finds a node in the DHT.
    Q_INVOKABLE virtual QString kadFindNode(const QString &peerId) = 0;

    /// Stores a value in the DHT.
    Q_INVOKABLE virtual QString kadPutValue(const QByteArray &key, const QByteArray &value) = 0;

    /// Retrieves a value from the DHT.
    Q_INVOKABLE virtual QString kadGetValue(const QByteArray &key, int quorum = -1) = 0;

    /// Announces a provider for a CID.
    Q_INVOKABLE virtual QString kadAddProvider(const QString &cid) = 0;

    /// Starts providing content for a CID.
    Q_INVOKABLE virtual QString kadStartProviding(const QString &cid) = 0;

    /// Stops providing content for a CID.
    Q_INVOKABLE virtual QString kadStopProviding(const QString &cid) = 0;

    /// Returns providers for a CID.
    Q_INVOKABLE virtual QString kadGetProviders(const QString &cid) = 0;

    /// Returns random DHT records.
    Q_INVOKABLE virtual QString kadGetRandomRecords() = 0;

    /* ----------- Sync Kademlia ----------- */

    Q_INVOKABLE virtual Libp2pResult syncToCid(const QByteArray &key) = 0;
    Q_INVOKABLE virtual Libp2pResult syncKadFindNode(const QString &peerId) = 0;
    Q_INVOKABLE virtual Libp2pResult syncKadPutValue(const QByteArray &key, const QByteArray &value) = 0;
    Q_INVOKABLE virtual Libp2pResult syncKadGetValue(const QByteArray &key, int quorum = -1) = 0;
    Q_INVOKABLE virtual Libp2pResult syncKadAddProvider(const QString &cid) = 0;
    Q_INVOKABLE virtual Libp2pResult syncKadGetProviders(const QString &cid) = 0;
    Q_INVOKABLE virtual Libp2pResult syncKadStartProviding(const QString &cid) = 0;
    Q_INVOKABLE virtual Libp2pResult syncKadStopProviding(const QString &cid) = 0;
    Q_INVOKABLE virtual Libp2pResult syncKadGetRandomRecords() = 0;

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

