#pragma once

#include <QtCore/QObject>
#include <QtCore/QVariant>
#include <QtCore/QByteArray>
#include <QtCore/QString>
#include <QtCore/QStringList>

#include "interface.h"

class Libp2pModuleInterface : public PluginInterface
{
public:
    virtual ~Libp2pModuleInterface() {}
    Q_INVOKABLE virtual bool foo(const QString &bar) = 0;

    Q_INVOKABLE virtual bool libp2pStart() = 0;
    Q_INVOKABLE virtual bool libp2pStop() = 0;

    /* Kademlia */
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
