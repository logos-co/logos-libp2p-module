#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "interface.h"

class Connection;

class Libp2pModuleInterface : public PluginInterface
{
public:
    virtual ~Libp2pModuleInterface() {}
    Q_INVOKABLE virtual bool foo(const QString &bar) = 0;

    Q_INVOKABLE virtual bool libp2pStart() = 0;
    Q_INVOKABLE virtual bool libp2pStop() = 0;

    /* ----------- Connectivity ----------- */
    Q_INVOKABLE virtual bool connectPeer(const QString peerId, const QStringList multiaddrs, int64_t timeoutMs = -1) = 0;
    Q_INVOKABLE virtual bool disconnectPeer(const QString peerId) = 0;
    Q_INVOKABLE virtual bool peerInfo() = 0;
    Q_INVOKABLE virtual bool connectedPeers(int direction = 0) = 0;
    Q_INVOKABLE virtual bool dial(const QString peerId, const QString proto) = 0;

    /* ----------- Streams ----------- */
    Q_INVOKABLE virtual bool streamClose(const Connection &conn) = 0;
    Q_INVOKABLE virtual bool streamCloseEOF(const Connection &conn) = 0;
    Q_INVOKABLE virtual bool streamRelease(const Connection &conn) = 0;
    Q_INVOKABLE virtual bool streamReadExactly(const Connection &conn, size_t len) = 0;
    Q_INVOKABLE virtual bool streamReadLp(const Connection &conn, int64_t maxSize) = 0;
    Q_INVOKABLE virtual bool streamWrite(const Connection &conn, const QByteArray &data) = 0;
    Q_INVOKABLE virtual bool streamWriteLp(const Connection &conn, const QByteArray &data) = 0;

    /* ----------- Kademlia ----------- */
    Q_INVOKABLE virtual bool toCid(const QByteArray &key) = 0;
    Q_INVOKABLE virtual bool kadFindNode(const QString &peerId) = 0;
    Q_INVOKABLE virtual bool kadPutValue(const QByteArray &key, const QByteArray &value) = 0;
    Q_INVOKABLE virtual bool kadGetValue(const QByteArray &key, int quorum = -1) = 0;
    Q_INVOKABLE virtual bool kadAddProvider(const QString &cid) = 0;
    Q_INVOKABLE virtual bool kadStartProviding(const QString &cid) = 0;
    Q_INVOKABLE virtual bool kadStopProviding(const QString &cid) = 0;
    Q_INVOKABLE virtual bool kadGetProviders(const QString &cid) = 0;
    Q_INVOKABLE virtual bool kadGetRandomRecords() = 0;

    Q_INVOKABLE virtual bool setEventCallback() = 0;

signals:
    // Generic async callback bridge
    void response(
        const QString &operation,
        int result,
        const QString &message,
        const QVariant &data);
};

#define Libp2pModuleInterface_iid "org.logos.Libp2pModuleInterface"
Q_DECLARE_INTERFACE(Libp2pModuleInterface, Libp2pModuleInterface_iid)
