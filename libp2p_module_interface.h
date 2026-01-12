#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "interface.h"

class Libp2pModuleInterface : public PluginInterface
{
    Q_OBJECT

public:
    virtual ~Libp2pModuleInterface() {}

    Q_INVOKABLE virtual bool newContext(const QVariantMap &config) = 0;
    Q_INVOKABLE virtual bool destroyContext() = 0;

    Q_INVOKABLE virtual bool start() = 0;
    Q_INVOKABLE virtual bool stop() = 0;

    Q_INVOKABLE virtual bool setEventCallback() = 0;

    Q_INVOKABLE virtual bool connectPeer(
        const QString &peerId,
        const QStringList &multiaddrs,
        qint64 timeoutMs) = 0;

    Q_INVOKABLE virtual bool disconnectPeer(
        const QString &peerId) = 0;

    Q_INVOKABLE virtual bool peerInfo() = 0;

    Q_INVOKABLE virtual bool connectedPeers(
        quint32 direction) = 0;

    Q_INVOKABLE virtual bool dial(
        const QString &peerId,
        const QString &protocol) = 0;

    Q_INVOKABLE virtual bool streamClose(
        quintptr streamHandle) = 0;

    Q_INVOKABLE virtual bool streamCloseWithEOF(
        quintptr streamHandle) = 0;

    Q_INVOKABLE virtual bool streamRelease(
        quintptr streamHandle) = 0;

    Q_INVOKABLE virtual bool streamReadExactly(
        quintptr streamHandle,
        quint64 size) = 0;

    Q_INVOKABLE virtual bool streamReadLp(
        quintptr streamHandle,
        qint64 maxSize) = 0;

    Q_INVOKABLE virtual bool streamWrite(
        quintptr streamHandle,
        const QByteArray &data) = 0;

    Q_INVOKABLE virtual bool streamWriteLp(
        quintptr streamHandle,
        const QByteArray &data) = 0;

    Q_INVOKABLE virtual bool gossipsubPublish(
        const QString &topic,
        const QByteArray &data) = 0;

    Q_INVOKABLE virtual bool gossipsubSubscribe(
        const QString &topic) = 0;

    Q_INVOKABLE virtual bool gossipsubUnsubscribe(
        const QString &topic) = 0;

    Q_INVOKABLE virtual bool findNode(
        const QString &peerId) = 0;

    Q_INVOKABLE virtual bool putValue(
        const QByteArray &key,
        const QByteArray &value) = 0;

    Q_INVOKABLE virtual bool getValue(
        const QByteArray &key,
        int quorumOverride) = 0;

    Q_INVOKABLE virtual bool addProvider(
        const QString &cid) = 0;

    Q_INVOKABLE virtual bool startProviding(
        const QString &cid) = 0;

    Q_INVOKABLE virtual bool stopProviding(
        const QString &cid) = 0;

    Q_INVOKABLE virtual bool getProviders(
        const QString &cid) = 0;

signals:
    // Generic async callback bridge
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
};

#define Libp2pModuleInterface_iid "org.logos.Libp2pModuleInterface"
Q_DECLARE_INTERFACE(Libp2pModuleInterface, Libp2pModuleInterface_iid)
