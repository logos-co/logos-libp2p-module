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

signals:
    // Generic async response
    void response(
        const QString &operation,
        int result,
        const QString &message,
        const QVariant &data);

private:
    libp2p_ctx_t *ctx = nullptr;
    void eventResponse(const QString& eventName, const QVariantList& data);

    static void genericCallback(
        int callerRet,
        const char *msg,
        size_t len,
        void *userData
    );
};
