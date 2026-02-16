#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include <logos_api.h>
#include <logos_api_client.h>
#include <libp2p.h>

#include "libp2p_module_interface.h"

struct WaitResult {
    bool ok;
    QVariant data;
};

class Libp2pModulePlugin : public QObject, public Libp2pModuleInterface
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID Libp2pModuleInterface_iid FILE "metadata.json")
    Q_INTERFACES(Libp2pModuleInterface PluginInterface)

public:
    explicit Libp2pModulePlugin();
    ~Libp2pModulePlugin() override;

    QString name() const override { return "libp2p_module"; }
    QString version() const override { return "1.0.0"; }

    Q_INVOKABLE bool foo(const QString &bar) override;

    Q_INVOKABLE QString libp2pStart() override;
    Q_INVOKABLE QString libp2pStop() override;

    /* ----------- Sync Libp2p ----------- */
    Q_INVOKABLE bool syncLibp2pStart();
    Q_INVOKABLE bool syncLibp2pStop();

    /* ----------- Connectivity ----------- */
    Q_INVOKABLE bool connectPeer(const QString peerId, const QList<QString> multiaddrs, int64_t timeoutMs = -1) override;
    Q_INVOKABLE bool disconnectPeer(const QString peerId) override;
    Q_INVOKABLE bool peerInfo() override;
    Q_INVOKABLE bool connectedPeers(int direction = 0) override;
    Q_INVOKABLE bool dial(const QString peerId, const QString proto) override;

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
    // Q_INVOKABLE bool syncKadFindNode(const QString &peerId);
    Q_INVOKABLE bool syncKadPutValue(const QByteArray &key, const QByteArray &value);
    Q_INVOKABLE QByteArray syncKadGetValue(const QByteArray &key, int quorum = -1);
    // Q_INVOKABLE bool syncKadAddProvider(const QString &cid);
    // Q_INVOKABLE bool syncKadGetProviders(const QString &cid);
    // Q_INVOKABLE bool syncKadStartProviding(const QString &cid);
    // Q_INVOKABLE bool syncKadStopProviding(const QString &cid);
    // Q_INVOKABLE bool syncKadGetRandomRecords();

    Q_INVOKABLE bool setEventCallback() override;
    Q_INVOKABLE void initLogos(LogosAPI* logosAPIInstance);

signals:
    void libp2pEvent(
        int result,
        QString reqId,
        QString caller,
        QString message,
        QVariant data
    );
    void eventResponse(const QString& eventName, const QVariantList& data);

private slots:
    void onLibp2pEventDefault(
        int result,
        const QString &reqId,
        const QString &caller,
        const QString &message,
        const QVariant &data
    );

private:
    libp2p_ctx_t *ctx = nullptr;
    libp2p_config_t config = {};
    QString lastCaller; // for logging

    WaitResult waitForUuid(const QString &uuid, const QString &caller);

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
        libp2p_stream_t *conn,
        const char *msg,
        size_t len,
        void *userData
    );
};

struct CallbackContext {
    QString caller;
    QString reqId;
    Libp2pModulePlugin *instance;
};

struct PeerInfo {
    QString peerId;
    QList<QString> addrs;
};

struct ServiceInfo {
    QString id;
    QByteArray data;
};

struct ExtendedPeerRecord {
    QString peerId;
    uint64_t seqNo;
    QList<QString> addrs;
    QList<ServiceInfo> services;
};

