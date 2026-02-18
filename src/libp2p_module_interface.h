#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QByteArray>
#include <QtCore/QString>

#include "interface.h"

struct PeerInfo {
    QString peerId;
    QList<QString> addrs;
};
Q_DECLARE_METATYPE(PeerInfo);
Q_DECLARE_METATYPE(QList<PeerInfo>);

struct ServiceInfo {
    QString id;
    QByteArray data;
};
Q_DECLARE_METATYPE(ServiceInfo);
Q_DECLARE_METATYPE(QList<ServiceInfo>);

struct ExtendedPeerRecord {
    QString peerId;
    uint64_t seqNo;
    QList<QString> addrs;
    QList<ServiceInfo> services;
};
Q_DECLARE_METATYPE(ExtendedPeerRecord);
Q_DECLARE_METATYPE(QList<ExtendedPeerRecord>);


class Libp2pModuleInterface : public PluginInterface
{
public:
    virtual ~Libp2pModuleInterface() {}
    Q_INVOKABLE virtual bool foo(const QString &bar) = 0;

    Q_INVOKABLE virtual QString libp2pStart() = 0;
    Q_INVOKABLE virtual QString libp2pStop() = 0;

    /* ----------- Connectivity ----------- */
    Q_INVOKABLE virtual QString connectPeer(const QString peerId, const QList<QString> multiaddrs, int64_t timeoutMs = -1) = 0;
    Q_INVOKABLE virtual QString disconnectPeer(const QString peerId) = 0;
    Q_INVOKABLE virtual QString peerInfo() = 0;
    Q_INVOKABLE virtual QString connectedPeers(int direction = 0) = 0;
    Q_INVOKABLE virtual QString dial(const QString peerId, const QString proto) = 0;

    /* ----------- Sync Connectivity ----------- */
    Q_INVOKABLE virtual bool            syncConnectPeer(const QString peerId, const QList<QString> multiaddrs, int64_t timeoutMs = -1) = 0;
    Q_INVOKABLE virtual bool            syncDisconnectPeer(const QString peerId) = 0;
    Q_INVOKABLE virtual PeerInfo        syncPeerInfo() = 0;
    Q_INVOKABLE virtual QList<QString>  syncConnectedPeers(int direction = 0) = 0;
    Q_INVOKABLE virtual QVariant        syncDial(const QString peerId, const QString proto) = 0;

    /* ----------- Streams ----------- */
    // Q_INVOKABLE virtual bool streamClose(quintptr stream) = 0;
    // Q_INVOKABLE virtual bool streamCloseWithEOF(quintptr stream) = 0;
    // Q_INVOKABLE virtual bool streamRelease(quintptr stream) = 0;
    // Q_INVOKABLE virtual bool streamReadExactly(quintptr stream, qsizetype dataLen) = 0;
    // Q_INVOKABLE virtual bool streamReadLp(quintptr stream, qint64 maxSize) = 0;
    // Q_INVOKABLE virtual bool streamWrite(quintptr stream, const QByteArray &data) = 0;
    // Q_INVOKABLE virtual bool streamWriteLp(quintptr stream, const QByteArray &data) = 0;

    /* ----------- Kademlia ----------- */
    Q_INVOKABLE virtual QString toCid(const QByteArray &key) = 0;
    Q_INVOKABLE virtual QString kadFindNode(const QString &peerId) = 0;
    Q_INVOKABLE virtual QString kadPutValue(const QByteArray &key, const QByteArray &value) = 0;
    Q_INVOKABLE virtual QString kadGetValue(const QByteArray &key, int quorum = -1) = 0;
    Q_INVOKABLE virtual QString kadAddProvider(const QString &cid) = 0;
    Q_INVOKABLE virtual QString kadStartProviding(const QString &cid) = 0;
    Q_INVOKABLE virtual QString kadStopProviding(const QString &cid) = 0;
    Q_INVOKABLE virtual QString kadGetProviders(const QString &cid) = 0;
    Q_INVOKABLE virtual QString kadGetRandomRecords() = 0;

    /* ----------- Sync Kademlia ----------- */
    Q_INVOKABLE virtual bool    syncLibp2pStart() = 0;
    Q_INVOKABLE virtual bool    syncLibp2pStop() = 0;

    Q_INVOKABLE virtual QString                   syncToCid(const QByteArray &key) = 0;
    Q_INVOKABLE virtual QList<QString>            syncKadFindNode(const QString &peerId) = 0;
    Q_INVOKABLE virtual bool                      syncKadPutValue(const QByteArray &key, const QByteArray &value) = 0;
    Q_INVOKABLE virtual QByteArray                syncKadGetValue(const QByteArray &key, int quorum = -1) = 0;
    Q_INVOKABLE virtual bool                      syncKadAddProvider(const QString &cid) = 0;
    Q_INVOKABLE virtual QList<PeerInfo>           syncKadGetProviders(const QString &cid) = 0;
    Q_INVOKABLE virtual bool                      syncKadStartProviding(const QString &cid) = 0;
    Q_INVOKABLE virtual bool                      syncKadStopProviding(const QString &cid) = 0;
    Q_INVOKABLE virtual QList<ExtendedPeerRecord> syncKadGetRandomRecords() = 0;

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
