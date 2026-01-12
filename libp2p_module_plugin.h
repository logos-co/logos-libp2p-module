#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "libp2p_module_interface.h"
#include "logos_api.h"
#include "logos_api_client.h"
#include "libp2p.h"

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
    Q_INVOKABLE void initLogos(LogosAPI* logosAPIInstance);

    Q_INVOKABLE bool newContext(const QVariantMap &config) override;
    Q_INVOKABLE bool destroyContext() override;

    Q_INVOKABLE bool start() override;
    Q_INVOKABLE bool stop() override;

    Q_INVOKABLE bool setEventCallback() override;

    Q_INVOKABLE bool connectPeer(
        const QString &peerId,
        const QStringList &multiaddrs,
        qint64 timeoutMs) override;

    Q_INVOKABLE bool disconnectPeer(const QString &peerId) override;

    Q_INVOKABLE bool peerInfo() override;
    Q_INVOKABLE bool connectedPeers(quint32 direction) override;

    Q_INVOKABLE bool dial(
        const QString &peerId,
        const QString &protocol) override;

    Q_INVOKABLE bool streamClose(quintptr streamHandle) override;
    Q_INVOKABLE bool streamCloseWithEOF(quintptr streamHandle) override;
    Q_INVOKABLE bool streamRelease(quintptr streamHandle) override;

    Q_INVOKABLE bool streamReadExactly(
        quintptr streamHandle,
        quint64 size) override;

    Q_INVOKABLE bool streamReadLp(
        quintptr streamHandle,
        qint64 maxSize) override;

    Q_INVOKABLE bool streamWrite(
        quintptr streamHandle,
        const QByteArray &data) override;

    Q_INVOKABLE bool streamWriteLp(
        quintptr streamHandle,
        const QByteArray &data) override;

    Q_INVOKABLE bool gossipsubPublish(
        const QString &topic,
        const QByteArray &data) override;

    Q_INVOKABLE bool gossipsubSubscribe(
        const QString &topic) override;

    Q_INVOKABLE bool gossipsubUnsubscribe(
        const QString &topic) override;

    Q_INVOKABLE bool findNode(const QString &peerId) override;

    Q_INVOKABLE bool putValue(
        const QByteArray &key,
        const QByteArray &value) override;

    Q_INVOKABLE bool getValue(
        const QByteArray &key,
        int quorumOverride) override;

    Q_INVOKABLE bool addProvider(const QString &cid) override;
    Q_INVOKABLE bool startProviding(const QString &cid) override;
    Q_INVOKABLE bool stopProviding(const QString &cid) override;
    Q_INVOKABLE bool getProviders(const QString &cid) override;

signals:
    // Generic async response
    void response(
        const QString &operation,
        int result,
        const QString &message,
        const QVariant &data);

    // Pubsub
    void pubsubMessage(
        const QString &topic,
        const QByteArray &data);

    // Streams
    void streamOpened(quintptr streamHandle);
    void streamData(
        quintptr streamHandle,
        const QByteArray &data);
    void streamClosed(quintptr streamHandle);

    // Peer / DHT
    void peerInfoReceived(const QVariantMap &info);
    void peersReceived(const QStringList &peerIds);
    void providersReceived(const QVariantList &providers);

private:
    libp2p_ctx_t *ctx = nullptr;
    void eventResponse(const QString& eventName, const QVariantList& data);
};