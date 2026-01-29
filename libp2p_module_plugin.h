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
    Q_INVOKABLE bool libp2pStart() override;
    Q_INVOKABLE bool libp2pStop() override;

    /* Kademlia-related functions */
    Q_INVOKABLE bool findNode(const QString &peerId);
    Q_INVOKABLE bool putValue(const QByteArray &key, const QByteArray &value);
    Q_INVOKABLE bool getValue(const QByteArray &key, int quorum = -1);
    Q_INVOKABLE bool addProvider(const QString &cid);
    Q_INVOKABLE bool startProviding(const QString &cid);
    Q_INVOKABLE bool stopProviding(const QString &cid);
    Q_INVOKABLE bool getProviders(const QString &cid);

    Q_INVOKABLE bool setEventCallback() override;

    Q_INVOKABLE void initLogos(LogosAPI* logosAPIInstance);


signals:
    void libp2pEvent(
        QString caller,
        int result,
        QString message,
        QVariant data
    );
    void eventResponse(const QString& eventName, const QVariantList& data);

private slots:
    void onLibp2pEventDefault(
        const QString &caller,
        int result,
        const QString &message,
        const QVariant &data
    );

private:
    libp2p_ctx_t *ctx = nullptr;
    libp2p_config_t config = {};
    QString lastCaller; // for logging

    static void libp2pCallback(
        int callerRet,
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
};

