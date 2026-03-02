#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QHash>
#include <QtCore/QReadWriteLock>
#include <QtCore/QMutex>
#include <QtCore/QQueue>
#include <QtCore/QWaitCondition>

#include <logos_api.h>
#include <logos_api_client.h>
#include <libp2p.h>

#include "libp2p_module_interface.h"

/**
 * Result used internally when waiting for async callbacks.
 */
struct WaitResult {
    bool ok;
    QVariant data;
};

/**
 * Implementation of the libp2p Logos module plugin.
 *
 * This class bridges:
 * - Logos plugin system
 * - libp2p C bindings
 * - Qt async/signal infrastructure
 */
class Libp2pModulePlugin : public QObject, public Libp2pModuleInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID Libp2pModuleInterface_iid FILE "metadata.json")
    Q_INTERFACES(Libp2pModuleInterface PluginInterface)

public:
    /**
     * Creates the plugin instance.
     *
     * bootstrapNodes are used to initially connect to the network.
     */
    explicit Libp2pModulePlugin(const QList<PeerInfo> &bootstrapNodes = {});
    ~Libp2pModulePlugin() override;

    /// Plugin name exposed to Logos.
    QString name() const override { return "libp2p_module"; }

    /// Plugin version.
    QString version() const override { return "1.0.0"; }

    /* ----------- Libp2p Core ----------- */

    /// Starts the libp2p node.
    Q_INVOKABLE QString libp2pStart() override;

    /// Stops the libp2p node.
    Q_INVOKABLE QString libp2pStop() override;

    /// Returns the public key for the libp2p node
    Q_INVOKABLE QString libp2pPublicKey() override;

    /// Generates a new libp2p private key
    Q_INVOKABLE QString libp2pNewPrivateKey() override;

    /* ----------- Sync Libp2p Core ----------- */

    Q_INVOKABLE Libp2pResult syncLibp2pStart() override;
    Q_INVOKABLE Libp2pResult syncLibp2pStop() override;
    Q_INVOKABLE Libp2pResult syncLibp2pPublicKey() override;
    Q_INVOKABLE Libp2pResult syncLibp2pNewPrivateKey() override;

    /* ----------- Connectivity ----------- */

    Q_INVOKABLE QString connectPeer(const QString &peerId, const QList<QString> multiaddrs, int64_t timeoutMs = -1) override;
    Q_INVOKABLE QString disconnectPeer(const QString &peerId) override;
    Q_INVOKABLE QString peerInfo() override;
    Q_INVOKABLE QString connectedPeers(int direction = 0) override;
    Q_INVOKABLE QString dial(const QString &peerId, const QString &proto) override;

    /* ----------- Streams ----------- */

    Q_INVOKABLE QString streamReadExactly(uint64_t streamId, size_t len) override;
    Q_INVOKABLE QString streamReadLp(uint64_t streamId, size_t maxSize) override;
    Q_INVOKABLE QString streamWrite(uint64_t streamId, const QByteArray &data) override;
    Q_INVOKABLE QString streamWriteLp(uint64_t streamId, const QByteArray &data) override;
    Q_INVOKABLE QString streamClose(uint64_t streamId) override;
    Q_INVOKABLE QString streamCloseWithEOF(uint64_t streamId) override;
    Q_INVOKABLE QString streamRelease(uint64_t streamId) override;

    /* ----------- Sync Streams ----------- */

    Q_INVOKABLE Libp2pResult syncStreamReadExactly(uint64_t streamId, size_t len) override;
    Q_INVOKABLE Libp2pResult syncStreamReadLp(uint64_t streamId, size_t maxSize) override;
    Q_INVOKABLE Libp2pResult syncStreamWrite(uint64_t streamId, const QByteArray &data) override;
    Q_INVOKABLE Libp2pResult syncStreamWriteLp(uint64_t streamId, const QByteArray &data) override;
    Q_INVOKABLE Libp2pResult syncStreamClose(uint64_t streamId) override;
    Q_INVOKABLE Libp2pResult syncStreamCloseWithEOF(uint64_t streamId) override;
    Q_INVOKABLE Libp2pResult syncStreamRelease(uint64_t streamId) override;

    /* ----------- Sync Connectivity ----------- */

    Q_INVOKABLE Libp2pResult syncConnectPeer(const QString &peerId, const QList<QString> multiaddrs, int64_t timeoutMs = -1) override;
    Q_INVOKABLE Libp2pResult syncDisconnectPeer(const QString &peerId) override;
    Q_INVOKABLE Libp2pResult syncPeerInfo() override;
    Q_INVOKABLE Libp2pResult syncConnectedPeers(int direction = Direction_In) override;
    Q_INVOKABLE Libp2pResult syncDial(const QString &peerId, const QString &proto) override;

    /* ----------- Gossipsub ----------- */

    Q_INVOKABLE QString gossipsubPublish(const QString &topic, const QByteArray &data) override;
    Q_INVOKABLE QString gossipsubSubscribe(const QString &topic) override;
    Q_INVOKABLE QString gossipsubUnsubscribe(const QString &topic) override;

    /* ----------- Sync Gossipsub ----------- */

    Q_INVOKABLE Libp2pResult syncGossipsubPublish(const QString &topic,const QByteArray &data) override;
    Q_INVOKABLE Libp2pResult syncGossipsubSubscribe(const QString &topic) override;
    Q_INVOKABLE Libp2pResult syncGossipsubUnsubscribe(const QString &topic) override;
    Q_INVOKABLE Libp2pResult syncGossipsubNextMessage(const QString &topic, int timeoutMs = 1000) override;

    /* ----------- Kademlia ----------- */

    Q_INVOKABLE QString toCid(const QByteArray &key) override;
    Q_INVOKABLE QString kadFindNode(const QString &peerId) override;
    Q_INVOKABLE QString kadPutValue(const QByteArray &key, const QByteArray &value) override;
    Q_INVOKABLE QString kadGetValue(const QByteArray &key, int quorum = -1) override;
    Q_INVOKABLE QString kadAddProvider(const QString &cid) override;
    Q_INVOKABLE QString kadStartProviding(const QString &cid) override;
    Q_INVOKABLE QString kadStopProviding(const QString &cid) override;
    Q_INVOKABLE QString kadGetProviders(const QString &cid) override;
    Q_INVOKABLE QString kadGetRandomRecords() override;

    /* ----------- Sync Kademlia ----------- */

    Q_INVOKABLE Libp2pResult syncToCid(const QByteArray &key) override;
    Q_INVOKABLE Libp2pResult syncKadFindNode(const QString &peerId) override;
    Q_INVOKABLE Libp2pResult syncKadPutValue(const QByteArray &key, const QByteArray &value) override;
    Q_INVOKABLE Libp2pResult syncKadGetValue(const QByteArray &key, int quorum = -1) override;
    Q_INVOKABLE Libp2pResult syncKadAddProvider(const QString &cid) override;
    Q_INVOKABLE Libp2pResult syncKadGetProviders(const QString &cid) override;
    Q_INVOKABLE Libp2pResult syncKadStartProviding(const QString &cid) override;
    Q_INVOKABLE Libp2pResult syncKadStopProviding(const QString &cid) override;
    Q_INVOKABLE Libp2pResult syncKadGetRandomRecords() override;

    /* ----------- Mix Network ----------- */

    /// Generates a new Curve25519 private key for mix networking.
    Q_INVOKABLE QByteArray mixGeneratePrivKey() override;

    /// Derives the public key from a given Curve25519 private key.
    Q_INVOKABLE QByteArray mixPublicKey(const QByteArray &privKey) override;

    /// Establishes a mix connection to a peer through a multiaddr and protocol.
    Q_INVOKABLE QString mixDial(
        const QString &peerId,
        const QString &multiaddr,
        const QString &proto
    ) override;

    /// Establishes a mix connection expecting a reply with SURBs.
    Q_INVOKABLE QString mixDialWithReply(
        const QString &peerId,
        const QString &multiaddr,
        const QString &proto,
        int expectReply,
        uint8_t numSurbs
    ) override;

    /// Registers how payloads should be read for a protocol at the mix destination.
    Q_INVOKABLE QString mixRegisterDestReadBehavior(
        const QString &proto,
        int behavior,
        uint32_t sizeParam
    ) override;

    /// Sets node information used by the mix layer.
    Q_INVOKABLE QString mixSetNodeInfo(
        const QString &multiaddr,
        const QByteArray &mixPrivKey
    ) override;

    /// Adds a node to the mix node pool.
    Q_INVOKABLE QString mixNodepoolAdd(
        const QString &peerId,
        const QString &multiaddr,
        const QByteArray &mixPubKey,
        const QByteArray &libp2pPubKey
    ) override;

    /* ----------- Sync Mix Network ----------- */

    Q_INVOKABLE Libp2pResult syncMixDial(
        const QString &peerId,
        const QString &multiaddr,
        const QString &proto
    ) override;
    Q_INVOKABLE Libp2pResult syncMixDialWithReply(
        const QString &peerId,
        const QString &multiaddr,
        const QString &proto,
        int expectReply,
        uint8_t numSurbs
    ) override;
    Q_INVOKABLE Libp2pResult syncMixRegisterDestReadBehavior(
        const QString &proto,
        int behavior,
        uint32_t sizeParam
    ) override;
    Q_INVOKABLE Libp2pResult syncMixSetNodeInfo(
        const QString &multiaddr,
        const QByteArray &mixPrivKey
    ) override;
    Q_INVOKABLE Libp2pResult syncMixNodepoolAdd(
        const QString &peerId,
        const QString &multiaddr,
        const QByteArray &mixPubKey,
        const QByteArray &libp2pPubKey
    ) override;

    /// Registers the event callback bridge with libp2p.
    Q_INVOKABLE bool setEventCallback() override;

    /// Initializes the Logos API instance used by the plugin.
    Q_INVOKABLE void initLogos(LogosAPI* logosAPIInstance);

signals:
    /**
     * Low-level libp2p event emitted by the underlying library.
     */
    void libp2pEvent(
        int result,
        QString reqId,
        QString caller,
        QString message,
        QVariant data
    );

    /**
     * High-level event forwarded to Logos.
     */
    void eventResponse(const QString& eventName, const QVariantList& data);

private slots:
    /**
     * Default handler for libp2p events.
     */
    void onLibp2pEventDefault(
        int result,
        const QString &reqId,
        const QString &caller,
        const QString &message,
        const QVariant &data
    );

private:
    /// Bootstrap peers used during node startup.
    QList<PeerInfo> m_bootstrapNodes;

    /// C-compatible bootstrap node structures.
    QVector<libp2p_bootstrap_node_t> m_bootstrapCNodes;

    /// Keeps UTF-8 address buffers alive.
    QVector<QVector<QByteArray>> m_addrUtf8Storage;

    /// Keeps char** arrays alive.
    QVector<QVector<char*>> m_addrPtrStorage;

    /// Storage for peer IDs.
    QVector<QByteArray> m_peerIdStorage;

    /// libp2p context.
    libp2p_ctx_t *ctx = nullptr;

    /// libp2p configuration.
    libp2p_config_t config = {};

    /// Helper for destructor to wait for libp2p_destroy and libp2p_new to be done
    std::atomic<bool> m_destroyDone{false};
    std::atomic<bool> m_newDone{false};

    /// Active streams indexed by internal ID.
    QHash<uint64_t, libp2p_stream_t*> m_streams;

    /// Lock protecting the stream registry.
    mutable QReadWriteLock m_streamsLock;

    /// Monotonic stream ID generator.
    std::atomic<uint64_t> m_nextStreamId {1};

    /* ----------- Stream Registry ----------- */

    /// Registers a new stream and returns its ID.
    uint64_t addStream(libp2p_stream_t *stream);

    /// Returns a stream by ID.
    libp2p_stream_t* getStream(uint64_t id) const;

    /// Removes a stream from the registry.
    libp2p_stream_t* removeStream(uint64_t id);

    /// Checks if a stream exists.
    bool hasStream(uint64_t id) const;

    /// Gossipsub messages map
    QMutex m_queueMutex;
    QWaitCondition m_queueCond;
    // topic queues: map topic -> shared pointer queue
    QMap<QString, QSharedPointer<QQueue<QByteArray>>> m_topicQueues;

    /* ----------- libp2p Callbacks ----------- */

    static void topicHandler(
        const char *topic,
        uint8_t *data,
        size_t len,
        void *userData
    );

    static void libp2pCallback(
        int callerRet,
        const char *msg,
        size_t len,
        void *userData
    );

    static void randomRecordsCallback(
        int callerRet,
        const Libp2pExtendedPeerRecord *records,
        size_t recordsLen,
        const char *msg,
        size_t len,
        void *userData
    );

    static void peersCallback(
        int callerRet,
        const char **peerIds,
        size_t peerIdsLen,
        const char *msg,
        size_t len,
        void *userData
    );

    static void libp2pBufferCallback(
        int callerRet,
        const uint8_t *data,
        size_t dataLen,
        const char *msg,
        size_t len,
        void *userData
    );

    static void getProvidersCallback(
        int callerRet,
        const Libp2pPeerInfo *providers,
        size_t providersLen,
        const char *msg,
        size_t len,
        void *userData
    );

    static void peerInfoCallback(
        int callerRet,
        const Libp2pPeerInfo *info,
        const char *msg,
        size_t len,
        void *userData
    );

    static void connectionCallback(
        int callerRet,
        libp2p_stream_t *stream,
        const char *msg,
        size_t len,
        void *userData
    );
};

/**
 * Context passed to async callbacks to map responses back
 * to the originating request.
 */
struct CallbackContext {
    QString caller;
    QString reqId;
    Libp2pModulePlugin *instance;
};

